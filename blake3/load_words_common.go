package blake3

import "encoding/binary"

func loadWordsSlow(dst *[16]uint32, b []byte) {
	_ = b[BlockLen-1]
	for i := 0; i < 16; i++ {
		dst[i] = binary.LittleEndian.Uint32(b[i*4:])
	}
}

func keyWordsFromBytes(key *[KeyLen]byte) [8]uint32 {
	var words [8]uint32
	for i := 0; i < 8; i++ {
		words[i] = binary.LittleEndian.Uint32(key[i*4:])
	}
	return words
}
