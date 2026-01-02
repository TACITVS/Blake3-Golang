//go:build amd64 && !purego

package blake3

//go:noescape
func compressSSE41(
	chain *[8]uint32,
	block *[16]uint32,
	counter uint64,
	blockLen uint32,
	flags uint32,
	out *[16]uint32,
)
