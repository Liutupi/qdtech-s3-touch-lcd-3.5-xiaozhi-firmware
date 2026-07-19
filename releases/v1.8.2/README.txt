QDTech ESP32-S3 3.5-inch v1.8.2 OTA release
=====================================================

Firmware version: 1.8.2
Release purpose: QDTech GitHub OTA update.

What changed:

  - Fixes first provisioning visibility on QDTech: channel 6, retained APSTA
    Wi-Fi driver, and a standards-based compatibility Beacon.
  - Restores normal full-application startup before saved Wi-Fi connection.
  - Starts the phone configuration page automatically after Wi-Fi connects:
    http://<board-ip>/ (Logo, Name, Weather City).
  - Localizes all Apps-center entry cards and common status text to Chinese.

Files:

  qdtech-s3-touch-lcd-3.5-v1.8.2-app.bin
    OTA/app-partition image. The on-device Firmware Update page discovers this
    exact asset in the latest GitHub Release. For USB App-only flashing, use
    offset 0x100000.

  qdtech-s3-touch-lcd-3.5-v1.8.2-full.bin
    Complete recovery image for USB flashing at offset 0x0 only. Do not use it
    as an OTA payload.

  qdtech-s3-touch-lcd-3.5-v1.8.2-firmware.zip
    Contains both App and full recovery images.

  qdtech-s3-touch-lcd-3.5-v1.8.2-sdcard.zip
    Unchanged additive SD-card package retained for Calendar content.

OTA instructions:

  1. Connect the board to Wi-Fi and wait for idle.
  2. Stop audio playback.
  3. Open Firmware Update, tap Check, then install v1.8.2 when offered.
  4. After restart, confirm the board reports v1.8.2. The phone configuration
     page is available at the IP shown by the Network card.

Check SHA256SUMS.txt before any manual USB flash.

Verification note: build and preceding functional hardware paths passed. The
final v1.8.2 App-only USB check was intentionally deferred at release time
because the board was removed; remote OTA is the requested final verification.
