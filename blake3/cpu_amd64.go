//go:build amd64 && !purego

package blake3

var haveAVX2 = detectAVX2()
var haveSSE41 = detectSSE41()

func detectSSE41() bool {
	_, _, c, _ := cpuid(1, 0)
	const ssse3 = 1 << 9
	const sse41 = 1 << 19
	return c&ssse3 != 0 && c&sse41 != 0
}

func detectAVX2() bool {
	_, _, c, _ := cpuid(1, 0)
	if c&(1<<27) == 0 { // OSXSAVE
		return false
	}
	if c&(1<<28) == 0 { // AVX
		return false
	}
	if xcr0()&0x6 != 0x6 { // XMM and YMM state enabled
		return false
	}
	_, b, _, _ := cpuid(7, 0)
	return b&(1<<5) != 0
}

func xcr0() uint64 {
	eax, edx := xgetbv()
	return uint64(edx)<<32 | uint64(eax)
}

func cpuid(eaxArg, ecxArg uint32) (eax, ebx, ecx, edx uint32)
func xgetbv() (eax, edx uint32)
