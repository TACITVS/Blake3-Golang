#include "fp_blake3_fast.h"

#include <string.h>

// Derived from FP_ASM_LIB's fp_blake3.c, with full tree hashing and streaming.

#ifdef __AVX2__
#include <cpuid.h>
#endif

extern void fp_blake3_compress_words_asm(const uint32_t cv[8],
                                         const uint32_t block_words[16],
                                         uint64_t counter,
                                         uint32_t block_len,
                                         uint32_t flags,
                                         uint32_t out[16]);
#ifdef __AVX2__
extern void fp_blake3_compress4_asm(uint32_t cv[4][8],
                                    const uint8_t *blocks[4],
                                    const uint64_t counters[4],
                                    uint32_t flags);
extern void fp_blake3_compress8_asm(uint32_t cv[8][8],
                                    const uint8_t *blocks[8],
                                    const uint64_t counters[8],
                                    uint32_t flags);
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

static void load_words_rec(uint32_t *out, const uint8_t *block, size_t count) {
    if (count == 0) {
        return;
    }
    out[0] = load32_le(block);
    load_words_rec(out + 1, block + 4, count - 1);
}

static void load_words(uint32_t out[16], const uint8_t block[64]) {
    load_words_rec(out, block, 16);
}

static void copy_u32_rec(uint32_t *dst, const uint32_t *src, size_t count) {
    if (count == 0) {
        return;
    }
    dst[0] = src[0];
    copy_u32_rec(dst + 1, src + 1, count - 1);
}

static void compress(const uint32_t cv[8],
                     const uint32_t block_words[16],
                     uint64_t counter,
                     uint32_t block_len,
                     uint32_t flags,
                     uint32_t out[16]) {
    fp_blake3_compress_words_asm(cv, block_words, counter, block_len, flags, out);
}

static void compress_cv(uint32_t cv[8],
                        const uint32_t block_words[16],
                        uint64_t counter,
                        uint32_t block_len,
                        uint32_t flags) {
    uint32_t out[16];
    compress(cv, block_words, counter, block_len, flags, out);
    copy_u32_rec(cv, out, 8);
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

static void output_words_rec(const uint32_t out_words[16],
                             uint8_t **out,
                             size_t *out_len,
                             size_t idx) {
    if (idx == 16 || *out_len == 0) {
        return;
    }
    uint8_t tmp[4];
    store32_le(tmp, out_words[idx]);
    size_t take = *out_len < sizeof(tmp) ? *out_len : sizeof(tmp);
    memcpy(*out, tmp, take);
    *out += take;
    *out_len -= take;
    output_words_rec(out_words, out, out_len, idx + 1);
}

static void output_root_bytes_rec(const output *o,
                                  uint8_t *out,
                                  size_t out_len,
                                  uint64_t output_counter) {
    if (out_len == 0) {
        return;
    }
    uint32_t out_words[16];
    compress(o->input_cv,
             o->block_words,
             output_counter,
             o->block_len,
             o->flags | ROOT,
             out_words);
    output_words_rec(out_words, &out, &out_len, 0);
    output_root_bytes_rec(o, out, out_len, output_counter + 1);
}

static void output_root_bytes(const output *o, uint8_t *out, size_t out_len) {
    output_root_bytes_rec(o, out, out_len, 0);
}

static void key_words_from_bytes(const uint8_t key[FP_BLAKE3_KEY_LEN],
                                 uint32_t out[8]) {
    load_words_rec(out, key, 8);
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

static void chunk_state_update_rec(FpBlake3Hasher *h,
                                   const uint8_t *input,
                                   size_t len) {
    if (len == 0) {
        return;
    }
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
        chunk_state_update_rec(h, input, len);
        return;
    }

    size_t want = FP_BLAKE3_BLOCK_LEN - h->block_len;
    if (want > len) {
        want = len;
    }
    memcpy(h->block + h->block_len, input, want);
    h->block_len += (uint8_t)want;
    chunk_state_update_rec(h, input + want, len - want);
}

static void chunk_state_update(FpBlake3Hasher *h,
                               const uint8_t *input,
                               size_t len) {
    chunk_state_update_rec(h, input, len);
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

static void parent_words_rec(uint32_t dst[16],
                             const uint32_t left[8],
                             const uint32_t right[8],
                             size_t idx) {
    if (idx == 8) {
        return;
    }
    dst[idx] = left[idx];
    dst[8 + idx] = right[idx];
    parent_words_rec(dst, left, right, idx + 1);
}

static output parent_output(const uint32_t left[8],
                            const uint32_t right[8],
                            const uint32_t key_words[8],
                            uint32_t flags) {
    output out;
    memcpy(out.input_cv, key_words, sizeof(out.input_cv));
    parent_words_rec(out.block_words, left, right, 0);
    out.counter = 0;
    out.block_len = FP_BLAKE3_BLOCK_LEN;
    out.flags = flags | PARENT;
    return out;
}

static void chunk_cv_full_rec(uint32_t state[8],
                              const uint8_t *block_ptr,
                              uint64_t chunk_counter,
                              uint32_t base_flags,
                              size_t block_idx) {
    if (block_idx == (FP_BLAKE3_CHUNK_LEN / FP_BLAKE3_BLOCK_LEN)) {
        return;
    }
    uint32_t block_words[16];
    uint32_t block_flags = base_flags;
    if (block_idx == 0) {
        block_flags |= CHUNK_START;
    }
    if (block_idx + 1 == (FP_BLAKE3_CHUNK_LEN / FP_BLAKE3_BLOCK_LEN)) {
        block_flags |= CHUNK_END;
    }
    load_words(block_words, block_ptr);
    compress_cv(state, block_words, chunk_counter,
                FP_BLAKE3_BLOCK_LEN, block_flags);
    chunk_cv_full_rec(state,
                      block_ptr + FP_BLAKE3_BLOCK_LEN,
                      chunk_counter,
                      base_flags,
                      block_idx + 1);
}

static void chunk_cv_full(const uint8_t *input,
                          const uint32_t key_words[8],
                          uint64_t counter,
                          uint32_t flags,
                          uint32_t out_cv[8]) {
    uint32_t cv[8];
    memcpy(cv, key_words, sizeof(cv));
    chunk_cv_full_rec(cv, input, counter, flags, 0);
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
#endif

static void chunk_cvs_scalar(const uint8_t *input,
                             size_t chunks,
                             const uint32_t key_words[8],
                             uint64_t counter,
                             uint32_t flags,
                             uint32_t out[][8]) {
    if (chunks == 0) {
        return;
    }
    chunk_cv_full(input, key_words, counter, flags, out[0]);
    chunk_cvs_scalar(input + FP_BLAKE3_CHUNK_LEN,
                     chunks - 1,
                     key_words,
                     counter + 1,
                     flags,
                     out + 1);
}

#ifdef __AVX2__
static uint32_t block_flags_for_index(uint32_t flags, size_t block_idx) {
    uint32_t block_flags = flags;
    if (block_idx == 0) {
        block_flags |= CHUNK_START;
    }
    if (block_idx + 1 == (FP_BLAKE3_CHUNK_LEN / FP_BLAKE3_BLOCK_LEN)) {
        block_flags |= CHUNK_END;
    }
    return block_flags;
}

static void fill_counters_rec(uint64_t *counters,
                              size_t lanes,
                              uint64_t counter) {
    if (lanes == 0) {
        return;
    }
    counters[0] = counter;
    fill_counters_rec(counters + 1, lanes - 1, counter + 1);
}

static void init_cv_lanes_rec(uint32_t (*cv)[8],
                              size_t lanes,
                              const uint32_t key_words[8]) {
    if (lanes == 0) {
        return;
    }
    memcpy(cv[0], key_words, sizeof(cv[0]));
    init_cv_lanes_rec(cv + 1, lanes - 1, key_words);
}

static void copy_cv_lanes_rec(uint32_t (*dst)[8],
                              uint32_t (*src)[8],
                              size_t lanes) {
    if (lanes == 0) {
        return;
    }
    memcpy(dst[0], src[0], sizeof(dst[0]));
    copy_cv_lanes_rec(dst + 1, src + 1, lanes - 1);
}

static void chunk_cvs_blocks4_rec(uint32_t cv[4][8],
                                  const uint8_t *input,
                                  const uint64_t counters[4],
                                  uint32_t flags,
                                  size_t block_idx) {
    if (block_idx == (FP_BLAKE3_CHUNK_LEN / FP_BLAKE3_BLOCK_LEN)) {
        return;
    }
    uint32_t block_flags = block_flags_for_index(flags, block_idx);
    size_t block_offset = block_idx * FP_BLAKE3_BLOCK_LEN;
    const uint8_t *blocks[4] = {
        input + (0 * FP_BLAKE3_CHUNK_LEN) + block_offset,
        input + (1 * FP_BLAKE3_CHUNK_LEN) + block_offset,
        input + (2 * FP_BLAKE3_CHUNK_LEN) + block_offset,
        input + (3 * FP_BLAKE3_CHUNK_LEN) + block_offset,
    };
    fp_blake3_compress4_asm(cv, blocks, counters, block_flags);
    chunk_cvs_blocks4_rec(cv, input, counters, flags, block_idx + 1);
}

static void chunk_cvs_blocks8_rec(uint32_t cv[8][8],
                                  const uint8_t *input,
                                  const uint64_t counters[8],
                                  uint32_t flags,
                                  size_t block_idx) {
    if (block_idx == (FP_BLAKE3_CHUNK_LEN / FP_BLAKE3_BLOCK_LEN)) {
        return;
    }
    uint32_t block_flags = block_flags_for_index(flags, block_idx);
    size_t block_offset = block_idx * FP_BLAKE3_BLOCK_LEN;
    const uint8_t *blocks[8] = {
        input + (0 * FP_BLAKE3_CHUNK_LEN) + block_offset,
        input + (1 * FP_BLAKE3_CHUNK_LEN) + block_offset,
        input + (2 * FP_BLAKE3_CHUNK_LEN) + block_offset,
        input + (3 * FP_BLAKE3_CHUNK_LEN) + block_offset,
        input + (4 * FP_BLAKE3_CHUNK_LEN) + block_offset,
        input + (5 * FP_BLAKE3_CHUNK_LEN) + block_offset,
        input + (6 * FP_BLAKE3_CHUNK_LEN) + block_offset,
        input + (7 * FP_BLAKE3_CHUNK_LEN) + block_offset,
    };
    fp_blake3_compress8_asm(cv, blocks, counters, block_flags);
    chunk_cvs_blocks8_rec(cv, input, counters, flags, block_idx + 1);
}

static void chunk_cvs_avx2_rec(const uint8_t *input,
                               size_t chunks,
                               const uint32_t key_words[8],
                               uint64_t counter,
                               uint32_t flags,
                               uint32_t out[][8]) {
    if (chunks >= 8) {
        uint32_t cv[8][8];
        uint64_t counters[8];
        fill_counters_rec(counters, 8, counter);
        init_cv_lanes_rec(cv, 8, key_words);
        chunk_cvs_blocks8_rec(cv, input, counters, flags, 0);
        copy_cv_lanes_rec(out, cv, 8);
        chunk_cvs_avx2_rec(input + (8 * FP_BLAKE3_CHUNK_LEN),
                           chunks - 8,
                           key_words,
                           counter + 8,
                           flags,
                           out + 8);
        return;
    }
    if (chunks >= 4) {
        uint32_t cv[4][8];
        uint64_t counters[4];
        fill_counters_rec(counters, 4, counter);
        init_cv_lanes_rec(cv, 4, key_words);
        chunk_cvs_blocks4_rec(cv, input, counters, flags, 0);
        copy_cv_lanes_rec(out, cv, 4);
        chunk_cvs_avx2_rec(input + (4 * FP_BLAKE3_CHUNK_LEN),
                           chunks - 4,
                           key_words,
                           counter + 4,
                           flags,
                           out + 4);
        return;
    }
    chunk_cvs_scalar(input, chunks, key_words, counter, flags, out);
}
#endif

static void chunk_cvs(const uint8_t *input,
                      size_t chunks,
                      const uint32_t key_words[8],
                      uint64_t counter,
                      uint32_t flags,
                      uint32_t out[][8]) {
#ifdef __AVX2__
    const size_t avx2_min_chunks = 4;
    if (chunks >= avx2_min_chunks && have_avx2()) {
        chunk_cvs_avx2_rec(input, chunks, key_words, counter, flags, out);
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
    if ((total_chunks & 1) != 0) {
        push_stack(h, new_cv);
        return;
    }
    uint32_t left[8];
    pop_stack(h, left);
    output parent = parent_output(left, new_cv, h->key_words, h->flags);
    output_chaining_value(&parent, new_cv);
    add_chunk_chaining_value(h, new_cv, total_chunks >> 1);
}

static uint64_t add_chunk_cv_batch_rec(FpBlake3Hasher *h,
                                       uint32_t (*cv_batch)[8],
                                       size_t batch,
                                       uint64_t chunk_counter) {
    if (batch == 0) {
        return chunk_counter;
    }
    uint64_t total_chunks = chunk_counter + 1;
    add_chunk_chaining_value(h, cv_batch[0], total_chunks);
    return add_chunk_cv_batch_rec(h, cv_batch + 1, batch - 1, total_chunks);
}

static uint64_t process_full_chunks_rec(FpBlake3Hasher *h,
                                        const uint8_t *input,
                                        size_t full_chunks,
                                        uint64_t chunk_counter) {
    if (full_chunks == 0) {
        return chunk_counter;
    }
    size_t batch = full_chunks > 8 ? 8 : full_chunks;
    uint32_t cv_batch[8][8];
    chunk_cvs(input, batch, h->key_words, chunk_counter, h->flags, cv_batch);
    uint64_t next_counter =
        add_chunk_cv_batch_rec(h, cv_batch, batch, chunk_counter);
    return process_full_chunks_rec(h,
                                   input + (batch * FP_BLAKE3_CHUNK_LEN),
                                   full_chunks - batch,
                                   next_counter);
}

static void fp_blake3_hasher_update_rec(FpBlake3Hasher *h,
                                        const uint8_t *input,
                                        size_t len) {
    if (len == 0) {
        return;
    }
    size_t state_len = chunk_state_len(h);
    if (state_len == 0 && len >= FP_BLAKE3_CHUNK_LEN) {
        size_t full_chunks = len / FP_BLAKE3_CHUNK_LEN;
        if (len % FP_BLAKE3_CHUNK_LEN == 0 && full_chunks > 0) {
            full_chunks--;
        }
        if (full_chunks > 0) {
            uint64_t next_counter =
                process_full_chunks_rec(h, input, full_chunks,
                                        h->chunk_counter);
            chunk_state_init(h, h->key_words, next_counter, h->flags);
            size_t consumed = full_chunks * FP_BLAKE3_CHUNK_LEN;
            fp_blake3_hasher_update_rec(h,
                                        input + consumed,
                                        len - consumed);
            return;
        }
    }

    if (state_len == FP_BLAKE3_CHUNK_LEN) {
        output out = chunk_state_output(h);
        uint32_t chunk_cv[8];
        output_chaining_value(&out, chunk_cv);
        uint64_t total_chunks = h->chunk_counter + 1;
        add_chunk_chaining_value(h, chunk_cv, total_chunks);
        chunk_state_init(h, h->key_words, total_chunks, h->flags);
        fp_blake3_hasher_update_rec(h, input, len);
        return;
    }

    size_t want = FP_BLAKE3_CHUNK_LEN - state_len;
    if (want > len) {
        want = len;
    }
    chunk_state_update(h, input, want);
    fp_blake3_hasher_update_rec(h, input + want, len - want);
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
    fp_blake3_hasher_update_rec(h, input, len);
}

static output reduce_stack_rec(const FpBlake3Hasher *h,
                               output out,
                               size_t idx) {
    if (idx == 0) {
        return out;
    }
    uint32_t cv[8];
    output_chaining_value(&out, cv);
    output next = parent_output(h->cv_stack[idx - 1],
                                cv,
                                h->key_words,
                                h->flags);
    return reduce_stack_rec(h, next, idx - 1);
}

void fp_blake3_hasher_finalize(const FpBlake3Hasher *h, uint8_t *output_bytes) {
    output out = chunk_state_output(h);
    out = reduce_stack_rec(h, out, h->cv_stack_len);
    output_root_bytes(&out, output_bytes, FP_BLAKE3_OUT_LEN);
}

void fp_blake3_hasher_finalize_xof(const FpBlake3Hasher *h,
                                   uint8_t *output_bytes,
                                   size_t output_len) {
    output out = chunk_state_output(h);
    out = reduce_stack_rec(h, out, h->cv_stack_len);
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
