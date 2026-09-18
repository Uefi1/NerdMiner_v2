NerdMiner_v2 — ESP32-S3-devKitv1 pool-password patch

Target:
  PlatformIO environment: ESP32-S3-devKitv1
  board: esp32-s3-devkitc-1

This patch:
  - fixes the WiFiManager parameter ID so the pool-password field is accepted;
  - makes the web field a password input;
  - keeps PoolPassword at 80 bytes end-to-end;
  - prevents the old 20-byte wPass overflow;
  - guarantees NUL termination after strncpy;
  - stops printing the actual pool password to Serial.

IMPORTANT:
  Do NOT apply the earlier pool-password patch from this conversation.
  Apply only this patch to a clean/current Uefi1/NerdMiner_v2 checkout.

Apply:
  git apply nerdminer_pool_password_esp32s3.patch

Build ONLY the requested target:
  pio run -e ESP32-S3-devKitv1

The project's post_build_merge.py creates:
  firmware/<version>/ESP32-S3-devKitv1_firmware.bin
  firmware/<version>/ESP32-S3-devKitv1_factory.bin

If using GitHub Actions, run the build for ESP32-S3-devKitv1 only. The source patch
does not alter other board environments.

Flashing:
  - Factory/full install: use ESP32-S3-devKitv1_factory.bin.
  - Firmware-only update on an already compatible installation: use
    ESP32-S3-devKitv1_firmware.bin at the firmware/application offset used by
    the project's flasher (normally 0x10000).

For a first install, prefer the factory image or the repository's web flasher.
Do not erase flash unless you deliberately want to remove existing configuration.

After flashing:
  1. connect to NerdMinerAP;
  2. open the configuration portal;
  3. the new "Pool password (Optional)" field should be visible;
  4. enter the pool password/options and save;
  5. reboot and check the serial log if needed.

Security note:
  The password is only masked in the web form and is no longer printed in clear
  text to Serial. It is still stored in the device configuration, so this is not
  encryption-at-rest.
