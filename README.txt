CLEAN restore of SHA256d from official BitMaker-hub/NerdMiner_v2

Previous "restore" still had BLAKE3 leftovers in mining.cpp
(JobPush with decred_header, soft-only path) → ~50-60 kH/s.

This package replaces mining stack with ORIGINAL BitMaker code:
  - HARDWARE_SHA265 enabled
  - minerWorkerHw (HW SHA) + minerWorkerSw
  - original JobPush / midstate / NONCE_PER_JOB_HW = 16*1024
  - NO decred/blake3 in mining path

Extra (safe):
  - stratum checkError handles JSON object errors (no crash)
  - MinerHw pinned to core 0
  - Bluetooth disabled in platformio

Copy into your fork root:
  platformio.ini
  src/mining.cpp
  src/mining.h
  src/utils.cpp
  src/utils.h
  src/NerdMinerV2.ino.cpp
  src/stratum.cpp

Build env: ESP32-S3-devKitv1
Pool: Bitcoin SHA256d only (not Decred)

Expected Serial:
  [MINER] 0 Started minerWorkerHw Task!
  [MINER] 1 Started minerWorkerSw Task on core ...
  hashrate ~200-400+ kH/s (HW SHA path)
