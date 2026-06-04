# QDTech Firmware Changelog

This changelog tracks QDTech-specific firmware maintenance. It is not a replacement for `git log`; it records the practical handoff facts that future maintainers need.

## 2026-06-04: Project Handoff Notebook

Scope:

- Added a documentation-only handoff set under `docs/`.
- No source code changed.
- No firmware behavior changed.

Reason:

- The project is becoming a long-term maintained firmware fork.
- Future Codex sessions and new computers need a reliable first-read path before touching code.

Verification:

- Documentation-only change. Firmware build/flash not required for this entry.

## 2026-06-04: Add MicroSD Photo Slideshow Entry

Scope:

- Replaced the Apps page Weather tile with a Photos tile.
- Added a dedicated Photos page with SD slideshow status, Refresh control, and fade transition image area.
- Added `PhotoService` to mount the MicroSD card, scan `/sdcard/photos`, decode JPEG files, and loop playback.
- Added QDTech MicroSD SDMMC pins from the product specification:
  - CLK GPIO5
  - CMD GPIO4
  - D0 GPIO6
  - D1 GPIO7
  - D2 GPIO2
  - D3 GPIO3
- Photo decoding pauses when leaving the Photos page.

Verification:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" reconfigure build
cmake --build build-qdtech
```

Build result:

- `cmake --build build-qdtech` completed successfully.
- `xiaozhi.bin` size: `0x3ae7c0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x251840` bytes, about 39%.

Hardware verification:

- Flashed to COM13 at 921600 baud successfully.
- Runtime boot verified after flashing: PSRAM, display, touch init, WiFi, MQTT, wake-word pipeline, `PhotoService: photo service started`, and `Application: STATE: idle`.
- Photos page SD mount and JPEG playback still need an inserted MicroSD card and an on-device tap into the Photos page.
- Prepare a FAT-formatted MicroSD card with JPG files in `/photos`.

## 2026-06-04: Improve Photo SD Filename Diagnostics

Scope:

- Enabled FATFS long filename support in the QDTech board defaults for the MicroSD photo slideshow.
- Added `errno` / `strerror` details to photo `stat()` and `fopen()` failure logs.

Reason:

- Hardware test reached SD card directory scanning, but photo files failed at open/stat with 8.3-style names such as `/sdcard/photos/WEIQU.JPG`.
- The active build config had `CONFIG_FATFS_LFN_NONE=y`, which is risky for copied photo files and long filenames.

Verification:

- Rebuilt successfully with `cmake --build build-qdtech`.
- Flashed successfully to COM13 at 921600 baud.

## 2026-06-04: Add Local Calendar Month View

Scope:

- Added a dedicated Calendar page in the QDTech desktop UI.
- Calendar app tile now opens the local Calendar page instead of starting a XiaoZhi chat prompt.
- Calendar page shows a 7x6 month grid, previous/next month controls, a Today control, weekend coloring, and today highlight.
- Time/weather service now passes the synced year into `DesktopUI::SetTime()` so the calendar can track the current date.
- Status bar time tracking was expanded to cover the added page.

Verification:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build
cmake --build build-qdtech
```

Build result:

- `cmake --build build-qdtech` completed successfully.
- `xiaozhi.bin` size: `0x39e7f0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x261810` bytes, about 40%.

Hardware verification:

- Flashed successfully to `COM13`.
- Boot log confirmed `SKU=qdtech-s3-touch-lcd-3.5`.
- Desktop UI was created.
- Touch reported `Touch max points: 5`.
- WiFi connected with IP `192.168.4.92`.
- MQTT connected.
- AFE/wake word started.
- Application reached `STATE: idle`.
- SNTP time synchronized.
- Touch path verified:
  - left swipe from main page opened Apps.
  - tapping Calendar tile opened the Calendar page.
  - tapping Next changed the Calendar month to `2026/07`.
- Weather API returned `502` during this run, but the firmware stayed running.

## 2026-06-04: Stabilize QDTech Desktop Base

Commit:

- `f4451c1 stabilize QDTech desktop base`

Scope:

- Added/updated QDTech board README and root `PATCHES.md`.
- Added lightweight device-state callback support in `Application`.
- Improved radio service audio focus, reconnect, fallback URL memory, and idempotent controls.
- Improved time/weather service retry and cached-weather behavior.
- Clarified touch polling and display flush performance logging in board code.

Verified build:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build
```

Verified flash:

```powershell
idf.py -B build-qdtech -p COM13 -b 921600 flash
```

Observed runtime:

- Board booted with `SKU=qdtech-s3-touch-lcd-3.5`.
- Desktop UI was created.
- Touch reported `Touch max points: 5`.
- WiFi connected and MQTT connected.
- AFE/wake word started.
- Application reached `STATE: idle`.
- Weather API returned transient failures during testing, but firmware stayed running.

## Earlier QDTech Milestones

Known recent commits from this fork:

- `d6f1f04 feat: WiFi管理优化、电台更新、小智AI面部动画改进`
- `83b1983 docs add qdtech reproducible build notes`
- `ac1e8a2 feat add network radio playback`
- `8d79b78 fix desktop button tap handling`
- `18ffaf7 fix app card tap coordinates`
- `1677e5f fix app cards open xiaozhi`
- `a4e98e0 feat configurable weather location`

When continuing from these commits, verify behavior on hardware rather than assuming the commit message proves runtime success.
