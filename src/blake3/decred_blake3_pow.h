// Decred (DCP0011) BLAKE3 proof-of-work hash construction.
//
// Implements the exact 180-byte block header serialization defined in
// DCP0011 ("Change PoW to BLAKE3 and ASERT"), and hashes it with BLAKE3
// to produce the value that is compared against the network/share target.
//
// This wrapper does not implement Stratum, the ASERT difficulty algorithm,
// or the compact "difficulty bits" <-> target conversion -- it only covers
// "given these header fields, produce the correct 32-byte PoW hash",
// verified against the official DCP0011 test vectors.
#ifndef DECRED_BLAKE3_POW_H
#define DECRED_BLAKE3_POW_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DECRED_HEADER_SERIALIZED_LEN 180
#define DECRED_HASH_LEN 32

// Mirrors the DCP0011 block header serialization table exactly, field
// for field, in on-the-wire order. All multi-byte integers are written
// little-endian; the hash fields (prev_block_hash, merkle_root,
// commitment_root) are written in "internal" byte order, i.e. exactly the
// raw bytes as produced by the hash function that created them (NOT the
// human-readable big-endian display order used on explorers).
typedef struct {
    int32_t  version;                 // offset 0,   4 bytes
    uint8_t  prev_block_hash[32];      // offset 4,   32 bytes (internal order)
    uint8_t  merkle_root[32];          // offset 36,  32 bytes (internal order)
    uint8_t  commitment_root[32];      // offset 68,  32 bytes (internal order)
    uint16_t vote_bits;                // offset 100, 2 bytes
    uint8_t  final_state[6];           // offset 102, 6 bytes
    uint16_t num_voters;               // offset 108, 2 bytes
    uint8_t  fresh_stake;              // offset 110, 1 byte
    uint8_t  num_revocations;          // offset 111, 1 byte
    uint32_t pool_size;                // offset 112, 4 bytes
    uint32_t difficulty_bits;          // offset 116, 4 bytes
    int64_t  stake_difficulty;         // offset 120, 8 bytes
    uint32_t block_height;             // offset 128, 4 bytes
    uint32_t block_size;               // offset 132, 4 bytes
    uint32_t timestamp;                // offset 136, 4 bytes
    uint8_t  extra_data[36];           // offset 140, 36 bytes
                                        //   recommended split (not enforced
                                        //   by consensus, just convention):
                                        //     bytes 0-7  : per-device nonce
                                        //     bytes 8-11 : pool nonce
                                        //     bytes 12-35: zero
    uint32_t stake_version;            // offset 176, 4 bytes
} decred_block_header_t;

// Serializes the header fields into `out` (must be at least
// DECRED_HEADER_SERIALIZED_LEN bytes). Returns the number of bytes written
// (always DECRED_HEADER_SERIALIZED_LEN on success).
size_t decred_header_serialize(const decred_block_header_t *h, uint8_t *out);

// Computes the BLAKE3 PoW hash of an already-serialized 180-byte header.
// `out_hash` receives the raw 32-byte digest, in the same byte order BLAKE3
// natively produces it (this is the order used for numeric/target
// comparison -- NOT the human-readable/explorer display order, which is
// byte-reversed).
void decred_blake3_pow_hash_raw(const uint8_t serialized[DECRED_HEADER_SERIALIZED_LEN],
                                 uint8_t out_hash[DECRED_HASH_LEN]);

// Convenience: serializes the header and hashes it in one call.
void decred_blake3_pow_hash(const decred_block_header_t *h,
                             uint8_t out_hash[DECRED_HASH_LEN]);

// Returns 1 if `hash` (as produced by decred_blake3_pow_hash, raw byte
// order) is numerically <= `target` when both are interpreted as
// little-endian unsigned 256-bit integers -- i.e. whether this hash meets
// the given share/network target. Returns 0 otherwise.
//
// `target` is expected in the same raw little-endian 256-bit representation
// a Stratum job / pool typically supplies (NOT the compact "difficulty
// bits" encoding -- decoding difficulty bits to a full target is a
// separate step this wrapper does not implement).
int decred_hash_meets_target(const uint8_t hash[DECRED_HASH_LEN],
                              const uint8_t target[DECRED_HASH_LEN]);

// Increments the 8-byte per-device nonce field (extra_data[0..7], treated
// as a little-endian uint64) by 1. This is the recommended nonce location
// per DCP0011; using it lets a pool support many devices without nonce
// collisions. Returns 1 on normal increment, or 0 if it wrapped around
// (extremely unlikely in practice, but handled for correctness).
int decred_header_increment_nonce(decred_block_header_t *h);

#ifdef __cplusplus
}
#endif

#endif // DECRED_BLAKE3_POW_H
