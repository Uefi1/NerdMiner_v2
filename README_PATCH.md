# BLAKE3 / Decred patch v4

## Root cause of "Low diff: 0.00"

Pool rebuilds the header from (job + extranonce2 + **ntime** + nonce)
and hashes it. We were hashing a header whose timestamp still came from
coinb1 and did not match the ntime we put in mining.submit → pool hash
had difficulty 0.00.

## v4 fix

Write `mJob.ntime` into header offset 136 as **raw hex bytes**
(same style as extranonce1), so local hash == pool hash.

## Apply

```
src/NerdMinerV2.ino.cpp
src/mining_blake3.cpp
```

Rebuild with `-D NERDMINER_BLAKE3=1`.

Success looks like: `"result":true` or a non-zero difficulty in the error
(not `0.00`).
