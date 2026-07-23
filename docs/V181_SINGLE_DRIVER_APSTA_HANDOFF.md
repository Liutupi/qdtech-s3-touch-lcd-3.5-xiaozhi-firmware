# v1.8.1 Single-Driver STA-to-APSTA Provisioning Handoff

Date: 2026-07-19

## Purpose and safety boundary

- Test whether the missing provisioning beacon is caused by stopping,
  deinitializing, and recreating the Wi-Fi driver between the early Station
  scan and SoftAP startup.
- Keep the experiment behind
  `CONFIG_QDTECH_EXPERIMENT_SINGLE_DRIVER_APSTA_PROVISIONING`, default `n`.
- Preserve the existing Wi-Fi driver and Station netif after the eight-second
  network-first timeout, unregister the Station handlers, and add the
  provisioning AP by changing the same running driver to APSTA mode.
- Write only the App partition at `0x100000`; preserve bootloader, partition
  table, OTA data, NVS, saved credentials, models, and every other partition.
- The preceding temporary-hotspot Station TX self-test was disabled. Its SSID,
  password, and self-test marker were absent from the flashed binary.

## Implementation

- `main/Kconfig.projbuild` adds the default-off single-driver APSTA experiment.
- `managed_components/78__esp-wifi-connect/include/wifi_station.h` and
  `wifi_station.cc` add an ownership-transfer handoff. The scan timer and
  Station event handlers are removed before disconnect/scan-stop, queues and
  connection state are cleared, and the running driver plus Station netif are
  retained.
- `managed_components/78__esp-wifi-connect/include/wifi_configuration_ap.h`
  and `wifi_configuration_ap.cc` accept the transferred Station netif, skip
  driver initialization/start, switch the running driver from STA to APSTA,
  and then configure the existing provisioning AP, DHCP, DNS, and web server.
- `main/boards/qdtech-s3-touch-lcd-3.5/network_first_boot.cc` selects the new
  handoff only under the experiment flag. The default-off path continues to
  stop/deinitialize Station and start the original pure SoftAP flow.
- `sdkconfig.single-driver-apsta.defaults` is the opt-in diagnostic build
  profile.
- No new task, task stack, timer, queue, or persistent allocation was added.
  The implementation reuses the one existing Wi-Fi driver task and the
  existing provisioning services.

## Build comparison

The control is the credential-free v1.8.1 receive-self-test App with the first
four provisioning diagnostic flags enabled and no Station TX self-test.

### Control

- App:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-wifi-rx-self-test-on.bin`
- Size: 6,742,880 bytes (`0x66e360`).
- SHA256:
  `40c26d5717761f106643db1144c035b66b396d5a0da9a7dee67eafe235b64e0d`.
- Static DIRAM: 171,999 / 341,760 bytes (50.33%).

### Single-driver APSTA enabled

- App:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-single-driver-apsta-on.bin`
- Size: 6,744,416 bytes (`0x66e960`).
- Delta from control: +1,536 bytes.
- Free 7 MB App slot: 595,616 bytes (`0x916a0`), 8%.
- SHA256:
  `db67778e07320d93b1fb4bb42eb1394cb44b57ad788a859e03b82355d8a66225`.
- Static DIRAM: 171,999 / 341,760 bytes (50.33%), unchanged.
- Total image size reported by the size tool: 6,744,295 bytes.
- No new static PSRAM reservation was introduced.

## App-only flash evidence

- The enabled App was written only at `0x100000` using DIO, 80 MHz, and 16 MB
  flash parameters.
- esptool wrote 6,744,416 bytes, verified the data hash, and hard-reset the
  board. No erase or write targeted NVS or any non-App partition.
- Boot confirmed App version `1.8.1`, compile time `Jul 19 2026 14:58:04`, and
  ESP-IDF 5.5.2.

## Hardware trace

- At 0.232 seconds network-first startup began with 222,111 bytes of free
  internal SRAM and one saved network available for scanning.
- Station received 17, 14, and 15 nearby APs before the timeout; the strongest
  signals were -26 to -27 dBm.
- At 8.362 seconds the saved network had not connected. The Station handoff
  retained the driver in mode 1 and transferred the existing Station netif.
- At 8.392 seconds the same driver switched directly to
  `sta + softAP`; there was no second Wi-Fi driver-task initialization.
- At 8.452 seconds the firmware reported `Xiaozhi-8E81` started. RF readback
  at 8.462 seconds showed APSTA mode 3, channel 1, 11b/g/n, HT20, country CN,
  transmit power 78, visible SSID, 100 ms beacon interval, driver NVS off, and
  `reused_driver=1`.
- DHCP, DNS, and the web server all started; minimal provisioning reported
  ready at 8.482 seconds.
- Free internal SRAM after AP startup was 178,711 bytes, largest block 102,400
  bytes, and free PSRAM 8,373,956 bytes. Alive logs remained stable through
  78.502 seconds with no panic, watchdog, reboot, or progressive memory loss.

## Independent visibility result

- Four independent Mac exact-name scans for `Xiaozhi-8E81` each returned
  `COUNT=0`.
- A full Mac scan found six nearby networks and returned `MATCHES=0` for both
  SSID `Xiaozhi-8E81` and BSSID `7c:e8:b1:b2:8e:81`.
- The AP therefore remained invisible over the air even though the firmware
  reported a healthy, live APSTA provisioning stack.

## Conclusion

- The single-driver STA-to-APSTA handoff works as designed, but it does not
  restore the missing provisioning beacon. The normal Station stop/deinit and
  SoftAP driver reinitialization transition is therefore ruled out as the
  cause.
- The result reinforces that startup memory is not the problem: the AP stayed
  live with about 178.7 KB free internal SRAM, and no memory decay occurred.
- Together with the successful Station receive/TX/authentication/DHCP/IP test,
  the remaining boundary is specifically SoftAP beacon/frame emission. Driver
  state and configuration readback alone are not proof that beacon frames
  reached the air.
- Do not promote the experiment to production. A next diagnostic requires a
  separately approved plan and should observe actual 802.11 AP transmissions
  or driver TX completion, rather than further tuning heap, startup ordering,
  or ordinary AP configuration.

## Current board state and rollback

- The board currently runs the credential-free single-driver APSTA diagnostic
  App. NVS and saved credentials were not modified.
- App-only rollback: write
  `xiaozhi-v1.8.1-wifi-rx-self-test-on.bin` at `0x100000` with DIO, 80 MHz, and
  16 MB parameters. Its SHA256 is
  `40c26d5717761f106643db1144c035b66b396d5a0da9a7dee67eafe235b64e0d`.
- No commit or push was made.
