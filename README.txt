SHA256d restore + max performance (no CPU overclock)

Files to copy into your fork (Uefi1/NerdMiner_v2):
  platformio.ini
  src/mining.h
  src/mining.cpp
  src/utils.cpp
  src/NerdMinerV2.ino.cpp
  src/stratum.cpp   (keeps crash-safe checkError)

What changed:
  1. #define HARDWARE_SHA265  — HW SHA engine on
  2. Decred/BLAKE3 path disabled when HARDWARE_SHA265 is on
  3. DEFAULT_DIFFICULTY 0.00015 (Bitcoin pools)
  4. MinerHw pinned CORE 0 prio 3, MinerSw pinned CORE 1 prio 3
  5. Bluetooth disabled: -D CONFIG_BT_ENABLED=0 + btStop() if present
  6. -O2 in platformio for S3 env

Pool: use a normal Bitcoin SHA256d pool (NOT dcr.suprnova).
Example: public-pool.io:21496 or your previous BTC pool.

Expected log:
  [MINER] 0 Started minerWorkerHw Task!   (or similar on core 0)
  [MINER] 1 Started minerWorkerSw (SHA256d) Task on core 1!
  hashrate hundreds of kH/s (HW path), not ~50-60 kH/s
