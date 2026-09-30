# BLAKE3 / Decred patch v3

## Why v2 still got Low diff: 0.00

Pool accepted the nonce format, but when it rebuilt the header from
(submit params) the PoW hash had difficulty 0.00 — header bytes did not
match what we hashed locally.

Root causes fixed in v3:
1. **extranonce2 was written at offset 152** — correct Decred layout is
   nonce@140, en1@144, **en2@148** (gominer Nonce2Word).
2. **timestamp was rewritten with strtoul+LE** which byte-swapped the
   ntime already present in coinb1; pool uses the ntime string as in notify.

## Full fix list (v1–v3)

- Stack 12288
- Seed extranonce2
- 8-char padded nonce in submit
- No mining.suggest_difficulty
- Share filter hash[31]==0
- en2 @ 148
- Leave ntime from coinb1 alone

## Apply

Copy into repo:
```
src/NerdMinerV2.ino.cpp
src/mining_blake3.cpp
```
Rebuild with `-D NERDMINER_BLAKE3=1`.

Expect either `result: true` or a real difficulty number — not `0.00`.
