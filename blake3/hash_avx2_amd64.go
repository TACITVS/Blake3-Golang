//go:build amd64

package blake3

//go:noescape
func hashFAVX2(input *[8192]byte, length, counter uint64, flags uint32, key *[8]uint32, out *[64]uint32, chain *[8]uint32)

//go:noescape
func hashPAVX2(left, right *[64]uint32, flags uint8, key *[8]uint32, out *[64]uint32, n int)
