# v1.8.1 Bidirectional Wi-Fi RF Receive Self-Test

Date: 2026-07-19

## Purpose and safety boundary

- Distinguish a general Wi-Fi receive/antenna failure from a transmit-only
  SoftAP beacon failure without changing credentials, PHY calibration, NVS,
  or normal provisioning behavior.
- Add only aggregate logging to the Station scans that already run during the
  approved network-first preflight: nearby AP count plus the strongest RSSI
  and channel. No SSID or BSSID is logged, and no additional scan is started.
- Keep the diagnostic behind
  `CONFIG_QDTECH_EXPERIMENT_WIFI_RX_SELF_TEST`, default `n`.
- The hardware test wrote only the App partition at `0x100000`. Bootloader,
  partition table, OTA data, NVS, credentials, models, and every other
  partition were preserved.

## Implementation

- `main/Kconfig.projbuild` adds the QDTech-only, default-off diagnostic flag.
- `managed_components/78__esp-wifi-connect/wifi_station.cc` logs only the
  aggregate result after an existing scan is sorted.
- `sdkconfig.wifi-rx-self-test.defaults` enables the receive diagnostic along
  with the three previously approved provisioning experiments for this
  controlled build.
- No task, timer, queue, stack, persistent allocation, or extra Wi-Fi scan was
  added.

## A/B build evidence

Both builds completed link, App-size, and merged-image generation checks with
ESP-IDF 5.5.2.

### Diagnostic off control

- App:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-wifi-rx-self-test-off.bin`
- Size: 6,742,624 bytes (`0x66e260`).
- Free 7 MB App slot: 597,408 bytes (`0x91da0`), 8%.
- SHA256:
  `bbe479330c3acefc2be56b7995844904e0f3fa5482eef5117be99e2a7be629cc`.

### Diagnostic on

- App:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-wifi-rx-self-test-on.bin`
- Size: 6,742,880 bytes (`0x66e360`).
- Delta from control: +256 bytes.
- Free 7 MB App slot: 597,152 bytes (`0x91ca0`), 8%.
- SHA256:
  `40c26d5717761f106643db1144c035b66b396d5a0da9a7dee67eafe235b64e0d`.
- Static DIRAM was unchanged at 171,999 / 341,760 bytes (50.33%). No static
  SRAM or PSRAM increase was reported by the size comparison.

## App-only flash and boot evidence

- The diagnostic-on App was written only at `0x100000` using DIO, 80 MHz,
  16 MB flash parameters; esptool verified the written hash and hard-reset the
  board.
- Boot confirmed App version `1.8.1` and ESP-IDF 5.5.2.
- The existing Station preflight produced three consecutive aggregate results:
  - 2.772 s: 11 nearby APs, strongest RSSI -29 dBm, channel 6.
  - 5.182 s: 15 nearby APs, strongest RSSI -29 dBm, channel 6.
  - 7.602 s: 14 nearby APs, strongest RSSI -29 dBm, channel 6.
- The saved network was unavailable, so the firmware stopped Station mode and
  started minimal pure SoftAP at about 8.5 seconds.
- SoftAP readback remained correct: channel 1, HT20, 11b/g/n, CN channels
  1-13, 19.5 dBm, visible SSID, 100 ms beacon interval, and Wi-Fi driver NVS
  off.
- Minimal provisioning retained 179,771 bytes of free internal SRAM with a
  106,496-byte largest block. It stayed alive beyond 78 seconds without a
  panic, watchdog, reboot, or material heap decline.

## Independent receiver evidence

- While firmware reported the SoftAP alive, Mac CoreWLAN exact-name scan for
  `Xiaozhi-8E81` returned `COUNT=0`.
- A separate full scan saw four nearby networks and returned
  `TOTAL=4 MATCHES=0` for SSID `Xiaozhi-8E81` or BSSID
  `7c:e8:b1:b2:8e:81`.

## Conclusion

- The board's 2.4 GHz receive path is working: it repeatedly receives many
  nearby APs at strong signal levels. This rules out startup memory starvation
  and makes a complete antenna/RF receive-chain failure unlikely.
- At the same time, an independent receiver still sees no frame identifying
  the board's SoftAP, despite correct driver state and stable memory. The fault
  boundary is therefore the board's Wi-Fi transmit/beacon path, not first-time
  provisioning UI timing or resource allocation.
- This diagnostic identifies the boundary but does not repair it. Do not
  promote any experiment flag to a normal default yet.
- A next firmware experiment, if separately approved, should directly verify
  802.11 transmit completion or emit a controlled raw test frame. If that also
  reports successful transmission with no over-the-air frame, inspect the RF
  transmit/PA/antenna-switch hardware before changing provisioning logic.

## Current board state and rollback

- The board currently runs the diagnostic-on v1.8.1 App listed above.
- Exact rollback is App-only: write
  `xiaozhi-v1.8.1-wifi-rx-self-test-off.bin` at `0x100000` using DIO, 80 MHz,
  and 16 MB flash parameters. No erase and no NVS operation are required.
- No commit or push was made for this experiment.
