# v1.8.1 SoftAP Channel 1-to-6 RF A/B Handoff

Date: 2026-07-19

## Scope and safety boundary

- The existing channel-1 raw-beacon App is the A control.
- The B build adds default-off
  `CONFIG_QDTECH_EXPERIMENT_SOFTAP_CHANNEL_6_AB`. When enabled, only the
  QDTech provisioning SoftAP channel changes from 1 to 6; the raw beacon
  follows that configured channel.
- SSID, BSSID, open authentication, DHCP, DNS, web provisioning, startup
  order, Wi-Fi driver lifetime, transmit power, country, and all persistence
  behavior are unchanged.
- Only the App partition at `0x100000` was flashed. NVS and every other
  partition were untouched.

## Changed files

- `main/Kconfig.projbuild`
- `managed_components/78__esp-wifi-connect/wifi_configuration_ap.cc`
- `sdkconfig.softap-channel6-ab.defaults`

The new option remains default `n`. It adds no task, timer, queue, stack, or
persistent allocation.

## Build and resource comparison

### A: channel 1

- App:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-raw-beacon-sta-interface-on.bin`
- size: 6,749,392 bytes (`0x66fcd0`)
- SHA256: `7564425b47b1afd14479f11acb6cadfba0ad23e54bc5bfe34f9c8f0d57decdef`
- external result: repeated scans found no target network.

### B: channel 6

- App:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-softap-channel6-ab-on.bin`
- size: 6,749,392 bytes (`0x66fcd0`)
- delta from A: 0 bytes
- SHA256: `18307a8bf9b71d938a1f9720b9130b655f5ff691743dfdea9fe3f766d793b43d`
- total image size: 6,749,271 bytes
- static DIRAM: 172,055 / 341,760 bytes (50.34%), unchanged from A
- free 7 MB App slot: 590,640 bytes (`0x90330`), 8%

The B binary contains no temporary hotspot SSID, password, or Station TX
self-test marker.

## Hardware evidence

- App-only flash completed and esptool verified the written data hash.
- Boot confirmed App version `1.8.1`, ESP-IDF 5.5.2, and compile time
  `Jul 19 2026 16:04:55`.
- Three Station receive scans found 11, 11, and 13 nearby APs. The strongest
  AP was on channel 6 at -36, -25, and -30 dBm.
- At 8.462 seconds the raw fallback started with SSID `Xiaozhi-8E81`, BSSID
  `7c:e8:b1:b2:8e:81`, and channel 6.
- RF readback at 8.472 seconds confirmed APSTA mode 3, channel 6, HT20,
  11b/g/n, CN channels 1-13, power 78, visible SSID, 100 ms beacon interval,
  driver NVS off, and reused driver.
- DHCP, DNS, and the provisioning web server started normally. Free internal
  SRAM after startup was 178,523 bytes with a 102,400-byte largest block;
  free PSRAM was 8,373,956 bytes.

## Visibility and connection result

- The first full Mac scan found one `Xiaozhi-8E81` at -26 dBm on channel 6.
  Three immediate repeat full scans each again found exactly one target at
  -26 dBm on channel 6.
- This is a strict contrast with channel 1, where the same computer and board
  repeatedly returned zero target SSID/BSSID matches.
- The board log recorded the Mac client joining, then DHCP assigning
  `192.168.4.2`.
- An HTTP request to `http://192.168.4.1/` returned `200 OK`,
  `Content-Length: 22036`, and the expected Wi-Fi configuration page.
- The client later left normally. The board remained stable through 3,800 raw
  beacon attempts with no API error, TX-done failure, panic, reboot, or memory
  leak.
- The Mac automatically returned to its prior WPA3 network with DHCP address
  `192.168.88.149`, gateway `192.168.88.1`, and successful gateway reachability.

## Conclusion

Channel selection is the first variable to produce an externally visible and
usable provisioning network. On this hardware and in this environment,
channel 1 is not externally discoverable while channel 6 is strong, repeatably
discoverable, joinable, assigns DHCP, and serves the provisioning page.

The approved A/B build still includes the raw-beacon experiment, so the next
production-candidate step must separately verify channel 6 with the raw beacon
disabled. If normal SoftAP alone remains visible and joinable, promote the
QDTech provisioning channel change while leaving all diagnostic raw-beacon,
RX logging, APSTA handoff, and other experiments default-off.

## Current board state and rollback

- The board currently runs the channel-6 A/B App, flashed App-only. NVS is
  unchanged.
- App-only A rollback: flash
  `xiaozhi-v1.8.1-raw-beacon-sta-interface-on.bin` at `0x100000`.
- Conservative credential-free rollback: flash
  `xiaozhi-v1.8.1-wifi-rx-self-test-on.bin` at `0x100000`; SHA256
  `40c26d5717761f106643db1144c035b66b396d5a0da9a7dee67eafe235b64e0d`.
- Do not erase flash and do not write NVS.

No commit or push was made.
