# v1.8.1 Stateless SoftAP RF Hardening

Date: 2026-07-19

## Scope and diagnosis

- Source baseline: Git tag `v1.8.1`, commit
  `1163242936af3d1384553a156d42dcb5531f86c7`.
- The earlier network-first experiment starts provisioning before display,
  audio, touch, sensors, and application services. It raised free internal
  SRAM after SoftAP startup from 4,959 bytes to about 179.7 KB, but the phone
  still could not discover `Xiaozhi-8E81`.
- A Mac CoreWLAN exact-name scan also returned `COUNT=0`, confirming that this
  is not only a phone UI or refresh issue.
- The SoftAP driver previously used `WIFI_INIT_CONFIG_DEFAULT()`, including
  Wi-Fi driver NVS. Station preflight already disabled driver NVS. Repeated
  App-only flashing preserves NVS, so this experiment tested whether retained
  driver state or implicit RF defaults were suppressing the beacon without
  erasing user credentials.

## Implementation

- Added default-off
  `CONFIG_QDTECH_EXPERIMENT_STATELESS_SOFTAP_RF`.
- When enabled, the existing SoftAP component initializes the Wi-Fi driver with
  driver NVS disabled. Application-level saved credentials remain untouched.
- Before starting SoftAP it explicitly applies:
  - visible SSID;
  - channel 1;
  - 20 MHz bandwidth;
  - 802.11b/g/n protocols;
  - manual CN country policy with channels 1-13;
  - 100 ms beacon interval.
- After startup it reads back and logs mode, channel, secondary channel,
  protocol bitmap, bandwidth, country, transmit power, hidden flag, beacon
  interval, and driver-NVS state.
- No NVS erase, credential change, AP+STA mode, partition change, hardware pin
  change, or PHY calibration was introduced.

## Changed runtime files

- `main/Kconfig.projbuild`
- `managed_components/78__esp-wifi-connect/wifi_configuration_ap.cc`
- `sdkconfig.softap-rf.defaults`

The App used for hardware testing also includes the previously approved
fast-fallback and network-first experimental changes documented in
`docs/V181_FAST_PROVISIONING_HANDOFF.md` and
`docs/V181_NETWORK_FIRST_BOOT_HANDOFF.md`.

## Build and resource impact

- ESP-IDF 5.5.2 experiment-off control build, link, size, and merge: passed.
- ESP-IDF 5.5.2 experiment-on build, link, size, and merge: passed.
- Stateless-RF off App: `0x66cd90` / 6,737,296 bytes.
- Stateless-RF on App: `0x66e260` / 6,742,624 bytes.
- Enabled delta: `+0x14d0` / +5,328 bytes.
- 7 MB App slot free when enabled: `0x91da0` / 597,408 bytes (8%).
- Static DIRAM is unchanged at 171,999 / 341,760 bytes (50.33%).
- New tasks and task stacks: none. The change only alters synchronous Wi-Fi
  setup and diagnostic logging in the existing provisioning path.
- Persistent internal-SRAM and PSRAM allocations added by this change: none.

## Build artifacts

- Stateless-RF off App rollback:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-softap-rf-off.bin`
  - SHA256:
    `82501a8234ddf085cd3119c915281349cf36cb8f88055e68202525e55d911365`
- Stateless-RF experiment App:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-softap-rf-on.bin`
  - SHA256:
    `700903aae018332a5ec58bc54556f00921bdce95ba0f49a2c12e12649b146fe6`

## Hardware verification

- The experiment App was flashed only at `0x100000`; esptool verified the
  written hash. Bootloader, partition table, OTA data, model data, and NVS were
  not written.
- Boot log confirmed App version `1.8.1` and ESP-IDF 5.5.2.
- Saved-network preflight timed out after 8 seconds. SoftAP mode began at
  8.482 seconds, the AP announcement appeared at 8.502 seconds, and the web
  server was ready at 8.532 seconds.
- Driver log confirmed `config NVS flash: disabled`.
- RF readback was:
  `mode=2 channel=1 secondary=0 protocol=0x07 bandwidth=1 country=CN`
  `start=1 count=13 tx_power=78 hidden=0 beacon=100 nvs=off`.
  Transmit power 78 is 19.5 dBm in ESP-IDF quarter-dBm units.
- After SoftAP startup, internal free memory was 179,771 bytes, largest free
  block was 106,496 bytes, and minimum free was 179,747 bytes. At 38.5
  seconds, free memory remained 179,771 bytes and minimum free was 178,783
  bytes.
- No panic, abort, watchdog, or reboot loop appeared.
- Despite correct RF readback, the Mac exact-name scan returned `COUNT=0`.
  A separate full scan found six nearby networks and zero matches for SSID
  `Xiaozhi-8E81` or BSSID `7c:e8:b1:b2:8e:81`. The phone also still did not
  display the AP.

## Result and next diagnostic boundary

- This experiment did not restore the over-the-air SoftAP beacon and remains
  default-off.
- Startup memory starvation is ruled out for the minimal provisioning path.
- Retained Wi-Fi driver NVS state and implicit channel/protocol/bandwidth/
  country/beacon settings are also ruled out by this test.
- The subsequent App-only historical comparison is documented in
  `docs/V181_HISTORICAL_APP_RF_BOUNDARY_HANDOFF.md`. v1.7.61 also failed to
  expose a beacon, despite having previously assigned a SoftAP client on this
  same board. The remaining boundary is persistent PHY state, RF/antenna
  hardware, or a condition that requires a true power cycle. PHY full
  calibration or any NVS repair must not be attempted without explicit
  authorization because it changes persistent device state.

## Rollback

- Disable `CONFIG_QDTECH_EXPERIMENT_STATELESS_SOFTAP_RF` and rebuild to restore
  the prior SoftAP initialization path.
- Immediate hardware rollback while preserving all NVS data: App-only flash
  `xiaozhi-v1.8.1-softap-rf-off.bin` at `0x100000` with DIO mode, 80 MHz flash
  frequency, and 16 MB flash size. Do not erase flash.
