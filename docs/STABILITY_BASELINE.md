# QDTech Stability Baseline

This document defines the known recovery points and verification expectations
for the QDTech ESP32-S3 3.5-inch touch-LCD firmware. It does not authorize a
checkout, reset, flash, OTA update, commit, push, or release operation.

## 1. Current Hardware Baseline

**CURRENT HARDWARE BASELINE — PARTIAL HARDWARE VERIFICATION**

- Firmware: `v1.7.97`
- Commit: `019b6c13c570f2eaf4a816c954a372e973e02bae`
- Build environment: ESP-IDF `5.5.2`
- App image size: `6,708,528` bytes
- Free space in the 7 MB app slot: `631,504` bytes (`9%`)

Verified:

- Firmware compiled and linked as `1.7.97`.
- App image was flashed at offset `0x100000`.
- esptool verified the written image hash.
- The board restarted normally.
- Serial reported `App version: 1.7.97`.
- OTA reported `Current version: 1.7.97`.
- Desktop UI creation completed.
- BMI270 polling remained active after boot.

Not yet verified:

- Wait at least 75 seconds after boot, rotate the board 90 degrees, and confirm
  entry into the particle hourglass.
- Confirm upper-grain placement, falling-grain motion, landing spread, lower-pile
  geometry, clipping, frame pacing, and overall physical-screen appearance.
- Confirm return from Hourglass to Main still preserves weather refresh and touch.

Do not describe the particle-hourglass visuals as hardware-verified until these
checks have been completed on the physical panel.

## 2. Media-Verified Fallback Baseline

**MEDIA-VERIFIED FALLBACK BASELINE**

- Firmware: `v1.7.96`
- Commit: `ec4239b2c47f83e2bf13b1a7fc096e607d546426`
- Build environment: ESP-IDF `5.5.2`
- App image size: `6,708,368` bytes
- Free space in the 7 MB app slot: `631,664` bytes (`9%`)

Verified:

- Music playback and audio output ran on physical hardware.
- LRC parsing recovered a 1,945-byte provider payload into 41 scheduled lines.
- Lyrics displayed and advanced on the board.
- MQTT, weather, daily quote, touch, and audio output were observed operating.
- The app image was flashed and the board booted as `v1.7.96`.

Important retained behavior:

- Lyrics use the broad-coverage 16 px font.
- Experimental 20/24 px lyric fonts were rejected after audible stutter returned.
- Experimental burst buffering and resampling changes were rolled back.
- The radio/music task uses the verified priority/core arrangement from v1.7.96.

Not fully established by the recorded v1.7.96 pass:

- It is not a proof that every station, song provider, podcast, or long-duration
  playback sequence is regression-free.
- Minimum internal SRAM remains sensitive to concurrent UI, network, voice, and
  media activity.

Use this commit as the recovery reference when a later change regresses music,
lyrics, radio, weather, touch, MQTT, or audio output. Recovery requires explicit
user authorization; do not reset or flash automatically.

## 3. Toolchain And QDTech Build

Current verified ESP-IDF version: `5.5.2`.

Preferred QDTech-specific build command:

```bash
idf.py -B build-qdtech \
  -D SDKCONFIG="build-qdtech/sdkconfig" \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" \
  build
```

The macOS repository path contains spaces. ESP-SR-generated unquoted linker
search paths have previously failed from this location. A no-space source/build
path may be required for a full build, but using one must not overwrite the
working tree or conceal uncommitted user files.

## 4. Flash And OTA Layout

QDTech uses the 16 MB layout in `partitions/v1/16m_qdtech_7m_ota.csv` with two
7 MB OTA application slots. NVS remains at `0x9000`. The app-only image is
normally written at `0x100000`; a merged recovery image is written at `0x0`.

Partition tables, offsets, Flash size, OTA slot size, bootloader data, and NVS
must not be changed as incidental work. OTA and flashing always require explicit
user authorization for the exact image and connected device.

## 5. Known High-Risk Areas

- LVGL `FULL` rendering, rotated flush, translucent weather GIFs, ST77922 init,
  and the 320x480 physical to 480x320 logical orientation path.
- CST9217 manual polling, coordinate parsing, tap/swipe classification, recovery,
  and shared-I2C interaction with BMI270.
- BMI270 sampling rate, hourglass orientation detection, Shake Lab thresholds,
  and the 75-second hourglass startup guard.
- XiaoZhi voice lifecycle, audio-channel allocation, ES8311 output, music/radio/
  podcast decoding, lyric scheduling, and audio focus.
- Internal SRAM pressure from Wi-Fi, TLS, MQTT, MCP descriptions, media buffers,
  photos, SD access, weather, optional services, and task stacks.
- OTA validation, download staging, partition selection, image size/hash checks,
  and NVS/rollback behavior.
- Tracked component hotfixes, especially
  `managed_components/78__esp-ml307/esp_udp.cc`.
- FC/Nofrendo direct display and active memory. Previous broad PSRAM moves caused
  physical-hardware freezes.
- Chinese font subsets and dynamic user/provider text.

Display, touch, audio, media scheduling, OTA, partitions, and AFE/WakeNet must
follow the experimental-switch and rollback requirements in `AGENTS.md`.

## 6. AFE And WakeNet Constraint

The current QDTech low-memory baseline intentionally keeps local AFE/WakeNet
disabled. Do not enable `CONFIG_USE_AFE_WAKE_WORD`, `CONFIG_USE_ESP_WAKE_WORD`,
or `CONFIG_USE_AUDIO_PROCESSOR` in a normal release build.

Recorded tests show that enabling the local pipelines with the full UI can push
internal SRAM below 1 KB or exhaust audio-related allocations. Any future test
must be separately planned, guarded by a default-off
`CONFIG_QDTECH_EXPERIMENT_*` option, retain the current touch-based voice path,
and have a one-step rollback. It must not be combined with another high-risk
subsystem change.

## 7. Physical Regression Checklist

Record the firmware version, complete commit SHA, serial port, build size, and
important logs for every hardware pass.

- Boot: version, board SKU, PSRAM, partition, no reboot/backtrace.
- Network: saved Wi-Fi reconnect, IP, RSSI, MQTT connection, idle state.
- Time/weather: SNTP or valid clock, daily card, weather success/cache/failure,
  weather animation intact, and refresh after returning from apps.
- Display: landscape orientation, no inversion, no flower screen, no stale direct
  pixels, accepted UI geometry, and intact translucent weather GIF composition.
- Touch: taps, swipes, scrolling, sliders, repeated same-position taps, all four
  screen edges, and no sustained I2C timeout/reset loop.
- BMI270: normal low-rate polling, no touch starvation, Hourglass entry/exit,
  Shake Lab behavior, cooldown, and false-trigger checks.
- Hourglass: wait 75 seconds, rotate to enter, start/pause/reset, particle layout,
  falling/landing animation, completion sound, exit, and Main/weather recovery.
- XiaoZhi: enter listening, recognize input, speak, and return to idle without a
  reboot or broken audio channel.
- Music/lyrics: request multiple songs, full URL acceptance, continuous output,
  lyric parsing/scheduling/display, song cutover, Pause/Play/Next/Stop, and no
  audible stutter.
- Radio: Play/Stop/Next/Previous, dead-URL fallback, page exit while playing, and
  recovery after XiaoZhi releases audio focus.
- Podcast/photos/SD: lazy startup, navigation, media output/rendering, exit, and
  no starvation of voice, touch, weather, or MQTT.
- Memory: stable internal SRAM, minimum SRAM, largest internal block, task-create
  failures, and TLS/audio allocation failures during concurrent activity.
- OTA, only when explicitly in scope: block unsafe active states, download and
  hash validation, target partition, reboot, new version, and validity marking.

## 8. Recovery Principles

1. Stop expanding the task when a regression appears. Capture logs and the exact
   user-visible symptom first.
2. Identify the last appropriate baseline: v1.7.97 for the current hardware
   state, or v1.7.96 for media behavior.
3. Compare one high-risk subsystem at a time. Do not combine display, touch,
   audio, OTA, or partition changes in a single recovery attempt.
4. Prefer disabling a default-off experiment and returning to the preserved
   stable path. Do not rewrite the stable implementation during rollback.
5. Never use `git clean`, `git reset`, `git stash`, force-push, NVS erase, flash,
   or OTA as an automatic recovery action.
6. Obtain explicit user authorization before checking out a baseline, modifying
   files, flashing, starting OTA, committing, pushing, tagging, or releasing.
7. Rebuild and repeat the relevant physical regression checklist. Compilation
   alone is not recovery evidence.
8. If only the build is verified, report exactly:
   `BUILD VERIFIED; HARDWARE VERIFICATION PENDING`

## 9. Working-Tree Ownership

`docs/ui-concepts/` is untracked user content. It is not part of this baseline
documentation task. Do not inspect, reorganize, delete, move, format, stage,
ignore, or commit it. Its presence means the working tree is intentionally not
clean and must be reported without attempting cleanup.
