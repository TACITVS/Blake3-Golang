//go:build !amd64

package blake3

func chunkCVs(input []byte, keyWords [8]uint32, counter uint64, flags uint32, out [][8]uint32) {
	chunkCVsPortable(input, keyWords, counter, flags, out)
}
