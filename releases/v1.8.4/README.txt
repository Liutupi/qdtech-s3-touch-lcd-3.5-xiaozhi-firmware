QDTech S3 Touch LCD 3.5 — v1.8.4 OTA Release

Use qdtech-s3-touch-lcd-3.5-v1.8.4-app.bin for an on-device OTA update.
Use qdtech-s3-touch-lcd-3.5-v1.8.4-full.bin only for serial recovery.

v1.8.4 adds the Chinese "掌卦" experience to Shake Lab. Three coins animate,
six lines appear from bottom to top, then the hexagram image and interpretation
are shown. The 64 hexagram images, text, and line patterns are on the SD card.

Copy the contents of qdtech-s3-touch-lcd-3.5-v1.8.4-sdcard.zip to the SD card
root. It retains the prior calendar/zodiac package and adds shake_lab/divination.

Stability fix: shake completion no longer executes UI, SD, or animation work in
the BMI270 sensor task. A captured BMI270 stack overflow at reveal was fixed by
dispatching the UI transition to the application thread and increasing the
sensor task stack guard margin.
