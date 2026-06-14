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
- `sdkconfig.defaults`: board type defaults for this board.

## Current Verified State

Last verified on 2026-06-14 in the macOS workspace:

- Workspace: `/tmp/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware`
- Branch: `main`
- User remote branch: `origin/main`
- Last verified update: 2026-06-14 added visual animations (breathing, shadow pulse, weather particles) and optimized time/weather sync
- Build directory used for board verification: `build-qdtech`
- Serial port used during the last device flash: `/dev/tty.usbmodem212401`

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

Important 2026-06-05 stability finding:

- A previous radio smoothing change (`834d55a`) was reverted by `76c1541` because the firmware became unstable.
- The XiaoZhi crash when entering chat was traced to internal SRAM pressure during audio-channel setup: `pthread: Failed to create task!` in `EspUdp::Connect()`.
- Commit `5b58b22` keeps photo/radio tasks smaller or lazy, catches UDP thread creation failure instead of aborting, shortens MCP tool descriptions, reduces MCP `tools/list` payload size, and treats MCP publish failures as warnings instead of user-facing network alerts.
- Do not reintroduce large always-running tasks, large MCP descriptions, or broad radio buffering changes without first proving XiaoZhi listen/speak on hardware.

## What Is Already Implemented

- XiaoZhi voice core from the upstream project.
- QDTech 3.5 inch landscape display adaptation.
- CST9217/TDDI touch polling with tap/swipe dispatch.
- LVGL desktop UI with main/app/XiaoZhi/radio/settings pages.
- Local Calendar page with month grid, Today, Prev, and Next controls.
- Local Focus Timer page with 25 minute focus mode, 5 minute break mode, start/pause, reset, and in-memory completion count.
- Local Photos page replacing the previous Weather app tile, reading JPEG photos from common SD directories and playing them as a pure full-screen slideshow.
- Time and weather service.
- Main-page daily card with date-linked festival, history-on-this-day, and local quote fallback content.
- Embedded LXGW WenKai LVGL subset fonts for the current daily-card Chinese text.
- MP3 network radio service.
- MCP tools for weather location and radio controls.
- Lightweight audio focus behavior so XiaoZhi states can pause the radio.
- Radio reconnect/fallback handling and remembered last successful station URL.
- Main page visual enhancements:
  - Daily card breathing animation (opacity 0.92-1.0) for subtle visual depth.
  - Clock digit shadow pulse effect (opacity 10-40) for time display ceremony.
  - Weather particle animation (golden particles floating up from sun) for clear/cloudy weather.
- Optimized time/weather sync speed:
  - SNTP wait time reduced from 30s to 15s (500ms polling).
  - Weather API timeout reduced from 20s to 10s.
  - Weather retry attempts reduced from 3 to 2.

## What Still Needs Work

Highest priority areas are listed in `docs/NEXT_TASKS.md`.

Short version:

- Keep proving XiaoZhi voice, radio, touch, and display together on hardware after every meaningful firmware change.
- Migrate touch toward standard LVGL `lv_indev_t` carefully.
- Improve display flush performance without rewriting the display path all at once.
- Move radio station configuration toward SPIFFS/SD/NVS while keeping built-in fallback stations.
- Expand settings UI only after the stability base is proven.

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
