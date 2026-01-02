//go:build amd64 && !purego

package blake3

import (
	"runtime"
	"sync"
)

const parallelMinChunks = 128 // 128 KiB in 1 KiB chunks.

var cvPool = sync.Pool{
	New: func() any {
		return make([][8]uint32, 0, parallelMinChunks)
	},
}

func sum256Fast(data []byte, keyWords [8]uint32, flags uint32) ([OutLen]byte, bool) {
	var out [OutLen]byte
	if !haveAVX2 && !haveSSE41 {
		return out, false
	}
	if len(data) <= ChunkLen {
		cs := newChunkState(keyWords, 0, flags)
		cs.update(data)
		cs.output().rootBytes(out[:])
		return out, true
	}

	fullChunks := len(data) / ChunkLen
	rem := len(data) % ChunkLen
	totalChunks := fullChunks
	if rem != 0 {
		totalChunks++
	}
	if totalChunks <= 1 {
		cs := newChunkState(keyWords, 0, flags)
		cs.update(data)
		cs.output().rootBytes(out[:])
		return out, true
	}

	if totalChunks <= maxChunkBatch {
		var cvs [maxChunkBatch][8]uint32
		fillChunkCVs(data, keyWords, flags, cvs[:totalChunks], fullChunks, rem)
		reduceCVsToOutput(cvs[:totalChunks], keyWords, flags).rootBytes(out[:])
		return out, true
	}

	cvs := getCVs(totalChunks)
	fillChunkCVs(data, keyWords, flags, cvs[:totalChunks], fullChunks, rem)
	reduceCVsToOutput(cvs[:totalChunks], keyWords, flags).rootBytes(out[:])
	putCVs(cvs)
	return out, true
}

func fillChunkCVs(data []byte, keyWords [8]uint32, flags uint32, cvs [][8]uint32, fullChunks, rem int) {
	if fullChunks > 0 {
		if shouldParallel(fullChunks) {
			chunkCVsParallel(data[:fullChunks*ChunkLen], keyWords, flags, cvs[:fullChunks])
		} else {
			chunkCVs(data[:fullChunks*ChunkLen], keyWords, 0, flags, cvs[:fullChunks])
		}
	}
	if rem != 0 {
		cs := newChunkState(keyWords, uint64(fullChunks), flags)
		cs.update(data[fullChunks*ChunkLen:])
		cvs[fullChunks] = cs.output().chainingValue()
	}
}

func reduceCVsToOutput(cvs [][8]uint32, keyWords [8]uint32, flags uint32) output {
	level := cvs
	parentFlags := uint8(parent | flags)
	for len(level) > 2 {
		outLen := len(level) / 2
		if haveAVX2 && outLen >= 8 {
			outIdx := 0
			var left [64]uint32
			var right [64]uint32
			var outBuf [64]uint32
			for outLen-outIdx >= 8 {
				for lane := 0; lane < 8; lane++ {
					l := level[(outIdx+lane)*2]
					r := level[(outIdx+lane)*2+1]
					left[lane+0] = l[0]
					left[lane+8] = l[1]
					left[lane+16] = l[2]
					left[lane+24] = l[3]
					left[lane+32] = l[4]
					left[lane+40] = l[5]
					left[lane+48] = l[6]
					left[lane+56] = l[7]
					right[lane+0] = r[0]
					right[lane+8] = r[1]
					right[lane+16] = r[2]
					right[lane+24] = r[3]
					right[lane+32] = r[4]
					right[lane+40] = r[5]
					right[lane+48] = r[6]
					right[lane+56] = r[7]
				}
				hashPAVX2(&left, &right, parentFlags, &keyWords, &outBuf, 8)
				for lane := 0; lane < 8; lane++ {
					level[outIdx+lane][0] = outBuf[lane+0]
					level[outIdx+lane][1] = outBuf[lane+8]
					level[outIdx+lane][2] = outBuf[lane+16]
					level[outIdx+lane][3] = outBuf[lane+24]
					level[outIdx+lane][4] = outBuf[lane+32]
					level[outIdx+lane][5] = outBuf[lane+40]
					level[outIdx+lane][6] = outBuf[lane+48]
					level[outIdx+lane][7] = outBuf[lane+56]
				}
				outIdx += 8
			}
			for i := outIdx; i < outLen; i++ {
				level[i] = parentCV(level[i*2], level[i*2+1], keyWords, flags)
			}
		} else {
			for i := 0; i < outLen; i++ {
				level[i] = parentCV(level[i*2], level[i*2+1], keyWords, flags)
			}
		}
		if len(level)%2 == 1 {
			level[outLen] = level[len(level)-1]
			outLen++
		}
		level = level[:outLen]
	}
	return parentOutput(level[0], level[1], keyWords, flags)
}

func shouldParallel(fullChunks int) bool {
	if fullChunks < parallelMinChunks {
		return false
	}
	return runtime.GOMAXPROCS(0) > 1
}

func chunkCVsParallel(data []byte, keyWords [8]uint32, flags uint32, out [][8]uint32) {
	chunks := len(out)
	if chunks == 0 {
		return
	}
	workers := runtime.GOMAXPROCS(0)
	if workers > chunks {
		workers = chunks
	}
	if haveAVX2 {
		minPerWorker := 8
		if maxWorkers := chunks / minPerWorker; workers > maxWorkers {
			workers = maxWorkers
		}
	}
	if workers < 2 {
		chunkCVs(data, keyWords, 0, flags, out)
		return
	}
	var wg sync.WaitGroup
	start := 0
	base := chunks / workers
	extra := chunks % workers
	for i := 0; i < workers; i++ {
		n := base
		if i < extra {
			n++
		}
		end := start + n
		wg.Add(1)
		go func(start, end int) {
			chunkCVs(data[start*ChunkLen:end*ChunkLen], keyWords, uint64(start), flags, out[start:end])
			wg.Done()
		}(start, end)
		start = end
	}
	wg.Wait()
}

func getCVs(chunks int) [][8]uint32 {
	cvs := cvPool.Get().([][8]uint32)
	if cap(cvs) < chunks {
		return make([][8]uint32, chunks)
	}
	return cvs[:chunks]
}

func putCVs(cvs [][8]uint32) {
	cvPool.Put(cvs[:0])
}
