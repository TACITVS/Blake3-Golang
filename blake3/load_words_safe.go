//go:build !(amd64 || 386 || arm64 || arm || riscv64 || ppc64le || mipsle || mips64le || loong64)

package blake3

func loadWords(dst *[16]uint32, b []byte) {
	loadWordsSlow(dst, b)
}
