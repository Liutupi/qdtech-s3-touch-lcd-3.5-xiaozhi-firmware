QDTech ESP32-S3 Touch LCD 3.5 - v1.8.5

Contents
========

- qdtech-s3-touch-lcd-3.5-v1.8.5-app.bin
  OTA application image. Use this for normal OTA updates.

- qdtech-s3-touch-lcd-3.5-v1.8.5-full.bin
  Complete recovery image. Flash at offset 0x0 only when recovering a board.
  It may clear local Wi-Fi provisioning state.

- qdtech-s3-touch-lcd-3.5-v1.8.5-firmware.zip
  Convenience archive containing the app and complete recovery images.

- qdtech-s3-touch-lcd-3.5-v1.8.5-sdcard.zip
  Extract this archive to the root of a FAT-formatted SD card. It supplies
  the Calendar Zodiac reading content and the twelve cute Zodiac images.

v1.8.5 fixes Shake Lab BMI270 high-rate sample starvation. It is an OTA
successor to v1.8.4; calendar Zodiac, Bone Weight, Wi-Fi, weather, and
other verified functions are retained.
