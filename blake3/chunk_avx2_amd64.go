//go:build amd64 && !purego

package blake3

import "unsafe"

func chunkCVsAVX2(input []byte, keyWords [8]uint32, counter uint64, flags uint32, out [][8]uint32) {
	if len(out) < 8 {
		chunkCVsPortable(input, keyWords, counter, flags, out)
		return
	}

	var outBuf [64]uint32
	var chain [8]uint32
	offset := 0
	for offset+8 <= len(out) {
		base := offset * ChunkLen
		hashFAVX2(
			(*[8192]byte)(unsafe.Pointer(&input[base])),
			ChunkLen*8,
			counter+uint64(offset),
			flags,
			&keyWords,
			&outBuf,
			&chain,
		)
		for lane := 0; lane < 8; lane++ {
			dst := &out[offset+lane]
			dst[0] = outBuf[lane+0]
			dst[1] = outBuf[lane+8]
			dst[2] = outBuf[lane+16]
			dst[3] = outBuf[lane+24]
			dst[4] = outBuf[lane+32]
			dst[5] = outBuf[lane+40]
			dst[6] = outBuf[lane+48]
			dst[7] = outBuf[lane+56]
		}
		offset += 8
	}

	if offset < len(out) {
		chunkCVsPortable(input[offset*ChunkLen:], keyWords, counter+uint64(offset), flags, out[offset:])
	}
}
