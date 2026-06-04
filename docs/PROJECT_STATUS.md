# QDTech Project Status

## Snapshot

Date: 2026-06-04

This fork currently builds and runs as the QDTech ESP32-S3 3.5 inch landscape XiaoZhi firmware. It should be treated as a working firmware base, not an experimental scratch tree.

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
- Desktop UI pages:
  - main page
  - apps page
  - photo page
  - calendar page
  - XiaoZhi page
  - radio page
  - settings page
- Calendar month view with Today, Prev, Next, today highlight, weekend coloring, and previous/next month filler days.
- Photo slideshow page from MicroSD card:
  - Apps tile `Photos` replaces the previous Weather app tile.
  - Reads JPG/JPEG files from `/sdcard/photos`.
  - Uses SDMMC 4-bit bus from the product specification.
  - Cross-fades between decoded photos.
  - Pauses decoding when leaving the Photos page.
- Time display through SNTP.
- Weather fetch with cached last successful data.
- Weather location MCP tool: `self.weather.set_location`.
- MP3 network radio with built-in station list.
- Radio MCP tools:
  - `self.radio.get_status`
  - `self.radio.play`
  - `self.radio.stop`
  - `self.radio.next`
  - `self.radio.previous`
- Radio and XiaoZhi audio avoidance through lightweight audio focus.

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

For radio/audio focus:

- Start radio.
- Trigger XiaoZhi listening or speaking.
- Radio should log audio focus yield/pause behavior.
- When XiaoZhi returns idle, radio should resume if the user had requested playback.

For weather:

- Network/API failure should not crash the app.
- If previous weather exists, UI should keep cached data and log cached use.

For calendar:

- Swipe left from the main page to Apps.
- Tap Calendar tile.
- Calendar page should open without starting XiaoZhi chat.
- Tap Next/Prev to change month.
- Tap Today to return to the synced current month.
- Back or right swipe should return to Apps.

For photos:

- Put JPG files under `/photos` on a FAT-formatted MicroSD card.
- Swipe left from the main page to Apps.
- Tap Photos tile.
- Logs should show `PhotoService` SD mount and photo scan results.
- Photos should loop with a soft fade between images.
- Back or right swipe should pause the slideshow and return to Apps.

## Known Limitations

- Touch is still manually polled and dispatched; it is not yet a standard LVGL input device.
- Display flush still rotates/copies into the physical panel path and can log slow flushes.
- Radio currently supports direct MP3 streams. HLS/AAC should be treated as future work.
- Weather provider failures such as 429/502 can still happen; current goal is graceful behavior, not guaranteed data.
- Settings UI is not yet a full configuration center.
- Radio stations are still compiled into `radio_service.cc`.
- Photo slideshow currently supports JPEG files only; PNG is not enabled.
- FATFS long filename support is enabled in the QDTech board defaults; if an old `build-qdtech/sdkconfig` is reused, reconfigure or clean the build directory.
- No release packaging or OTA artifact process is defined in this handoff set.
