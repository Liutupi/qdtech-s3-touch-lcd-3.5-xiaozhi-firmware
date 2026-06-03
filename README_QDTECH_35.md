# QDTech 3.5 XiaoZhi Firmware

This repository contains the working ESP32-S3 + QDTech 3.5 inch LCD firmware state.

## Hardware

- ESP32-S3 with 16 MB flash and PSRAM
- QDTech 3.5 inch 480x320 SPI LCD
- ST77922 LCD controller
- CST9217 touch controller

## Current Features

- Landscape LVGL desktop UI
- Zhongshan time and weather sync
- Configurable weather location through `self.weather.set_location`
- XiaoZhi AI app card, Talk/Menu/Back touch handling
- Network radio page with MP3 stream playback
- Radio MCP tools:
  - `self.radio.get_status`
  - `self.radio.play`
  - `self.radio.stop`
  - `self.radio.next`
  - `self.radio.previous`

## Rebuild From A Fresh Clone

Use ESP-IDF 5.5 or newer.

```bash
git clone https://github.com/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware.git
cd qdtech-s3-touch-lcd-3.5-xiaozhi-firmware
source /Users/tupi/esp/esp-idf-v5.5/export.sh
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem212401 flash monitor
```

If the serial port changes, replace `/dev/cu.usbmodem212401` with the detected port.

## Notes

- `build/`, `managed_components/`, `sdkconfig`, and generated language headers are intentionally not committed.
- `dependencies.lock` is committed to keep ESP-IDF component versions reproducible.
- Wi-Fi credentials and XiaoZhi activation state are stored on the device NVS after provisioning, not in this repository.
