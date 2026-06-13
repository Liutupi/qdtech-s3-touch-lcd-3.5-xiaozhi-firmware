# QDTech Firmware Changelog

This changelog tracks QDTech-specific firmware maintenance. It is not a replacement for `git log`; it records the practical handoff facts that future maintainers need.

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
