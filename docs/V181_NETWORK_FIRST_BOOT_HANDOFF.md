# v1.8.1 Network-First Provisioning Boot

Date: 2026-07-19

## Scope and diagnosis

- Source baseline: Git tag `v1.8.1`, commit
  `1163242936af3d1384553a156d42dcb5531f86c7`.
- The first fallback optimization reduced SoftAP readiness from about 32.3
  seconds to 10.4 seconds, but the phone still could not discover the SSID.
- A second hardware trace found only 4,959 bytes of free internal SRAM after
  the original full-application SoftAP startup, with a minimum of 4,619 bytes.
- Before networking, v1.8.1 constructed the display, touch, BMI270, MCP tools,
  Radio, Podcast, Photos, FC, firmware update, battery, audio codec, and audio
  task. The SoftAP parameters themselves were already appropriate: pure 2.4
  GHz AP, channel 1, open authentication, visible SSID, power saving disabled,
  and maximum transmit power.

## Implementation

- Added default-off
  `CONFIG_QDTECH_EXPERIMENT_NETWORK_FIRST_BOOT`.
- When enabled, `app_main` runs a QDTech-only network preflight before
  constructing `Application` or the QDTech board.
- With no saved network, or with `force_ap`, the existing pure-SoftAP web
  provisioning component starts immediately in the minimal boot state.
- With saved networks, the preflight gives Station mode 8 seconds to connect.
  A successful connection is handed to the unchanged full application startup;
  an unavailable saved network is stopped before pure SoftAP starts.
- During minimal provisioning, LCD, touch, sensors, audio, media, weather, UI,
  and application services are not constructed. After credentials are saved
  and the board reboots, a successful saved-network connection allows the
  complete v1.8.1 application to start normally.
- No NVS clearing, AP+STA background mode, partition change, pin change, or
  Wi-Fi driver modification was introduced.

## Changed runtime files

- `main/Kconfig.projbuild`
- `main/main.cc`
- `main/boards/common/wifi_board.cc`
- `main/boards/common/wifi_board.h`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/network_first_boot.h`
- `main/boards/qdtech-s3-touch-lcd-3.5/network_first_boot.cc`
- `sdkconfig.fast-provisioning.defaults`
- `sdkconfig.network-first.defaults`

## Build and resource impact

- ESP-IDF 5.5.2 default-off control build, link, size, and merge: passed.
- ESP-IDF 5.5.2 experiment-on build, link, size, and merge: passed.
- Network-first off App: `0x66c0c0` / 6,734,016 bytes.
- Network-first on App: `0x66cd90` / 6,737,296 bytes.
- Enabled delta: `+0xcd0` / +3,280 bytes.
- 7 MB App slot free: `0x93270` / 602,736 bytes (8%).
- Static DIRAM is unchanged at 171,999 / 341,760 bytes (50.33%).
- New tasks and task stacks: none. Minimal provisioning runs synchronously
  from the existing main task and uses the existing Wi-Fi/DNS/HTTP component
  tasks.
- Persistent internal SRAM/PSRAM allocations added by this code: none.

## Build artifacts

- Default-off App rollback:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-network-first-off.bin`
  - SHA256:
    `4cc51cc73bbc374482eec33cd73c3bfe819eb1ce04113b249efd536789ee4f22`
- Network-first experiment App:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-network-first-on.bin`
  - SHA256:
    `c2b60ac735f4eea5c6239bf30fb2a2cf21b60d1c1646bb00ad430877e995dbbc`

## Hardware verification

- The experiment App was flashed only at `0x100000`; esptool verified the
  written hash. Bootloader, partition table, OTA data, model data, and NVS were
  not written.
- Boot log confirmed App version `1.8.1` and
  `Experimental network-first boot enabled`.
- Preflight began at 0.232 seconds with 222,111 bytes free internal SRAM and a
  131,072-byte largest block.
- One unavailable saved network was tested for 8 seconds. Pure SoftAP started
  at 8.482 seconds, `Xiaozhi-8E81` was announced at 8.502 seconds, and the
  provisioning web server was ready at 8.522 seconds.
- After SoftAP and web startup, free internal SRAM was 179,683 bytes, the
  largest block was 106,496 bytes, and minimum free internal SRAM was 179,659
  bytes. At 28.5 seconds it remained stable at 179,683 bytes free with a
  178,695-byte minimum.
- Compared with the full-application provisioning path, SoftAP free internal
  SRAM increased from 4,959 to 179,683 bytes.
- No panic, abort, watchdog, or reboot loop appeared in the observed window.
- Subsequent phone testing still could not discover `Xiaozhi-8E81`. A Mac
  CoreWLAN scan also found no target beacon, so the network-first memory fix
  alone does not solve RF discovery. See
  `docs/V181_STATELESS_SOFTAP_RF_HANDOFF.md` for the follow-up experiment.

## Expected provisioning behavior

- The LCD can remain blank while minimal provisioning owns startup resources.
- Firmware starts `Xiaozhi-8E81` about 8.5 seconds after boot when existing
  saved networks are unavailable, but current hardware testing has not found
  its beacon over the air.
- After successful credential submission and reboot, the board first connects
  to the saved network and only then initializes the full display, audio, UI,
  sensor, and media stack.

## Rollback

- Disable `CONFIG_QDTECH_EXPERIMENT_NETWORK_FIRST_BOOT` and rebuild to restore
  the prior full-application startup path.
- Immediate hardware rollback while preserving NVS: App-only flash
  `xiaozhi-v1.8.1-network-first-off.bin` at `0x100000` with DIO mode,
  80 MHz flash frequency, and 16 MB flash size. Do not erase flash.
