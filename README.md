# Blake3-Golang

High-performance, pure-Go BLAKE3 with an AVX2 fast path and a streaming API.
This repo is intentionally minimal: only the hashing implementation and the
bench tooling needed to compare against the official reference implementation.

## Overview

BLAKE3 is a modern, cryptographically secure hash function with an XOF (extendable
output) design and a tree-based structure for parallelism. This implementation
focuses on:

- Fast portable Go code with zero external dependencies.
- An AVX2 accelerated path on amd64 where available.
- A streaming API that can report progress while hashing large inputs.
- Compatibility with the official test vectors.

## Features

- Standard, keyed, and derive-key modes.
- Extendable-output API (write any number of output bytes).
- Implements `hash.Hash`.
- Zero allocations in hot paths (unless you ask it to allocate a buffer).
- AVX2 batch compression on amd64, with safe fallbacks elsewhere.

## Usage

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

### Streaming and progress

```go
h := blake3.New()
buf := make([]byte, 256*1024)
_, err := h.WriteReader(reader, buf, totalBytes, func(p blake3.Progress) {
	// Snapshot current digest if needed (does not mutate the hasher).
	_ = h.Sum256()
})
if err != nil {
	// handle error
}
digest := h.Sum256()
```

Notes:
- If `totalBytes` is unknown, pass `0`.
- Progress callback is called on each successful read.
- If you pass a zero-length buffer, a default size (256 KiB) is allocated.

## Implementation approach

This implementation tracks the official BLAKE3 spec, but is optimized for Go:

- **Chunk batching:** For full 1024-byte chunks, hashing can proceed in batches
  to amortize overhead. The batch size is 8 chunks.
- **AVX2 vectorization:** On amd64 with AVX2, we process 8 chunks in parallel
  via `hashFAVX2` (Go assembly). This is gated by CPU feature detection and
  only used when there are at least 16 full chunks available to avoid
  small-input overhead.
- **Zero-allocation hot path:** The core hashing code avoids heap allocations.
  Benchmarks show `0 B/op` and `0 allocs/op` for fixed-size inputs.
- **No cgo:** Everything builds with the Go toolchain for easy portability.

### How this differs from the official reference implementation

The official reference implementation (https://github.com/BLAKE3-team/BLAKE3)
is a multi-language codebase with multiple SIMD backends and a richer API
surface. Key differences:

- **Backend breadth:** The reference includes SSE2/SSE4.1/AVX2/AVX-512/NEON
  backends. This Go implementation currently uses portable Go plus AVX2
  only. That means the reference can be faster on many CPUs, especially
  on x86 with SSE4.1 or on ARM with NEON.
- **Language/runtime:** The reference is in C (and Rust) with hand-tuned
  assembly. This project stays in Go (plus a small AVX2 assembly file) to
  keep portability and a Go-idiomatic API.
- **Parallelism:** The reference can expose threaded parallel hashing.
  This Go implementation is single-threaded; it only uses SIMD within
  a single thread.
- **API focus:** This repo adds a Go-friendly streaming/progress API
  and conforms to `hash.Hash` for easy integration.

In short, the reference is the speed ceiling and feature superset; this repo
targets a fast, idiomatic Go implementation with a minimal surface area.

## AVX2 support

AVX2 is enabled on amd64 when:
- The CPU supports AVX2.
- The OS has AVX enabled (OSXSAVE + XCR0 checks).

If those checks fail, or the input is too small for the threshold, the
implementation falls back to the portable Go path. The AVX2 code path
processes 8 chunks at a time.

## Benchmarks

All benchmarks below were run on Windows/amd64 with Go 1.24.7 and AVX2 enabled.
Your results will vary by CPU, memory, and compiler flags. The commands used
are provided so you can reproduce on your hardware.

### Go benchmarks

Run:
```powershell
go test ./blake3 -bench . -benchmem
```

Observed results:

| Benchmark | Size | Time/op | Throughput | B/op | allocs/op |
| --- | --- | --- | --- | --- | --- |
| BenchmarkSum256_1K | 1 KiB | ~10.4 us | ~98.6 MB/s | 0 | 0 |
| BenchmarkSum256_8K | 8 KiB | ~78.8 us | ~103.9 MB/s | 0 | 0 |
| BenchmarkSum256_1M | 1 MiB | ~1.28 ms | ~819 MB/s | 0 | 0 |
| BenchmarkSum256_1M_4KAligned | 1 MiB | ~1.25 ms | ~838 MB/s | 0 | 0 |
| BenchmarkHasherWrite_1K | 1 KiB | ~9.76 us | ~104.9 MB/s | 0 | 0 |
| BenchmarkHasherWrite_8K | 8 KiB | ~75.3 us | ~108.8 MB/s | 0 | 0 |
| BenchmarkHasherWrite_1M | 1 MiB | ~1.37 ms | ~763 MB/s | 0 | 0 |
| BenchmarkHasherWrite_1M_4KAligned | 1 MiB | ~1.32 ms | ~792 MB/s | 0 | 0 |

Why `B/op` and `allocs/op` are zero:
- The benchmarks allocate the input buffers once, outside the timing loop.
- The hashing code uses stack arrays and avoids heap allocations.
- The streaming API only allocates if you pass a zero-length buffer.

### Reference C benchmarks

This repo includes a small C benchmark harness that targets the official BLAKE3
reference implementation. You need to clone the reference repo locally (no Rust
required).

Setup:
```powershell
git clone https://github.com/BLAKE3-team/BLAKE3 _ref/BLAKE3
```

Run (expects GCC in `C:\msys64\mingw64\bin\gcc.exe`):
```powershell
./tools/ref_bench/run.ps1
```

Observed results (same machine):

| Size | Throughput |
| --- | --- |
| 1 KiB | ~661 MB/s |
| 8 KiB | ~2397 MB/s |
| 1 MiB | ~2550 MB/s |

This gap is expected: the reference uses multiple SIMD backends and more
aggressive low-level tuning. The Go implementation trades some peak speed for
portability and a Go-native API.

## Testing

```powershell
go test ./blake3
```

The test vectors in `blake3/testdata/test_vectors.json` come from the official
reference to validate correctness.

## License

MIT. See `LICENSE`.
