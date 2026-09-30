# BLAKE3 / Decred patch v7

This patch fixes three independent problems found in the previous v6 build.

## 1. Exact share difficulty

The old BLAKE3 worker treated `hash[31] == 0` as enough for difficulty 1 and
estimated difficulty from only the first non-zero byte.  That is not the
Stratum share test.  A diff-1 share requires the full 256-bit hash to beat the
full diff-1 target.

v7 reuses NerdMiner's existing `diff_from_target()` 256-bit conversion, so a
candidate is submitted only when its real difficulty is >= the pool's
`mining.set_difficulty` value.

This alone explains the previous log pattern:

    local diff=370
    pool -> Low diff: 0.00 < 1

The old code was generating many false positives (70 submissions from only
~98 kH of hashing).

## 2. Correct Decred header byte order and extranonce

For yiimp/Suprnova, `mining.notify` sends `prevhash_be` in display order.
The Decred header stores the previous hash in internal order, which requires a
FULL 32-byte reversal. v6 only swapped bytes inside each 32-bit word.

v7 therefore does:

    header.prevhash = reverse(notify.prevhash, 32 bytes)

The DCP0011 extra-data field is 36 bytes at header offset 144. The pool's
Stratum identity is:

    extranonce1 || extranonce2

v6 incorrectly put only extranonce2 into the header. v7 writes both values,
which also supports yiimp's Decred-specific 24-byte extranonce1 + 12-byte
extranonce2 layout as well as the 4+4 layout seen from dcr.suprnova.cc in the
captured log.

## 3. Continuous hashing

v6 queued only six 8192-nonce chunks (49,152 nonces) for each job and then
went idle until another `mining.notify`.

v7 keeps allocating fresh nonce chunks from the current job until a new job
arrives. The two ESP32 cores receive separate nonce ranges.

## Share accounting

`shares` is now incremented only after the pool returns a successful
`mining.submit` response. Rejected submissions are printed explicitly.

## Expected testing

Your captured pool session reported difficulty 1. At ~5 KH/s, a true diff-1
share is statistically rare (on the order of 10 days on average), so do not
judge correctness by waiting for an accepted share at diff=1.

For a quick test, use a fixed low difficulty accepted by the pool, e.g. the
pool's `d=...` password option. If the pool enforces a higher minimum, use its
lowest supported value.

After flashing, the important diagnostics are:

    [BLAKE3] job=... prev=... en1=... en2=... ntime=...
    [BLAKE3-WORKER] Submitting share, nonce=... diff=...
    [BLAKE3] SHARE ACCEPTED ...

or, if the header is still wrong:

    [BLAKE3] SHARE REJECTED ... Low diff ...

The first v7 test should therefore focus on whether the worker keeps hashing
continuously and whether any low-difficulty test share is accepted.
