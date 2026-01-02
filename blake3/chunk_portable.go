package blake3

func chunkCVsPortable(input []byte, keyWords [8]uint32, counter uint64, flags uint32, out [][8]uint32) {
	for i := range out {
		start := i * ChunkLen
		out[i] = chunkCVFull(input[start:start+ChunkLen], keyWords, counter+uint64(i), flags)
	}
}
