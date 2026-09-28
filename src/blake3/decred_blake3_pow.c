#pragma GCC optimize ("O3")

#include "decred_blake3_pow.h"
#include "blake3.h"
#include <string.h>

#if defined(ESP_PLATFORM) || defined(ARDUINO)
  #include "esp_attr.h"
  #ifndef POW_IRAM
    #define POW_IRAM IRAM_ATTR
  #endif
#else
  #define POW_IRAM
#endif

/* Little-endian helpers (kept local, always inlined by compiler) */
static inline size_t put_u8(uint8_t *p, uint8_t v)  { p[0]=v; return 1; }
static inline size_t put_u16le(uint8_t *p, uint16_t v) {
  p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); return 2;
}
static inline size_t put_u32le(uint8_t *p, uint32_t v) {
  p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
  p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); return 4;
}
static inline size_t put_u64le(uint8_t *p, uint64_t v) {
  put_u32le(p, (uint32_t)v);
  put_u32le(p+4, (uint32_t)(v>>32));
  return 8;
}

size_t decred_header_serialize(const decred_block_header_t *h, uint8_t *out) {
    size_t off = 0;
    off += put_u32le(out + off, (uint32_t)h->version);
    memcpy(out + off, h->prev_block_hash, 32); off += 32;
    memcpy(out + off, h->merkle_root, 32);     off += 32;
    memcpy(out + off, h->commitment_root, 32); off += 32;
    off += put_u16le(out + off, h->vote_bits);
    memcpy(out + off, h->final_state, 6);      off += 6;
    off += put_u16le(out + off, h->num_voters);
    off += put_u8(out + off, h->fresh_stake);
    off += put_u8(out + off, h->num_revocations);
    off += put_u32le(out + off, h->pool_size);
    off += put_u32le(out + off, h->difficulty_bits);
    off += put_u64le(out + off, (uint64_t)h->stake_difficulty);
    off += put_u32le(out + off, h->block_height);
    off += put_u32le(out + off, h->block_size);
    off += put_u32le(out + off, h->timestamp);
    /* Official wire: ExtraData 32 bytes, then StakeVersion, then Nonce */
    memcpy(out + off, h->extra_data, 32);      off += 32;
    off += put_u32le(out + off, h->stake_version);
    off += put_u32le(out + off, h->nonce);
    return off;
}

/*
 * HOT PATH — called once per nonce.
 * IRAM keeps it out of flash cache misses on ESP32.
 * Full ASM BLAKE3 compress for Xtensa does not exist upstream;
 * this is the fastest safe portable path.
 */
void POW_IRAM decred_blake3_pow_hash_raw(const uint8_t serialized[DECRED_HEADER_SERIALIZED_LEN],
                                 uint8_t out_hash[DECRED_HASH_LEN]) {
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, serialized, DECRED_HEADER_SERIALIZED_LEN);
    blake3_hasher_finalize(&hasher, out_hash, DECRED_HASH_LEN);
}

void decred_blake3_pow_hash(const decred_block_header_t *h,
                             uint8_t out_hash[DECRED_HASH_LEN]) {
    uint8_t serialized[DECRED_HEADER_SERIALIZED_LEN];
    decred_header_serialize(h, serialized);
    decred_blake3_pow_hash_raw(serialized, out_hash);
}

int decred_hash_meets_target(const uint8_t hash[DECRED_HASH_LEN],
                              const uint8_t target[DECRED_HASH_LEN]) {
    for (int i = DECRED_HASH_LEN - 1; i >= 0; i--) {
        if (hash[i] < target[i]) return 1;
        if (hash[i] > target[i]) return 0;
    }
    return 1;
}

int decred_header_increment_nonce(decred_block_header_t *h) {
    h->nonce++;
    return h->nonce != 0;
}
