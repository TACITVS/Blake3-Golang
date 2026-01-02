package blake3

import "math/bits"

func g(state *[16]uint32, a, b, c, d int, mx, my uint32) {
	state[a] = state[a] + state[b] + mx
	state[d] = bits.RotateLeft32(state[d]^state[a], -16)
	state[c] = state[c] + state[d]
	state[b] = bits.RotateLeft32(state[b]^state[c], -12)
	state[a] = state[a] + state[b] + my
	state[d] = bits.RotateLeft32(state[d]^state[a], -8)
	state[c] = state[c] + state[d]
	state[b] = bits.RotateLeft32(state[b]^state[c], -7)
}

func round(state *[16]uint32, m *[16]uint32) {
	// Mix the columns.
	g(state, 0, 4, 8, 12, m[0], m[1])
	g(state, 1, 5, 9, 13, m[2], m[3])
	g(state, 2, 6, 10, 14, m[4], m[5])
	g(state, 3, 7, 11, 15, m[6], m[7])
	// Mix the diagonals.
	g(state, 0, 5, 10, 15, m[8], m[9])
	g(state, 1, 6, 11, 12, m[10], m[11])
	g(state, 2, 7, 8, 13, m[12], m[13])
	g(state, 3, 4, 9, 14, m[14], m[15])
}

func permute(m *[16]uint32) {
	var permuted [16]uint32
	for i := 0; i < 16; i++ {
		permuted[i] = m[msgPermutation[i]]
	}
	*m = permuted
}

func compress(
	cv *[8]uint32,
	block *[16]uint32,
	counter uint64,
	blockLen uint32,
	flags uint32,
) [16]uint32 {
	var state [16]uint32
	state[0] = cv[0]
	state[1] = cv[1]
	state[2] = cv[2]
	state[3] = cv[3]
	state[4] = cv[4]
	state[5] = cv[5]
	state[6] = cv[6]
	state[7] = cv[7]
	state[8] = iv[0]
	state[9] = iv[1]
	state[10] = iv[2]
	state[11] = iv[3]
	state[12] = uint32(counter)
	state[13] = uint32(counter >> 32)
	state[14] = blockLen
	state[15] = flags

	blockWords := *block

	round(&state, &blockWords) // 1
	permute(&blockWords)
	round(&state, &blockWords) // 2
	permute(&blockWords)
	round(&state, &blockWords) // 3
	permute(&blockWords)
	round(&state, &blockWords) // 4
	permute(&blockWords)
	round(&state, &blockWords) // 5
	permute(&blockWords)
	round(&state, &blockWords) // 6
	permute(&blockWords)
	round(&state, &blockWords) // 7

	for i := 0; i < 8; i++ {
		state[i] ^= state[i+8]
		state[i+8] ^= cv[i]
	}
	return state
}

func first8Words(out [16]uint32) [8]uint32 {
	return [8]uint32{
		out[0], out[1], out[2], out[3],
		out[4], out[5], out[6], out[7],
	}
}
