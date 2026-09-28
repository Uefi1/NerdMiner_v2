S3_SHA256d_MAX — apply ONLY this package

Based on BitMaker-hub/NerdMiner_v2 pure SHA256d + HARDWARE_SHA265

Changes vs stock:
1. MinerHw pinned to CORE 0 (prio 3)
2. MinerSw-1 pinned to CORE 1
3. NONCE_PER_JOB_HW 32K (was 16K)
4. S3: midstate words byte-swapped when writing SHA_H (endian fix attempt)
5. stratum checkError safe for array+object errors
6. blake3/ excluded from build
7. btStop if BT present

Copy into fork root:
  platformio.ini
  src/mining.cpp
  src/mining.h
  src/utils.cpp
  src/utils.h
  src/NerdMinerV2.ino.cpp
  src/stratum.cpp

Pool: Bitcoin SHA256d only
Expect Serial: [MINER] 0 Started minerWorkerHw Task!
Expect hashrate: ~200-400 kH/s class (S3 HW), valid shares

If shares=0 and hashrate high: S3 may ignore midstate CONTINUE — report log, we revert bswap.
