# QDTech Firmware Changelog

This changelog tracks QDTech-specific firmware maintenance. It is not a replacement for `git log`; it records the practical handoff facts that future maintainers need.

## 2026-06-18: v1.7.10 Time/Weather Recovery Release

Scope:

- Bumped firmware version to `1.7.10` for the time/weather recovery release.
- Added an FC-exit callback from the desktop UI to the time/weather service. Leaving FC now immediately requests a time refresh and a weather retry if the last weather fetch failed.
- Added a general main-page callback. Returning to the main page from any secondary page now requests an immediate clock refresh, not only the FC path.
- Increased background clock refresh cadence to 5 seconds while avoiding unnecessary redraws when the displayed minute has not changed.
- Time is now treated as trustworthy only after SNTP reports a real sync at least once, avoiding stale but valid-looking local time being displayed as corrected time.
- While the main page is visible, the big clock digits are rewritten every 5 seconds even if the minute is unchanged, so stale LVGL label state can self-repair quickly.
- Fixed OTA server-time handling so `timezone_offset` is not added before `settimeofday()`. System time remains UTC and the display layer applies `TZ=CST-8`, avoiding the later +8 hour jump after OTA/MQTT startup.
- Switched Open-Meteo weather fetch from HTTPS to HTTP with a shorter timeout and a quick second attempt. Weather data is public, and avoiding TLS reduces startup contention with OTA/MQTT TLS handshakes.
- Forced a full active-screen invalidation when switching pages, plus targeted main-page invalidation after time or weather labels change. This is intended to clear stale direct-drawn FC pixels and make the main page repaint immediately after exiting the emulator.
- Fixed weather retry cadence. The service previously checked the 10-second retry counter only after a 60-second wait loop; failed weather fetches now break out and retry on the intended 10-second cadence.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x3c8f30`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x2370d0`, about 37%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Boot logs confirmed WiFi initialized normally with static RX buffers `6`, dynamic RX buffers `8`, and RX BA window `6`.
- Time/Weather task still started with PSRAM stack allocation.
- Time display stayed on the correct local time after OTA/MQTT startup. The earlier +8 hour jump did not recur after removing `timezone_offset` from OTA `settimeofday()`.
- Weather completed quickly over HTTP during the boot window: `weather ok 28 C Zhongshan Storm 15:51 code=95 updated=15:51`, roughly 15 seconds after boot and before MQTT fully settled.
- Clock logs advanced normally from `15:51` to `15:52`, confirming the main-page clock continued refreshing after the initial sync.
- Internal SRAM stayed stable during the observed idle window after weather success, around `13-14KB` free with minimum around `10051` bytes.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.10-full.bin` and `qdtech-s3-touch-lcd-3.5-v1.7.10-firmware.zip`.

## 2026-06-18: System Stability Pass After FC Release

Scope:

- Removed the QDTech boot-time FC SD-card prepare call. SD is now mounted lazily when Photos or FC actually needs it, avoiding boot-time internal SRAM pressure.
- Added FC SD mount reuse: if Photos or another service already mounted `/sdcard`, FC now reuses that mount instead of trying a second mount.
- Moved the Time/Weather task stack to PSRAM first, with an internal-memory fallback and diagnostic logging.
- Started SNTP immediately after WiFi connects instead of waiting for the full XiaoZhi application/MQTT readiness path.
- Shortened the first weather HTTP attempt from two 10-second attempts to one 5-second attempt; failed weather fetches keep cached UI state and retry later instead of tying up the task.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x3c8a70`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x237590`, about 37%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Boot logs confirmed no early FC SD mount during board construction; FC only mounted SD after entering the FC page.
- WiFi initialized normally with the original low-memory WiFi settings: static RX buffers `6`, dynamic RX buffers `8`, RX BA window `6`.
- Time/Weather logs confirmed PSRAM task stack allocation: `time weather task started stack=6144 memory=psram`.
- SNTP started immediately after WiFi IP acquisition and synchronized time in the same boot window.
- Internal SRAM improved substantially: after WiFi/time startup logs showed about `20KB` free internal SRAM with minimum around `17KB`, compared with earlier post-MQTT/FC pressure around `5-7KB`.
- FC page activation remained stable after the change. Logs showed FC task created in PSRAM, SD mounted on demand, `/sdcard/FC` scanned, and `fc scan found 64 nes files`.

Rejected experiment:

- `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y` was tested as a deeper network-memory optimization, but it caused WiFi initialization failure on this board: `wifi:malloc buffer fail`, `Expected to init 6 rx buffer, actual is 2`, then `ESP_ERR_NO_MEM` in `esp_wifi_init()`. This option was reverted and should not be reintroduced without a separate WiFi-driver memory plan.

## 2026-06-18: v1.7.9 FC Playability, ROM Names, and Firmware Release

Scope:

- Switched QDTech FATFS defaults from CP437/ANSI-OEM to CP936 with UTF-8 API encoding so Chinese long filenames on the MicroSD card are returned to the UI as UTF-8 instead of 8.3 aliases or garbled text.
- Changed the FC ROM list display to show a stable 3-digit row number plus a cleaned game title: directory path is removed, `.nes` is hidden, spaces are trimmed, and UTF-8 names are truncated without splitting Chinese characters.
- Added optional ROM alias files for numeric or messy ROM sets. The active ROM directory can contain `roms.txt`, `roms.csv`, or `games.txt`; each line may use `filename.nes=Display Name`, `filename.nes,Display Name`, or `filename.nes<Tab>Display Name`.
- ROM aliases are loaded once during the FC list scan, then games are sorted by the displayed name. This keeps gameplay rendering unchanged and does not add work to the frame loop.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src` after regenerating the temporary `build-qdtech/sdkconfig`.
- Generated config confirmed `CONFIG_FATFS_CODEPAGE_936=y`, `CONFIG_FATFS_CODEPAGE=936`, and `CONFIG_FATFS_API_ENCODING_UTF_8=y`.
- `xiaozhi.bin` size: `0x3c89f0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x237610`, about 37%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Hardware monitor confirmed stable boot, early SD mount, `/sdcard/FC` scan, and Chinese ROM names in FC logs such as `10.能源战士2(无限HP+无限生命)` and `100.恶魔城1无敌版`.
- Entered Nofrendo after the directory fix; gameplay remained around `27-32` FPS during the monitor window, so the ROM-name fix did not break the smoothness-first display path.
- Follow-up display fix: the FC list/detail labels were switched from Montserrat to the existing `font_puhui_16_4` Chinese font. The previous firmware could decode Chinese filenames correctly but still show missing glyphs on screen because Montserrat has no Chinese coverage.
- Follow-up font build completed and was flashed to `/dev/cu.usbmodem212401`; binary size remained `0x3c89f0`, with `0x237610` bytes free in the smallest app partition. Boot was verified after flashing; final FC-page visual inspection is left to the user because the monitor session did not capture a new FC page entry after this font-only pass.
- User visually confirmed the FC ROM list is readable after the font pass.
- Release prep bumped `PROJECT_VER` to `1.7.9` and allowed `components/nofrendo` to be tracked even though other generated `components/` content remains ignored.
- Final `v1.7.9` release build completed from `/private/tmp/qdtech_s3_build_src`: `xiaozhi.bin` `0x3c89f0`, smallest app partition `0x600000`, free `0x237610`.
- Final `v1.7.9` build was flashed to `/dev/cu.usbmodem212401`. Boot logs confirmed `App version: 1.7.9`, OTA current version `1.7.9`, early SD mount, `/sdcard/FC` scan, and readable Chinese FC list navigation through entries such as `10.能源战士2(无限HP+无限生命)`, `100.恶魔城1无敌版`, and `115.未来战士HACK`.

## 2026-06-18: FC Smoothness-First Layout and Stop Stability Pass

Scope:

- Reworked the FC bottom controls so the D-pad uses a wider left-side region with a larger center dead zone, A/B are farther apart, and LIST/Select/Start are more regular in the middle.
- Kept multi-touch controller combining for gameplay; direction plus A/B combinations are still supported through the manual touch path.
- Removed per-change controller serial logging from the hot path. Heavy A/B/D-pad testing previously flooded serial output and cost frame time.
- Changed Nofrendo quit handling so LIST/Stop requests power off the NES loop first and release Nofrendo state after the loop exits, avoiding the heap crash seen when stopping during active input polling.
- Added conservative touch I2C failure handling: after read failures the poller backs off, and repeated failures reset the I2C bus at a low rate. This avoids continuous timeout spam and CPU loss during touch-heavy FC list navigation.
- Tested enlarged draw widths `336x240` and `328x240`; both were rejected as defaults because they stayed around `24-26` FPS on hardware. The final default is back to `320x240` to prioritize smoothness.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x39ae30`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x2651d0`, about 40%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Boot logs confirmed `Touch max points: 5`, early SD mount, WiFi, MQTT, and normal idle state.
- `336x240` and `328x240` hardware tests entered Nofrendo but held only about `24-26` FPS, with occasional display-lock pressure. They are not smoothness-safe defaults.
- LIST/Stop was retested after the Nofrendo quit change: Nofrendo logged `nofrendo stopped result=0` and `nofrendo finished result=0` instead of crashing.
- Rapid FC list tapping after the I2C backoff/bus-reset change no longer produced the prior continuous I2C timeout flood during the captured boot/list test.

Current FC status:

- The practical smoothness ceiling for the current direct-draw path is `320x240`. Larger views need a deeper display strategy, such as a dedicated game mode that pauses competing LVGL/network redraws, a faster/asynchronous display path, or a controller/display-size setting exposed to the user.
- The latest flashed firmware is the smoothness-first `320x240` build with the improved layout and stop/touch stability fixes.

## 2026-06-18: FC Multi-Touch Controls and Larger Game View

Scope:

- Added multi-touch reading for the QDTech touch controller and changed FC game mode to combine all active touch points into one NES controller mask.
- Suppressed LVGL touch delivery while FC gameplay is active, so D-pad/A/B touches no longer also trigger page gestures or LVGL button events.
- Changed LVGL touch input to read cached coordinates from the manual touch poller instead of reading the I2C touch chip independently, removing repeated I2C timeout spam seen during touch-heavy testing.
- Widened and separated the FC virtual controller hit zones: D-pad uses a larger directional region with a center dead zone, and A/B are spaced farther apart.
- Enlarged standard NES frames from `256x240` to `320x240` before direct drawing, centered in the top `480x240` game area.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x39abb0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x265450`, about 40%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Boot logs confirmed `Touch max points: 5`, early SD mount, and `/sdcard/FC` ROM scanning.
- Nofrendo entered gameplay on multiple ROMs without reboot; logs showed `rgb frame buffer ready 256x240 pixels=61440`.
- Larger `320x240` direct draw held about `29-31` FPS in the final monitor window.
- LIST stopped Nofrendo cleanly. The final monitor window did not show the repeated touch I2C timeout spam seen before the cached-touch change.

Remaining FC status:

- User should test real play feel, especially hold-direction plus A/B combinations.
- If the larger view feels too slow, add a speed/display-size option or reduce the scaled draw width before deeper emulator changes.

## 2026-06-18: FC SD Scan Limit and AFE Memory Relief

Scope:

- Disabled QDTech local AFE wake word/audio processor defaults to preserve scarce internal SRAM for FC runtime while keeping audio capture/playback available.
- Kept SDMMC mount after LCD initialization and confirmed early SD mount succeeds on hardware.
- Reduced FC ROM scanning work: prefer `/sdcard/FC`, stop after the first populated ROM directory, cap the ROM list at 64 files, and defer ROM header validation until Start.
- This addresses the observed "SD contents missing" symptom, which was caused by FC scanning/runtime pressure rather than an actually missing SD mount.
- Fixed two Nofrendo startup crashes:
  - QDTech video adapter now returns a PSRAM-backed hardware bitmap from `lock_write()` so `vid_findmode()` has valid screen dimensions.
  - Nofrendo now allocates a `256x240` primary buffer instead of `256x224`, matching the PPU's 240 rendered scanlines.

Verification:

- Clean build completed from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size after the final crash fixes: `0x39a780`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x265880`, about 40%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Boot logs confirmed `fc sd card mounted width=4` and `SD card prepared early`.
- Post-flash monitor stayed stable for several minutes without the previous AFE ringbuffer memory-exhausted reboot loop.
- FC Start entered Nofrendo without reboot. Logs showed `rgb frame buffer ready 256x240 pixels=61440`.
- Runtime held about `35-39` Nofrendo FPS on tested ROMs.
- Virtual controls were visible in logs during gameplay: Start/A/B/D-pad events reached `FcEmulator: controller=0x..`.

Remaining FC status:

- FC no longer instantly crashes on Start. Continue testing real gameplay compatibility across ROMs and mappers.
- One occasional display-lock warning appeared during heavy gameplay tapping, but it did not reboot the board.

## 2026-06-18: FC Direct Frame Path and Controller Verification

Scope:

- Added a QDTech display fast path that draws RGB565 landscape rectangles directly through the existing stable rotated transfer code.
- Wired FC gameplay frames to direct-draw the native `256x240` NES image at the top center of the `480x320` screen, instead of asking LVGL to refresh the whole screen for every game frame.
- Kept `LV_DISPLAY_RENDER_MODE_FULL` and `timer_period_ms = 50`; the earlier LVGL partial-render experiment still remains reverted because it corrupted the rotated display output.
- Changed FC frame publication so old/stale NES frames are not repeatedly converted and redrawn. The display is updated only when the NES core actually produces a new frame.
- Increased the FC emulation slice to `12000` instructions or about `12ms` per service loop after touch input was moved to the direct polling path.

Hardware observations:

- A second hotfix build proved bottom virtual buttons now reach both the FC service and the NES bus while the game is running.
- Captured examples included `FcEmulator: controller=0x01`, `0x02`, `0x10`, `0x20`, `0x40`, `0x80`, and `NesBus: controller latch=0x..`.
- This proves the remaining "no reaction" symptom is no longer a basic LVGL button event/layout failure.
- Before the stale-frame skip and larger emulation slice, `fc perf` still showed only about `3-5` produced NES frames per second on the tested ROM.
- The final third build was flashed successfully, but a fresh post-flash in-game `fc perf` sample was not captured because no new FC interaction occurred during the last monitor window.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x3df920`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x2206e0` bytes, about 35%.
- Flashed successfully to `/dev/cu.usbmodem212401`.

Current FC status:

- Touch/controller delivery is now confirmed on hardware.
- The display path no longer needs to use LVGL full-screen refresh for each game frame.
- FC should still be treated as not fully playable until the final build is tested in-game and `fc perf produced_fps=...` is captured again.
- If final `produced_fps` remains low, continue with emulator-core optimization/correctness or replace the minimal core with a known-good core.

## 2026-06-18: FC Responsiveness Hotfix and Display Rollback

Scope:

- Added a direct touch-poll fallback for FC game mode so bottom virtual controller touches feed `FcEmulatorService::SetController()` even when LVGL button events are delayed.
- Shortened FC controller release hold from `1500ms` to `120ms` to avoid sticky-feeling buttons.
- Reduced the FC emulator task priority from `3` to `1` so UI/touch and XiaoZhi tasks get scheduler time first.
- Split NES execution into shorter slices: `4500` instructions or about `6ms` per service loop, instead of a longer per-frame burst.
- Added `fc perf produced_fps=...` runtime logging for FC frame-rate and loop diagnostics.

Display note:

- A test change from LVGL full-screen render mode to partial render mode caused visible screen corruption on the QDTech rotated display path.
- That partial-render experiment was reverted immediately. Current code is back to `LV_DISPLAY_RENDER_MODE_FULL` and `timer_period_ms = 50`.
- Do not retry LVGL partial render mode without first rewriting and hardware-testing the custom rotated `Flush()` path.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x3df5a0`.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Post-rollback serial logs returned to the known full-screen flush path, for example `flush area=480x320 at 0,0`.

## 2026-06-18: FC/NES Core Timing and Sprite Correctness Pass

Scope:

- Replaced the previous fixed `18` PPU ticks per CPU instruction estimate with a 6502 base-cycle table and `CPU cycles * 3` PPU advancement.
- Advanced PPU time during NMI service cycles so NMI handling no longer consumes CPU cycles without corresponding PPU progress.
- Raised the NES frame instruction guard from `5200` to `12000` while keeping the existing per-call time budget.
- Fixed Mapper 2 bank switching to wrap selected PRG banks by the actual ROM bank count instead of masking to 16 banks.
- Fixed sprite rendering to keep low/high CHR bitplanes separate instead of OR-ing them into one byte.
- Fixed sprite X placement direction, OAM Y top-line offset, and sprite-0-hit detection against the original sprite index.
- Moved background tile fetch to the beginning of each 8-pixel group so visible pixels use the tile data for the current group.
- Changed `FcEmulatorService::Start()` to register callbacks only, so the FC task, SD mount, and ROM scan no longer start during normal boot.
- Releasing the FC page now stops playback, clears controller state, cancels scans, and releases the ROM list from the FC task.

Verification:

- Direct build in the original macOS workspace path reached link but failed because the parent path contains spaces/Chinese text and an ESP-SR linker search path was split into `3.5` and `寸/...`. This was treated as an environment/path issue, not an FC source compile error.
- Full validation build was run from a temporary no-space copy at `/private/tmp/qdtech_s3_build_src`.
- Build completed successfully for ESP32-S3/QDTech.
- Final `xiaozhi.bin` size after the FC lazy-start change: `0x3df3b0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x220c50` bytes, about 35%.

Hardware status:

- Flashed successfully from `/private/tmp/qdtech_s3_build_src` to `/dev/cu.usbmodem212401`.
- Boot log confirmed SKU `qdtech-s3-touch-lcd-3.5`, desktop UI creation, `Touch max points: 5`, WiFi, OTA latest, MQTT, AFE load, and `Application: STATE: idle`.
- Pre-lazy-start flash showed FC boot-time SD/ROM scan increased internal SRAM pressure and produced `esp-aes: Failed to allocate memory` during MQTT publish.
- Final lazy-start flash no longer starts the FC task, SD mount, or ROM scan at boot; captured boot logs showed only `fc emulator service registered` before normal XiaoZhi startup.
- Residual issue: AFE `Ringbuffer of AFE(FEED) is full` warnings still appear under the captured runtime, and internal SRAM remains tight. Treat this as a separate XiaoZhi/audio stability follow-up.
- FC/NES should still be treated as not yet proven playable until tested on the actual QDTech board with at least one known-good Mapper 0/NROM ROM and one Mapper 2 ROM.

## 2026-06-18: FC/NES App Card, ROM List, and Unfinished Emulator Core

Scope:

- Added an `FC NES` card to the desktop Apps page.
- Added a dedicated FC page with ROM status, ROM list text, preview/game screen, and touch actions.
- Removed the top FC page brand/title/status text to give the game screen more usable space.
- Added visible virtual controller buttons for Up/Down/Left/Right, A, B, Select, and Start.
- Added controller-state serial logging as `FcEmulator: controller=0x..` for touch debugging.
- Removed the right-side ROM information panel and Play/Stop/Prev/Next controls; FC now uses a handheld-style layout with the game screen on top and a classic controller on the bottom.
- Changed FC behavior to list-first: opening the FC page scans the SD card and shows a ROM list; Prev/Next only move selection; Start loads the selected game.
- Added a `LIST` button in game mode to stop emulation and return to the ROM list.
- Blocked right-swipe page exit while the game view is active to avoid accidental exits during D-pad swipes.
- Moved the NES PPU framebuffer from internal RAM to PSRAM after serial logs showed SDMMC mount failures with `sdmmc_read_sectors: not enough mem`.
- Reduced FC SD mount file handles and logged internal heap before mount attempts to make future SD failures diagnosable.
- Added `FcEmulatorService` for lazy task startup, SD card mount/reuse, `.nes` discovery, and iNES header validation.
- Scans these SD card locations for ROMs: `/sdcard/nes`, `/sdcard/NES`, `/sdcard/FC`, `/sdcard/fc`, `/sdcard/roms`, `/sdcard/ROMS`, and `/sdcard`.
- Reuses the existing SD mount when Photos already mounted `/sdcard`; otherwise mounts the QDTech SDMMC pins with 4-bit mode and 1-bit fallback.
- Added a minimal NES core:
  - 6502 CPU execution.
  - PPU background/sprite rendering into `256x240` frames.
  - Mapper 0/NROM and Mapper 2/UxROM PRG banking.
  - CHR ROM and CHR RAM handling.
  - Horizontal/vertical nametable mirroring.
  - Touch-screen virtual controller mapping for A/B/Select/Start/Up/Down/Left/Right.
- Optimized frame transfer by keeping the PPU framebuffer as 8-bit palette indices and converting to RGB565 only when publishing a frame to LVGL.
- Fixed one 6502 CPU bug found during the hardware session: `TSX (0xBA)` now copies `SP` to `X` instead of corrupting `SP`.
- Adjusted `RTI (0x40)` to restore the unused status bit.
- Mirrored controller reads on both `0x4016` and `0x4017` while debugging input compatibility.

Current limitation:

- This is not yet a playable emulator. It is a hardware-visible integration milestone with a ROM list, game view, and touch controller wiring.
- It only accepts Mapper 0 and Mapper 2 ROMs and has no APU/audio output yet.
- CPU/PPU timing and opcode compatibility are still incomplete. Hardware logs show the game can latch controller states, but tested ROMs still show little or no visible response to input.
- Unsupported mappers now fail explicitly instead of pretending to run as Mapper 0.
- The next implementation step should replace this minimal core with a more complete emulator core, or continue fixing CPU/PPU compatibility against a known-good small NROM ROM on the real board.
- A local reference checkout existed during this session at `C:\tmp\esp32-nesemu` using Nofrendo. It was not integrated because the original example reads a fixed Flash ROM and drives ILI9341 directly; integration would need a clean SD-ROM, LVGL-frame, and touch-input adapter. Check license obligations before importing it.

Verification:

- Build succeeded with the minimal emulator core.
- `xiaozhi.bin` size: `0x3dd8b0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x222750` bytes, about 36%.
- Flashed successfully to `COM13`.
- Serial logs confirmed SD mount, scan, and 146 supported `.nes` files found on the user's SD card.
- Serial logs confirmed list navigation, selected ROM load, and NES frame publication.
- Serial logs confirmed virtual buttons reach the emulator service as `controller=0x..`.
- Serial logs confirmed the NES bus latches controller states such as `0x40`, `0x20`, `0x08`, `0x02`, `0x01`.
- User-visible result at handoff: ROM list works and games can open, but gameplay remains effectively non-responsive and not ready to call playable.

## 2026-06-15: Radio Visual Enhancement - Audio Spectrum Bars

Scope:

- Added 16 audio spectrum bars at the bottom of Radio page
- Bars animate randomly when radio is playing (simulating audio frequency)
- Bars show static 5px height when radio is stopped
- Animation refresh rate: 100ms
- Uses COLOR_GOLD for visual consistency

Implementation:

- Added `radio_bars_[16]` array for bar objects
- Added `radio_playing_` state flag
- Added `RadioAnimTimerCb` callback for animation
- Modified `SetRadioState` to update playing state based on state string

Verification:

- Build: `idf.py -B build-qdtech build`
- Flash: `idf.py -B build-qdtech -p COM13 -b 921600 flash`
- Bars visible on Radio page
- Animation active during playback

## 2026-06-15: P0-P6 Optimization and LVGL Touch Migration

Scope:

- P0: Verified build environment and reproducible build process
- P1: Hardware runtime validation (boot log, WiFi, MQTT, SNTP, weather)
- P2: Expanded daily card content (festivals 19→19, history 8→31, quotes 16→32)
- P3: Focus timer NVS persistence for completion count
- P4: Enhanced radio audio focus logging
- P5: Radio station index NVS persistence, error state after 5 failures
- P6: LVGL touch input device migration (hybrid mode)

Font fix:

- Regenerated LXGW WenKai subset fonts (499 Chinese characters)
- Fixed garbled text on daily card and UI elements
- Adjusted daily card layout (title width 82→100px, divider 112→120px)

Touch architecture:

- Created LVGL input device for button clicks and slider interactions
- Kept manual touch polling for gesture detection (LVGL 9.x limitation)
- Disabled duplicate HandleTap coordinate detection for migrated pages
- Added Radio button event callbacks (prev/play/stop/next)

Verification:

- Build: `idf.py -B build-qdtech build`
- Flash: `idf.py -B build-qdtech -p COM13 -b 921600 flash`
- Boot: SKU=qdtech-s3-touch-lcd-3.5, Desktop UI created, WiFi connected
- Daily card: Festival/history/quote content displays correctly
- Focus timer: Completion count persists across reboots
- Radio: Station index persists, error state shows after failures

## 2026-06-13: Fix Black Photos Page After Settings Growth

Problem:

- Opening Photos showed a black screen.
- Runtime logs proved the Photos page activated, but the lazy photo task failed to start:
  - `PhotoService: photo service task create failed ret=-1`
- The SD card and JPEG files were not the cause.

Fix:

- Removed the obsolete Settings controls that were still being created and then hidden behind the new Settings layout.
- Moved the 6144-byte PhotoService task stack to PSRAM with an internal-memory fallback.
- Added internal-memory and largest-free-block diagnostics around photo task creation.
- Kept normal Photos behavior lazy: the task still starts only when Photos is opened.

Verification:

- Final firmware built successfully.
- `xiaozhi.bin` size: `0x3c3c30`.
- Final firmware flashed successfully to COM13 at 921600 baud.
- A temporary, uncommitted boot self-test activated PhotoService and confirmed:
  - task created with `memory=psram`
  - SD card mounted in 4-bit mode at 20 MHz
  - 53 JPG files found in the physical photos directory
  - repeated 480x320 JPEG decode succeeded
- The temporary auto-activation was removed before the final build and flash.
- No panic, abort, or Guru Meditation appeared during the self-test.

Maintainer notes:

- Do not restore hidden duplicate Settings object trees; hidden LVGL objects still consume memory.
- Keep optional service task stacks in PSRAM where the ESP-IDF configuration and workload permit it.
- If task creation fails again, inspect both total internal SRAM and the largest contiguous internal block.

## 2026-06-13: Unify Desktop Navigation Rules

Scope:

- Added a single `NavigateBack()` rule:
  - Apps returns to Main.
  - Feature pages return to Apps.
- Aligned manual swipe behavior with the visible navigation model:
  - Main left swipe opens Apps.
  - Apps right swipe returns to Main.
  - Feature pages right swipe return to Apps.
  - Photos keeps the previously accepted left-or-right swipe exit.
- Removed hidden tap-to-exit and refresh zones from the full-screen Photos page.
- Leaving the Radio page no longer implicitly stops playback; Stop remains an explicit transport action.
- Simplified the Apps footer hint to `Swipe right: Home`.

Verification:

- Build completed successfully.
- `xiaozhi.bin` size: `0x3c3cc0`.
- Firmware flashed successfully to COM13 at 921600 baud.
- Boot reached `Desktop UI created`, touch initialization, `STATE: idle`, and SNTP synchronization.
- Physical right-swipe exit from Photos logged `Navigate back page=2 target=1` and returned to Apps.
- No panic, abort, or Guru Meditation appeared in the captured logs.
- Main/Apps/Radio navigation and background-radio continuity still need a complete user-visible pass on the touchscreen.

## 2026-06-13: Make Settings Visible And Functional

Scope:

- Replaced the off-screen Settings content with a scrollable layout sized for the 480x320 display.
- Added working brightness and output-volume sliders.
- Added a manual-touch release bridge for the current non-`lv_indev_t` input path:
  - horizontal slider release applies the final brightness or volume value
  - vertical release scrolls the Settings content
  - hidden slider regions cannot receive touches outside the visible content viewport
  - the Settings Back control has a matching manual touch zone
- Hardware values are synchronized when Settings opens.
- Changes are persisted only when slider interaction finishes, avoiding repeated NVS writes while dragging.
- Kept WiFi scan results and weather-location status available in the page.
- Fixed an initialization deadlock found during hardware verification by deferring `Board::GetInstance()` access until after board construction.

Verification:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build
idf.py -B build-qdtech -p COM13 -b 921600 flash
```

Build result:

- Build completed successfully.
- `xiaozhi.bin` size: `0x3c3cd0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x23c330` bytes, about 37%.

Hardware/runtime verification on COM13:

- Firmware flashed successfully at 921600 baud.
- Boot reached `Desktop UI created`.
- Touch controller reported 5 points.
- WiFi and MQTT connected.
- Application reached `STATE: idle`.
- SNTP synchronized.
- No panic, abort, or Guru Meditation appeared in the captured startup log.
- Physical brightness dragging was confirmed with values from 16% through 100%.
- Physical vertical Settings scrolling was confirmed in both directions.
- Physical volume dragging, Back control, and persisted-value restoration still require user-visible confirmation on the touchscreen.

Maintainer notes:

- Do not call `Board::GetInstance()` from `DesktopUI::Create()` while the QDTech board singleton is still being constructed.
- Slider changes are applied on release to limit persistent-storage writes.
- Settings currently has an explicit manual-touch adapter because the board has not yet migrated to a standard LVGL input device.

## 2026-06-13: Replace QTE With Focus Timer And Reduce Layout Overlap

Scope:

- Replaced the Apps page `QTE / Quote / Daily` tile with `FOC / Focus / 25 min`.
- The tile now opens a local Focus Timer page instead of starting a XiaoZhi quote chat.
- Added a lightweight LVGL focus timer state machine:
  - 25 minute focus mode
  - 5 minute break mode
  - start/pause
  - reset
  - completed focus-session counter
- Reworked the Focus Timer page after hardware feedback that the first version had overlapping elements.
- Current layout is a small-screen three-column dashboard:
  - left status/quote card
  - center timer ring and time readout
  - right focus/break controls and completed count
  - bottom start/reset buttons
- Updated manual touch hot zones for the new Focus Timer button positions.
- Regenerated embedded LXGW WenKai 16 px and 20 px subset fonts for the added Chinese UI text.

Verification:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build
idf.py -B build-qdtech -p COM13 -b 921600 flash
```

Build result:

- Build completed successfully. The outer `idf.py` command timed out once while Ninja continued in the background; the background build finished successfully.
- `xiaozhi.bin` size: `0x3c2ff0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x23d010` bytes, about 37%.

Hardware/runtime verification on COM13:

- Firmware flashed successfully at 921600 baud.
- Boot reached `Desktop UI created`.
- Application reached `STATE: idle`.
- SNTP synchronized.
- Daily card updated for `2026-06-13`.
- Weather fetch completed for Zhongshan.
- No panic, abort, or Guru Meditation was observed in the captured startup log.

Maintainer notes:

- Focus Timer is currently local UI state only; completed count is not persisted in NVS.
- If Focus Timer Chinese labels change, regenerate both `qd_font_lxgw_16.c` and `qd_font_lxgw_20.c`.
- Keep the Focus page sparse on the 480x320 screen; avoid re-adding large decorative elements unless hardware-visible spacing is checked.

## 2026-06-06: Weather Visuals And Chinese Daily Card

Scope:

- Improved the main-page weather card so Open-Meteo weather codes select clear, cloudy, rain, snow, and storm visuals instead of always showing the sunny icon.
- Adjusted weather temperature and detail label positions to avoid overlap.
- Replaced the English quote card content flow with a date-linked daily card.
- Daily card priority is now fixed Gregorian festival first, history-on-this-day second, and local daily quote fallback third.
- Added embedded LXGW WenKai 16 px and 20 px LVGL subset fonts for the current Chinese daily-card text.
- Laid out the daily card as left-side date/category and right-side larger Chinese body text to use the full card width.
- Kept the font source TTF out of Git; only generated LVGL subset C files are tracked.

Verification:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" reconfigure build
idf.py -B build-qdtech -p COM13 -b 921600 flash
```

Build result:

- Build completed successfully after reconfigure so the new generated font C files were included.
- `xiaozhi.bin` size: `0x3c0c10`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x23f3f0` bytes, about 37%.

Hardware/runtime verification on COM13:

- Firmware flashed successfully at 921600 baud.
- Boot reached `Desktop UI created`.
- Application reached `STATE: idle`.
- SNTP synchronized.
- Daily card logged `Daily card updated for 2026-06-06`.
- Weather card logged a cloudy visual mapping for weather code `1`.
- Weather fetch completed for Zhongshan with temperature and cached update time.
- No panic, abort, or Guru Meditation was observed in the captured startup log.

Maintainer notes:

- If new Chinese daily-card text is added, regenerate both `qd_font_lxgw_16.c` and `qd_font_lxgw_20.c` so all glyphs exist.
- Do not commit `.codex_tmp/`; it only held the temporary LXGW WenKai TTF used to generate the subset fonts.
- The historical-events table is intentionally small for this phase. Expand it gradually and verify card wrapping on the real 480x320 screen.

## 2026-06-05: Finish MicroSD Full-Screen Photo Slideshow

Scope:

- Made the Photos page a pure full-screen slideshow with no title, status bar, Back button, Refresh button, hint text, or status text overlay.
- Added left-swipe and right-swipe exit from Photos back to Apps.
- Expanded photo scanning to common SD directory names: `/photos`, `/PHOTOS`, `/Photos`, `/PHOTOS_READY`, `/photos_ready`, and the SD root.
- Skips hidden/macOS resource files such as `._001.jpg`.
- Added SDMMC mount fallback from 4-bit default speed to 1-bit 10 MHz when 4-bit mount fails.
- Added clearer SD mount and photo directory scan logs.
- Fixed JPEG RGB565 color output by disabling the extra JPEG byte swap.
- Changed photo scaling to cover the full 480x320 landscape screen.

Verification:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build
idf.py -B build-qdtech -p COM13 -b 921600 flash
```

Build result:

- Build completed successfully.
- `xiaozhi.bin` size: `0x3af950`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x2506b0` bytes, about 39%.

Hardware/user verification:

- SD card mounted and photos displayed.
- Photo colors were confirmed normal after disabling JPEG byte swap.
- Photos were confirmed full-screen.
- Visible Photos page text and buttons were removed.
- Photos page exits with left or right swipe.

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

## 2026-06-04: Restore XiaoZhi AI Stability After Photo/Radio Work

Problem observed:

- Entering XiaoZhi AI could reboot the board.
- Backtrace resolved to `MqttProtocol::OpenAudioChannel()` -> `EspUdp::Connect()` -> `std::thread`, where UDP receive thread creation failed and C++ terminated the process.
- MCP `tools/list` replies were large enough to cause repeated MQTT publish failures and `esp-aes: Failed to allocate memory`.
- Each MCP publish failure triggered the generic network error alert, causing repeated speaker pops.

Fix:

- Reverted the risky radio streaming optimization commit.
- Made `EspUdp::Connect()` catch `std::system_error` when creating the UDP receive thread and return `false` instead of aborting.
- Reduced MCP `tools/list` page size and shortened common/QDTech MCP tool descriptions.
- Treated MCP publish failures as warnings instead of user-facing audio alerts.
- Made the photo slideshow task lazy-start only when the Photos page is opened.
- Reduced radio task stack from 8192 to 6144 bytes.

Verification:

- Built successfully with `cmake --build build-qdtech`.
- Flashed successfully to COM13 at 921600 baud.
- XiaoZhi AI reached `STATE: listening` and `STATE: speaking`.
- Serial log showed XiaoZhi replies including `你好，小志。` and no reboot.

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
