# v1.8.0 Calendar Zodiac Reader + Dice Face Fix

Date: 2026-07-18

## Scope

- Baseline: tag commit `f889c6b69a8ac3ba6d1f72232c82a708b41622fb`, project version `1.8.0`.
- Added default-off `CONFIG_QDTECH_EXPERIMENT_CALENDAR_ZODIAC`.
- The explicit `sdkconfig.zodiac.defaults` profile enables the existing
  Calendar bone-weight reader and the new Zodiac reader.
- No Wi-Fi source, partition, pin, touch-driver, weather, voice, media, or OTA
  behavior was changed.

## Behavior and resources

- Calendar accepts validated Gregorian birth dates from 1900 through 2100,
  including leap-year and month-length adjustment, and applies standard
  Western zodiac boundaries.
- Result summary includes sign range, element, modality, and ruler. The reader
  contains six pages per sign.
- All 72 UTF-8 detail records and twelve cute 160x160 JPEG illustrations live
  under `/calendar/zodiac/` on the SD card.
- Firmware loads only the selected detail page and image. SD text/JPEG work is
  dispatched to the existing 28 KB `background_task`, then applied under the
  LVGL lock. A 160x160 RGB565 image uses about 51,200 bytes of PSRAM and is
  released on page exit or when a stale request is cancelled.
- Fixed the D6 face-six visual mask by disabling the centre pip. Existing
  result generation remains `% 6 + 1`.

## Resource impact

- New tasks/stacks: none; asynchronous SD/JPEG loading reuses the existing
  28 KB application `background_task`.
- New timers, queues, semaphores, NVS records: none.
- Internal SRAM: active LVGL page objects plus small parser/file state only;
  no complete zodiac dataset or persistent bitmap is retained.
- PSRAM: approximately 51,200 bytes while one 160x160 image is displayed,
  plus JPEG decoder scratch use; released when leaving the reader.

## Verification

- ESP-IDF 5.5.0 build and link: passed.
- `idf.py size` and `idf.py merge-bin`: passed.
- App: `0x66b250` / 6,730,320 bytes.
- Delta from v1.8.0 release app baseline: +8,384 bytes.
- 7 MB app slot free: `0x94db0` / 609,712 bytes (8%).
- Merged image: `0x76b250` / 7,778,896 bytes.
- Static DIRAM: 172,279 / 341,760 bytes (50.41%).
- Automated checks passed for 24 sign boundary cases, all 72 UTF-8 records,
  all twelve JPEG names/format/dimensions/size, and the dice face-six mask.
- `git diff --check`: passed.
- Wi-Fi implementation was hash-compared with the untouched v1.8.0 base and
  remained identical.
- App-only flashing to COM3 at `0x100000` completed with esptool hash
  verification, preserving Wi-Fi/NVS and the existing partition table.
- The first hardware image exposed an `esp_timer` stack overflow immediately
  after JPEG decode. The final image moves both detail and JPEG loading to the
  existing background task and uses request IDs to discard stale results.
- Final hardware logs loaded `cancer.jpg` and `pisces.jpg` as 160x160 / 51,200
  byte RGB565 frames, repeatedly entered/exited the Zodiac reader, and did not
  panic, reboot, or leak the displayed image allocation.
- Hardware boot confirmed app 1.8.0, 8 MB PSRAM, LCD, five-point touch,
  BMI270, SDMMC 4-bit mount, saved `liutupi` Wi-Fi, IP `192.168.4.214`, OTA,
  MQTT, SNTP, weather HTTP 200, Radio play/stop, and return to idle.
- Shake Lab hardware produced D6 totals 2, 6, and 5. The face-six source mask
  has no centre pip. Minimum observed internal SRAM was about 6.1 KB and
  recovered to about 9.6 KB after leaving the tested pages.
- Intermittent I2C read failures remain observable during aggressive touch or
  high-rate shake sampling; touch/BMI270 recovered without reboot. This is an
  existing shared-bus behavior and was not changed in this feature task.

## Rollback

- Disable `CONFIG_QDTECH_EXPERIMENT_CALENDAR_ZODIAC` and rebuild to remove the
  Zodiac UI path. Installed SD files may remain because disabled firmware
  ignores them.
- To roll back the dice-only fix, restore the face-six mask in
  `desktop_ui.cc`; this is independent of the Zodiac feature flag.

The legacy `docs/HANDOFF.md`, `docs/PROJECT_STATUS.md`, and
`docs/CHANGELOG_QDTECH.md` contain pre-existing invalid UTF-8 byte sequences.
They were deliberately not normalized or rewritten in this worktree. This
standalone handoff preserves the required status without altering unrelated
historical content.
