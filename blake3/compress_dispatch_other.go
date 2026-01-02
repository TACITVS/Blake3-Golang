//go:build !amd64

package blake3

func compress(
	cv *[8]uint32,
	block *[16]uint32,
	counter uint64,
	blockLen uint32,
	flags uint32,
) [16]uint32 {
	return compressPortable(cv, block, counter, blockLen, flags)
}
