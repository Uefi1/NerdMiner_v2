# NerdMiner_v2 → BLAKE3 / Decred

Полный NerdMiner (WiFi Manager, экраны, Stratum, dual-core) с майнингом **BLAKE3** вместо SHA256d.

## Что изменено

| Компонент | Статус |
|-----------|--------|
| WiFi Manager / config portal | без изменений |
| Экраны / monitor | без изменений |
| Stratum subscribe / auth / set_difficulty | без изменений |
| `minerWorkerSw` × 2 ядра | **BLAKE3** PoW |
| Hardware SHA path | отключён |
| Header | 180 байт Decred (DCP0011) |
| Default difficulty | `0.00001` (чтобы ESP32 ~70 kH/s мог находить shares) |

## Как собрать

1. Возьми **свой** рабочий клон `Uefi1/NerdMiner_v2` (или BitMaker-hub).
2. Скопируй из этого архива:
   ```
   src/blake3/          →  src/blake3/          (весь каталог)
   src/mining.cpp       →  src/mining.cpp
   src/mining.h         →  src/mining.h
   src/utils.cpp        →  src/utils.cpp
   src/NerdMinerV2.ino.cpp  (только если у тебя старый — stack 8192)
   ```
3. В `platformio.ini` для твоего env (например `ESP32-S3-devKitv1`) добавь:
   ```
   -D NERDMINER_BLAKE3=1
   ```
4. Собери и прошей как обычно:
   ```
   pio run -e ESP32-S3-devKitv1 -t upload
   ```

## Настройка пула (config portal)

1. Подключись к **NerdMinerAP** / `MineYourCoins`
2. Открой `192.168.4.1`
3. Параметры для теста на Decred pool:

| Поле | Пример |
|------|--------|
| Pool URL | `dcr.suprnova.cc` |
| Port | `9332` (low diff) |
| Wallet / Worker | `ТвойDCRАдрес.nerd1` |
| Password | `d=0.00001` или `x` |

Другие варианты:
- `stratum+tcp://dcr.suprnova.cc:9332`
- Локальный [dcrpool](https://github.com/decred/dcrpool) на `:5550` (solo)

## Что смотреть в Serial

```
[MINER] 0 Started minerWorkerSw (BLAKE3/Decred) Task on core 0!
[MINER] 1 Started minerWorkerSw (BLAKE3/Decred) Task on core 1!
    [DECRED] Building 180-byte BLAKE3 header from notify
    [DECRED] BLAKE3 job ready, skipping SHA256 midstate
```

Hashrate на экране / в serial должен быть порядка **~65–70 kH/s** (как на dual-core бенчмарке).

## Важно про Stratum

Decred pools используют формат **getwork-over-stratum** (не чистый Bitcoin notify):

- `params[2]` (coinb1) — длинный hex кусок header, а не короткий coinbase
- детект: `coinb1.length() >= 200` → включается Decred/BLAKE3 path

Если pool шлёт другой диалект (чистый dcrpool Haste / другой layout полей), header может собраться неверно — shares будут rejected. В этом случае пришли пример `mining.notify` из Serial, подправим offsets.

## Submit

Формат submit тот же, что у NerdMiner:
```
mining.submit [worker, job_id, extranonce2, ntime, nonce]
```
Для многих Decred pools этого достаточно. Если pool требует другой порядок полей — тоже правится точечно в `tx_mining_submit`.

## Ожидания по shares

При ~70 kH/s и difficulty `0.00001`:
- share roughly каждые несколько секунд–минут (зависит от VarDiff пула)
- на network difficulty Decred (GPU-era) блок на ESP32 нереален — цель: accepted shares на pool

## Файлы в этом пакете

```
src/blake3/          — portable BLAKE3 + decred_blake3_pow
src/mining.cpp/h     — dual-core BLAKE3 workers + job queue
src/utils.cpp        — calculateMiningData с Decred branch
README_BLAKE3.md     — этот файл
```
'''
