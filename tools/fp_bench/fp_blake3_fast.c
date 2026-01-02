#include "fp_blake3_fast.h"

#include <string.h>

// Derived from FP_ASM_LIB's fp_blake3.c, with full tree hashing and streaming.

#ifdef __AVX2__
#include <cpuid.h>
#include <immintrin.h>
#endif

enum {
    CHUNK_START = 1 << 0,
    CHUNK_END = 1 << 1,
    PARENT = 1 << 2,
    ROOT = 1 << 3,
    KEYED_HASH = 1 << 4,
    DERIVE_KEY_CONTEXT = 1 << 5,
    DERIVE_KEY_MATERIAL = 1 << 6,
};

static const uint32_t IV[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
};

static const uint8_t MSG_SCHEDULE[7][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8},
    {3, 4, 10, 12, 13, 2, 7, 14, 6, 5, 9, 0, 11, 15, 8, 1},
    {10, 7, 12, 9, 14, 3, 13, 15, 4, 0, 11, 2, 5, 8, 1, 6},
    {12, 13, 9, 11, 15, 10, 14, 8, 7, 2, 5, 3, 0, 1, 6, 4},
    {9, 14, 11, 5, 8, 12, 15, 1, 13, 3, 0, 10, 2, 6, 4, 7},
    {11, 15, 5, 0, 1, 9, 8, 6, 14, 10, 2, 12, 3, 4, 7, 13},
};

static inline uint32_t rotr32(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

static inline uint32_t load32_le(const uint8_t *p) {
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

static inline void store32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void load_words(uint32_t out[16], const uint8_t block[64]) {
    for (int i = 0; i < 16; i++) {
        out[i] = load32_le(block + (i * 4));
    }
}

static void compress(const uint32_t cv[8],
                     const uint32_t block_words[16],
                     uint64_t counter,
                     uint32_t block_len,
                     uint32_t flags,
                     uint32_t out[16]) {
    uint32_t state[16];

    for (int i = 0; i < 8; i++) {
        state[i] = cv[i];
    }
    state[8] = IV[0];
    state[9] = IV[1];
    state[10] = IV[2];
    state[11] = IV[3];
    state[12] = (uint32_t)counter;
    state[13] = (uint32_t)(counter >> 32);
    state[14] = block_len;
    state[15] = flags;

    for (int round = 0; round < 7; round++) {
        uint32_t msg[16];
        const uint8_t *schedule = MSG_SCHEDULE[round];
        for (int i = 0; i < 16; i++) {
            msg[i] = block_words[schedule[i]];
        }

        state[0] += state[4] + msg[0];
        state[12] = rotr32(state[12] ^ state[0], 16);
        state[8] += state[12];
        state[4] = rotr32(state[4] ^ state[8], 12);

        state[1] += state[5] + msg[2];
        state[13] = rotr32(state[13] ^ state[1], 16);
        state[9] += state[13];
        state[5] = rotr32(state[5] ^ state[9], 12);

        state[2] += state[6] + msg[4];
        state[14] = rotr32(state[14] ^ state[2], 16);
        state[10] += state[14];
        state[6] = rotr32(state[6] ^ state[10], 12);

        state[3] += state[7] + msg[6];
        state[15] = rotr32(state[15] ^ state[3], 16);
        state[11] += state[15];
        state[7] = rotr32(state[7] ^ state[11], 12);

        state[0] += state[4] + msg[1];
        state[12] = rotr32(state[12] ^ state[0], 8);
        state[8] += state[12];
        state[4] = rotr32(state[4] ^ state[8], 7);

        state[1] += state[5] + msg[3];
        state[13] = rotr32(state[13] ^ state[1], 8);
        state[9] += state[13];
        state[5] = rotr32(state[5] ^ state[9], 7);

        state[2] += state[6] + msg[5];
        state[14] = rotr32(state[14] ^ state[2], 8);
        state[10] += state[14];
        state[6] = rotr32(state[6] ^ state[10], 7);

        state[3] += state[7] + msg[7];
        state[15] = rotr32(state[15] ^ state[3], 8);
        state[11] += state[15];
        state[7] = rotr32(state[7] ^ state[11], 7);

        state[0] += state[5] + msg[8];
        state[15] = rotr32(state[15] ^ state[0], 16);
        state[10] += state[15];
        state[5] = rotr32(state[5] ^ state[10], 12);

        state[1] += state[6] + msg[10];
        state[12] = rotr32(state[12] ^ state[1], 16);
        state[11] += state[12];
        state[6] = rotr32(state[6] ^ state[11], 12);

        state[2] += state[7] + msg[12];
        state[13] = rotr32(state[13] ^ state[2], 16);
        state[8] += state[13];
        state[7] = rotr32(state[7] ^ state[8], 12);

        state[3] += state[4] + msg[14];
        state[14] = rotr32(state[14] ^ state[3], 16);
        state[9] += state[14];
        state[4] = rotr32(state[4] ^ state[9], 12);

        state[0] += state[5] + msg[9];
        state[15] = rotr32(state[15] ^ state[0], 8);
        state[10] += state[15];
        state[5] = rotr32(state[5] ^ state[10], 7);

        state[1] += state[6] + msg[11];
        state[12] = rotr32(state[12] ^ state[1], 8);
        state[11] += state[12];
        state[6] = rotr32(state[6] ^ state[11], 7);

        state[2] += state[7] + msg[13];
        state[13] = rotr32(state[13] ^ state[2], 8);
        state[8] += state[13];
        state[7] = rotr32(state[7] ^ state[8], 7);

        state[3] += state[4] + msg[15];
        state[14] = rotr32(state[14] ^ state[3], 8);
        state[9] += state[14];
        state[4] = rotr32(state[4] ^ state[9], 7);
    }

    for (int i = 0; i < 8; i++) {
        state[i] ^= state[i + 8];
        state[i + 8] ^= cv[i];
    }

    for (int i = 0; i < 16; i++) {
        out[i] = state[i];
    }
}

static void compress_cv(uint32_t cv[8],
                        const uint32_t block_words[16],
                        uint64_t counter,
                        uint32_t block_len,
                        uint32_t flags) {
    uint32_t out[16];
    compress(cv, block_words, counter, block_len, flags, out);
    for (int i = 0; i < 8; i++) {
        cv[i] = out[i];
    }
}

typedef struct {
    uint32_t input_cv[8];
    uint32_t block_words[16];
    uint64_t counter;
    uint32_t block_len;
    uint32_t flags;
} output;

static void output_chaining_value(const output *o, uint32_t out_cv[8]) {
    uint32_t cv[8];
    memcpy(cv, o->input_cv, sizeof(cv));
    compress_cv(cv, o->block_words, o->counter, o->block_len, o->flags);
    memcpy(out_cv, cv, sizeof(cv));
}

static void output_root_bytes(const output *o, uint8_t *out, size_t out_len) {
    uint64_t output_counter = 0;
    while (out_len > 0) {
        uint32_t out_words[16];
        compress(o->input_cv,
                 o->block_words,
                 output_counter,
                 o->block_len,
                 o->flags | ROOT,
                 out_words);
        for (int i = 0; i < 16 && out_len > 0; i++) {
            uint8_t tmp[4];
            store32_le(tmp, out_words[i]);
            size_t take = out_len < sizeof(tmp) ? out_len : sizeof(tmp);
            memcpy(out, tmp, take);
            out += take;
            out_len -= take;
        }
        output_counter++;
    }
}

static void key_words_from_bytes(const uint8_t key[FP_BLAKE3_KEY_LEN],
                                 uint32_t out[8]) {
    for (int i = 0; i < 8; i++) {
        out[i] = load32_le(key + (i * 4));
    }
}

static void chunk_state_init(FpBlake3Hasher *h,
                             const uint32_t key_words[8],
                             uint64_t chunk_counter,
                             uint32_t flags) {
    memcpy(h->cv, key_words, sizeof(h->cv));
    h->chunk_counter = chunk_counter;
    h->block_len = 0;
    h->blocks_compressed = 0;
    h->flags = flags;
    memset(h->block, 0, sizeof(h->block));
}

static size_t chunk_state_len(const FpBlake3Hasher *h) {
    return (size_t)FP_BLAKE3_BLOCK_LEN * h->blocks_compressed + h->block_len;
}

static uint32_t chunk_state_start_flag(const FpBlake3Hasher *h) {
    return h->blocks_compressed == 0 ? CHUNK_START : 0;
}

static void chunk_state_update(FpBlake3Hasher *h,
                               const uint8_t *input,
                               size_t len) {
    while (len > 0) {
        if (h->block_len == FP_BLAKE3_BLOCK_LEN) {
            uint32_t block_words[16];
            load_words(block_words, h->block);
            compress_cv(h->cv,
                        block_words,
                        h->chunk_counter,
                        FP_BLAKE3_BLOCK_LEN,
                        h->flags | chunk_state_start_flag(h));
            h->blocks_compressed++;
            h->block_len = 0;
            memset(h->block, 0, sizeof(h->block));
        }

        size_t want = FP_BLAKE3_BLOCK_LEN - h->block_len;
        if (want > len) {
            want = len;
        }
        memcpy(h->block + h->block_len, input, want);
        h->block_len += (uint8_t)want;
        input += want;
        len -= want;
    }
}

static output chunk_state_output(const FpBlake3Hasher *h) {
    output out;
    uint8_t block[FP_BLAKE3_BLOCK_LEN] = {0};
    memcpy(block, h->block, h->block_len);
    load_words(out.block_words, block);
    memcpy(out.input_cv, h->cv, sizeof(out.input_cv));
    out.counter = h->chunk_counter;
    out.block_len = h->block_len;
    out.flags = h->flags | chunk_state_start_flag(h) | CHUNK_END;
    return out;
}

static output parent_output(const uint32_t left[8],
                            const uint32_t right[8],
                            const uint32_t key_words[8],
                            uint32_t flags) {
    output out;
    memcpy(out.input_cv, key_words, sizeof(out.input_cv));
    for (int i = 0; i < 8; i++) {
        out.block_words[i] = left[i];
        out.block_words[8 + i] = right[i];
    }
    out.counter = 0;
    out.block_len = FP_BLAKE3_BLOCK_LEN;
    out.flags = flags | PARENT;
    return out;
}

static void chunk_cv_full(const uint8_t *input,
                          const uint32_t key_words[8],
                          uint64_t counter,
                          uint32_t flags,
                          uint32_t out_cv[8]) {
    uint32_t cv[8];
    memcpy(cv, key_words, sizeof(cv));

    for (int block = 0; block < (FP_BLAKE3_CHUNK_LEN / FP_BLAKE3_BLOCK_LEN);
         block++) {
        uint32_t block_words[16];
        uint32_t block_flags = flags;
        if (block == 0) {
            block_flags |= CHUNK_START;
        }
        if (block == (FP_BLAKE3_CHUNK_LEN / FP_BLAKE3_BLOCK_LEN) - 1) {
            block_flags |= CHUNK_END;
        }
        load_words(block_words,
                   input + (block * FP_BLAKE3_BLOCK_LEN));
        compress_cv(cv, block_words, counter, FP_BLAKE3_BLOCK_LEN, block_flags);
    }
    memcpy(out_cv, cv, sizeof(cv));
}

#ifdef __AVX2__
static int have_avx2_cached = -1;

static int have_avx2(void) {
    if (have_avx2_cached >= 0) {
        return have_avx2_cached;
    }

    unsigned int eax, ebx, ecx, edx;
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        have_avx2_cached = 0;
        return 0;
    }
    if ((ecx & (1u << 27)) == 0 || (ecx & (1u << 28)) == 0) {
        have_avx2_cached = 0;
        return 0;
    }

    uint32_t xcr0_lo, xcr0_hi;
    __asm__ volatile("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
    {
        uint64_t xcr0 = ((uint64_t)xcr0_hi << 32) | xcr0_lo;
        if ((xcr0 & 0x6u) != 0x6u) {
            have_avx2_cached = 0;
            return 0;
        }
    }

    __cpuid_count(7, 0, eax, ebx, ecx, edx);
    have_avx2_cached = (ebx & (1u << 5)) ? 1 : 0;
    return have_avx2_cached;
}

static inline void g_avx2(__m256i *va,
                          __m256i *vb,
                          __m256i *vc,
                          __m256i *vd,
                          __m256i mx,
                          __m256i my) {
    *va = _mm256_add_epi32(_mm256_add_epi32(*va, *vb), mx);
    *vd = _mm256_xor_si256(*vd, *va);
    *vd = _mm256_or_si256(_mm256_srli_epi32(*vd, 16),
                          _mm256_slli_epi32(*vd, 16));
    *vc = _mm256_add_epi32(*vc, *vd);
    *vb = _mm256_xor_si256(*vb, *vc);
    *vb = _mm256_or_si256(_mm256_srli_epi32(*vb, 12),
                          _mm256_slli_epi32(*vb, 20));

    *va = _mm256_add_epi32(_mm256_add_epi32(*va, *vb), my);
    *vd = _mm256_xor_si256(*vd, *va);
    *vd = _mm256_or_si256(_mm256_srli_epi32(*vd, 8),
                          _mm256_slli_epi32(*vd, 24));
    *vc = _mm256_add_epi32(*vc, *vd);
    *vb = _mm256_xor_si256(*vb, *vc);
    *vb = _mm256_or_si256(_mm256_srli_epi32(*vb, 7),
                          _mm256_slli_epi32(*vb, 25));
}

static void compress_4way_ptrs(uint32_t cv[4][8],
                               const uint8_t *blocks[4],
                               const uint64_t counters[4],
                               uint32_t flags) {
    __m256i state[16];
    __m256i msg[16];

    for (int i = 0; i < 8; i++) {
        state[i] = _mm256_set_epi32(
            cv[3][i], cv[2][i], cv[1][i], cv[0][i],
            cv[3][i], cv[2][i], cv[1][i], cv[0][i]);
    }

    state[8] = _mm256_set1_epi32(IV[0]);
    state[9] = _mm256_set1_epi32(IV[1]);
    state[10] = _mm256_set1_epi32(IV[2]);
    state[11] = _mm256_set1_epi32(IV[3]);

    state[12] = _mm256_set_epi32(
        (uint32_t)counters[3], (uint32_t)counters[2],
        (uint32_t)counters[1], (uint32_t)counters[0],
        (uint32_t)counters[3], (uint32_t)counters[2],
        (uint32_t)counters[1], (uint32_t)counters[0]);
    state[13] = _mm256_set_epi32(
        (uint32_t)(counters[3] >> 32), (uint32_t)(counters[2] >> 32),
        (uint32_t)(counters[1] >> 32), (uint32_t)(counters[0] >> 32),
        (uint32_t)(counters[3] >> 32), (uint32_t)(counters[2] >> 32),
        (uint32_t)(counters[1] >> 32), (uint32_t)(counters[0] >> 32));
    state[14] = _mm256_set1_epi32(FP_BLAKE3_BLOCK_LEN);
    state[15] = _mm256_set1_epi32(flags);

    for (int i = 0; i < 16; i++) {
        msg[i] = _mm256_set_epi32(
            load32_le(blocks[3] + (i * 4)),
            load32_le(blocks[2] + (i * 4)),
            load32_le(blocks[1] + (i * 4)),
            load32_le(blocks[0] + (i * 4)),
            load32_le(blocks[3] + (i * 4)),
            load32_le(blocks[2] + (i * 4)),
            load32_le(blocks[1] + (i * 4)),
            load32_le(blocks[0] + (i * 4)));
    }

    for (int round = 0; round < 7; round++) {
        __m256i m[16];
        const uint8_t *schedule = MSG_SCHEDULE[round];
        for (int i = 0; i < 16; i++) {
            m[i] = msg[schedule[i]];
        }

        g_avx2(&state[0], &state[4], &state[8], &state[12], m[0], m[1]);
        g_avx2(&state[1], &state[5], &state[9], &state[13], m[2], m[3]);
        g_avx2(&state[2], &state[6], &state[10], &state[14], m[4], m[5]);
        g_avx2(&state[3], &state[7], &state[11], &state[15], m[6], m[7]);

        g_avx2(&state[0], &state[5], &state[10], &state[15], m[8], m[9]);
        g_avx2(&state[1], &state[6], &state[11], &state[12], m[10], m[11]);
        g_avx2(&state[2], &state[7], &state[8], &state[13], m[12], m[13]);
        g_avx2(&state[3], &state[4], &state[9], &state[14], m[14], m[15]);
    }

    uint32_t out[4][16];
    for (int i = 0; i < 16; i++) {
        _mm256_storeu_si256((__m256i *)out[0] + (i / 4), state[i]);
    }

    for (int lane = 0; lane < 4; lane++) {
        for (int i = 0; i < 8; i++) {
            cv[lane][i] ^= out[lane][i + 8];
        }
    }
}
#endif

static void chunk_cvs_scalar(const uint8_t *input,
                             size_t chunks,
                             const uint32_t key_words[8],
                             uint64_t counter,
                             uint32_t flags,
                             uint32_t out[][8]) {
    for (size_t i = 0; i < chunks; i++) {
        chunk_cv_full(input + (i * FP_BLAKE3_CHUNK_LEN),
                      key_words,
                      counter + i,
                      flags,
                      out[i]);
    }
}

static void chunk_cvs(const uint8_t *input,
                      size_t chunks,
                      const uint32_t key_words[8],
                      uint64_t counter,
                      uint32_t flags,
                      uint32_t out[][8]) {
#ifdef __AVX2__
    const size_t avx2_min_chunks = 16;
    if (chunks >= avx2_min_chunks && have_avx2()) {
        size_t i = 0;
        for (; i + 4 <= chunks; i += 4) {
            uint32_t cv[4][8];
            uint64_t counters[4] = {
                counter + i,
                counter + i + 1,
                counter + i + 2,
                counter + i + 3,
            };
            for (int lane = 0; lane < 4; lane++) {
                memcpy(cv[lane], key_words, sizeof(cv[lane]));
            }

            for (int block = 0;
                 block < (FP_BLAKE3_CHUNK_LEN / FP_BLAKE3_BLOCK_LEN);
                 block++) {
                uint32_t block_flags = flags;
                if (block == 0) {
                    block_flags |= CHUNK_START;
                }
                if (block == (FP_BLAKE3_CHUNK_LEN / FP_BLAKE3_BLOCK_LEN) - 1) {
                    block_flags |= CHUNK_END;
                }

                const uint8_t *blocks[4] = {
                    input + ((i + 0) * FP_BLAKE3_CHUNK_LEN) +
                        (block * FP_BLAKE3_BLOCK_LEN),
                    input + ((i + 1) * FP_BLAKE3_CHUNK_LEN) +
                        (block * FP_BLAKE3_BLOCK_LEN),
                    input + ((i + 2) * FP_BLAKE3_CHUNK_LEN) +
                        (block * FP_BLAKE3_BLOCK_LEN),
                    input + ((i + 3) * FP_BLAKE3_CHUNK_LEN) +
                        (block * FP_BLAKE3_BLOCK_LEN),
                };
                compress_4way_ptrs(cv, blocks, counters, block_flags);
            }

            for (int lane = 0; lane < 4; lane++) {
                memcpy(out[i + lane], cv[lane], sizeof(cv[lane]));
            }
        }

        if (i < chunks) {
            chunk_cvs_scalar(input + (i * FP_BLAKE3_CHUNK_LEN),
                             chunks - i,
                             key_words,
                             counter + i,
                             flags,
                             out + i);
        }
        return;
    }
#endif
    chunk_cvs_scalar(input, chunks, key_words, counter, flags, out);
}

static void push_stack(FpBlake3Hasher *h, const uint32_t cv[8]) {
    memcpy(h->cv_stack[h->cv_stack_len], cv, sizeof(h->cv_stack[0]));
    h->cv_stack_len++;
}

static void pop_stack(FpBlake3Hasher *h, uint32_t out[8]) {
    h->cv_stack_len--;
    memcpy(out, h->cv_stack[h->cv_stack_len], sizeof(h->cv_stack[0]));
}

static void add_chunk_chaining_value(FpBlake3Hasher *h,
                                     uint32_t new_cv[8],
                                     uint64_t total_chunks) {
    uint32_t left[8];
    while ((total_chunks & 1) == 0) {
        pop_stack(h, left);
        output parent = parent_output(left, new_cv, h->key_words, h->flags);
        output_chaining_value(&parent, new_cv);
        total_chunks >>= 1;
    }
    push_stack(h, new_cv);
}

void fp_blake3_hasher_init(FpBlake3Hasher *hasher) {
    memcpy(hasher->key_words, IV, sizeof(hasher->key_words));
    hasher->cv_stack_len = 0;
    chunk_state_init(hasher, hasher->key_words, 0, 0);
}

void fp_blake3_hasher_init_keyed(FpBlake3Hasher *hasher, const uint8_t *key) {
    key_words_from_bytes(key, hasher->key_words);
    hasher->cv_stack_len = 0;
    chunk_state_init(hasher, hasher->key_words, 0, KEYED_HASH);
}

void fp_blake3_hasher_init_derive_key(FpBlake3Hasher *hasher,
                                      const char *context,
                                      size_t context_len) {
    uint8_t context_key[FP_BLAKE3_KEY_LEN];
    FpBlake3Hasher context_hasher;
    fp_blake3_hasher_init(&context_hasher);
    context_hasher.flags = DERIVE_KEY_CONTEXT;
    chunk_state_init(&context_hasher,
                     context_hasher.key_words,
                     0,
                     context_hasher.flags);
    fp_blake3_hasher_update(&context_hasher,
                            (const uint8_t *)context,
                            context_len);
    fp_blake3_hasher_finalize(&context_hasher, context_key);

    key_words_from_bytes(context_key, hasher->key_words);
    hasher->cv_stack_len = 0;
    chunk_state_init(hasher, hasher->key_words, 0, DERIVE_KEY_MATERIAL);
}

void fp_blake3_hasher_update(FpBlake3Hasher *h,
                             const uint8_t *input,
                             size_t len) {
    while (len > 0) {
        if (chunk_state_len(h) == 0 && len >= FP_BLAKE3_CHUNK_LEN) {
            size_t full_chunks = len / FP_BLAKE3_CHUNK_LEN;
            if (len % FP_BLAKE3_CHUNK_LEN == 0) {
                full_chunks--;
            }
            if (full_chunks > 0) {
                uint64_t chunk_counter = h->chunk_counter;
                while (full_chunks > 0) {
                    size_t batch = full_chunks;
                    if (batch > 8) {
                        batch = 8;
                    }

                    uint32_t cv_batch[8][8];
                    chunk_cvs(input,
                              batch,
                              h->key_words,
                              chunk_counter,
                              h->flags,
                              cv_batch);
                    for (size_t i = 0; i < batch; i++) {
                        uint64_t total_chunks = chunk_counter + 1;
                        add_chunk_chaining_value(h,
                                                 cv_batch[i],
                                                 total_chunks);
                        chunk_counter = total_chunks;
                    }

                    input += batch * FP_BLAKE3_CHUNK_LEN;
                    len -= batch * FP_BLAKE3_CHUNK_LEN;
                    full_chunks -= batch;
                }
                chunk_state_init(h, h->key_words, chunk_counter, h->flags);
                continue;
            }
        }

        if (chunk_state_len(h) == FP_BLAKE3_CHUNK_LEN) {
            output out = chunk_state_output(h);
            uint32_t chunk_cv[8];
            output_chaining_value(&out, chunk_cv);
            uint64_t total_chunks = h->chunk_counter + 1;
            add_chunk_chaining_value(h, chunk_cv, total_chunks);
            chunk_state_init(h, h->key_words, total_chunks, h->flags);
        }

        size_t want = FP_BLAKE3_CHUNK_LEN - chunk_state_len(h);
        if (want > len) {
            want = len;
        }
        chunk_state_update(h, input, want);
        input += want;
        len -= want;
    }
}

void fp_blake3_hasher_finalize(const FpBlake3Hasher *h, uint8_t *output_bytes) {
    output out = chunk_state_output(h);
    for (int i = (int)h->cv_stack_len - 1; i >= 0; i--) {
        uint32_t cv[8];
        output_chaining_value(&out, cv);
        out = parent_output(h->cv_stack[i], cv, h->key_words, h->flags);
    }
    output_root_bytes(&out, output_bytes, FP_BLAKE3_OUT_LEN);
}

void fp_blake3_hasher_finalize_xof(const FpBlake3Hasher *h,
                                   uint8_t *output_bytes,
                                   size_t output_len) {
    output out = chunk_state_output(h);
    for (int i = (int)h->cv_stack_len - 1; i >= 0; i--) {
        uint32_t cv[8];
        output_chaining_value(&out, cv);
        out = parent_output(h->cv_stack[i], cv, h->key_words, h->flags);
    }
    output_root_bytes(&out, output_bytes, output_len);
}

void fp_blake3_hash(const uint8_t *input, size_t len, uint8_t *output) {
    FpBlake3Hasher h;
    fp_blake3_hasher_init(&h);
    fp_blake3_hasher_update(&h, input, len);
    fp_blake3_hasher_finalize(&h, output);
}

void fp_blake3_hash_keyed(const uint8_t *key,
                          const uint8_t *input,
                          size_t len,
                          uint8_t *output) {
    FpBlake3Hasher h;
    fp_blake3_hasher_init_keyed(&h, key);
    fp_blake3_hasher_update(&h, input, len);
    fp_blake3_hasher_finalize(&h, output);
}

void fp_blake3_derive_key(const char *context,
                          size_t context_len,
                          const uint8_t *key_material,
                          size_t km_len,
                          uint8_t *output) {
    FpBlake3Hasher h;
    fp_blake3_hasher_init_derive_key(&h, context, context_len);
    fp_blake3_hasher_update(&h, key_material, km_len);
    fp_blake3_hasher_finalize(&h, output);
}
