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
- `radio_service.cc` / `radio_service.h`: MP3 network radio and audio focus behavior.
- `time_weather_service.cc` / `time_weather_service.h`: SNTP clock, weather fetch, cached weather display.
- `sdkconfig.defaults`: board type defaults for this board.

## Current Verified State

Last verified on 2026-06-04 in the Windows workspace:

- Workspace: `D:\3.5inch_ESP32-S3\xiaozhi-esp32`
- Branch: `codex/qdtech-landscape-v176`
- User remote branch: `qdtech-new/main`
- Last code stabilization commit before this handoff set: `f4451c1 stabilize QDTech desktop base`
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
- Weather API may return 429 or 502; the firmware should keep running and retain cached data when available.

## What Is Already Implemented

- XiaoZhi voice core from the upstream project.
- QDTech 3.5 inch landscape display adaptation.
- CST9217/TDDI touch polling with tap/swipe dispatch.
- LVGL desktop UI with main/app/XiaoZhi/radio/settings pages.
- Time and weather service.
- MP3 network radio service.
- MCP tools for weather location and radio controls.
- Lightweight audio focus behavior so XiaoZhi states can pause the radio.
- Radio reconnect/fallback handling and remembered last successful station URL.

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
