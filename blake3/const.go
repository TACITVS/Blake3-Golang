package blake3

const (
	// Output and key sizes, in bytes.
	OutLen   = 32
	KeyLen   = 32
	BlockLen = 64
	ChunkLen = 1024
)

const (
	chunkStart        uint32 = 1 << 0
	chunkEnd          uint32 = 1 << 1
	parent            uint32 = 1 << 2
	root              uint32 = 1 << 3
	keyedHash         uint32 = 1 << 4
	deriveKeyContext  uint32 = 1 << 5
	deriveKeyMaterial uint32 = 1 << 6
)

var iv = [8]uint32{
	0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
	0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19,
}

var msgPermutation = [16]uint8{2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8}
