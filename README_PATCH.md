# BLAKE3 / Decred patch v2 for NerdMiner_v2

## Fixes in this zip

1. **Stack** MinerBlake3: 4096 → 12288 (no more canary crash)
2. **extranonce2** seeded to `00000000` after subscribe
3. **Nonce in mining.submit** — always 8 hex digits (`%08x`), was `"0"` / `"2000"` → pool answered `Invalid nonce`
4. **No mining.suggest_difficulty** — Suprnova returns Unknown method
5. **Share filter** — require `hash[31]==0` so difficulty=1 does not flood the pool
6. Submit throttle + disconnect handling

## Apply

```
src/NerdMinerV2.ino.cpp
src/mining_blake3.cpp
```

Rebuild with `-D NERDMINER_BLAKE3=1`.

## Expected Serial

```
[BLAKE3] seeded extranonce2=00000000 (size=4)
...
[BLAKE3-WORKER] Submitting share, nonce=00002000 diff=... en2=00000000
  Sending  : {"id":...,"method":"mining.submit","params":[...,"00000000",ntime,"00002000"]}
```

If still `Invalid nonce`, the remaining issue is header layout (offsets of en1/en2 vs partial coinb1) — send a full notify + one rejected submit for the next fix.
