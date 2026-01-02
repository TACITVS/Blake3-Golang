package blake3

import (
	"crypto/sha256"
	"testing"
)

func BenchmarkSum256_1K(b *testing.B) {
	data := patternBytes(1024)
	b.SetBytes(int64(len(data)))
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = Sum256(data)
	}
}

func BenchmarkSum256_8K(b *testing.B) {
	data := patternBytes(8 * 1024)
	b.SetBytes(int64(len(data)))
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = Sum256(data)
	}
}

func BenchmarkSum256_1M(b *testing.B) {
	data := patternBytes(1 << 20)
	b.SetBytes(int64(len(data)))
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = Sum256(data)
	}
}

func BenchmarkHasherWrite_1M(b *testing.B) {
	data := patternBytes(1 << 20)
	b.SetBytes(int64(len(data)))
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		h := New()
		_, _ = h.Write(data)
		_ = h.Sum256()
	}
}

func BenchmarkSHA256_1M(b *testing.B) {
	data := patternBytes(1 << 20)
	b.SetBytes(int64(len(data)))
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		_ = sha256.Sum256(data)
	}
}
