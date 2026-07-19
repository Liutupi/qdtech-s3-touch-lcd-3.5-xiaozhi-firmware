# v1.8.1 First-Provisioning Fast Fallback

Date: 2026-07-19

Follow-up: the phone still could not discover the SSID after this timing-only
change. Hardware then showed only 4,959 bytes of free internal SRAM in the
full-application SoftAP path. The superseding network-first implementation and
current flashed artifact are documented in
`docs/V181_NETWORK_FIRST_BOOT_HANDOFF.md`.

## Scope and root cause

- Source baseline: Git tag `v1.8.1`, commit
  `1163242936af3d1384553a156d42dcb5531f86c7`.
- Hardware diagnosis on `/dev/cu.usbmodem212401` showed the saved-network
  station path waiting the full fixed 30 seconds before starting provisioning.
  The original App started pure SoftAP at about 32.3 seconds after boot; the
  SoftAP itself needed only about 120 ms to start.
- The optimization changes only the QDTech startup wait from 30 seconds to
  8 seconds when
  `CONFIG_QDTECH_EXPERIMENT_FAST_PROVISIONING_FALLBACK=y`.
- The switch is QDTech-only and default-off. With it disabled, the common
  30-second behavior remains unchanged.
- The existing sequential station-then-pure-SoftAP path is preserved. No
  AP+STA concurrency, NVS clearing, partition change, pin change, or Wi-Fi
  driver change was introduced.

## Changed files

- `main/Kconfig.projbuild`
- `main/boards/common/wifi_board.h`
- `main/boards/common/wifi_board.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`
- `sdkconfig.fast-provisioning.defaults`
- `PATCHES.md`
- `docs/NEXT_TASKS.md`
- `docs/V181_FAST_PROVISIONING_HANDOFF.md`

## Build and resource impact

- Build profile retained the v1.8.1 Zodiac defaults and used ESP-IDF 5.5.2.
- Default-off build, link, size check, and merged image: passed.
- Experiment-on build, link, size check, and merged image: passed.
- Default-off App: `0x66c050` / 6,733,904 bytes.
- Experiment-on App: `0x66c0c0` / 6,734,016 bytes.
- Enabled delta against the same-build default-off control: `+0x70` /
  +112 bytes.
- Published v1.8.1 handoff App baseline: `0x66b250` / 6,730,320 bytes.
  The experiment-on build is +3,696 bytes; that comparison also includes the
  ESP-IDF 5.5.0-to-5.5.2 toolchain difference.
- 7 MB App slot free with experiment enabled: `0x93f40` / 606,016 bytes
  (8%).
- Static DIRAM with experiment enabled: 171,999 / 341,760 bytes (50.33%).
- New tasks, stacks, timers, queues, semaphores, NVS records: none.
- Internal SRAM/PSRAM: no new persistent allocation; the change adds only a
  timeout selection method and startup logging.

## Build artifacts

- Default-off rollback App:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-fast-provisioning-off.bin`
  - SHA256:
    `bc9b56e7f73c6d289e5db0855fbb8dd3926e5a945dc32ea28173dd6ce02e242f`
- Experiment-on App:
  `build-v181-fast-provisioning-off/xiaozhi-v1.8.1-fast-provisioning-on.bin`
  - SHA256:
    `a1ee795d99b17431de0a8ca893868d5043becfb43bef3aa13d6c6eb8d515e406`

## Hardware verification

- The experiment-on App was flashed only at `0x100000`; esptool verified
  the written hash. Bootloader, partition table, model data, OTA data, and NVS
  were not written.
- Boot log confirmed App version `1.8.1` and
  `Experimental fast provisioning fallback enabled timeout=8000 ms`.
- With saved networks unavailable, the board logged the timeout at 10.252
  seconds, entered pure SoftAP at 10.382 seconds, started DHCP at 10.392
  seconds, broadcast `Xiaozhi-8E81` at 10.402 seconds, and started the web
  server at 10.412 seconds.
- This improves provisioning readiness by about 22 seconds versus the
  pre-change hardware trace.
- No panic, watchdog, abort, or reboot loop appeared during the monitored
  window.
- Pending physical checks: confirm `Xiaozhi-8E81` appears on the phone and
  completes one full provisioning flow; later verify normal saved-network
  connection when that access point is available.

## Rollback

- Software rollback: remove
  `CONFIG_QDTECH_EXPERIMENT_FAST_PROVISIONING_FALLBACK=y` from the build
  defaults or disable it in menuconfig, then rebuild. The QDTech board returns
  to the stable 30-second path.
- Immediate hardware rollback, preserving NVS: App-only flash the default-off
  artifact above to `0x100000` with the same flash mode, frequency, and
  16 MB flash-size arguments. Do not erase flash or write the NVS partition.

The legacy handoff/changelog files contain pre-existing invalid UTF-8 byte
sequences and contradictory historical entries, so this standalone handoff
records the v1.8.1 networking work without rewriting unrelated history.
