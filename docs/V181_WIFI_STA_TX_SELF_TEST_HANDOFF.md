# v1.8.1 Temporary-Hotspot Station TX Self-Test

Date: 2026-07-19

## Purpose and safety boundary

- Verify the board's transmit, authentication, DHCP, and normal data paths by
  connecting Station mode to a user-provided 2.4 GHz WPA2 network.
- Keep the test behind
  `CONFIG_QDTECH_EXPERIMENT_WIFI_STA_TX_SELF_TEST`, default `n`.
- Add the test credential only to the in-RAM candidate list used by the
  existing startup scan. The credential is never passed to `SsidManager`,
  never added to the saved-network list, and never written to NVS.
- Write only the App partition at `0x100000`; preserve bootloader, partition
  table, OTA data, NVS, saved credentials, models, and every other partition.

## Implementation

- `main/Kconfig.projbuild` adds the default-off QDTech Station TX self-test and
  its build-time SSID/password fields.
- `managed_components/78__esp-wifi-connect/include/wifi_station.h` and
  `wifi_station.cc` add an experimental RAM-only temporary authentication
  candidate. The existing scan, association state machine, and DHCP path are
  reused without an extra task or scan.
- `main/boards/qdtech-s3-touch-lcd-3.5/network_first_boot.cc` arms the temporary
  candidate for a 15-second test window and clears it immediately after
  connection or timeout.
- `sdkconfig.wifi-sta-tx-self-test.defaults` is an opt-in build profile. Its
  reusable copy has a blank password so a real credential is not left in the
  working tree. The password was present only in the local test build and was
  never printed to the firmware log or copied into this handoff.
- No task, timer, queue, stack, persistent allocation, or NVS write was added.

## Build comparison

The control is the immediately preceding v1.8.1 receive-self-test App with the
first four provisioning diagnostics enabled and the Station TX test absent.
The test build uses the same ESP-IDF 5.5.2 baseline with the Station TX test
enabled.

### Control

- App:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-wifi-rx-self-test-on.bin`
- Size: 6,742,880 bytes (`0x66e360`).
- SHA256:
  `40c26d5717761f106643db1144c035b66b396d5a0da9a7dee67eafe235b64e0d`.
- Static DIRAM: 171,999 / 341,760 bytes (50.33%).

### Station TX self-test enabled

- App:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-wifi-sta-tx-self-test-on.bin`
- Size: 6,745,648 bytes (`0x66ee30`).
- Delta from control: +2,768 bytes.
- Free 7 MB App slot: 594,384 bytes (`0x911d0`), 8%.
- SHA256:
  `be12644fd94ba212c8e141d41fc54dccac459e125e25ff5bcdd43185ba85478a`.
- Static DIRAM: 172,047 / 341,760 bytes (50.34%), a 48-byte increase.
- Total image size reported by the size tool: 6,745,527 bytes.

## App-only flash evidence

- The enabled App was written only at `0x100000` using DIO, 80 MHz, and 16 MB
  flash parameters.
- esptool wrote 6,745,648 bytes, verified the data hash, and hard-reset the
  board. No erase or write targeted NVS or any non-App partition.
- Boot confirmed App version `1.8.1` and ESP-IDF 5.5.2.

## Hardware result

- The existing scan found the temporary target at RSSI -21 dBm on channel 6.
- At 3.522 seconds the Wi-Fi state entered authentication, at 3.532 seconds it
  entered association, and at 3.572 seconds it entered the running state.
- At 3.952 seconds the board reported a successful WPA2-PSK connection with
  RSSI -20 dBm.
- At 5.082 seconds DHCP completed with IP `192.168.1.103`, gateway
  `192.168.1.1`; the temporary candidate was then cleared with
  `nvs=unchanged`.
- Full application startup reused the early connection. A weather HTTP request
  returned status 200, MQTT connected successfully at about 30.2 seconds, and
  the application reached idle.

## Conclusion

- The ESP32-S3 Wi-Fi receive path, transmit path, RF power amplifier, antenna,
  WPA2 authentication, association, DHCP, IP traffic, TLS, and MQTT all work
  on this board in Station mode.
- The missing provisioning network is therefore not caused by insufficient
  startup memory or a general antenna/RF transmit hardware failure.
- Combined with the earlier correct pure-SoftAP configuration readback and
  independent `COUNT=0` scans, the remaining fault boundary is specific to
  SoftAP beacon transmission or the Station-stop/deinit-to-SoftAP driver
  transition.
- The next lowest-risk firmware experiment, if separately approved, is a
  default-off single-driver handoff: keep the already initialized Wi-Fi driver,
  switch from Station to APSTA/SoftAP without stop/deinit/reinit, and check
  whether `Xiaozhi-8E81` becomes visible. That test directly targets the last
  untested boundary and may provide the production fix.

## Current board state and rollback

- The board currently runs the Station TX self-test App and is connected using
  the build-time temporary credential. The credential is not in NVS, but it is
  necessarily embedded in this one test App image.
- Credential-free rollback is App-only: write
  `xiaozhi-v1.8.1-wifi-rx-self-test-on.bin` at `0x100000` with DIO, 80 MHz, and
  16 MB parameters. Its SHA256 is
  `40c26d5717761f106643db1144c035b66b396d5a0da9a7dee67eafe235b64e0d`.
- No commit or push was made.
