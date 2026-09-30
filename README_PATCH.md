# BLAKE3 / Decred patch v5 (yiimp/Suprnova semantics)

## Root cause of persistent "Low diff: 0.00"

Suprnova runs yiimp-style Decred stratum. On share validation
(`create_decred_header`) the pool builds the header as:

```
memcpy(template_from_job);
sscanf(nonce_from_submit);   // LE uint32 @ offset 140
binlify(extra, nonce2_from_submit);  // ONLY extranonce2 → offset 144
```

It does **not** put extranonce1 into the header. Our miner was writing
en1 at offset 144, so local BLAKE3 ≠ pool BLAKE3 → difficulty 0.00.

## Fix (v5)

- Header = version + prevhash + coinb1 (ntime already correct inside)
- Nonce @140 rolled LE
- Extra @144 = only extranonce2 (zeros, size=4)
- No en1 memcpy
- 8-char padded nonce in submit
- Stack 12288, no suggest_difficulty, share filter

## Apply

```
src/NerdMinerV2.ino.cpp
src/mining_blake3.cpp
```

Rebuild `-D NERDMINER_BLAKE3=1`.

Expect `"result":true` or a non-zero difficulty reject.
