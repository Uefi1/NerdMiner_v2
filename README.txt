NerdMiner_v2 — фикс поля "Pool password" + сборка только под ESP32-S3-devKitv1
================================================================================

ЧТО БЫЛО СЛОМАНО
-----------------
В src/wManager.cpp поле пароля пула создавалось так:

  WiFiManagerParameter password_text_box("Poolpassword - Optional", "Pool password", Settings.PoolPassword, 80);

Первый аргумент — это ID параметра. Библиотека WiFiManager (v2.0.17) в
методе addParameter() проверяет ID и ДОПУСКАЕТ только буквы, цифры и "_".
Так как в ID были пробелы и дефис, addParameter() возвращал false и поле
молча НЕ добавлялось на страницу настройки — поэтому в веб-интерфейсе
его не было видно, хотя вся остальная логика (сохранение в NVS/SD,
использование пароля при подключении к пулу в mining.cpp) уже была на месте.

ЧТО ИСПРАВЛЕНО
--------------
1. src/wManager.cpp
   - ID параметра исправлен на "Poolpassword" (только буквы/цифры).
   - Подпись поля вынесена в label: "Pool password (Optional)".
   - Добавлен атрибут type="password", чтобы пароль отображался
     звёздочками, а не открытым текстом.

2. platformio.ini
   - default_envs сокращён до одного окружения: ESP32-S3-devKitv1
     (это то самое, которое даёт ESP32-S3-devKitv1_factory.bin и
     ESP32-S3-devKitv1_firmware.bin). Полный список плат оставлен
     закомментированным строкой выше — можно вернуть в любой момент.
     Теперь `pio run` без -e соберёт только нужную плату.

КАК ПРИМЕНИТЬ
-------------
Вариант А (проще всего — вручную заменить файлы):
  1. Скопируй wManager.cpp -> <твой форк>/src/wManager.cpp
  2. Скопируй platformio.ini -> <твой форк>/platformio.ini
  3. Закоммить и запушь.

Вариант Б (через git patch):
  cd <твой форк>
  git apply nerdminer_pool_password_fix.patch

ПОСЛЕ ЭТОГО
-----------
  pio run                     # соберёт только ESP32-S3-devKitv1
  # бинарники появятся в .pio/build/ESP32-S3-devKitv1/
  #   - firmware.bin  (обычная прошивка)
  #   - merged-*.bin / factory.bin (если используется post_build_merge.py)

Если у тебя настроен GitHub Actions в форке — он тоже соберёт только эту
плату после пуша, релизные .bin с "_factory" и "_firmware" в имени
будут именно под ESP32-S3-devKitv1.

ПРОВЕРКА В ВЕБ-ИНТЕРФЕЙСЕ
--------------------------
Зайди в портал настройки устройства (AP-режим или кнопка сброса конфига) —
теперь между "Pool port" и "Your BTC address" появится поле
"Pool password (Optional)". Введённое значение сохраняется в
Settings.PoolPassword и передаётся в mWorker.wPass при авторизации
на stratum-пуле (src/mining.cpp), т.е. полностью рабочее end-to-end.

================================================================================
ДОПОЛНИТЕЛЬНО: фикс CI-релиза (.github/workflows/release.yml)
================================================================================

СИМПТОМ
-------
В логе GitHub Actions джоба "release" падала на шаге trstringer/manual-approval:
  error creating issue: POST .../issues: 410 Issues has been disabled in this repository.

ПРИЧИНА
-------
1) В форке в настройках репозитория выключены Issues (Settings > General >
   Features > Issues), а manual-approval создаёт issue для подтверждения релиза.
2) Даже если включить Issues, approver там прописан как "BitMaker-hub" —
   аккаунт мейнтейнера апстрима, у него нет прав аппрувить в твоём форке,
   релиз просто провисит 120 минут и свалится по таймауту.

ЧТО ИСПРАВЛЕНО
--------------
Шаг trstringer/manual-approval и permission "issues: write" удалены из job
"release". Теперь после успешной сборки релиз на GitHub публикуется сразу,
без ожидания стороннего подтверждения — то есть логично для личного форка.

ЕСЛИ ХОЧЕШЬ ОСТАВИТЬ РУЧНОЕ ПОДТВЕРЖДЕНИЕ (альтернатива, ничего патчить не надо)
---------------------------------------------------------------------------------
1. Включи Issues: Settings > General > Features > Issues (галочка).
2. В release.yml замени approvers: BitMaker-hub на approvers: <твой GitHub логин>.
