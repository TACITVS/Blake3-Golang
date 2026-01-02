package blake3

import (
	"encoding/binary"
	"hash"
)

const maxChunkBatch = 8
const avx2MinChunks = 16

type output struct {
	inputChainingValue [8]uint32
	blockWords         [16]uint32
	counter            uint64
	blockLen           uint32
	flags              uint32
}

func (o output) chainingValue() [8]uint32 {
	return first8Words(compress(
		&o.inputChainingValue,
		&o.blockWords,
		o.counter,
		o.blockLen,
		o.flags,
	))
}

func (o output) rootBytes(out []byte) {
	var outputBlockCounter uint64
	for len(out) > 0 {
		words := compress(
			&o.inputChainingValue,
			&o.blockWords,
			outputBlockCounter,
			o.blockLen,
			o.flags|root,
		)
		for i := 0; i < 16 && len(out) > 0; i++ {
			if len(out) >= 4 {
				binary.LittleEndian.PutUint32(out, words[i])
				out = out[4:]
				continue
			}
			var tmp [4]byte
			binary.LittleEndian.PutUint32(tmp[:], words[i])
			copy(out, tmp[:len(out)])
			return
		}
		outputBlockCounter++
	}
}

type chunkState struct {
	chainingValue    [8]uint32
	chunkCounter     uint64
	block            [BlockLen]byte
	blockLen         uint8
	blocksCompressed uint8
	flags            uint32
}

func newChunkState(keyWords [8]uint32, chunkCounter uint64, flags uint32) chunkState {
	return chunkState{
		chainingValue: keyWords,
		chunkCounter:  chunkCounter,
		flags:         flags,
	}
}

func (c *chunkState) len() int {
	return BlockLen*int(c.blocksCompressed) + int(c.blockLen)
}

func (c *chunkState) startFlag() uint32 {
	if c.blocksCompressed == 0 {
		return chunkStart
	}
	return 0
}

func (c *chunkState) update(input []byte) {
	for len(input) > 0 {
		if c.blockLen == BlockLen {
			var blockWords [16]uint32
			loadWords(&blockWords, c.block[:])
			c.chainingValue = first8Words(compress(
				&c.chainingValue,
				&blockWords,
				c.chunkCounter,
				BlockLen,
				c.flags|c.startFlag(),
			))
			c.blocksCompressed++
			clear(c.block[:])
			c.blockLen = 0
		}

		want := BlockLen - int(c.blockLen)
		if want > len(input) {
			want = len(input)
		}
		copy(c.block[int(c.blockLen):], input[:want])
		c.blockLen += uint8(want)
		input = input[want:]
	}
}

func (c *chunkState) output() output {
	var blockWords [16]uint32
	loadWords(&blockWords, c.block[:])
	return output{
		inputChainingValue: c.chainingValue,
		blockWords:         blockWords,
		counter:            c.chunkCounter,
		blockLen:           uint32(c.blockLen),
		flags:              c.flags | c.startFlag() | chunkEnd,
	}
}

func parentOutput(
	leftChildCV [8]uint32,
	rightChildCV [8]uint32,
	keyWords [8]uint32,
	flags uint32,
) output {
	var blockWords [16]uint32
	copy(blockWords[:8], leftChildCV[:])
	copy(blockWords[8:], rightChildCV[:])
	return output{
		inputChainingValue: keyWords,
		blockWords:         blockWords,
		counter:            0,
		blockLen:           BlockLen,
		flags:              parent | flags,
	}
}

func parentCV(
	leftChildCV [8]uint32,
	rightChildCV [8]uint32,
	keyWords [8]uint32,
	flags uint32,
) [8]uint32 {
	return parentOutput(leftChildCV, rightChildCV, keyWords, flags).chainingValue()
}

func chunkCVFull(input []byte, keyWords [8]uint32, chunkCounter uint64, flags uint32) [8]uint32 {
	var cv = keyWords
	var blockWords [16]uint32
	for block := 0; block < ChunkLen/BlockLen; block++ {
		loadWords(&blockWords, input[block*BlockLen:])
		blockFlags := flags
		if block == 0 {
			blockFlags |= chunkStart
		}
		if block == (ChunkLen/BlockLen)-1 {
			blockFlags |= chunkEnd
		}
		cv = first8Words(compress(&cv, &blockWords, chunkCounter, BlockLen, blockFlags))
	}
	return cv
}

// Hasher is a streaming BLAKE3 hasher with extendable output.
type Hasher struct {
	chunkState chunkState
	keyWords   [8]uint32
	cvStack    [54][8]uint32
	cvStackLen uint8
	flags      uint32
}

var _ hash.Hash = (*Hasher)(nil)

func newHasher(keyWords [8]uint32, flags uint32) *Hasher {
	return &Hasher{
		chunkState: newChunkState(keyWords, 0, flags),
		keyWords:   keyWords,
		flags:      flags,
	}
}

// New constructs a new hasher for the standard hash function.
func New() *Hasher {
	return newHasher(iv, 0)
}

// NewKeyed constructs a new hasher for the keyed hash function.
func NewKeyed(key [KeyLen]byte) *Hasher {
	return newHasher(keyWordsFromBytes(&key), keyedHash)
}

// NewDeriveKey constructs a new hasher for the key derivation function.
func NewDeriveKey(context string) *Hasher {
	contextHasher := newHasher(iv, deriveKeyContext)
	_, _ = contextHasher.Write([]byte(context))
	var contextKey [KeyLen]byte
	contextHasher.finalize(contextKey[:])
	return newHasher(keyWordsFromBytes(&contextKey), deriveKeyMaterial)
}

func (h *Hasher) pushStack(cv [8]uint32) {
	h.cvStack[h.cvStackLen] = cv
	h.cvStackLen++
}

func (h *Hasher) popStack() [8]uint32 {
	h.cvStackLen--
	return h.cvStack[h.cvStackLen]
}

func (h *Hasher) addChunkChainingValue(newCV [8]uint32, totalChunks uint64) {
	for totalChunks&1 == 0 {
		newCV = parentCV(h.popStack(), newCV, h.keyWords, h.flags)
		totalChunks >>= 1
	}
	h.pushStack(newCV)
}

// Write adds input to the hash state.
func (h *Hasher) Write(p []byte) (int, error) {
	n := len(p)
	for len(p) > 0 {
		if h.chunkState.len() == 0 && len(p) >= ChunkLen {
			fullChunks := len(p) / ChunkLen
			if len(p)%ChunkLen == 0 {
				fullChunks--
			}
			if fullChunks > 0 {
				chunkCounter := h.chunkState.chunkCounter
				if haveAVX2 && fullChunks < avx2MinChunks {
					for i := 0; i < fullChunks; i++ {
						cv := chunkCVFull(p[:ChunkLen], h.keyWords, chunkCounter, h.flags)
						totalChunks := chunkCounter + 1
						h.addChunkChainingValue(cv, totalChunks)
						chunkCounter = totalChunks
						p = p[ChunkLen:]
					}
					h.chunkState = newChunkState(h.keyWords, chunkCounter, h.flags)
					continue
				}
				var cvBatch [maxChunkBatch][8]uint32
				for fullChunks > 0 {
					batch := fullChunks
					if batch > maxChunkBatch {
						batch = maxChunkBatch
					}
					chunkCVs(p[:batch*ChunkLen], h.keyWords, chunkCounter, h.flags, cvBatch[:batch])
					for i := 0; i < batch; i++ {
						totalChunks := chunkCounter + 1
						h.addChunkChainingValue(cvBatch[i], totalChunks)
						chunkCounter = totalChunks
					}
					p = p[batch*ChunkLen:]
					fullChunks -= batch
				}
				h.chunkState = newChunkState(h.keyWords, chunkCounter, h.flags)
				continue
			}
		}

		if h.chunkState.len() == ChunkLen {
			chunkCV := h.chunkState.output().chainingValue()
			totalChunks := h.chunkState.chunkCounter + 1
			h.addChunkChainingValue(chunkCV, totalChunks)
			h.chunkState = newChunkState(h.keyWords, totalChunks, h.flags)
		}

		want := ChunkLen - h.chunkState.len()
		if want > len(p) {
			want = len(p)
		}
		h.chunkState.update(p[:want])
		p = p[want:]
	}
	return n, nil
}

// Sum appends the 32-byte hash to b and returns the resulting slice.
func (h *Hasher) Sum(b []byte) []byte {
	var out [OutLen]byte
	h.finalize(out[:])
	return append(b, out[:]...)
}

// Reset clears the hash state and keeps the same key/flags configuration.
func (h *Hasher) Reset() {
	h.chunkState = newChunkState(h.keyWords, 0, h.flags)
	h.cvStackLen = 0
}

// Size returns the default output size of BLAKE3.
func (h *Hasher) Size() int { return OutLen }

// BlockSize returns the block size of the underlying compression function.
func (h *Hasher) BlockSize() int { return BlockLen }

// Finalize writes any number of output bytes into out.
func (h *Hasher) Finalize(out []byte) {
	h.finalize(out)
}

// Sum256 returns the 32-byte BLAKE3 hash of the current state.
func (h *Hasher) Sum256() [OutLen]byte {
	var out [OutLen]byte
	h.finalize(out[:])
	return out
}

func (h *Hasher) finalize(out []byte) {
	output := h.chunkState.output()
	for i := int(h.cvStackLen) - 1; i >= 0; i-- {
		output = parentOutput(
			h.cvStack[i],
			output.chainingValue(),
			h.keyWords,
			h.flags,
		)
	}
	output.rootBytes(out)
}

// Sum256 returns the 32-byte BLAKE3 hash of data.
func Sum256(data []byte) [OutLen]byte {
	h := New()
	_, _ = h.Write(data)
	return h.Sum256()
}

// Sum writes an extended-length hash of data into out.
func Sum(data []byte, out []byte) {
	h := New()
	_, _ = h.Write(data)
	h.Finalize(out)
}

// SumKeyed returns the 32-byte keyed BLAKE3 hash of data.
func SumKeyed(key [KeyLen]byte, data []byte) [OutLen]byte {
	h := NewKeyed(key)
	_, _ = h.Write(data)
	return h.Sum256()
}

// DeriveKey returns a derived key of length out using the given context string.
func DeriveKey(context string, out []byte) {
	h := NewDeriveKey(context)
	h.Finalize(out)
}
