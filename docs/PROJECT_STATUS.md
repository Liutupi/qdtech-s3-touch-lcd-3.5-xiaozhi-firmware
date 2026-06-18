# QDTech Project Status

## Snapshot

Date: 2026-06-18

This fork currently builds and runs as the QDTech ESP32-S3 3.5 inch landscape XiaoZhi firmware. It should be treated as a working firmware base, not an experimental scratch tree.

## Version History

- v1.0 (2026-06-14): Stable release with optimized time/weather sync
- v1.1 (2026-06-14): Visual animations and further sync optimization
- v1.2 (2026-06-15): P0-P6 optimization, LVGL touch migration, font fix, NVS persistence
- v1.3 (2026-06-15): Radio visual enhancement with audio spectrum bars

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
- Unified navigation hierarchy:
  - Main left swipe opens Apps.
  - Apps right swipe returns to Main.
  - Feature pages right swipe return to Apps.
  - Photos exits to Apps with either horizontal swipe.
- Desktop UI pages:
  - main page
  - apps page
  - photo page
  - FC/NES page
  - calendar page
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
  - Exits the photo page with either left or right swipe back to Apps.
  - Cross-fades between decoded photos.
  - Photo task lazy-starts only when opening Photos.
  - Photo task stack is allocated from PSRAM first, with an internal-memory fallback and allocation diagnostics.
- FC/NES page from MicroSD card:
  - Apps tile `FC / NES / SD ROMs` opens a ROM list first.
  - Reads `.nes` files from `/sdcard/FC`, `/sdcard/nes`, `/sdcard/roms`, then `/sdcard`; scanning stops after the first populated directory and caps the list at 64 ROMs.
  - FATFS is configured for Chinese CP936 filenames with UTF-8 API output. Numeric ROM sets can add `roms.txt`, `roms.csv`, or `games.txt` in the active ROM directory to map filenames to friendly display names.
  - Prev/Next move the selected ROM; Start loads the selected ROM.
  - Game view uses a top 320x240 scaled NES screen area and a bottom virtual controller.
  - Game view has D-pad, A, B, Select, Start, and LIST controls.
  - Nofrendo is the active emulator path; see `docs/HANDOFF.md` and `docs/CHANGELOG_QDTECH.md` for current hardware-test evidence and remaining risks.
- Time display through SNTP.
- Weather fetch with cached last successful data.
- Main-page weather visuals map current weather codes to clear, cloudy, rain, snow, and storm states.
- Main-page daily card uses embedded LXGW WenKai subset fonts and rotates date-linked local content:
  - fixed Gregorian festival first
  - history-on-this-day second
  - daily quote fallback third
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
- Weather location MCP tool: `self.weather.set_location`.
- MP3 network radio with built-in station list.
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
- FC/NES is an unfinished integration milestone, not a usable emulator yet.
- FC/NES can scan SD, show a list, load supported ROMs, display frames, and latch touch-controller input at the NES bus, but tested games still show little or no visible gameplay response.
- FC/NES currently has no audio/APU and supports only Mapper 0/2.
- FATFS long filename support is enabled in the QDTech board defaults; if an old `build-qdtech/sdkconfig` is reused, reconfigure or clean the build directory.
- The Photos page is intentionally controlled only by gestures after entry; there is no visible Back or Refresh button on that page.
- Photos has no hidden tap exit or refresh zone; use a horizontal swipe to leave it.
- No release packaging or OTA artifact process is defined in this handoff set.
