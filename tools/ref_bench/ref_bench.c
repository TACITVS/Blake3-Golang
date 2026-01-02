#include "blake3.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static volatile uint8_t sink;

static double now_seconds(void) {
  LARGE_INTEGER freq;
  LARGE_INTEGER counter;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&counter);
  return (double)counter.QuadPart / (double)freq.QuadPart;
}

static void fill_pattern(uint8_t *buf, size_t n) {
  for (size_t i = 0; i < n; i++) {
    buf[i] = (uint8_t)(i % 251);
  }
}

static void hash_once(const uint8_t *buf, size_t n, uint8_t out[BLAKE3_OUT_LEN]) {
  blake3_hasher hasher;
  blake3_hasher_init(&hasher);
  blake3_hasher_update(&hasher, buf, n);
  blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);
  sink ^= out[0];
}

static void bench_size(size_t n, double target_seconds) {
  uint8_t *buf = (uint8_t *)malloc(n);
  if (!buf) {
    fprintf(stderr, "alloc failed\n");
    exit(1);
  }
  fill_pattern(buf, n);

  uint8_t out[BLAKE3_OUT_LEN];
  uint64_t iters = 0;
  double start = now_seconds();
  double elapsed = 0.0;
  while (elapsed < target_seconds) {
    hash_once(buf, n, out);
    iters++;
    elapsed = now_seconds() - start;
  }
  if (elapsed == 0.0) {
    elapsed = 1e-9;
  }

  double bytes = (double)n * (double)iters;
  double mbps = bytes / (1024.0 * 1024.0) / elapsed;
  double ns_per_op = (elapsed * 1e9) / (double)iters;

  printf("ref_c size=%zu bytes  iters=%llu  ns/op=%.0f  MB/s=%.2f\n",
         n, (unsigned long long)iters, ns_per_op, mbps);
  free(buf);
}

int main(void) {
  const double target_seconds = 1.0;
  bench_size(1024, target_seconds);
  bench_size(8 * 1024, target_seconds);
  bench_size(1 * 1024 * 1024, target_seconds);
  return 0;
}
