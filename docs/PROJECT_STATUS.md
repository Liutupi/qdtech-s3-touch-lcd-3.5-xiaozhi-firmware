# 2026-06-28 Project Status: v1.7.60

Firmware:

- Version bumped to `1.7.60`.
- Build target: QDTech ESP32-S3 3.5 inch touch LCD.
- Build directory: `build-qdtech`.
- Latest app image: `build-qdtech\xiaozhi.bin`.
- Latest observed app size: `0x5f7f30`, leaving about `0x80d0` bytes in the `0x600000` app partition.
- Latest flash method: app-only flash to new board `COM16` at `0x100000`; hash verified and boot verified as `App version: 1.7.60`.
- Release assets are in `releases/v1.7.60/` as standalone `app.bin`, full merged `full.bin`, and `firmware.zip`.

WiFi provisioning status:

- New board setup hotspot is visible as `Xiaozhi-85A1`.
- Provisioning now starts in pure SoftAP mode and performs WiFi scanning only when the web page requests `/scan`.
- Hardware log verified `manual scan done ap_num=15`, including `liutupi` at `RSSI: -7`.
- Default BSSID memory is off for QDTech to avoid same-SSID mesh/AP lock-in.

# 2026-06-28 Project Status: v1.7.56

Firmware:

- Version bumped to `1.7.56`.
- Build target: QDTech ESP32-S3 3.5 inch touch LCD.
- Build directory: `build-qdtech`.
- Latest app image: `build-qdtech\xiaozhi.bin`.
- Latest observed app size: `0x5f7c00`, leaving about `0x8400` bytes in the `0x600000` app partition.
- Latest flash method: app-only flash to `COM13` at `0x100000`; hash verified and boot verified as `App version: 1.7.56`.
- Release assets are in `releases/v1.7.56/` as standalone `app.bin`, full merged `full.bin`, and `firmware.zip`.

FC/NES status:

- Mapper correction and diagnostics are in place for tested problematic ROMs.
- `轩辕剑` is improved and now runs through mapper 224.
- `快打旋风 [Cony Soft]` is identified as mapper 83 but still flower-screens.
- `吞食天地2` is identified as mapper 198 but still black/solid-color output.
- CPU/audio/frame generation continue for the two failing ROMs, so the remaining work is mapper/PPU correctness rather than SD loading or display transport.

# QDTech Project Status

## Snapshot

Date: 2026-06-27

This fork currently builds and runs as the QDTech ESP32-S3 3.5 inch landscape XiaoZhi firmware. It should be treated as a working firmware base, not an experimental scratch tree.

## Version History

- v1.0 (2026-06-14): Stable release with optimized time/weather sync
- v1.1 (2026-06-14): Visual animations and further sync optimization
- v1.2 (2026-06-15): P0-P6 optimization, LVGL touch migration, font fix, NVS persistence
- v1.3 (2026-06-15): Radio visual enhancement with audio spectrum bars
- v1.7.14 (2026-06-19): FC ROM scan cap raised to 192 and clearer ROM load diagnostics
- v1.7.15 (2026-06-19): Calendar visual redesign, larger calendar controls, and secondary-page brand polish
- v1.7.16 (2026-06-19): Apps page reference-style visual polish and centered Network WiFi icon
- v1.7.17 (2026-06-19): Main-page daily card lunar festival support, including 2026 Dragon Boat Festival
- v1.7.18 (2026-06-19): FC emulator Nofrendo APU audio routed to ES8311 output
- v1.7.19 (2026-06-20): Real QDTech battery ADC monitor plus first BOOT/GPIO0 deep-sleep soft-power implementation
- v1.7.20 (2026-06-20): BOOT soft-power waits for key release before deep sleep so the same long press does not immediately wake the board
- v1.7.21 (2026-06-20): BOOT startup clicks no longer clear saved WiFi after wake
- v1.7.22 (2026-06-21): GitHub OTA download/write path tolerates missing content length and short first TLS chunks
- v1.7.23 (2026-06-21): Persisted Classic/Cat theme switcher with Cat-style pink UI, brand mark, main clock, daily-card cat, and XiaoZhi face polish
- v1.7.24 (2026-06-21): Shared LXGW WenKai UI font subset for Classic and Cat fixed Chinese UI text
- v1.7.25 (2026-06-21): FC/NES dynamic ROM names restored to broad Puhui font coverage
- v1.7.26 (2026-06-22): GitHub OTA firmware-download buffers enlarged for long release-asset redirects
- v1.7.27 (2026-06-23): Added Tupi Warm theme and restored its weather card to the proven Classic weather animation layout
- v1.7.28 (2026-06-23): GitHub Release asset OTA fallback via proxy-prefixed download URLs after direct GitHub TLS connection failures
- v1.7.33 (2026-06-23): Settings OTA uses a PSRAM-stack HTTPS task plus an internal-RAM flash worker so TLS and esp_ota_write no longer fight over one internal task stack
- v1.7.32 (2026-06-23): Settings OTA upgrade reduces internal stack and firmware HTTP buffer so header/download buffers fit in low internal RAM
- v1.7.31 (2026-06-23): Settings OTA check uses a PSRAM stack while flash-writing upgrade uses an internal stack, fixing the task-create failure found during v1.7.30 testing
- v1.7.30 (2026-06-23): Settings OTA writes now run from internal RAM task/buffers, fixing the cache-freeze assertion seen when v1.7.28 tried to install a GitHub Release asset
- v1.7.29 (2026-06-23): Main-page weather module uses a six-scene GIF animation pack for clear, cloudy, rain, snow, fog, and storm
- v1.7.40 (2026-06-23): Code quality optimization, SD card radio.json support, FC stability fixes, thread safety improvements
- v1.7.43 (2026-06-24): Rolled back the withdrawn Caiyun weather integration and restored the no-token Open-Meteo weather path for stable multi-board OTA
- v1.7.44 (2026-06-25): Added persistent profile/weather config plumbing, a verified WiFi phone config page/API, automatic city geocoding, Chinese weather text rendering, memory-guarded BLE prototype code, and a fixed main-page logo/name brand layout
- v1.7.45 (2026-06-25): Brand text auto-wrap for long text, weather refresh on page switch, BLE compilation fix
- v1.7.46 (2026-06-26): Broader Chinese UI font coverage, stable Phone Web IP/status display, delayed/retried Phone Web startup, strongest-BSSID WiFi preference, cleaned brand earth GIF, and refined Open-Meteo weather accuracy using rain/cloud/humidity context
- v1.7.47 (2026-06-26): SD-card `Nothing Impossible` podcast player with a third Media page, two-level episode UI, seekable progress bar, volume leveling, playback/list stability fixes, and UTF-8-safe podcast list rendering
- v1.7.48 (2026-06-26): System-level smoothness pass for Podcast and FC/NES: lazy podcast startup, lighter episode selection, asynchronous ROM loading, and media mutual exclusion to reduce SD/audio contention and UI freezes
- v1.7.49 (2026-06-26): Photos portrait display polish: landscape photos still fill the screen, while portrait photos show fully centered with a generated dark blurred background from the same image
- v1.7.50 (2026-06-26): Radio and Podcast audio quality polish: radio automatic gain/soft limiting, per-stream gain reset, and removal of mono sample over-boosting to reduce clipping
- v1.7.51 (2026-06-26): Theme-specific XiaoZhi face animation metrics so Cat, Classic, and Tupi Warm keep their intended eye/mouth bounds during idle, listening, speaking, and blink animation
- v1.7.52 (2026-06-27): Cat theme XiaoZhi animated kitten face pack derived from the supplied reference expression grid, with no overlapping eye highlights or duplicate speaking mouths
- v1.7.53 (2026-06-27): Tupi Warm XiaoZhi animated warm robot face pack derived from the supplied reference image, with full-character GIF motion and complete speaking mouth frames
- v1.7.54 (2026-06-27): Podcast cover memory recovery by lazily creating Cat/Tupi Warm XiaoZhi GIF objects only while the XiaoZhi page is visible, plus a Cat daily-card kitten mark matching the animated Cat face pack
- v1.7.55 (2026-06-27): Classic theme XiaoZhi robot GIF face pack with cleaned dialogue subtitles and a compact Classic robot avatar beside the main daily quote

## Confirmed Hardware From Source

From `main/boards/qdtech-s3-touch-lcd-3.5/config.h`:

- MCU target: ESP32-S3.
- Display logical size: 480x320 landscape.
- Display physical panel size: 320x480.
- LCD controller path: ST77922 initialization in board code.
- LCD color inversion: enabled.
- LCD QSPI pins: CS GPIO10, CLK GPIO12, D0 GPIO11, D1 GPIO13, D2 GPIO14, D3 GPIO9.
- LCD backlight: GPIO41.
- Touch I2C address: `0x55`.
- Touch pins: INT GPIO47, RST GPIO48.
- MicroSD SDMMC pins from the product specification:
  - CLK GPIO5
  - CMD GPIO4
  - D0 GPIO6
  - D1 GPIO7
  - D2 GPIO2
  - D3 GPIO3
- Audio codec: ES8311.
- Audio I2S: MCLK GPIO17, BCLK GPIO18, WS GPIO21, DOUT GPIO15, DIN GPIO16.
- Audio and touch I2C: SDA GPIO38, SCL GPIO39.
- PA pin: GPIO1, inverted enable.
- Boot button: GPIO0.
- Battery voltage ADC: ADC1_CH7 / GPIO8 with x2 divider, calibrated in firmware when available.

Anything not confirmed by source should be treated as unknown until checked against hardware schematics.

## Build Configuration

Board type is enabled by:

- Root `sdkconfig.defaults`: `CONFIG_BOARD_TYPE_QDTECH_S3_TOUCH_LCD_3_5=y`
- Board defaults: `main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults`
- ESP32-S3 defaults: `sdkconfig.defaults.esp32s3`, including 16 MB flash and PSRAM options.

Use an independent build directory for QDTech validation:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build
```

Flash example:

```powershell
idf.py -B build-qdtech -p COM13 -b 921600 flash monitor
```

Always enumerate serial ports first if the board has moved to a new machine.

## Implemented Features

- XiaoZhi voice stack remains the core application.
- Landscape LVGL display with QDTech board-specific flush path.
- Touch tap/swipe polling for desktop navigation.
- Persisted Settings -> Appearance -> Theme switcher with `Classic`, `Cat`, and `Tupi Warm` themes.
- XiaoZhi face animation uses theme-specific metrics. Cat keeps compact eyes and mouth inside the face frame, while Classic and Tupi Warm keep the larger original face layout.
- Unified navigation hierarchy:
  - Main left swipe opens Apps.
  - Apps right swipe returns to Main.
  - Feature pages right swipe return to Apps.
  - Photos exits to Apps with either horizontal swipe.
- Desktop UI pages:
  - main page
  - apps page with the reference-style dark two-column app center, main-screen `Nothing impossible` brand mark, and centered Network WiFi glyph
  - calendar page with a left today card, right monthly grid, and enlarged Prev / Today / Next touch targets
  - photo page
  - FC/NES page
  - focus timer page
  - XiaoZhi page
  - radio page
  - settings page
- Calendar month view with Today, Prev, Next, today highlight, weekend coloring, and previous/next month filler days.
- Photo slideshow page from MicroSD card:
  - Apps tile `Photos` replaces the previous Weather app tile.
  - Reads JPG/JPEG files from `/sdcard/photos`, `/sdcard/PHOTOS`, `/sdcard/Photos`, `/sdcard/PHOTOS_READY`, `/sdcard/photos_ready`, or the SD root.
  - Skips macOS resource files such as `._001.jpg`.
  - Uses SDMMC 4-bit bus from the product specification, with a 1-bit low-speed fallback if 4-bit mount fails.
  - Displays photos as a pure full-screen 480x320 slideshow with no status bar, Back button, Refresh button, or text overlay.
  - Landscape photos fill the screen.
  - Portrait photos are fit fully and centered, with a generated same-photo dark blurred background filling both sides.
  - Exits the photo page with either left or right swipe back to Apps.
  - Cross-fades between decoded photos.
  - Photo task lazy-starts only when opening Photos.
  - Photo task stack is allocated from PSRAM first, with an internal-memory fallback and allocation diagnostics.
- FC/NES page from MicroSD card:
  - Apps tile `FC / NES / SD ROMs` opens a ROM list first.
  - Reads `.nes` files from `/sdcard/FC`, `/sdcard/nes`, `/sdcard/roms`, then `/sdcard`; scanning stops after the first populated directory and caps the list at 192 ROMs.
  - FATFS is configured for Chinese CP936 filenames with UTF-8 API output. Numeric ROM sets can add `roms.txt`, `roms.csv`, or `games.txt` in the active ROM directory to map filenames to friendly display names.
  - Prev/Next move the selected ROM; Start loads the selected ROM.
  - Start validates the selected ROM before entering Nofrendo and reports bad headers, NES 2.0 headers, ROMs over 2 MB, and unsupported mappers instead of failing generically.
  - Game view uses a top 320x240 scaled NES screen area and a bottom virtual controller.
  - Game view has D-pad, A, B, Select, Start, and LIST controls.
  - Nofrendo is the active emulator path; see `docs/HANDOFF.md` and `docs/CHANGELOG_QDTECH.md` for current hardware-test evidence and remaining risks.
- Time display through SNTP.
- Weather fetch with cached last successful data.
- Main-page weather visuals map current weather codes to clear, cloudy, rain, snow, and storm states. Tupi Warm intentionally reuses the proven Classic weather icon/particle layout while keeping the warm-paper palette.
- Main-page daily card rotates date-linked local content:
  - lunar and fixed Gregorian festival first
  - history-on-this-day second
  - daily quote fallback third
  - compact built-in 2024-2030 lunar festival date table for Spring Festival, Lantern Festival, Dragon Boat Festival, Qixi, Mid-Autumn Festival, Double Ninth Festival, Laba Festival, and Chinese New Year's Eve
- Focus Timer page:
  - Apps tile `FOC / Focus / 25 min` replaces the previous QTE/Quote chat tile.
  - Opens a local 25 minute focus / 5 minute break timer instead of starting XiaoZhi chat.
  - Supports start/pause, reset, focus/break mode selection, and an in-memory completed-session counter.
  - Uses a sparse three-column layout to avoid overlap on the 480x320 screen.
- Settings page:
  - Uses a vertically scrollable layout sized for the 480x320 display.
  - Provides working brightness and output-volume sliders.
  - Synchronizes controls from the current hardware values when the page opens.
  - Persists brightness and volume changes when slider interaction finishes.
  - Bridges the current manual touch path to slider release and vertical content scrolling.
  - Keeps WiFi scan results and weather-location status in the same page.
  - Shows phone-sync status plus the current custom text logo, owner name, and weather city.
- WiFi phone configuration:
  - Serves a local phone-friendly page at `http://<device-ip>/`.
  - Provides `GET /config` and `POST /config` for `logo`, `name`/`owner`, `city`, `latitude`, and `longitude`.
  - The user-facing form only requires the weather city; omitted coordinates are resolved automatically through Open-Meteo geocoding.
  - Last hardware verification used `http://192.168.1.111/`.
  - Stores values in NVS so they survive reboot.
  - Uses RAM config caching and a PSRAM-stack HTTP task to fit the current low-internal-RAM runtime.
  - Hardware POST verification with `city=中山` resolved `22.5231,113.3791`, logged `wifi config synced profile=1 weather=1`, refreshed weather, updated brand labels in place, and did not reboot.
  - Hardware POST verification with `city=dongguan` resolved to `东莞市 23.0180,113.7487`; Chinese weather text rendered through a Chinese-capable font.
- BLE phone configuration:
  - Can advertise as `QDTech-Config` when the low-memory guard allows NimBLE startup.
  - Accepts JSON writes for `logo`, `name`/`owner`, `city`, `latitude`, and `longitude`.
  - Reads back the current profile/weather JSON for phone-side synchronization.
  - Stores values in NVS so they survive reboot.
  - Current hardware validation after WiFi startup skipped BLE with `BLE low memory` (`free_internal=14743`, `largest_internal=14336`) to avoid rebooting the board.
- Weather location MCP tool: `self.weather.set_location`.
- MP3 network radio with built-in station list.
- SD-card podcast player:
  - Media page card `Nothing Impossible` opens the podcast list.
  - Reads `/sdcard/podcast/index.json` and per-episode MP3/JPG/TXT files.
  - Current prepared SD card contains 80 episodes.
  - List view shows 8 visible rows with Back / Up / Open / Down controls.
  - Detail view shows cover, title, summary, progress slider, and playback controls.
  - Progress slider can seek by percent.
  - MP3 playback uses a lightweight leveler to reduce inconsistent source volume.
  - Podcast list rendering uses UTF-8-safe truncation and broad Puhui font helpers for dynamic Chinese titles; do not switch arbitrary podcast titles to the narrow LXGW subset.
- Radio MCP tools:
  - `self.radio.get_status`
  - `self.radio.play`
  - `self.radio.stop`
  - `self.radio.next`
  - `self.radio.previous`
- Radio and XiaoZhi audio avoidance through lightweight audio focus.
- Main page visual enhancements (v1.1):
  - Daily card breathing animation (opacity 0.92-1.0) for subtle visual depth.
  - Clock digit shadow pulse effect (opacity 10-40) for time display ceremony.
  - Weather particle animation (golden particles floating up from sun) for clear/cloudy weather.
- Optimized time/weather sync speed (v1.0/v1.1):
  - SNTP wait time reduced from 30s to 15s (500ms polling).
  - Weather API timeout reduced from 20s to 10s.
  - Weather retry attempts reduced from 3 to 2.

## Current Runtime Behavior To Verify

After flashing, look for these logs:

- `SKU=qdtech-s3-touch-lcd-3.5`
- `Desktop UI created`
- `Touch max points: 5`
- WiFi connected log with an IP address.
- MQTT connected.
- AFE/wake word startup.
- `Application: STATE: idle`
- `PodcastService: loaded podcast episodes=80` when the SD card has `/podcast/index.json`.
- `TimeWeather: Time synchronized`
- `TimeWeather: Daily card updated for YYYY-MM-DD`
- `DesktopUI: Weather visual code=...`

For radio/audio focus:

- Start radio.
- Leave the Radio page and confirm playback continues.
- Trigger XiaoZhi listening or speaking.
- Radio should log audio focus yield/pause behavior.
- When XiaoZhi returns idle, radio should resume if the user had requested playback.

For weather:

- Network/API failure should not crash the app.
- If previous weather exists, UI should keep cached data and log cached use.
- Weather visuals should change with Open-Meteo weather codes instead of always showing the clear/sun state.
- Daily card should render Chinese text without missing glyph boxes or mojibake.

For calendar:

- Swipe left from the main page to Apps.
- Tap Calendar tile.
- Calendar page should open without starting XiaoZhi chat.
- Tap Next/Prev to change month.
- Tap Today to return to the synced current month.
- Back or right swipe should return to Apps.

For focus timer:

- Swipe left from the main page to Apps.
- Tap the `FOC / Focus / 25 min` tile.
- Focus page should open without starting XiaoZhi chat.
- Tap Start to begin/pause the countdown.
- Tap Reset to restore the selected mode duration.
- Tap focus/break controls to switch between 25 minute and 5 minute modes.
- Right swipe should return to Apps.

For settings:

- Open Settings and confirm the Brightness and Volume rows are visible.
- Drag each slider and release it; the displayed percentage and hardware output should change.
- Leave and reopen Settings; controls should reflect the current values.
- Reboot and confirm the saved brightness and volume are restored.

For photos:

- Put baseline JPG/JPEG files under `/photos` or `/PHOTOS` on a FAT-formatted MicroSD card.
- For best results, use 480x320 landscape JPG files with simple ASCII filenames such as `001.jpg`.
- Swipe left from the main page to Apps.
- Tap Photos tile.
- Logs should show `PhotoService` SD mount and photo scan results.
- Photos should loop full-screen with a soft fade between images and no visible text controls.
- Left or right swipe should pause the slideshow and return to Apps.

## Known Limitations

- Touch is still manually polled and dispatched; it is not yet a standard LVGL input device.
- Settings has a scoped manual-touch adapter, but new interactive LVGL widgets still need explicit handling until `lv_indev_t` migration.
- Display flush still rotates/copies into the physical panel path and can log slow flushes.
- Radio currently supports direct MP3 streams. HLS/AAC should be treated as future work.
- Weather provider failures such as 429/502 can still happen; current goal is graceful behavior, not guaranteed data.
- Daily-card festival/history data is currently a small built-in table, not a full calendar database.
- The daily-card Chinese font is an embedded subset; adding new Chinese text requires regenerating `qd_font_lxgw_16.c` and `qd_font_lxgw_20.c`.
- Focus Timer completed count is in-memory only and resets on reboot.
- Focus Timer has no alarm sound or persisted settings yet.
- Settings does not yet provide an on-device weather city editor, radio station editor, or startup preferences.
- Radio stations are still compiled into `radio_service.cc`.
- MCP tool descriptions must stay compact; large `tools/list` MQTT messages can exhaust AES/TLS memory and break XiaoZhi chat.
- Photo slideshow currently supports JPEG files only; PNG is not enabled.
- PhotoService depends on `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y`; retain this setting for the current PSRAM-backed task stack.
- FC/NES is still constrained by ESP32-S3 memory and Nofrendo compatibility, even though the current path is much more usable than the early minimal core.
- FC/NES can scan SD, show a list, load supported ROMs, display frames, and latch touch-controller input through the Nofrendo adapter.
- FC/NES audio output is routed through the ES8311 path as of v1.7.18. v1.7.19 reduces audio-buffer churn and suspends XiaoZhi protocol/weather work while the FC page is active, which keeps internal SRAM high enough for gameplay. Some ROMs still fail because they are larger than the 2 MB PSRAM load guard, use NES 2.0 headers, or require mappers not listed in `components/nofrendo/nes/mmclist.c`.
- FC/NES still has a known LIST/Stop edge case on some ROMs: a hardware crash was captured in Nofrendo `rom_free()` while freeing SRAM after gameplay. The attempted fix that moved SRAM/VRAM to large static PSRAM buffers was reverted because it caused worse gameplay freezes.
- FATFS long filename support is enabled in the QDTech board defaults; if an old `build-qdtech/sdkconfig` is reused, reconfigure or clean the build directory.
- The Photos page is intentionally controlled only by gestures after entry; there is no visible Back or Refresh button on that page.
- Photos has no hidden tap exit or refresh zone; use a horizontal swipe to leave it.
- QDTech releases should publish `full.bin`, `firmware.zip`, and standalone `app.bin` assets; the on-device updater consumes the `*-app.bin` asset from the latest GitHub Release.
- From `v1.7.28` onward, if the direct GitHub Release asset download fails, OTA tries proxy-prefixed download URLs for the same `browser_download_url`. Older firmware still needs a one-time USB flash to get this fallback.
