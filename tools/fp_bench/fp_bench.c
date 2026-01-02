#include "fp_blake3_fast.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static void hash_once(const uint8_t *buf, size_t n,
                      uint8_t out[FP_BLAKE3_OUT_LEN]) {
    FpBlake3Hasher hasher;
    fp_blake3_hasher_init(&hasher);
    fp_blake3_hasher_update(&hasher, buf, n);
    fp_blake3_hasher_finalize(&hasher, out);
    sink ^= out[0];
}

static void print_hex(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", buf[i]);
    }
    printf("\n");
}

static void self_test(void) {
    static const uint8_t expected[FP_BLAKE3_OUT_LEN] = {
        0xaf, 0x13, 0x49, 0xb9, 0xf5, 0xf9, 0xa1, 0xa6,
        0xa0, 0x40, 0x4d, 0xea, 0x36, 0xdc, 0xc9, 0x49,
        0x9b, 0xcb, 0x25, 0xc9, 0xad, 0xc1, 0x12, 0xb7,
        0xcc, 0x9a, 0x93, 0xca, 0xe4, 0x1f, 0x32, 0x62,
    };
    uint8_t out[FP_BLAKE3_OUT_LEN];
    fp_blake3_hash(NULL, 0, out);
    if (memcmp(out, expected, sizeof(expected)) != 0) {
        fprintf(stderr, "self-test failed for empty input\n");
        fprintf(stderr, "got: ");
        print_hex(out, sizeof(out));
        exit(1);
    }
}

static void bench_size(size_t n, double target_seconds) {
    uint8_t *buf = (uint8_t *)malloc(n);
    if (!buf) {
        fprintf(stderr, "alloc failed\n");
        exit(1);
    }
    fill_pattern(buf, n);

    uint8_t out[FP_BLAKE3_OUT_LEN];
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

    printf("fp_c size=%zu bytes  iters=%llu  ns/op=%.0f  MB/s=%.2f\n",
           n, (unsigned long long)iters, ns_per_op, mbps);
    free(buf);
}

int main(void) {
    self_test();
    const double target_seconds = 1.0;
    bench_size(1024, target_seconds);
    bench_size(8 * 1024, target_seconds);
    bench_size(1 * 1024 * 1024, target_seconds);
    return 0;
}
