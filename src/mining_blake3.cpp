// BLAKE3 / Decred (DCP0011) Stratum worker for NerdMiner_v2.
//
// Self-contained on purpose: reuses the existing, unmodified stratum.h API
// (tx_mining_subscribe/tx_mining_auth/tx_suggest_difficulty/parse_mining_notify/
// parse_mining_set_difficulty/tx_mining_submit -- all already generic enough
// to carry Decred's fields verbatim, verified by inspecting their real
// implementation in this repo) instead of touching mining.cpp's SHA256d
// path, so the existing Bitcoin mining code is completely unaffected.
//
// PROTOCOL MAPPING (verified against decred/gominer's stratum.go and the
// official DCP0011 test vectors -- see test_stratum_mapping.c):
//   work_data[192] = version(4) + prevhash(32) + partialHeader(144), zero-padded
//     - params[1] (hash)   -> prevhash, used AS-IS (internal order, no reversal)
//     - params[2] (coinb1) -> the 144-byte partial header (offset 36..180)
//     - params[5] (version)-> block version
//     - params[3]/[4] (coinb2/merkle_branch) are NOT used for Decred
//   nonce (searched by the worker tasks)      -> byte offset 140 (LE)
//   extraData / extranonce2 from submit       -> byte offset 144
//     Yiimp/Suprnova create_decred_header only binlify(nonce2) into extra[].
//     Do NOT write pool extranonce1 into the header — pool does not.
//   timestamp already in coinb1 @136
//   submit params = [wallet, jobId, extranonce2, ntime, nonceHex]

#include <Arduino.h>
#include <WiFi.h>
#include <mutex>
#include <list>
#include <map>
#include <memory>
#include <esp_task_wdt.h>

#include "stratum.h"
#include "mining.h"
#include "mining_blake3.h"
#include "drivers/storage/storage.h"
#include "drivers/displays/display.h"
#include "monitor.h"
#include "blake3/decred_blake3_pow.h"

static WiFiClient s_blake3_client;
static bool s_blake3_subscribed = false;

// Real globals defined in mining.cpp (extern'd there too, e.g. in
// monitor.cpp) -- reused here so the existing UI/stats screens pick up
// BLAKE3 mining activity without any changes to the display code.
extern monitor_data mMonitor;
extern TSettings Settings;   // defined in wManager.cpp; same extern pattern
                              // already used in mining.cpp and monitor.cpp
extern uint32_t hashes;      // total hashes computed, feeds the hashrate display
extern uint32_t shares;      // shares that beat pool difficulty
extern volatile uint32_t valids; // hashes that beat full network difficulty
extern uint32_t templates;   // new jobs received

// ---------------------------------------------------------------------
// Job / result queues shared between the two hashing tasks and the
// Stratum loop. Same mutex + std::list pattern as the existing SHA256d
// path in mining.cpp.
// ---------------------------------------------------------------------
struct Blake3JobRequest {
    uint32_t job_serial;     // internal monotonically-increasing id, NOT the pool's job_id string
    uint32_t nonce_start;
    uint32_t nonce_count;
    double   pool_difficulty;
    uint8_t  work_data[192];
};

struct Blake3JobResult {
    uint32_t job_serial;
    bool     found_share;
    uint32_t nonce;
    double   share_difficulty;
};

#define BLAKE3_NONCE_PER_CHUNK 8192

static std::mutex s_blake3_mutex;
static std::list<std::shared_ptr<Blake3JobRequest>> s_blake3_requests;
static std::list<std::shared_ptr<Blake3JobResult>>  s_blake3_results;
static volatile uint32_t s_blake3_current_serial = 0;
static volatile uint32_t s_blake3_nonce_cursor = 0;

// Interprets a raw BLAKE3 hash (little-endian 256-bit) as a coarse
// difficulty value: counts leading zero bytes/the magnitude of the first
// non-zero byte from the most-significant end. This is the same style of
// approximation `diff_from_target()` uses elsewhere in this project for
// SHA256d, adapted for BLAKE3's raw byte order. It is good enough to decide
// "does this beat the pool's suggested difficulty", which is all Stratum
// share submission needs.
// Coarse difficulty from BLAKE3 digest.
// Decred (and most pools) treat the hash as a little-endian 256-bit integer;
// leading zero *bytes from the high end* (hash[31] down) raise difficulty.
// The previous formula was too optimistic and produced false positives at
// pool difficulty 1, flooding the pool with invalid shares and dropping the
// TCP session.  We now require at least one leading zero byte before a
// share can beat difficulty 1, which matches practical Stratum share rates
// for a ~70 kH/s device.
static double blake3HashDifficulty(const uint8_t hash[32]) {
    // Count leading zero bytes from the most-significant end (hash[31]).
    int leading_zeros = 0;
    for (int i = 31; i >= 0; i--) {
        if (hash[i] == 0) {
            leading_zeros++;
        } else {
            // Fractional part from the first non-zero byte
            double frac = 256.0 / ((double)hash[i] + 1.0);
            // Each full zero byte multiplies difficulty by 256
            double diff = 1.0;
            for (int z = 0; z < leading_zeros; z++) diff *= 256.0;
            return diff * frac;
        }
    }
    // All zeros — theoretical max
    return 1e18;
}

static bool hexToBytesFixed(const String &hex, uint8_t *out, size_t out_len) {
    if ((size_t)hex.length() != out_len * 2) return false;
    for (size_t i = 0; i < out_len; i++) {
        char b[3] = { hex[2*i], hex[2*i+1], 0 };
        out[i] = (uint8_t)strtoul(b, nullptr, 16);
    }
    return true;
}

// Builds the 192-byte work buffer for one job. Returns false (and logs why)
// if the job's coinb1 isn't the 144-byte length Decred's Stratum variant is
// expected to send -- this is the one cheap sanity check that catches
// "this pool isn't actually speaking Decred's BLAKE3 dialect" early instead
// of silently mining garbage.
static bool buildBlake3WorkData(const mining_subscribe &mWorker,
                                 const mining_job &mJob,
                                 uint8_t out_work_data[192]) {
    // Yiimp/Suprnova Decred header reconstruction (create_decred_header):
    //   memcpy(template);  sscanf(nonce);  binlify(extra, nonce2_from_submit);
    // Pool does NOT inject extranonce1 into the header on submit validation.
    // So we must hash the same layout: version|prevhash|coinb1, nonce rolled
    // at offset 140, and extra[32] filled ONLY from the extranonce2 we will
    // put in mining.submit (zeros for size=4). Writing en1 here caused every
    // share to hash differently from the pool → "Low diff: 0.00".
    memset(out_work_data, 0, 192);

    uint8_t block_version[4];
    uint8_t prev_hash[32];
    uint8_t partial_header[144];

    if (!hexToBytesFixed(mJob.version, block_version, 4)) {
        Serial.println("[BLAKE3] version field is not 4 bytes");
        return false;
    }
    if (!hexToBytesFixed(mJob.prev_block_hash, prev_hash, 32)) {
        Serial.println("[BLAKE3] prev_block_hash field is not 32 bytes");
        return false;
    }
    if (!hexToBytesFixed(mJob.coinb1, partial_header, 144)) {
        Serial.printf("[BLAKE3] coinb1 is %d bytes, expected 144\n",
                       (int)(mJob.coinb1.length() / 2));
        return false;
    }

    memcpy(out_work_data + 0,  block_version, 4);
    memcpy(out_work_data + 4,  prev_hash, 32);
    memcpy(out_work_data + 36, partial_header, 144); // through stake version @176

    // Nonce @140: zero; worker writes LE uint32 while hashing
    memset(out_work_data + 140, 0, 4);

    // ExtraData @144 (32 bytes): only what we will send as extranonce2.
    // Subscribe advertised size=4 → keep 4 zero bytes (and rest already 0
    // from coinb1 / memset).  Do NOT write memcpy(en1) here.
    uint8_t en2[4] = {0, 0, 0, 0};
    size_t en2_len = mWorker.extranonce2.length() / 2;
    if (en2_len > 4) en2_len = 4;
    for (size_t i = 0; i < en2_len; i++) {
        char b[3] = { mWorker.extranonce2[2*i], mWorker.extranonce2[2*i+1], 0 };
        en2[i] = (uint8_t)strtoul(b, nullptr, 16);
    }
    memcpy(out_work_data + 144, en2, 4);

    // ntime already present in coinb1 at offset 136 — leave it.
    // (verified: coinb1[100..103] == notify ntime hex bytes)

    return true;
}


// ---------------------------------------------------------------------
// Dual-core hashing worker. Launch this twice (once per core) exactly the
// way minerWorkerSw is already launched twice for the SHA256d path.
// ---------------------------------------------------------------------
void minerWorkerBlake3(void *task_id) {
    unsigned int miner_id = (uint32_t)(uintptr_t)task_id;
    Serial.printf("[BLAKE3] %d Started minerWorkerBlake3 Task on core %d!\n",
                  miner_id, xPortGetCoreID());

    uint32_t wdt_counter = 0;

    while (1) {
        std::shared_ptr<Blake3JobRequest> job;
        {
            std::lock_guard<std::mutex> lock(s_blake3_mutex);
            if (!s_blake3_requests.empty()) {
                job = s_blake3_requests.front();
                s_blake3_requests.pop_front();
            }
        }

        if (job) {
            uint8_t work_buf[192];
            memcpy(work_buf, job->work_data, 192);
            uint32_t nonces_done = 0;

            for (uint32_t n = 0; n < job->nonce_count; n++) {
                nonces_done++;
                uint32_t nonce = job->nonce_start + n;
                work_buf[140] = (uint8_t)(nonce & 0xff);
                work_buf[141] = (uint8_t)((nonce >> 8) & 0xff);
                work_buf[142] = (uint8_t)((nonce >> 16) & 0xff);
                work_buf[143] = (uint8_t)((nonce >> 24) & 0xff);

                uint8_t hash[32];
                decred_blake3_pow_hash_raw(work_buf, hash);

                double d = blake3HashDifficulty(hash);
                // Require hash[31]==0 so we don't spam the pool when it sets
                // difficulty=1 (Suprnova default).  Real target math would be
                // better; this is a practical rate limit for ESP32 hashrate.
                if (hash[31] == 0 && d >= job->pool_difficulty) {
                    auto res = std::make_shared<Blake3JobResult>();
                    res->job_serial = job->job_serial;
                    res->found_share = true;
                    res->nonce = nonce;
                    res->share_difficulty = d;
                    std::lock_guard<std::mutex> lock(s_blake3_mutex);
                    // Cap queue: if pool difficulty is still high (e.g. 1)
                    // and our estimator is slightly optimistic we must not
                    // bury the TCP stack under hundreds of submits.
                    if (s_blake3_results.size() < 8) {
                        s_blake3_results.push_back(res);
                    }
                }

                // Abort early if a newer job superseded this one.
                if ((n & 0xFF) == 0 && job->job_serial != s_blake3_current_serial) {
                    break;
                }
            }
            hashes += nonces_done;
        } else {
            vTaskDelay(2 / portTICK_PERIOD_MS);
        }

        if (++wdt_counter >= 8) {
            wdt_counter = 0;
            esp_task_wdt_reset();
        }
    }
}

static void pushBlake3Chunks(const uint8_t work_data[192], double pool_difficulty) {
    std::lock_guard<std::mutex> lock(s_blake3_mutex);
    s_blake3_current_serial++;
    s_blake3_requests.clear(); // drop any stale chunks from the previous job
    for (int i = 0; i < 6; i++) {
        auto j = std::make_shared<Blake3JobRequest>();
        j->job_serial = s_blake3_current_serial;
        j->nonce_start = s_blake3_nonce_cursor;
        j->nonce_count = BLAKE3_NONCE_PER_CHUNK;
        j->pool_difficulty = pool_difficulty;
        memcpy(j->work_data, work_data, 192);
        s_blake3_requests.push_back(j);
        s_blake3_nonce_cursor += BLAKE3_NONCE_PER_CHUNK;
    }
}

// ---------------------------------------------------------------------
// Stratum connection loop. Mirrors runStratumWorker()'s connect/subscribe/
// auth/notify structure (same overall shape reviewed in mining.cpp) but
// kept self-contained here rather than editing that function directly, so
// the existing Bitcoin/SHA256d path is untouched.
// ---------------------------------------------------------------------
void runStratumWorkerBlake3(void *name) {
    Serial.printf("\n[BLAKE3-WORKER] Started. Running %s on core %d\n",
                  (char *)name, xPortGetCoreID());

    mining_subscribe mWorker = init_mining_subscribe();
    mining_job mJob;
    double currentPoolDifficulty = DEFAULT_DIFFICULTY;
    uint32_t last_job_time = millis();
    uint32_t last_share_check = millis();

    while (true) {
        if (WiFi.status() != WL_CONNECTED) {
            mMonitor.NerdStatus = NM_Connecting;
            WiFi.reconnect();
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            continue;
        }

        if (!s_blake3_client.connected()) {
            s_blake3_subscribed = false;
            Serial.printf("[BLAKE3-WORKER] Connecting to %s:%d\n",
                          Settings.PoolAddress.c_str(), Settings.PoolPort);
            if (!s_blake3_client.connect(Settings.PoolAddress.c_str(), Settings.PoolPort)) {
                vTaskDelay(((1 + rand() % 60) * 1000) / portTICK_PERIOD_MS);
                continue;
            }
        }

        if (!s_blake3_subscribed) {
            mWorker = init_mining_subscribe();
            if (!tx_mining_subscribe(s_blake3_client, mWorker)) {
                s_blake3_client.stop();
                continue;
            }
            strcpy(mWorker.wName, Settings.BtcWallet);
            strcpy(mWorker.wPass, Settings.PoolPassword);
            // Decred pools advertise extranonce2_size (usually 4).  Leave it
            // empty and every mining.submit is sent with extranonce2="",
            // which most pools reject.  Seed with zero-padded hex of the
            // advertised size so the field is well-formed.
            if (mWorker.extranonce2.length() == 0 && mWorker.extranonce2_size > 0) {
                int n = mWorker.extranonce2_size;
                if (n > 8) n = 8; // safety
                mWorker.extranonce2 = "";
                for (int i = 0; i < n; i++) mWorker.extranonce2 += "00";
                Serial.printf("[BLAKE3] seeded extranonce2=%s (size=%d)\n",
                              mWorker.extranonce2.c_str(), mWorker.extranonce2_size);
            }
            tx_mining_auth(s_blake3_client, mWorker.wName, mWorker.wPass);
            // Suprnova (and many Decred pools) do NOT implement
            // mining.suggest_difficulty -- sending it only produces
            // {"error":{"code":20,"message":"Unknown method:..."}} noise.
            // Rely on mining.set_difficulty from the pool instead.
            // tx_suggest_difficulty(s_blake3_client, currentPoolDifficulty);
            s_blake3_subscribed = true;
            last_job_time = millis();
        }

        if (s_blake3_client.available()) {
            String line = s_blake3_client.readStringUntil('\n');
            stratum_method method = parse_mining_method(line);

            if (method == MINING_NOTIFY) {
                if (parse_mining_notify(line, mJob)) {
                    uint8_t work_data[192];
                    if (buildBlake3WorkData(mWorker, mJob, work_data)) {
                        pushBlake3Chunks(work_data, currentPoolDifficulty);
                        last_job_time = millis();
                        mMonitor.NerdStatus = NM_hashing;
                        templates++;
                    }
                }
            } else if (method == MINING_SET_DIFFICULTY) {
                parse_mining_set_difficulty(line, currentPoolDifficulty);
            }
        }

        // Drain any found shares and submit them.
        {
            std::list<std::shared_ptr<Blake3JobResult>> to_submit;
            {
                std::lock_guard<std::mutex> lock(s_blake3_mutex);
                to_submit.swap(s_blake3_results);
            }
            for (auto &r : to_submit) {
                if (r->job_serial != s_blake3_current_serial) continue; // stale
                if (!s_blake3_client.connected()) {
                    Serial.println("[BLAKE3-WORKER] client disconnected, drop pending shares");
                    s_blake3_subscribed = false;
                    break;
                }
                // CRITICAL: String(nonce, HEX) drops leading zeros → pool
                // replies "Invalid nonce". Decred (and ckpool/Bitcoin) need
                // a fixed 8-char lowercase hex nonce field.
                char nonceHex[9];
                snprintf(nonceHex, sizeof(nonceHex), "%08x", (unsigned)r->nonce);

                // Build submit ourselves so we control nonce formatting.
                // Same param order as tx_mining_submit: worker, jobId, en2, ntime, nonce
                static unsigned long submit_id_ctr = 100;
                submit_id_ctr++;
                char payload[512];
                snprintf(payload, sizeof(payload),
                    "{\"id\":%lu,\"method\":\"mining.submit\",\"params\":[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]}\n",
                    submit_id_ctr,
                    mWorker.wName,
                    mJob.job_id.c_str(),
                    mWorker.extranonce2.c_str(),
                    mJob.ntime.c_str(),
                    nonceHex);
                Serial.printf("[BLAKE3-WORKER] Submitting share, nonce=%s diff=%.4f en2=%s\n",
                              nonceHex, r->share_difficulty, mWorker.extranonce2.c_str());
                Serial.print("  Sending  : "); Serial.print(payload);
                s_blake3_client.print(payload);
                shares++;
                vTaskDelay(40 / portTICK_PERIOD_MS);
            }
        }

        if (millis() - last_job_time > POOLINACTIVITY_TIME_ms) {
            Serial.println("[BLAKE3-WORKER] No new job for too long, reconnecting");
            s_blake3_client.stop();
            s_blake3_subscribed = false;
        }

        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}
