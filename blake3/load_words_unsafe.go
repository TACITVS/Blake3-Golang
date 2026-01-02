//go:build amd64 || 386 || arm64 || arm || riscv64 || ppc64le || mipsle || mips64le || loong64

package blake3

import "unsafe"

func loadWords(dst *[16]uint32, b []byte) {
	_ = b[BlockLen-1]
	if uintptr(unsafe.Pointer(&b[0]))&3 == 0 {
		*dst = *(*[16]uint32)(unsafe.Pointer(&b[0]))
		return
	}
	loadWordsSlow(dst, b)
}
