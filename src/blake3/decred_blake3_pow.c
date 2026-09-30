#include <string.h>
#include "decred_blake3_pow.h"
#include "blake3.h"

static size_t put_u8(uint8_t *buf, uint8_t val) {
    buf[0] = val;
    return 1;
}

static size_t put_u16le(uint8_t *buf, uint16_t val) {
    buf[0] = (uint8_t)(val & 0xff);
    buf[1] = (uint8_t)((val >> 8) & 0xff);
    return 2;
}

static size_t put_u32le(uint8_t *buf, uint32_t val) {
    buf[0] = (uint8_t)(val & 0xff);
    buf[1] = (uint8_t)((val >> 8) & 0xff);
    buf[2] = (uint8_t)((val >> 16) & 0xff);
    buf[3] = (uint8_t)((val >> 24) & 0xff);
    return 4;
}

static size_t put_u64le(uint8_t *buf, uint64_t val) {
    for (int i = 0; i < 8; i++) {
        buf[i] = (uint8_t)((val >> (8 * i)) & 0xff);
    }
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
    memcpy(out + off, h->extra_data, 36);      off += 36;
    off += put_u32le(out + off, h->stake_version);

    // off must equal DECRED_HEADER_SERIALIZED_LEN (180) here, per DCP0011.
    return off;
}

void decred_blake3_pow_hash_raw(const uint8_t serialized[DECRED_HEADER_SERIALIZED_LEN],
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
    // Little-endian 256-bit compare: start from the most significant byte,
    // which is the LAST byte in a little-endian layout.
    for (int i = DECRED_HASH_LEN - 1; i >= 0; i--) {
        if (hash[i] < target[i]) return 1;
        if (hash[i] > target[i]) return 0;
    }
    return 1; // exactly equal counts as meeting the target
}

int decred_header_increment_nonce(decred_block_header_t *h) {
    uint64_t nonce;
    memcpy(&nonce, h->extra_data, 8); // extra_data is already little-endian bytes
    nonce++;
    memcpy(h->extra_data, &nonce, 8);
    return nonce != 0;
}
