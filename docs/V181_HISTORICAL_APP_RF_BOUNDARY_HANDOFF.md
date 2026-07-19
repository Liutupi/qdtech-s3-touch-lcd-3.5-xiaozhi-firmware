# v1.8.1 Historical-App RF Boundary Diagnostic

Date: 2026-07-19

## Purpose

- Determine whether the missing over-the-air SoftAP beacon is a v1.8.1 App
  regression or persists with a historical App that was known to expose a
  working provisioning hotspot on this same board.
- Preserve NVS, credentials, partition table, bootloader, OTA data, and model
  data. Only the App partition at `0x100000` was temporarily replaced and then
  restored.

## Historical control

- App:
  `releases/v1.7.61/qdtech-s3-touch-lcd-3.5-v1.7.61-app.bin`
- Size: 6,470,960 bytes.
- SHA256:
  `363a0b7ff8f790345636414e5019d563ab0ae8b04cdf244e04d15da850127591`.
- Repository handoff evidence from 2026-06-29 records this release on
  `/dev/cu.usbmodem212401`: fallback entered SoftAP, DHCP/web remained stable,
  and a client received `192.168.4.2`.
- The historical App uses ESP-IDF 5.5.2 and reports the same Wi-Fi firmware and
  PHY versions observed in the current App.

## Flash and boot evidence

- v1.7.61 was written only at `0x100000`; esptool verified the written hash.
- Boot log confirmed App version `1.7.61` and ESP-IDF 5.5.2.
- Station mode found no usable saved network and fell back after 30 seconds.
- At 31.612 seconds the Wi-Fi driver reported pure SoftAP mode for BSSID
  `7c:e8:b1:b2:8e:81`.
- DHCP started at 31.622 seconds, `Xiaozhi-8E81` was announced at 31.632
  seconds, and the web server started at 31.642 seconds.
- The original full-application path had only 3,263 bytes of free internal
  SRAM immediately after SoftAP startup. It remained running beyond 71
  seconds without panic, watchdog, or reboot; minimum internal SRAM reached
  2,583 bytes.

## RF scan evidence

- While v1.7.61 SoftAP was active, Mac CoreWLAN exact-name scan result:
  `COUNT=0`.
- A separate full scan result was `TOTAL=6 MATCHES=0`, matching neither SSID
  `Xiaozhi-8E81` nor BSSID `7c:e8:b1:b2:8e:81`.
- This reproduces the current v1.8.1 symptom with a historical App that was
  previously known to be visible and accept a client.

## Restoration

- Restored App:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-softap-rf-on.bin`.
- Size: 6,742,624 bytes.
- SHA256:
  `700903aae018332a5ec58bc54556f00921bdce95ba0f49a2c12e12649b146fe6`.
- It was written only at `0x100000`; esptool verified the written hash.
- Boot log reconfirmed App version `1.8.1`, network-first preflight, 8-second
  fallback, stateless Wi-Fi driver startup, and RF readback:
  `mode=2 channel=1 secondary=0 protocol=0x07 bandwidth=1 country=CN`
  `start=1 count=13 tx_power=78 hidden=0 beacon=100 nvs=off`.
- Minimal provisioning again retained 179,771 bytes of free internal SRAM and
  a 106,496-byte largest block.

## Conclusion and next boundary

- The missing beacon is not specific to the v1.8.1 App code, fast fallback,
  network-first startup, SoftAP memory availability, driver-NVS enable state,
  or explicit channel/protocol/bandwidth/country/beacon settings.
- Because only App images were exchanged, this test intentionally does not
  rule out persistent PHY calibration/NVS state, non-App flash contents, or
  RF/antenna hardware failure.
- Before changing persistent data, remove USB power completely for at least
  ten seconds and reconnect the board. A reset caused by flashing is not a
  true analog/RF power cycle.
- If the beacon remains absent after a power cycle, the next lowest-risk
  firmware diagnostic is a default-off early Station scan self-test that logs
  total nearby AP count, channels, and strongest RSSI before SoftAP starts.
  This distinguishes a general receive/antenna failure from a transmit-only
  beacon failure.
- PHY full calibration, PHY NVS deletion, NVS repair, or a full-image flash
  must have separate explicit authorization.

## True power-cycle follow-up

- USB power was physically removed for more than ten seconds and reconnected.
- On the cold boot, ESP-IDF logged:
  `Saving new calibration data due to checksum failure or outdated calibration data, mode(0)`.
  This automatic framework action refreshed stored PHY calibration data; no
  manual NVS erase or calibration command was issued.
- v1.8.1 then entered minimal SoftAP at 8.572 seconds, announced
  `Xiaozhi-8E81` at 8.592 seconds, and started the web server at 8.622 seconds.
- RF readback remained channel 1, HT20, 11b/g/n, CN channels 1-13, 19.5 dBm,
  visible SSID, 100 ms beacon, and Wi-Fi driver NVS off.
- Mac exact-name scan remained `COUNT=0`. A full scan saw four nearby networks
  and returned `MATCHES=0` for the target SSID/BSSID.
- SoftAP remained alive through 48.6 seconds with 179,759 bytes of free
  internal SRAM and a minimum of 178,787 bytes, without panic or reboot.
- A true RF/analog power cycle plus automatic mode-0 PHY recalibration did not
  restore the over-the-air beacon. The next non-destructive boundary remains
  an aggregate Station receive scan self-test before any explicit persistent
  calibration or full-image operation.
