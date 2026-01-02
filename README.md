# Blake3-Golang

High-performance BLAKE3 implemented in idiomatic Go with optional amd64 assembly
(SSE4.1/AVX2) for acceleration and a separate C/NASM benchmark harness aligned
with FP_ASM_LIB calling conventions. This repo targets correctness first, then
performance with hard data and repeatable benchmarks.

Reference spec and upstream implementation:
https://github.com/BLAKE3-team/BLAKE3

## Highlights
- Go API that mirrors the standard hash.Hash patterns plus BLAKE3 XOF output.
- Streaming support with progress callbacks for large inputs.
- SSE4.1 row-based compression on amd64 for short inputs.
- AVX2-accelerated chunk hashing and parent reduction on amd64 (Go assembly).
- Parallel chunk hashing for large inputs in Sum256 on amd64.
- C/NASM AVX2 8-way compressor for FP_ASM_LIB-style benchmarking.
- Portable pure-Go fallback for non-amd64 or without SIMD; the fastest path is
  amd64-only.

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

Interleaved 10-run comparison (from `tools/bench/compare.ps1`, Go vs upstream
reference C; each run alternates Go and ref C to reduce thermal bias):

Go Sum256 (MB/s, avg/min/max/std):
- 1K: 573.41 / 437.91 / 654.83 / 69.79
- 8K: 2081.64 / 1300.10 / 2558.24 / 456.41
- 1M: 3557.86 / 2174.63 / 4436.04 / 737.16

Ref C (MB/s, avg/min/max/std):
- 1K: 777.52 / 637.56 / 832.43 / 59.79
- 8K: 2702.10 / 1873.27 / 3011.68 / 395.55
- 1M: 2702.58 / 1307.83 / 3211.86 / 557.93

Relative Go vs Ref:
- 1K: -26.25%
- 8K: -22.96%
- 1M: +31.65%
- Mean across sizes: +0.50%
- Weighted by bytes (1K/8K/1M): +31.21%

C benchmark (FP_ASM_LIB-style NASM AVX2 8-way compressor, `tools/fp_bench/run.ps1`):
- 1K: 197.17 MB/s
- 8K: 282.82 MB/s
- 1M: 1013.57 MB/s

Note on Go `B/op` and `allocs/op`: Sum256 uses small temporary buffers and
spawns goroutines for large inputs, so you may see small allocations there;
streaming Hasher.Write remains allocation-free.

## Design notes and tradeoffs vs the reference implementation
- Go implementation uses AVX2 for chunk batching and parent reduction, with
  parallel chunk hashing for large inputs in Sum256; the streaming Hasher
  remains single-threaded for predictable incremental behavior.
- The Go SIMD paths are Go assembly optimized for the Go ABI; no cgo is used,
  but the accelerated code is architecture-specific, with a portable pure-Go
  fallback on other platforms.
- The reference C implementation is more aggressively tuned (wider SIMD, tighter
  scheduling, and multiple dispatch paths). It remains the peak-performance
  baseline on this CPU.
- The FP_ASM_LIB NASM path is intentionally ABI-clean and uses the library's
  standard prologue/epilogue, favoring portability and integration over absolute
  throughput. It uses 8-way SIMD with a loop-free C harness; small inputs can
  pay a setup/transpose tax, while large inputs see higher throughput.

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

Interleaved 10-run Go vs reference:
```powershell
cd C:\Users\baian\GOLANG\Blake3-Golang
tools\bench\compare.ps1
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
- `blake3/`: Go implementation (portable core + amd64 assembly for SSE4.1/AVX2).
- `tools/fp_bench/`: C/NASM bench harness using FP_ASM_LIB-style AVX2.
- `tools/ref_bench/`: benchmark harness for upstream reference C.
- `tools/bench/`: interleaved Go vs reference benchmark script.

## Status
Correctness tests pass for the Go and C paths. Performance is measured and
tracked; short-input throughput still trails the reference baseline on this CPU
while large inputs exceed it.

## License
MIT. See `LICENSE`.
