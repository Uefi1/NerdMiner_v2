#ifndef MINING_API_H
#define MINING_API_H

// Mining
#define MAX_NONCE_STEP  5000000U
#define MAX_NONCE       25000000U
#define TARGET_NONCE    471136297U
// Very low default so ESP32 (~70 kH/s) can actually find shares on Decred pools
#define DEFAULT_DIFFICULTY  0.00001
#define KEEPALIVE_TIME_ms       30000
#define POOLINACTIVITY_TIME_ms  60000

// BLAKE3 / Decred — no hardware SHA path
//#define HARDWARE_SHA265

#define TARGET_BUFFER_SIZE 64
#define DECRED_HEADER_LEN  180

void runMonitor(void *name);
void runStratumWorker(void *name);
void runMiner(void *name);

void minerWorkerSw(void * task_id);
void minerWorkerHw(void * task_id);

String printLocalTime(void);
void resetStat();

typedef struct {
  uint8_t bytearray_target[32];
  uint8_t bytearray_pooltarget[32];
  uint8_t merkle_result[32];
  // Bitcoin path kept for size compatibility; Decred uses decred_header
  uint8_t bytearray_blockheader[128];
  // Full Decred 180-byte serialized header (PoW input for BLAKE3)
  uint8_t decred_header[DECRED_HEADER_LEN];
  bool is_decred;
} miner_data;

#endif
