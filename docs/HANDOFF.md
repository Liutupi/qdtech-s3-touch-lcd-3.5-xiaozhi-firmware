# QDTech 3.5 XiaoZhi Project Handoff

> Future Codex note: read this file, `docs/PROJECT_STATUS.md`, `docs/NEXT_TASKS.md`, and `docs/CODEX_RULES.md` before changing code.

## What This Project Is

This repository is a long-term firmware fork based on official `xiaozhi-esp32`, adapted for the QDTech ESP32-S3 3.5 inch touch LCD board.

The current goal is not to rewrite XiaoZhi. The goal is to keep the official voice core working while turning this board port into a stable desktop-system base with display, touch, time/weather, network radio, and XiaoZhi state integration.

Main user GitHub remote:

- `qdtech-new`: `https://github.com/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware.git`

Official upstream remote:

- `origin`: `https://github.com/78/xiaozhi-esp32.git`

## Current Board Entry Point

The board-specific code lives here:

- `main/boards/qdtech-s3-touch-lcd-3.5/`

Important files:

- `config.h`: board pins, display size, audio codec pins, touch pins.
- `qdtech_s3_touch_lcd_3_5.cc`: board class, display init/flush, touch polling, MCP tools, service startup.
- `desktop_ui.cc` / `desktop_ui.h`: LVGL desktop UI.
- `photo_service.cc` / `photo_service.h`: MicroSD JPEG photo slideshow service.
- `radio_service.cc` / `radio_service.h`: MP3 network radio and audio focus behavior.
- `time_weather_service.cc` / `time_weather_service.h`: SNTP clock, weather fetch, cached weather display.
- `firmware_update_service.cc` / `firmware_update_service.h`: Settings-page GitHub release check and on-device OTA trigger.
- `sdkconfig.defaults`: board type defaults for this board.

## Current Verified State

Last verified on 2026-06-15 in the Windows workspace:

- Workspace: `C:\Users\Administrator\qdtech-s3-touch-lcd-3.5-xiaozhi-firmware`
- Branch: `main`
- User remote branch: `qdtech-new/main`
- Last verified update: 2026-06-15 P0-P6 optimization, LVGL touch migration, font fix
- Build directory used for board verification: `build-qdtech`
- Serial port used during the last device flash: `COM13`

Observed boot/runtime facts after flashing:

- Firmware logs `SKU=qdtech-s3-touch-lcd-3.5`.
- Desktop UI is created.
- Touch controller reports `Touch max points: 5`.
- WiFi connected during the last run.
- OTA check completed and reported latest firmware.
- MQTT connected.
- AFE/wake word started.
- Application reached `STATE: idle`.
- XiaoZhi AI chat was tested after the stability repair: the device entered `STATE: listening`, opened the audio channel, reached `STATE: speaking`, and did not reboot during the verified test.
- SNTP time synchronization completed.
- Daily card updated after SNTP sync and rendered date-linked Chinese content with embedded LXGW WenKai subset fonts.
- Weather visual mapping updated after weather fetch; captured log showed cloudy visual mapping for Open-Meteo code `1`.
- Apps tile `FOC / Focus / 25 min` opens the local Focus Timer page instead of starting a XiaoZhi quote chat.
- Focus Timer was reworked into a sparse three-column layout after the first version visually overlapped on the 480x320 display.
- Settings now uses a scrollable layout that fits the 480x320 display.
- Settings brightness and volume sliders read the current hardware values when the page opens and persist changes when the user releases the slider.
- The current manual touch path has an explicit Settings adapter for slider release, vertical scrolling, and Back navigation until standard LVGL input migration.
- Navigation now follows Main -> Apps -> feature pages, with right-swipe back; Photos preserves left-or-right swipe exit.
- Leaving the Radio page does not stop playback. Radio Stop is an explicit user action.
- Calendar tile opens the local Calendar page.
- Calendar Next button changed the displayed month during hardware testing.
- Photos page exists and the photo task is lazy-started only when the Photos page is opened.
- Photos were verified on hardware after the SD scan and display fixes: the slideshow reads the prepared MicroSD photos, colors render normally, images fill the 480x320 screen, no text controls are shown, and left/right swipe exits back to Apps.
- PhotoService now allocates its 6144-byte task stack from PSRAM first and logs internal-memory diagnostics; a boot self-test confirmed SD mount and repeated 480x320 JPEG decode after the black-screen repair.
- Weather API may return 429 or 502; the firmware should keep running and retain cached data when available.

## Current FC/NES Emulator Handoff

Last worked on 2026-06-18 in this Windows workspace:

- Workspace: `D:\3.5inch_ESP32-S3\xiaozhi-esp32`
- Branch used locally: `codex/qdtech-landscape-v176`
- Target user remote: `qdtech-new/main`
- Build directory: `build-qdtech`
- Serial port: `COM13`

Current user-visible FC/NES state:

- Apps contains an `FC / NES / SD ROMs` tile.
- Entering FC now shows a ROM list first; it no longer auto-loads or auto-runs a ROM.
- Prev/Next move the selected ROM in the list.
- Start loads the selected ROM and switches to the game view.
- Game view is top screen plus bottom virtual controller.
- Game view has a `LIST` button to stop and return to the ROM list.
- Right-swipe exit is disabled while in game view to avoid accidental exits during D-pad swipes.
- The SD card is detected. FC scans the first populated ROM directory and now keeps up to 192 `.nes` files in the on-device list.
- The active emulator path is the Nofrendo adapter under `components/nofrendo`. Start now pre-validates the selected ROM and reports bad iNES headers, NES 2.0 headers, ROMs over 2 MB, and mappers not supported by the tracked Nofrendo mapper list.

Important FC/NES files:

- `main/boards/qdtech-s3-touch-lcd-3.5/fc_emulator_service.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/fc_emulator_service.h`
- `main/boards/qdtech-s3-touch-lcd-3.5/nes_bus.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/nes_bus.h`
- `main/boards/qdtech-s3-touch-lcd-3.5/nes_cpu.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/nes_cpu.h`
- `main/boards/qdtech-s3-touch-lcd-3.5/nes_ppu.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/nes_ppu.h`
- `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`

Current FC/NES verification evidence:

- Build passed with `ninja -C build-qdtech all`.
- Flash passed with `idf.py -B build-qdtech -p COM13 -b 921600 flash`.
- Latest app size: `xiaozhi.bin` `0x3dd8b0`; smallest app partition `0x600000`; free `0x222750`.
- Serial confirmed ROM selection and load, for example `NES ROM loaded: 144??~1.NES`.
- Serial confirmed virtual buttons enter the service as `FcEmulator: controller=0x..`.
- Serial confirmed the NES bus can latch controller states, for example `NesBus: controller latch=0x40`, `0x20`, `0x08`, `0x02`, and `0x01`.

2026-06-18 macOS continuation:

- Pulled `https://github.com/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware.git` into `/Users/tupi/Documents/带小智 3.5 寸/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware`.
- Improved the in-repo minimal NES core timing and sprite correctness: CPU base-cycle accounting, `CPU cycles * 3` PPU advancement, NMI-cycle PPU advancement, Mapper 2 bank wrapping, sprite bitplane/X/Y fixes, and background fetch timing.
- Changed FC startup to be fully lazy: normal boot only registers callbacks; the FC task, SD mount, and ROM scan start when the FC page or FC controls request them. Leaving the FC page stops playback and releases the ROM list.
- Full ESP32-S3 build passed from `/private/tmp/qdtech_s3_build_src` because the original macOS workspace path contains spaces/Chinese text that split an ESP-SR linker search path at link time.
- Final build result from the no-space validation path: `xiaozhi.bin` `0x3df3b0`; smallest app partition `0x600000`; free `0x220c50` bytes, about 35%.
- Flashed successfully to `/dev/cu.usbmodem212401` on macOS.
- Boot verification after the lazy-start change reached desktop UI, touch max points `5`, WiFi, OTA latest, MQTT, AFE load, and `Application: STATE: idle`.
- The first pre-lazy-start flash proved FC boot-time SD/ROM scan could create internal SRAM pressure, including `esp-aes: Failed to allocate memory`. The final lazy-start flash removed the FC task/SD scan from boot.
- Residual runtime risk remains: AFE `Ringbuffer of AFE(FEED) is full` warnings still appeared in the captured monitor output, and internal SRAM remains tight. Do not treat this as a finished XiaoZhi/audio stability pass.
- Do not declare FC playable from this build alone; ROM display/gameplay still needs a controlled hardware test with known-good Mapper 0/NROM and Mapper 2 ROMs.
- User then reported FC remained unplayable: very low frame rate and virtual controls felt unresponsive.
- Added an FC game-mode direct touch-poll fallback, shortened controller release hold to `120ms`, reduced FC task priority to `1`, shortened emulator execution slices, and added `fc perf` logging.
- A brief LVGL partial-render experiment corrupted the screen on hardware. It was reverted; keep `LV_DISPLAY_RENDER_MODE_FULL` and `timer_period_ms = 50` unless the rotated flush path is redesigned and tested.
- Responsiveness hotfix build passed and was flashed to `/dev/cu.usbmodem212401`; final app size `0x3df5a0`, free `0x220a60`.
- Added a QDTech direct RGB565 frame path for FC gameplay. It draws the native `256x240` NES frame into the top-center screen area through the existing stable rotated transfer routine, avoiding per-frame LVGL full-screen image refresh.
- FC frame publishing now skips stale frames and only redraws when `NesBus` reports a new complete frame. The emulation slice was raised to `12000` instructions or about `12ms` per loop.
- Hardware logs from the second hotfix build proved the bottom virtual controller reaches both the FC service and the NES bus during gameplay: `controller=0x01/0x02/0x10/0x20/0x40/0x80` and `NesBus: controller latch=0x..` were captured.
- Before the stale-frame skip and larger emulation slice, `fc perf` still showed only about `3-5` produced NES frames per second on the tested ROM. This established that the remaining "no reaction" symptom is mostly low emulator throughput/core behavior, not a missing touch event.
- Latest final build passed and was flashed to `/dev/cu.usbmodem212401`: `xiaozhi.bin` `0x3df920`, smallest app partition `0x600000`, free `0x2206e0`. A fresh in-game `fc perf` sample from this exact final build was not captured because there was no new FC interaction during the final monitor window.
- Later 2026-06-18 macOS continuation replaced the in-repo minimal FC core path with a Nofrendo adapter under `components/nofrendo` and switched board defaults to performance optimization (`CONFIG_COMPILER_OPTIMIZATION_PERF=y`).
- The board's internal SRAM was too tight with the local AFE wake word/noise processor enabled. The QDTech board defaults now intentionally disable local wake word and audio processor (`CONFIG_USE_AFE_WAKE_WORD`, `CONFIG_USE_ESP_WAKE_WORD`, and `CONFIG_USE_AUDIO_PROCESSOR` are unset) while keeping audio capture/playback. A clean build verified `no_wake_word.cc` and `no_audio_processor.cc` are compiled.
- SD startup was retested on `/dev/cu.usbmodem212401`. Logs confirmed early SDMMC mount succeeds after LCD init: `FcEmulator: fc sd card mounted width=4` and `QDtech35: SD card prepared early`.
- The user's "SD card contents disappeared" symptom was reproduced as an FC-page scanning/runtime problem, not a missing SD mount. Serial logs showed `/sdcard/FC` entries being discovered.
- FC ROM scanning was reduced to avoid freezing the device or exhausting internal SRAM: it now scans preferred directories in order (`/sdcard/FC`, `/sdcard/nes`, `/sdcard/roms`, `/sdcard`), stops after the first directory with ROMs, caps the list at 64 `.nes` files, and validates the selected ROM only when Start is pressed.
- The latest build after this scan-limit fix passed and was flashed to `/dev/cu.usbmodem212401`: `xiaozhi.bin` `0x39a630`, smallest app partition `0x600000`, free `0x2659d0`.
- Post-flash boot stayed stable for several minutes, SD mounted successfully, WiFi/weather continued, and no `SR_RINGBUF Memory exhausted` / AFE reboot loop appeared. No final FC-page touch test was captured after this last scan-limit flash because no new touch input occurred during the final monitor window.
- Follow-up crash fix: Nofrendo initially crashed in `vid_findmode()` because the QDTech video adapter returned `NULL` from `lock_write()`. The adapter now creates a small PSRAM-backed hardware bitmap so Nofrendo can read screen dimensions while still using the direct RGB565 custom blit for real frames.
- Follow-up crash fix: Nofrendo then crashed in `ppu_renderbg()` because the primary bitmap was allocated as `256x224` while the PPU renders 240 scanlines. The QDTech port now uses `NES_SCREEN_HEIGHT` (`240`) for both OSD video info and `vid_setmode()`.
- Latest hardware result after both fixes: Start no longer crashes. Logs showed `vid_init done`, `rgb frame buffer ready 256x240 pixels=61440`, and stable `nofrendo fps=35-39`.
- Virtual FC buttons were verified again in the Nofrendo path: logs showed Start/A/B/D-pad values such as `controller=0x08`, `0x01`, `0x02`, `0x80`, `0xa0`, and `0x60`. Leaving the FC page stopped Nofrendo cleanly with `nofrendo stopped result=0`.
- Follow-up playability pass for the user's "bigger screen, no multitouch, key crash" report:
  - QDTech touch now reads all active touch points from the controller instead of only the first point.
  - FC game mode ORs multiple touch points into one controller state, so holding a D-pad direction while pressing A/B can produce combined masks.
  - FC game mode makes LVGL report touch released and uses the direct manual touch path only, preventing gameplay touches from also triggering LVGL gestures/buttons.
  - Normal LVGL pages now read cached touch coordinates from the manual poller instead of reading the I2C touch chip a second time. This removed the observed repeated `i2c.master: I2C software timeout` spam during touch-heavy testing.
  - The D-pad hit map was widened and given a center dead zone; A/B hit zones were separated further to reduce accidental hits.
  - The direct FC frame path now scales standard `256x240` NES frames to `320x240` before drawing, centered at the top of the `480x320` screen.
- Latest flashed build after this pass: `xiaozhi.bin` `0x39abb0`, smallest app partition `0x600000`, free `0x265450`.
- Latest hardware monitor result: boot stable, `Touch max points: 5`, SD mounted, `/sdcard/FC` scanned, Nofrendo entered multiple ROMs, and gameplay held about `29-31` FPS with the larger `320x240` draw. LIST stopped Nofrendo cleanly. No repeated I2C timeout spam appeared in the final monitor window.
- Follow-up smoothness/layout pass after the user asked whether the game screen could be larger without hurting smoothness:
  - Tried `336x240` and `328x240` scaled FC draw widths. Both entered Nofrendo, but hardware logs stayed around `24-26` FPS, so they were rejected as defaults.
  - Final default is back to `320x240`, which remains the best tested balance for readability and smoothness on the current direct-draw path.
  - Bottom controls were rearranged: D-pad visual keys are less crowded, the actual D-pad hit region is a wider left-side joystick-style area with a center dead zone, A/B are farther apart, and LIST/Select/Start are centered more cleanly.
  - Controller serial logging was removed from the hot path to avoid losing frame time while rapidly pressing A/B/D-pad.
  - Nofrendo quit handling was changed so LIST/Stop first asks the NES loop to power off and only releases Nofrendo memory after the loop exits. This fixed the heap crash seen when pressing LIST during gameplay.
  - Touch I2C failures now use a conservative backoff and low-rate `i2c_master_bus_reset()` instead of continuous reads. Rapid FC list tapping no longer produced the previous continuous I2C timeout flood in the captured final boot/list test.
  - Latest final build passed and was flashed to `/dev/cu.usbmodem212401`: `xiaozhi.bin` `0x39ae30`, smallest app partition `0x600000`, free `0x2651d0`.
  - Latest final boot monitor confirmed `Touch max points: 5`, early SD mount, WiFi, MQTT, and idle state. No final in-game FPS sample was captured after returning from `328x240` to `320x240`, but `320x240` had already been measured at about `29-31` FPS earlier in this same pass.
- Follow-up ROM directory/name pass after the user reported garbled, numeric, and hard-to-identify game names:
  - QDTech board defaults now select FATFS CP936 with UTF-8 API encoding instead of CP437/ANSI-OEM, so Chinese long filenames should reach LVGL as UTF-8.
  - FC list rows now display a stable 3-digit index plus a cleaned title: no directory path, no `.nes` suffix, trimmed whitespace, and UTF-8-safe truncation.
  - Numeric ROM sets can place `roms.txt`, `roms.csv`, or `games.txt` in the active ROM directory. Lines may be `001.nes=Super Mario`, `001.nes,Super Mario`, or tab-separated. Aliases load once during scan and are not used in the gameplay frame loop.
  - Validation regenerated `build-qdtech/sdkconfig` in `/private/tmp/qdtech_s3_build_src`; generated config confirmed `CONFIG_FATFS_CODEPAGE_936=y`, `CONFIG_FATFS_CODEPAGE=936`, and `CONFIG_FATFS_API_ENCODING_UTF_8=y`.
  - Latest build was flashed to `/dev/cu.usbmodem212401`: `xiaozhi.bin` `0x3c89f0`, smallest app partition `0x600000`, free `0x237610`.
  - Hardware monitor confirmed `/sdcard/FC` scan and readable Chinese ROM names in logs, for example `10.能源战士2(无限HP+无限生命)` and `100.恶魔城1无敌版`.
  - Entering Nofrendo after the ROM-name pass stayed stable and logged about `27-32` FPS, so this directory/name fix did not regress the smoothness-first `320x240` path.
  - After the user reported that later ROM names still looked garbled on the panel, the FC list/detail labels were switched from Montserrat to the existing `font_puhui_16_4` Chinese font. This addresses the separate display-font layer: decoded UTF-8 names need a font with Chinese glyph coverage.
  - The font-only pass built and flashed successfully to `/dev/cu.usbmodem212401` with the same `xiaozhi.bin` size `0x3c89f0`; boot was verified. The monitor session did not capture a fresh FC page entry after this last flash, so the user still needs to visually confirm the FC list on the panel.
  - User confirmed the FC ROM list became readable after the font pass.
  - Release prep bumped `PROJECT_VER` to `1.7.9` and changed `.gitignore` so `components/nofrendo` is tracked while other generated `components/` content remains ignored. Do not drop the Nofrendo component from future commits; `FcEmulatorService` includes `qdtech_nofrendo.h`.
  - Final `v1.7.9` release build passed from `/private/tmp/qdtech_s3_build_src`: `xiaozhi.bin` `0x3c89f0`, smallest app partition `0x600000`, free `0x237610`.
  - Final `v1.7.9` build was flashed to `/dev/cu.usbmodem212401`; monitor logs confirmed `App version: 1.7.9`, OTA current version `1.7.9`, early SD mount, FC page activation, `/sdcard/FC` scan, and readable Chinese list navigation through names such as `10.能源战士2(无限HP+无限生命)`, `100.恶魔城1无敌版`, and `115.未来战士HACK`.
  - Follow-up system stability pass after the user reported slow time/weather sync and occasional crashes after flashing FC:
    - Removed boot-time FC SD prepare; SD now mounts lazily when FC/Photos needs it.
    - FC now reuses an existing `/sdcard` mount instead of trying a second mount.
    - Time/Weather task now allocates its 6144-byte stack from PSRAM first.
    - SNTP starts immediately after WiFi connects instead of waiting for application/MQTT readiness.
    - Weather fetch was shortened to one 5-second initial attempt; failures keep cached UI state and retry later.
  - Latest stability build passed and was flashed to `/dev/cu.usbmodem212401`: `xiaozhi.bin` `0x3c8a70`, smallest app partition `0x600000`, free `0x237590`.
  - Latest stability monitor result: boot stable, WiFi initialized normally, SNTP synchronized immediately after IP acquisition, `time weather task started stack=6144 memory=psram`, free internal SRAM around `20KB` with minimum around `17KB`, FC page entered, FC task started in PSRAM, SD mounted on demand, and `/sdcard/FC` scanned 64 capped ROMs.
  - Rejected stability experiment: enabling `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y` caused WiFi boot failure (`wifi:malloc buffer fail`, `Expected to init 6 rx buffer, actual is 2`, `ESP_ERR_NO_MEM` from `esp_wifi_init`). It was reverted. Do not re-enable it casually; it made the board reboot-loop.

Unresolved FC/NES problem:

- User still needs to test actual play feel on hardware after the multi-touch/cache/scale pass.
- User also needs to verify real ROM list rendering on the panel after the FATFS/alias/font build. If any names are still unreadable, check whether the serial log shows correct Chinese; if serial is correct but the panel is wrong, extend the UI font coverage. If serial is wrong too, use `roms.txt`/`roms.csv` aliases or inspect mixed-encoding filenames on the SD card.
- The `320x240` draw is currently the largest smoothness-safe default tested. `328x240` and `336x240` were visibly more ambitious but measured only `24-26` FPS on hardware.
- If the user still wants a bigger view without losing FPS, do not just increase `target_width` again. The next real path is a deeper display/game mode optimization: reduce competing LVGL/network redraws during gameplay, make the direct display path more asynchronous, or expose a user-selectable display-size/performance mode.
- The Nofrendo adapter is now the active path. The older in-repo minimal CPU/PPU core remains in the tree but should no longer be treated as the primary path unless Nofrendo is deliberately removed.

Recommended next FC/NES step:

- Have the user test combinations explicitly: hold Left/Right/Up/Down and tap A/B, then try Start/Select and LIST.
- If multi-touch still fails on the physical panel, capture raw point counts in `QdtechTouch::ReadPoints()` for one monitor run to distinguish firmware mapping from panel/driver behavior.
- If FPS/latency is still unacceptable, first add a fast display option such as native `256x240` or intermediate `300x240`, then evaluate Bluetooth gamepad support.

Important 2026-06-05 stability finding:

- A previous radio smoothing change (`834d55a`) was reverted by `76c1541` because the firmware became unstable.
- The XiaoZhi crash when entering chat was traced to internal SRAM pressure during audio-channel setup: `pthread: Failed to create task!` in `EspUdp::Connect()`.
- Commit `5b58b22` keeps photo/radio tasks smaller or lazy, catches UDP thread creation failure instead of aborting, shortens MCP tool descriptions, reduces MCP `tools/list` payload size, and treats MCP publish failures as warnings instead of user-facing network alerts.
- Do not reintroduce large always-running tasks, large MCP descriptions, or broad radio buffering changes without first proving XiaoZhi listen/speak on hardware.

## What Is Already Implemented

- XiaoZhi voice core from the upstream project.
- QDTech 3.5 inch landscape display adaptation.
- CST9217/TDDI touch polling with tap/swipe dispatch.
- LVGL touch input device for button clicks and slider interactions.
- Hybrid touch mode: LVGL for buttons/sliders, manual polling for gestures.
- LVGL desktop UI with main/app/XiaoZhi/radio/settings pages.
- Radio page with audio spectrum visualization (16 animated bars).
- Local Calendar page with month grid, Today, Prev, and Next controls.
- Local Focus Timer page with 25 minute focus mode, 5 minute break mode, start/pause, reset, and NVS-persisted completion count.
- Local Photos page replacing the previous Weather app tile, reading JPEG photos from common SD directories and playing them as a pure full-screen slideshow.
- Time and weather service.
- Main-page daily card with date-linked festival (19), history-on-this-day (31), and daily quote (32) content.
- Embedded LXGW WenKai LVGL subset fonts (499 Chinese characters).
- MP3 network radio service with 37 Chinese stations.
- Radio station categories: National (CNR), Beijing, Shanghai, Guangdong, Zhejiang, Jiangsu, Sichuan, Hunan, Hubei, Shandong, Music, Traffic, Other.
- Radio station favorites with NVS storage persistence.
- Radio station index persistence (last played station remembered).
- MCP tools for weather location and radio controls.
- Lightweight audio focus behavior so XiaoZhi states can pause the radio.
- Radio reconnect/fallback handling and remembered last successful station URL.
- Radio error state display after 5 consecutive failures.
- Main page visual enhancements:
  - Daily card breathing animation (opacity 0.92-1.0) for subtle visual depth.
  - Clock digit shadow pulse effect (opacity 10-40) for time display ceremony.
  - Weather particle animation (golden particles floating up from sun) for clear/cloudy weather.
- Optimized time/weather sync speed:
  - SNTP wait time reduced from 30s to 15s (500ms polling).
  - Weather API timeout reduced from 20s to 10s.
  - Weather retry attempts reduced from 3 to 2.

## Latest Runtime Notes: 2026-06-19 v1.7.14 FC ROM Scan Cap And Load Diagnostics

- Latest release target is `v1.7.14`.
- FC ROM scanning now keeps up to 192 `.nes` files from the first populated ROM directory instead of stopping at 64. The old `fc scan found 64 nes files` result was the firmware's protective cap, not missing SD contents.
- FC still scans conservatively in this order: `/sdcard/FC`, `/sdcard/nes`, `/sdcard/roms`, `/sdcard`.
- Start now validates the selected ROM before entering Nofrendo and reports clearer failure reasons:
  - invalid or missing iNES header
  - NES 2.0 header
  - ROM file larger than the current 2 MB Nofrendo PSRAM load guard
  - mapper not present in `components/nofrendo/nes/mmclist.c`
- The active emulator path remains the Nofrendo adapter under `components/nofrendo`; do not use the older in-repo minimal CPU/PPU core as the current compatibility reference.
- Final `v1.7.14` release build passed from the Windows checkout: `xiaozhi.bin` `0x3cc8e0`, smallest app partition `0x600000`, free `0x233720`.
- Final `v1.7.14` build was flashed to `COM13`. Boot logs confirmed `App version: 1.7.14`, `Ota: Current version: 1.7.14`, WiFi, MQTT, time sync, weather update, and `Application: STATE: idle`.
- Release assets:
  - `qdtech-s3-touch-lcd-3.5-v1.7.14-app.bin`, SHA256 `d08ce99d118d456439f43bad6acec751d36a6ae2b7967c38d040e289edea4b17`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.14-firmware.zip`, SHA256 `b99afb216cc6e1dcf8ef294a39b80a667b367b339989f9b793a2a2448ce02a8e`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.14-full.bin`, SHA256 `951f7cec9904d2b25781ef57774c28c4867d0c9b94fb0cc91c0bf6d03ea8b96b`.
- FC page scan verification still needs a tap into the FC page with the user's ROM SD card inserted.
- After the GitHub Release exists, the board-initiated OTA path should be verified from a device running `1.7.13` or older: Settings -> Firmware -> Check -> Update -> reboot -> `1.7.14`.

## Previous Runtime Notes: 2026-06-18 v1.7.13 On-Device Firmware Update Bootstrap

- Latest release target is `v1.7.13`.
- `SET / Settings / Firmware` now has a real `Check` / `Update` button, not just reserved status text.
- New QDTech-only files:
  - `main/boards/qdtech-s3-touch-lcd-3.5/firmware_update_service.h`
  - `main/boards/qdtech-s3-touch-lcd-3.5/firmware_update_service.cc`
- The firmware update service checks the latest GitHub Release:
  - `https://api.github.com/repos/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware/releases/latest`
  - It compares the release tag against `esp_app_get_description()->version`.
  - It looks for `qdtech-s3-touch-lcd-3.5-vX.Y.Z-app.bin` and uses that URL for OTA.
- `Ota` now exposes `StartUpgradeFromUrl()` and the app-image download path handles GitHub redirects directly through ESP-IDF HTTP.
- The update flow blocks unsafe starts:
  - no WiFi -> `WiFi needed`
  - radio/external audio active -> `Stop audio first`
  - XiaoZhi not idle -> `Wait idle`
- Release packaging now needs three assets:
  - `qdtech-s3-touch-lcd-3.5-vX.Y.Z-full.bin` for full USB flashing.
  - `qdtech-s3-touch-lcd-3.5-vX.Y.Z-firmware.zip` for manual download/archival.
  - `qdtech-s3-touch-lcd-3.5-vX.Y.Z-app.bin` for on-device OTA.
- Final `v1.7.13` release build passed from `/private/tmp/qdtech_s3_build_src`: `xiaozhi.bin` `0x3cd140`, smallest app partition `0x600000`, free `0x232ec0`.
- Hardware monitor after flashing:
  - App version and OTA current version both reported `1.7.13`.
  - ESP-IDF runtime reported `v5.5.2`.
  - WiFi initialized normally and obtained IP `192.168.1.104`.
  - Time synchronized successfully: `2026-06-18 17:49`.
  - Weather completed during boot: `weather ok 27 C Zhongshan Storm 17:49 code=96 updated=17:49`.
  - MQTT connected and the app reached `Application: STATE: idle`.
  - Internal SRAM remained stable in the observed idle window, with `free sram` around `13KB` and minimum around `8KB`.
- Release assets:
  - `qdtech-s3-touch-lcd-3.5-v1.7.13-app.bin`, SHA256 `f0ad472babf25b0c5fcde313bc53c302396f22a13b2761c0d472c3d77819593d`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.13-firmware.zip`, SHA256 `a49a4290c8718a66fd6f270f701fdf7a53ed053db2cc0ffa074ead5d8785a94a`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.13-full.bin`, SHA256 `64d914e33bbd3d41c123b9726e0129dfa68f8bbf099ed395b0f19270b411be08`.
- Important limitation: `v1.7.13` is the bootstrap firmware that adds the on-device updater. A full board-initiated download/write/reboot should be verified with the next higher GitHub Release. A board already running `1.7.13` should report `Latest` when the latest release is also `v1.7.13`.

## Previous Runtime Notes: 2026-06-18 v1.7.12 Network/System Settings Split

- Latest release target is `v1.7.12`.
- `NET` and `SET` are no longer duplicate entry points:
  - `NET / Network / WiFi Hub` opens a dedicated Network page.
  - `SET / Settings / System` opens the System Settings page.
- The Network page contains:
  - Connection status, mirrored from `DesktopUI::SetNetworkStatus()`.
  - Saved WiFi count.
  - A scrollable saved WiFi list from `SsidManager::GetSsidList()`.
  - Tap-to-set-default behavior through `SsidManager::SetDefaultSsid(index)`.
- Destructive WiFi actions are intentionally not exposed as touch buttons yet. Use the existing MCP tools for remove/switch until the on-device confirmation flow is designed.
- The Settings page now focuses on Display & Sound, Weather, and Firmware:
  - Brightness and volume sliders retain the existing hardware-backed behavior.
  - Weather location remains a status row tied to the existing XiaoZhi/MCP configuration path.
  - Firmware row reads the running app version through `esp_app_get_description()`.
  - OTA status text is a reserved anchor for the next real on-device firmware-update workflow.
- Radio page spectrum bars changed from one yellow color to `RADIO_BAR_COLORS[16]`, a soft rainbow palette. Bars are dim at idle and full opacity during playback.
- Final `v1.7.12` release build passed from `/private/tmp/qdtech_s3_build_src`: `xiaozhi.bin` `0x3ca6a0`, smallest app partition `0x600000`, free `0x235960`.
- Hardware monitor after flashing:
  - App version and OTA current version both reported `1.7.12`.
  - WiFi initialized normally and obtained IP `192.168.1.104`.
  - Time synchronized successfully: `2026-06-18 17:16`.
  - Weather completed during boot: `weather ok 28 C Zhongshan Storm 17:16 code=96 updated=17:16`.
  - MQTT connected and the app reached `Application: STATE: idle`.
  - Internal SRAM remained stable in the observed window, with `free sram` around `10-21KB` and minimum around `9KB`.
- Release assets:
  - `qdtech-s3-touch-lcd-3.5-v1.7.12-full.bin`, SHA256 `6ce97bb6b910420e0fd52c5b6185c8c5512e8c52f91cf7050dfbecb9276dbb65`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.12-firmware.zip`, SHA256 `b50b5510007093a9c90ba0c2accf4abf3580ed92d35d43d323e08f967102adcb`.

## Previous Runtime Notes: 2026-06-18 v1.7.11 Clock Visual Polish

- Latest release target is `v1.7.11`.
- Main page layout is intentionally unchanged. Only the large clock rendering was adjusted.
- Replaced the old four-card `lv_font_montserrat_48` digit layout with two complete labels using a small QDTech-only `qd_font_clock_72` generated from Montserrat Bold and limited to `0123456789:`.
- The clock keeps the warmer visual direction: hour digits use `COLOR_CREAM`, minute digits use `COLOR_GOLD`, and the colon is rendered as two soft gray round dots.
- The block/segment clock prototype was rejected on hardware because it looked blurry, visually broken, and too cold in color. Do not revive that path unless the design direction changes.
- The accepted clock labels are vertically lowered and use a shorter label box so the large digits visually align with the two colon dots and do not crowd the date row.
- `FlipDigit()` and `ClockShadowCb()` are now retained as no-op compatibility methods because other animation helpers still use the shared opacity/Y callbacks; the current clock path updates hour/minute labels directly in `RenderBigTime()`.
- Final `v1.7.11` release build passed from `/private/tmp/qdtech_s3_build_src`: `xiaozhi.bin` `0x3c9d80`, smallest app partition `0x600000`, free `0x236280`.
- Hardware monitor after flashing:
  - App version and OTA current version both reported `1.7.11`.
  - WiFi initialized normally and obtained IP `192.168.1.104`.
  - Time synchronized successfully: `2026-06-18 16:43`.
  - Weather completed during boot: `weather ok 28 C Zhongshan Storm 16:43 code=95 updated=16:43`.
  - Internal SRAM was stable in the observed window, around `25KB` free with minimum around `20KB`.
- Release assets:
  - `qdtech-s3-touch-lcd-3.5-v1.7.11-full.bin`, SHA256 `971fb48e6516bb5263db061e67e39512f245e853de22d72f049182bc5d10b710`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.11-firmware.zip`, SHA256 `49bd8e1b2c62cdb5daa8da7f8b85940b2d1d12747333514c4aca72488ab26622`.

## Previous Runtime Notes: 2026-06-18 v1.7.10 Time/Weather Recovery

- Latest release target is `v1.7.10`.
- FC and Photos no longer force SD-card mounting during board construction. SD is mounted lazily when a feature needs it, and FC reuses an existing `/sdcard` mount.
- The Time/Weather task is created in PSRAM first, with an internal-memory fallback and diagnostic logging.
- Exiting FC now notifies the Time/Weather service to refresh the clock immediately and retry weather if the last weather fetch failed.
- Returning from any secondary page to the main page now notifies the Time/Weather service to refresh the clock immediately.
- The Time/Weather loop refreshes the clock every 5 seconds, but `DesktopUI::SetTime()` skips redraws when the displayed minute and date are unchanged.
- Time is accepted for display only after SNTP reports a real sync at least once, so stale valid-looking local time is less likely to be shown as corrected time.
- While the main page is visible, `DesktopUI::SetTime()` rewrites the big clock digits every 5 seconds even when the minute is unchanged. This is deliberate to self-repair stale LVGL label state.
- OTA server-time parsing no longer adds `timezone_offset` before `settimeofday()`. That offset caused a later +8 hour jump after OTA/MQTT startup; the system clock should stay UTC and `TZ=CST-8` handles display time.
- Weather uses Open-Meteo over HTTP instead of HTTPS, with a 3-second timeout and one quick retry, to avoid TLS contention with OTA/MQTT during boot.
- Desktop page switches invalidate the active screen, and main-page time/weather label updates invalidate the main page. This is intended to clear stale FC direct-draw pixels after leaving the emulator.
- Weather retry timing is now real 10-second retry behavior after a failed fetch. Previously the retry was delayed by the 60-second outer wait loop.
- Latest flashed local build: `xiaozhi.bin` `0x3c8f30`, smallest app partition `0x600000`, free `0x2370d0`.
- Hardware monitor after flashing:
  - WiFi initialized normally with static RX `6`, dynamic RX `8`, RX BA `6`.
  - Time stayed on the correct local hour after OTA/MQTT startup; the prior +8 hour jump did not recur.
  - Weather completed quickly over HTTP during boot: `weather ok 28 C Zhongshan Storm 15:51 code=95 updated=15:51`.
  - Main clock advanced normally from `15:51` to `15:52` in the monitor session.
  - Internal SRAM stayed around `13-14KB` free after startup, minimum around `10051` bytes during the observed window.
- Release assets should follow the existing two-asset pattern: `qdtech-s3-touch-lcd-3.5-v1.7.10-full.bin` and `qdtech-s3-touch-lcd-3.5-v1.7.10-firmware.zip`.

## What Still Needs Work

Highest priority areas are listed in `docs/NEXT_TASKS.md`.

Short version:

- Keep proving XiaoZhi voice, radio, touch, and display together on hardware after every meaningful firmware change.
- Improve display flush performance without rewriting the display path all at once.
- Move radio station configuration toward SPIFFS/SD/NVS while keeping built-in fallback stations.
- Expand settings UI only after the stability base is proven.

## Touch Architecture Note

The current touch system uses a hybrid approach:

- **LVGL input device** (`lv_indev_t`): Handles button clicks and slider interactions automatically.
- **Manual touch polling** (20ms timer): Handles gesture detection (swipe navigation) because LVGL 9.x does not expose gesture sensitivity tuning APIs.

This hybrid approach was adopted after discovering that:
1. LVGL gesture detection requires 50px minimum swipe distance (too insensitive).
2. LVGL 9.x does not provide `lv_indev_set_gesture_limit()` or similar APIs.
3. Direct struct member access is blocked by opaque type definitions.

Do not remove the manual touch polling without providing an alternative gesture detection mechanism.

## Do Not Randomly Change These Areas

- Do not delete or bypass the official XiaoZhi `Application` voice flow.
- Do not broadly refactor official upstream core code just to support QDTech UI.
- Do not commit generated `sdkconfig`, build output, WiFi credentials, activation data, or NVS dumps.
- Do not assume another 3.5 inch ESP32-S3 board uses the same pins, touch chip, or display controller.
- Do not declare radio or touch fixed from a compile result alone. Hardware logs matter.
- Do not let optional features such as Photos, Radio, or MCP tools consume enough internal SRAM to break XiaoZhi audio-channel creation.
- Do not create and hide duplicate LVGL layouts as a migration shortcut; remove obsolete objects after the replacement layout is proven.
- Do not remove the tracked `managed_components/78__esp-ml307/esp_udp.cc` hotfix unless the equivalent behavior is moved into a maintained component patch or upstream update.
- Do not add new Chinese daily-card strings without regenerating both tracked LXGW WenKai subset fonts.
- Do not add new Chinese Focus Timer strings without regenerating both tracked LXGW WenKai subset fonts.
- Do not let the Focus Timer page become visually dense again; verify spacing on the 480x320 hardware screen.
- Do not read `Board::GetInstance()` while `DesktopUI::Create()` is running inside the board constructor. Defer hardware-backed control synchronization until the page is opened.
- Do not assume LVGL widget callbacks receive touch input on this board; current touch is manually polled and dispatched.
- Do not commit `.codex_tmp/` or the full LXGW WenKai TTF used only for local font generation.

## How To Continue Development

1. Read this handoff and the other files in this handoff set.
2. Confirm you are in the intended checkout and branch.
3. Run `git status --short --branch` and protect any user changes.
4. Use the QDTech build command from `docs/PROJECT_STATUS.md`.
5. Make small scoped changes.
6. Build before finishing source changes.
7. Flash and capture boot logs when the task affects runtime behavior.
8. Commit with a message that states the actual firmware behavior changed.
9. Push to `qdtech-new/main` only when the user asked to update GitHub.
