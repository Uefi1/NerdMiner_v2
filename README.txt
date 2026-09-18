# Исправленный патч Pool Password для Uefi1/NerdMiner_v2

Это ИСПРАВЛЕННАЯ версия патча после проверки актуального `main` Uefi1/NerdMiner_v2.

Причина, почему поле не появлялось:
в `src/wManager.cpp` поле создавалось с ID:
    Poolpassword - Optional

WiFiManager не принимает пробелы/дефисы в ID параметров. Поэтому параметр
фактически не отображался. Исправление использует ID `Poolpassword` и
`type="password"`.

Дополнительно:
- `wPass` увеличен с 20 до 80 байт;
- копирование сделано через snprintf;
- добавлена гарантированная NUL-терминация после strncpy;
- пароль больше не печатается открытым текстом в Serial.

ВАЖНО:
Не применяй предыдущий ZIP-патч. Используй только этот.

Применение к ЧИСТОМУ Uefi1/NerdMiner_v2:
    git apply nerdminer_pool_password_ESP32S3_fixed.patch

Проверка:
    git diff --check
    git diff

Сборка только ESP32-S3-devKitv1:
    pio run -e ESP32-S3-devKitv1

Для GitHub Actions убедись, что workflow собирает именно:
    ESP32-S3-devKitv1

После сборки нужны:
    ESP32-S3-devKitv1_factory.bin
    ESP32-S3-devKitv1_firmware.bin

Для первой установки factory.bin является merged/full image и прошивается с 0x000000.
Для обычного обновления firmware.bin используй адрес, предусмотренный flasher
проекта (обычно 0x10000).

После прошивки:
1. Снова открой Config Portal.
2. Должно появиться `Pool password (Optional)` между Pool port и Your BTC address.
3. Поле будет скрывать введённые символы.
4. Введи пароль и сохрани настройки.

Если после применения ЭТОГО патча поле снова не появится, не прошивай ничего
дальше наугад: пришли ссылку на commit/Action run, который GitHub собрал, или
лог GitHub Actions. Тогда можно будет проверить именно тот исходник, из которого
получился BIN.
