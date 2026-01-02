#pragma once

#include <stddef.h>
#include <stdint.h>

#define FP_BLAKE3_OUT_LEN   32
#define FP_BLAKE3_KEY_LEN   32
#define FP_BLAKE3_BLOCK_LEN 64
#define FP_BLAKE3_CHUNK_LEN 1024

typedef struct {
    uint32_t cv[8];
    uint64_t chunk_counter;
    uint8_t block[FP_BLAKE3_BLOCK_LEN];
    uint8_t block_len;
    uint8_t blocks_compressed;
    uint16_t _reserved;
    uint32_t flags;
    uint32_t key_words[8];
    uint32_t cv_stack[54][8];
    uint8_t cv_stack_len;
    uint8_t _pad[3];
} FpBlake3Hasher;

void fp_blake3_hasher_init(FpBlake3Hasher *hasher);
void fp_blake3_hasher_init_keyed(FpBlake3Hasher *hasher, const uint8_t *key);
void fp_blake3_hasher_init_derive_key(FpBlake3Hasher *hasher,
                                      const char *context,
                                      size_t context_len);
void fp_blake3_hasher_update(FpBlake3Hasher *hasher,
                             const uint8_t *input,
                             size_t len);
void fp_blake3_hasher_finalize(const FpBlake3Hasher *hasher, uint8_t *output);
void fp_blake3_hasher_finalize_xof(const FpBlake3Hasher *hasher,
                                   uint8_t *output,
                                   size_t output_len);

void fp_blake3_hash(const uint8_t *input, size_t len, uint8_t *output);
void fp_blake3_hash_keyed(const uint8_t *key,
                          const uint8_t *input,
                          size_t len,
                          uint8_t *output);
void fp_blake3_derive_key(const char *context,
                          size_t context_len,
                          const uint8_t *key_material,
                          size_t km_len,
                          uint8_t *output);
