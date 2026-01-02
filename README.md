# Blake3-Golang

High-performance BLAKE3 implemented in idiomatic Go, with AVX2 acceleration on
amd64 and a separate C/NASM benchmark harness aligned with FP_ASM_LIB calling
conventions. This repo targets correctness first, then performance with hard
data and repeatable benchmarks.

Reference spec and upstream implementation:
https://github.com/BLAKE3-team/BLAKE3

## Highlights
- Go API that mirrors the standard hash.Hash patterns plus BLAKE3 XOF output.
- Streaming support with progress callbacks for large inputs.
- AVX2-accelerated chunk hashing on amd64 (Go assembly).
- C/NASM AVX2 4-way compressor for FP_ASM_LIB-style benchmarking.
- Portable fallbacks for non-AVX2 systems.

## Quick start (Go)
```go
package main

import (
    "fmt"
    "github.com/TACITVS/Blake3-Golang/blake3"
)

func main() {
    sum := blake3.Sum256([]byte("hello"))
    fmt.Printf("%x\n", sum)
}
```

## Streaming with progress
```go
package main

import (
    "fmt"
    "github.com/TACITVS/Blake3-Golang/blake3"
)

func main() {
    sum, err := blake3.HashFile("big.bin", 256*1024, func(p blake3.Progress) {
        fmt.Printf("processed=%d total=%d elapsed=%s\n", p.Processed, p.Total, p.Elapsed)
    })
    if err != nil {
        panic(err)
    }
    fmt.Printf("%x\n", sum)
}
```

For streaming a reader directly, use:
```go
h := blake3.New()
_, err := h.WriteReader(r, nil, totalBytes, onProgress)
```

## Benchmarks
Environment:
- OS: Windows (goos=windows)
- CPU: Intel(R) Core(TM) i7-4600M CPU @ 2.90GHz
- Go: amd64 (goarch=amd64)
- GCC: 15.1.0 (C:\msys64\mingw64\bin\gcc.exe)
- NASM: C:\Users\baian\AppData\Local\bin\NASM\nasm.exe

Go benchmarks (from `go test ./blake3 -run=^$ -bench=Benchmark -benchmem`):
- Sum256 1K: 163.79 MB/s
- Sum256 8K: 161.58 MB/s
- Sum256 1M: 1191.29 MB/s
- Hasher.Write 1M: 1190.06 MB/s
- SHA256 1M: 383.92 MB/s

C benchmark (FP_ASM_LIB-style NASM AVX2 4-way compressor, `tools/fp_bench/run.ps1`):
- 1K: 224.49 MB/s
- 8K: 349.52 MB/s
- 1M: 812.47 MB/s

Reference C benchmark (upstream BLAKE3, `tools/ref_bench/run.ps1`):
- 1K: 826.69 MB/s
- 8K: 3044.57 MB/s
- 1M: 3289.13 MB/s

Note on Go `B/op` and `allocs/op`: the benchmarks allocate input buffers once
and keep the hot loop allocation-free, so Go reports 0 B/op and 0 allocs/op.

## Design notes and tradeoffs vs the reference implementation
- Go implementation is single-threaded but uses AVX2 for chunk batching; this
  keeps the API simple and deterministic while still gaining SIMD speedups.
- The Go AVX2 path is assembly generated and optimized for Go's ABI; it avoids
  cgo and keeps portability across platforms with portable fallbacks.
- The reference C implementation is more aggressively tuned (wider SIMD, tighter
  scheduling, and multiple dispatch paths). It remains the peak-performance
  baseline on this CPU.
- The FP_ASM_LIB NASM path is intentionally ABI-clean and uses the library's
  standard prologue/epilogue, favoring portability and integration over absolute
  throughput. It currently uses 4-way SIMD and can be expanded to match the
  reference layout later.

## API overview
- `Sum256(data []byte) [32]byte`
- `Sum(data []byte, out []byte)` (XOF)
- `SumKeyed(key [32]byte, data []byte) [32]byte`
- `DeriveKey(context string, out []byte)`
- `New()` / `NewKeyed(key)` / `NewDeriveKey(context)`
- `Hasher.Write`, `Hasher.Sum`, `Hasher.Sum256`, `Hasher.Finalize`
- Streaming helpers in `blake3/stream.go` for progress reporting

## Running benchmarks locally
Go:
```powershell
cd C:\Users\baian\GOLANG\Blake3-Golang
go test ./blake3 -run=^$ -bench=Benchmark -benchmem
```

FP_ASM_LIB C benchmark (NASM + GCC):
```powershell
cd C:\Users\baian\GOLANG\Blake3-Golang
tools\fp_bench\run.ps1
```

Reference C benchmark (upstream BLAKE3):
```powershell
cd C:\Users\baian\GOLANG\Blake3-Golang
tools\ref_bench\run.ps1
```

The reference benchmark expects upstream sources at:
`C:\Users\baian\GOLANG\_ref\BLAKE3\c`

## Project layout
- `blake3/`: Go implementation (portable + AVX2 on amd64).
- `tools/fp_bench/`: C/NASM bench harness using FP_ASM_LIB-style AVX2.
- `tools/ref_bench/`: benchmark harness for upstream reference C.

## Status
Correctness tests pass for the Go and C paths. Performance is measured and
documented; further SIMD tuning is planned for the FP_ASM_LIB NASM path to close
the gap with the reference baseline.
