//go:build amd64 && !purego

package blake3

func chunkCVs(input []byte, keyWords [8]uint32, counter uint64, flags uint32, out [][8]uint32) {
	if haveAVX2 {
		chunkCVsAVX2(input, keyWords, counter, flags, out)
		return
	}
	chunkCVsPortable(input, keyWords, counter, flags, out)
}
