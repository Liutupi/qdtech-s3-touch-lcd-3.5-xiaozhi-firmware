# v1.8.2 QDTech Provisioning, Phone Web, and Chinese Apps Release Handoff

Date: 2026-07-19

## Scope

v1.8.2 promotes the hardware-verified QDTech first-provisioning repair to a
new OTA version. It also restores the normal full-application startup order,
starts the phone configuration page automatically after normal Wi-Fi
connection, and localizes every Apps-center entry to Chinese.

## First provisioning

- QDTech retains the proven production compatibility path only during first
  provisioning: channel 6, one Wi-Fi driver handed from Station to APSTA, and
  a standard provisioning Beacon submitted through the Station transmit queue.
- Native AP association, DHCP, DNS, and the XiaoZhi provisioning web page are
  unchanged.
- RX/TX self-tests, temporary credentials, raw-TX counters, and periodic
  provisioning diagnostics are disabled in the production profile.
- Previous hardware evidence: four independent scans found `Xiaozhi-8E81` on
  channel 6; a client associated, received `192.168.4.2`, and read the
  22,036-byte provisioning page with HTTP 200. A real phone then provisioned
  `MERCURY_A59F` successfully.

## Normal startup and phone web

- The production configuration no longer invokes network-first startup.
  Display, touch, BMI270, audio, UI, radio, photo, and application services
  start through the normal path before saved Wi-Fi is connected.
- If normal startup later needs first provisioning, that original entry point
  hands its active Station driver directly to the QDTech APSTA compatibility
  path instead of stopping/recreating it.
- After a normal Wi-Fi connection, `QdWifiConfigServer` starts automatically.
  The page is reachable at `http://<board-ip>/` and exposes Logo, Name, and
  Weather City. The manual Settings action remains available as a retry/status
  entry point.
- Hardware verification on the preceding identical functional App confirmed
  normal startup first, Wi-Fi address `192.168.1.102`, automatic web-server
  startup, and a real HTTP 200 response of 1,102 bytes containing Logo, Name,
  and Weather City. The GET request did not alter NVS.

## Chinese Apps center

- Apps-center title, navigation labels, all ten card icon/title/default-status
  strings, the More toggle, and the Shake Lab entry are Chinese.
- Common dynamic card states are localized while actual IP addresses, dates,
  percentages, and song titles remain unmodified.
- The existing broad-coverage Chinese font is reused. No font asset, task,
  queue, timer, or persistent runtime allocation was added.

## v1.8.2 build

- App: `xiaozhi-v1.8.2-normal-boot-phone-web-cn-apps.bin`
- App size: 6,745,792 bytes (`0x66eec0`); 594,240 bytes (8%) remain in the
  7 MB App partition.
- App SHA256:
  `1613f52e40b4c5452cf901dd20a5007559806dc7c8128580e47f7b149af32118`
- Full recovery image size: 7,794,368 bytes (`0x76eec0`).
- Full SHA256:
  `1d65b20428edcfb1b20d19471e628b33f11cfca11334877fa79559d0e0845d6a`
- Static DIRAM: 172,031 / 341,760 bytes (50.34%), leaving 169,729 bytes.
- No new task, task stack, queue, semaphore, or static PSRAM reservation.

## Flash and OTA boundary

- v1.8.2 verification writes only the App at `0x100000`; it does not erase or
  write NVS, OTA data, bootloader, partition table, model data, or any other
  partition.
- The GitHub Release must be Latest and include the exact OTA asset name
  `qdtech-s3-touch-lcd-3.5-v1.8.2-app.bin` and `SHA256SUMS.txt`. Existing
  boards on v1.8.1 can then detect and download v1.8.2 through Firmware
  Update.
- `full.bin` is recovery-only and must be flashed at offset `0x0`; it is not
  the OTA payload.

## Release verification boundary

`BUILD VERIFIED; FINAL v1.8.2 USB HARDWARE VERIFICATION PENDING`

The connected board was removed before the final v1.8.2 App-only write began.
The user explicitly chose to publish and complete final verification through
remote OTA. The Release therefore documents this boundary rather than claiming
an unobserved local v1.8.2 boot.
