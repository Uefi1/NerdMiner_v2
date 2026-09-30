# BLAKE3 / Decred patch for NerdMiner_v2 (Uefi1 fork)

## What this fixes (from your Serial log)

1. **Stack canary crash on MinerBlake3-0**
   - Was: task stack **4096** bytes
   - Now: **12288** bytes
   - Symptom: `Guru Meditation ... Stack canary watchpoint triggered (MinerBlake3-0)` right after first `mining.notify`

2. **Empty extranonce2 in every mining.submit**
   - Was: `"params":[wallet, jobId, "", ntime, nonce]` → pools reject
   - Now: after subscribe, extranonce2 is seeded to zero-padded hex of `extranonce2_size` (e.g. `"00000000"`)
   - Log line: `[BLAKE3] seeded extranonce2=00000000 (size=4)`

3. **Share flood + TCP drop**
   - Pool sent `set_difficulty = 1`; old difficulty estimator accepted almost every nonce
   - Connection died (`connected>false`), hashrate collapsed
   - Now: stricter difficulty estimate + max 8 pending shares + 30 ms between submits + drop queue if client disconnected

## How to apply

Copy into your tree (same paths as upstream):

```
src/NerdMinerV2.ino.cpp   ← from this patch
src/mining_blake3.cpp     ← from this patch
```

Rebuild with Blake3 flag:

```
pio run -e <your_env> -t upload
```

(Make sure `platformio.ini` still has `-D NERDMINER_BLAKE3=1` for that env.)

## Expected Serial after flash

```
[BLAKE3] 0 Started minerWorkerBlake3 Task on core 0!
[BLAKE3] 1 Started minerWorkerBlake3 Task on core 1!
...
[BLAKE3] seeded extranonce2=00000000 (size=4)
...
Parsing Method [MINING NOTIFY]
  (no Guru Meditation)
[BLAKE3-WORKER] Submitting share, nonce=........ diff=... en2=00000000
```

Hashrate on monitor should settle around **~50–70 kH/s** (software BLAKE3, dual core).

## Still possible issues

- Suprnova may keep difficulty at **1** (too high for ESP32). If shares stay rejected, try a low-diff Decred port or local `dcrpool`.
- If header layout for another pool differs, `buildBlake3WorkData` will log `coinb1 is N bytes, expected 144` and skip the job — send that notify line if it happens.
