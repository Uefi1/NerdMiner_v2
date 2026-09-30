# BLAKE3 / Decred patch v6

## Root cause (confirmed from yiimp source)

`job_mining_notify_buffer` sends `templ->prevhash_be` in mining.notify.
Pool share validation uses the binary template where prevhash is in
**internal** byte order.

We were writing the BE form into the header → BLAKE3 never matched →
`Low diff: 0.00`.

## Fix

Word-swap (swab32 each 4-byte group) of prevhash when building the
180-byte header — same as ccminer ALGO_DECRED.

Also retained from earlier patches:
- no en1 in header (yiimp only binlify nonce2 into extra)
- LE nonce @140, padded 8-char submit
- stack 12288, etc.

## Apply

src/NerdMinerV2.ino.cpp
src/mining_blake3.cpp

Rebuild with -D NERDMINER_BLAKE3=1
