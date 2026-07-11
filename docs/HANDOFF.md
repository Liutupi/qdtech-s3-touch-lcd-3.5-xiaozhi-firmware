## 2026-07-11 Handoff: v1.7.94 Streaming + Private FM Continuation

- Firmware version is `v1.7.94`. The existing UI, weather artwork, and full-screen weather animation are preserved; the display remains on LVGL `FULL` render mode because `PARTIAL` and `DIRECT` corrupt translucent GIF composition on this panel.
- Music and radio streaming now use PSRAM-backed compressed/decoder/output buffers, larger steady-state prebuffer targets, 4 KB reads, and an allocation-free `AudioCodec::OutputData` path. Custom music no longer remains at the small startup target.
- Stream handoff invalidates the old generation before preparing a new URL, removes the duplicate Content-Length probe/TLS session, rejects short preview URLs using real response headers, and prevents stale responses from mutating current playback state.
- Dead radio URLs with permanent HTTP status codes are skipped without repeated reconnect delay; stations whose sources are all permanently unavailable advance automatically.
- Touch and BMI270 serialize complete I2C transactions. Successful status reads clear the touch failure streak, failures separated by five seconds do not accumulate into a disruptive bus reset, and BMI startup waits briefly for the shared lock.
- Hidden XiaoZhi face updates no longer invalidate LVGL outside that page. Clock/date and battery widgets avoid redundant updates. The attempted GIF pause/resume optimization was removed after it reproduced weather-screen corruption; no UI or weather animation was replaced.
- NAS `xiaozhi-ws-mcp.js` persists private-FM autoplay per account, restores it after container/watchdog restart, resolves the active WebSocket at timer fire time, retries after reconnect, and uses subnet broadcast as the autoplay-only fallback so DHCP address changes cannot silently send the next track to an old board IP.
- Hardware evidence: CNR streams ran for more than 1,100/1,400 decoded frames without reconnect errors; NetEase track `故湘，风` streamed its full 5.2 MB and drained 13,584 frames cleanly. The NAS selected and scheduled later tracks, exposing the stale UDP host (`192.168.1.114` while the board was `192.168.1.112`); the broadcast fallback fixes that root cause. A complete post-fix two-track natural transition is still the first follow-up test.
- The stable full-render image was flashed and hash-verified on `/dev/cu.usbmodem212401`. Boot, PSRAM, display, touch, BMI270, Wi-Fi, MQTT, time, and weather were observed. Minimum internal SRAM during the long music run reached 975 bytes, so future optimization should continue reducing concurrent voice/MQTT/UI pressure.
- ESP-IDF 5.5.2 `reconfigure build merge-bin` passed with CMake reporting `1.7.94`. App size is `0x665960` / 6,707,552 bytes with `0x9a6a0` bytes (9%) free in the 7 MB slot; merged image size is `0x765960` / 7,756,128 bytes.
- SHA256: app `b29e7b9b4b1013aa8f7070ae2357ad3b7b8586351a86c43acfad3b4a46f1424f`; full `f39fe17031e073bdaeb1e5690c2a6899d39edb7bd58e9aa454806b4c897976c4`; ZIP `f5063aed82710c85ecbf0b78d4113731d49989b67e90bc2a4787f00ae5131850`.

## 2026-07-11 Handoff: v1.7.93 Shake Lab Fortune + Weather/Radio Polish

- Firmware version is `v1.7.93`. Shake Lab now offers Ask Ball, selectable 1-6 dice, and an offline Fortune Stick mode with 24 numbered fortunes, levels, poems, and interpretations.
- Dice uses a compact 3x2 layout, reports totals, and highlights lucky sixes while retaining the existing single BMI270 task and ShakeDetector state machine.
- Rebuilt the QDTech LXGW 16/20 px subsets with all desktop Chinese characters. Fortune text and mixed Chinese/English bottom status use these fonts; automated audit: zero missing glyphs.
- Weather refresh survives transient LVGL lock contention and refreshes on return to Main. Hidden weather particles pause behind apps; Hourglass/Shake Lab trees are reclaimed asynchronously.
- Radio buttons use one completed-tap path, preventing missed release clicks and duplicate Play/Pause toggles.
- User physically verified expanded dice and Fortune Stick. The reported mixed-language bottom status glyph issue was corrected before the release build.
- ESP-IDF 5.5 `reconfigure build merge-bin` passed; CMake reported `1.7.93`. App `0x665510`, slot free `0x9aaf0` (9%), merged `0x765510`.
- The preceding functional image was flashed and hash-verified on `/dev/cu.usbmodem211401`. The final version-metadata rebuild could not be reflashed because USB serial disconnected during release preparation.
- SHA256: app `5de3b17389cc50fbec2abe0c94ef7a35bde3dd02b5934dc9bac5b7dc634e9d7b`; full `6cc6da85e1728783d671b0a632eaa08c68e7dbb1969ec4d9db565f251fbeb6d5`; ZIP `403ce403ed3e6ae42e5743b574a241f66a8eca3290ef4b79fa2cb82ab78e8ee8`.
- Publish `v1.7.93` as latest. App binary is OTA; full binary is recovery flash at offset `0x0`.

## 2026-07-10 Handoff: v1.7.92 Shake Lab MVP

- Firmware version is v1.7.92. Shake Lab adds offline Ask Ball and D6/2D6 Dice through Apps -> More.
- `ShakeDetector` is board-local and owns no task or I2C access. The existing single BMI270 task remains the only reader.
- Ask Ball/Dice enable 30 ms sampling only while active; all other pages keep 250 ms Hourglass orientation polling. Reads retain the shared-I2C non-blocking lock and 800 ms touch-active backoff.
- Shake Lab is lazy-created. Exit pauses/deletes its LVGL timer and page object tree. P3_SUCCESS plays once on reveal only when external audio is inactive.
- Thresholds: motion accel deviation >=2800 or gyro >=750; strong peak >=4400 or >=1000; 4 peaks plus 2 reversals in 800 ms for >=260 ms; 700 ms still -> settling; 650 ms stable -> reveal; 1400 ms cooldown.
- ESP-IDF 5.5 build/merge passed in `build-v1792`: `xiaozhi.bin` 0x652ec0 / 6631104 bytes; 7 MB slot free 0xad140 / 708928 bytes; `merged-binary.bin` 0x752ec0 / 7679680 bytes.
- Flashed to COM3 at 460800 baud. esptool verified every written segment hash. The Apps -> More -> Shake Lab -> Dice path and high-rate BMI270 sampling were verified on the immediately preceding functional image; the final image only corrects the diagnostic interval format and removes duplicate LVGL callback dispatch.
- Remaining physical follow-up: leave Shake Lab, keep landscape baseline, wait 75 seconds after boot, then rotate 90 degrees to confirm Hourglass entry/exit; also perform the existing touch/photo/Settings/XiaoZhi regression sweep and shake false-trigger/cooldown checks.
## 2026-07-09 Handoff: v1.7.91 Hourglass Alarm

Current target:

- Firmware version target is now v1.7.91.
- This release keeps the v1.7.90 stable touch/BMI270 base and adds an audible hourglass completion alarm.
- Built on Windows with ESP-IDF 5.5 from D:\3.5inch_ESP32-S3\worktree-v1786-hourglass, build directory build-v1790.
- Release assets were generated in releases/v1.7.91/.
- The connected board on COM3 was flashed with the v1.7.91 image; esptool hash verification passed.

What changed:

- Bumped PROJECT_VER to 1.7.91.
- Added Application::PlayNotificationSound(), which resets the decoder and re-enables speaker output before queueing a short system sound.
- Hourglass countdown completion now plays Lang::Sounds::P3_SUCCESS exactly once per timer run.
- Hourglass reset, exit, and new preset selection clear the one-shot alarm latch.
- Added low-rate hourglass countdown progress logs at each minute and during the final five seconds for future diagnosis.

Verification:

- idf.py -B build-v1790 reconfigure build merge-bin passed.
- CMake reported App "xiaozhi" version 1.7.91.
- xiaozhi.bin size is 0x64e7c0 / 6612928 bytes; QDTech 7 MB OTA app slot has 0xb1840 bytes free, about 10 percent.
- merged-binary.bin size is 0x74e7c0 / 7661504 bytes.
- Flash to COM3 completed with esptool hash verification.
- Serial monitor confirmed a real 5 minute hourglass run: remaining=240, 180, 120, 60, then 5/4/3/2/1/0.
- At completion the monitor logged Hourglass alarm sound requested, AudioCodec: Set output enable to true, and Hourglass timer done; output later returned to disabled after the sound finished.

Release asset SHA256:

- releases/v1.7.91/qdtech-s3-touch-lcd-3.5-v1.7.91-app.bin: 21839bde70e75ada23c8fa96679b3626518386f53e1ddb3507fac46573385585
- releases/v1.7.91/qdtech-s3-touch-lcd-3.5-v1.7.91-full.bin: 99bf3c7175bdd02b726a0c63564135613dd23e4c0a17b1ac984094279759baff
- releases/v1.7.91/qdtech-s3-touch-lcd-3.5-v1.7.91-firmware.zip: b15325b37fcc19266cf14a78f91e71fadad5aa1f70638a1b9d4c2aaa38add1a9

Release/OTA note:

- GitHub Release v1.7.91 is published as latest with qdtech-s3-touch-lcd-3.5-v1.7.91-app.bin plus SHA256SUMS.txt so other boards can download it through the on-device updater.
# QDTech 3.5 XiaoZhi Project Handoff

> Future Codex note: read this file, docs/PROJECT_STATUS.md, docs/NEXT_TASKS.md, and docs/CODEX_RULES.md before changing code.

## 2026-07-09 Handoff: v1.7.90 Stable Touch Base with Hourglass

Current target:

- Firmware version target is now v1.7.90.
- This release keeps the latest main history but restores the QDTech board touch behavior to the v1.7.86-stable path where needed.
- Built on Windows with ESP-IDF 5.5 from D:\3.5inch_ESP32-S3\worktree-v1786-hourglass, build directory build-v1790.
- Release assets were generated in releases/v1.7.90/.
- The connected board on COM3 has been flashed with the final v1.7.90 image; esptool hash verification passed.

What changed:

- Bumped PROJECT_VER to 1.7.90.
- Kept the v1.7.86 QDTech touch driver behavior as the stable baseline while preserving main-line hourglass UI support.
- Added BMI270 hourglass orientation logic with I2C protection and touch-active backoff.
- Removed the unused QDTech BLE phone-config service from this board to reduce internal SRAM pressure.
- Kept the HTTP phone/web config server path; QdWifiConfig starts without the BLE low-memory skip path.

Verification:

- idf.py -B build-v1790 build merge-bin passed.
- CMake reported App "xiaozhi" version 1.7.90 and the app image contains the 1.7.90 version string.
- xiaozhi.bin size is 0x64e530 / 6612272 bytes; QDTech 7 MB OTA app slot has 0xb1ad0 bytes free, about 10 percent.
- merged-binary.bin size is 0x74e530 / 7660848 bytes.
- Final v1.7.90 flash to COM3 completed with esptool hash verification.
- Serial monitor monitor-v1786-hourglass-noble-20260709.log confirmed HTTP config service startup, no BLE low-memory init path, normal tap down/release events, photo right-swipe back navigation, Settings vertical scroll, and BMI270 hourglass enter/exit. A final v1.7.90 short monitor confirmed QdWifiConfig HTTP startup and BMI270 polling after the final flash.

Release asset SHA256:

- releases/v1.7.90/qdtech-s3-touch-lcd-3.5-v1.7.90-app.bin: b59d353927167b3aa55ec10e9a9180283f67a58f46c693e364ff077992493fc4
- releases/v1.7.90/qdtech-s3-touch-lcd-3.5-v1.7.90-full.bin: f4913860c5f219b548193fc9a91212eef064331dc4445db7d7d592264d49a285
- releases/v1.7.90/qdtech-s3-touch-lcd-3.5-v1.7.90-firmware.zip: 251ecf0d95fe83525d1cffb1eba036cbb70640ce4b5559f4c0310147d9be60cb

Release/OTA note:

- GitHub Release v1.7.90 is published as latest with qdtech-s3-touch-lcd-3.5-v1.7.90-app.bin plus SHA256SUMS.txt so other boards can download it through the on-device updater.
## 2026-07-08 Handoff: v1.8.18 Hourglass + Startup Sync Memory Fix

Summary:
- Firmware version target is now `v1.8.18`.
- Added independent `DesktopPage::HOURGLASS` with the cleaned hourglass bitmap body plus lightweight LVGL sand strips/dots for animated falling sand and second-by-second sand volume changes.
- Re-enabled BMI270 and replaced the old 180-degree Focus Timer motion trigger with stable 90-degree portrait detection for Hourglass entry, plus landscape recovery to exit the page.
- Hourglass page is created lazily and deleted on motion exit so the large UI does not stay resident after returning to the normal horizontal desktop.
- Weather startup sync now defers while the app is still starting/activating/connecting, lowers the weather fetch internal memory floor to 6 KB, and retries low-memory/deferred weather every 20 seconds.
- Preserved the current v1.8.17 touch/photo recovery path while adding the hourglass behavior.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.h`
- `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/time_weather_service.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/qd_hourglass_body.c`
- `releases/v1.8.18/`

Validation:
- Built successfully from `/private/tmp/qdtech-v1818-build-src` to avoid the local path-with-spaces linker issue.
- Flashed successfully to `/dev/cu.usbmodem212401`; boot log reports app version `1.8.18`.
- Monitor confirmed BMI270 initializes at `0x68`, establishes a landscape baseline, and stays inactive while the board remains horizontal.
- Monitor confirmed startup time/daily card/weather recovered: daily card updated for `2026-07-08`, time synchronized, and weather fetched `中山 多云 27 C`.
- Monitor did not capture a physical 90-degree flip during this run, so hourglass entry still needs one real flip check after the user rotates the board.

## 2026-07-08 Handoff: v1.8.08 Touch Coordinate Parser Fix

Summary:
- Firmware version target is now `v1.8.08`.
- v1.8.07 confirmed real touch data appears when pressing: `0x0010=0x08`, `0x0014=01 1c 01 a4`, and INT goes low.
- Fixed the legacy parser to treat `0x0014` as `raw_x_hi raw_x_lo raw_y_hi raw_y_lo` instead of expecting a `0x80` valid flag.
- Removed the heavy diagnostic register dump/probe path.
- Clears the touch status only after a forced stale release, so repeated taps at the same position can recover.
- BMI270 remains disabled until touch is verified fixed.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`
- `docs/HANDOFF.md`

Validation:
- Pending build/flash in the current repair pass.

## 2026-07-08 Handoff: v1.8.07 Touch Register Map Scan

Summary:
- Firmware version target is now `v1.8.07`.
- User confirmed v1.8.06 still cannot tap.
- Delayed scan confirmed shared I2C is alive and devices respond at `0x18`, `0x55`, and `0x68`, but the current `0x55 / 0x0014` point read still returns zeros.
- Added once-per-second touch register dumps after startup using both 16-bit and 8-bit register address reads, so holding the panel should reveal which register map actually changes.
- BMI270 remains disabled while touch is isolated.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`
- `docs/HANDOFF.md`

Validation:
- Pending build/flash in the current repair pass.

## 2026-07-08 Handoff: v1.8.06 Delayed Locked Address Probe

Summary:
- Firmware version target is now `v1.8.06`.
- v1.8.05 startup probing timed out on all scanned addresses, likely because the probe ran too early or without the shared bus mutex.
- Restored the shorter touch reset timing and moved the I2C address scan to a delayed, mutex-protected pass after the system is running.
- Added audio codec address `0x18` to the delayed scan as a sanity check for the shared I2C bus.
- Touch remains forced to legacy `0x55`, single-point short reads, with BMI270 disabled for isolation.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`
- `docs/HANDOFF.md`

Validation:
- Pending build/flash in the current repair pass.

## 2026-07-08 Handoff: v1.8.05 Long Touch Reset + Address Probe

Summary:
- Firmware version target is now `v1.8.05`.
- v1.8.04 confirmed single-point short reads and 100 kHz I2C, but touch data remained all zero and INT stayed high.
- Extended the touch-controller reset sequence to hold release/reset longer before first reads.
- Added startup logging for common touch/BMI270 I2C addresses so the next monitor run shows which devices are actually responding after reset.
- BMI270 remains temporarily disabled while touch is isolated.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`
- `docs/HANDOFF.md`

Validation:
- Pending build/flash in the current repair pass.

## 2026-07-08 Handoff: v1.8.04 Legacy Single-Point Short Read

Summary:
- Firmware version target is now `v1.8.04`.
- v1.8.03 still returned all-zero CST9217/legacy touch data and introduced I2C timeouts during the split-read experiment.
- Removed CST auto-detection from the boot path for this board and returned to the known ACKing `0x55` legacy route.
- Forced legacy touch to single-point short reads, matching the earlier version that produced real coordinates.
- Lowered the touch I2C device speed to 100 kHz to reduce bus sensitivity while BMI270 remains disabled for isolation.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`
- `docs/HANDOFF.md`

Validation:
- Pending build/flash in the current repair pass.

## 2026-07-08 Handoff: v1.8.03 CST9217 Split Read

Summary:
- Firmware version target is now `v1.8.03`.
- User still reported no tap response on v1.8.02, while logs showed the touch controller ACKing on I2C but returning all-zero legacy data.
- Switched CST9217 probing and CST9217 point reads to the controller-driver style split transaction: write register address, wait 2 ms, then read data.
- Kept legacy `0x55 / 0x0014` parsing as fallback if CST9217 packet ACK is not present.
- BMI270 remains temporarily disabled while touch protocol is being isolated.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`
- `docs/HANDOFF.md`

Validation:
- Pending build/flash in the current repair pass.

## 2026-07-08 Handoff: v1.8.02 CST9217-on-0x55 Probe

Summary:
- Firmware version target is now `v1.8.02`.
- v1.8.01 showed this board does not ACK CST9217 at `0x5A`; it still ACKs touch at `0x55`.
- Added CST9217 data-packet probing on `0x55` before falling back to the old legacy `0x0014` register path.
- BMI270 remains temporarily disabled while touch protocol is being isolated.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`
- `docs/HANDOFF.md`

Validation:
- Pending build/flash in the current repair pass.

## 2026-07-08 Handoff: v1.8.01 CST9217 Touch Protocol

Summary:
- Firmware version target is now `v1.8.01`.
- v1.8.00 proved touch polling was running, but the legacy `0x55 / 0x0014` register path always returned zero data even with BMI270 disabled.
- Added CST9217 detection at I2C address `0x5A` and CST9217 data parsing from register `0xD000`.
- Kept the legacy touch protocol as a fallback if `0x5A` is not present.
- BMI270 remains temporarily disabled in this isolation build until touch is confirmed recovered.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`
- `docs/HANDOFF.md`

Validation:
- Pending build/flash in the current repair pass.

## 2026-07-08 Handoff: v1.8.00 Touch Isolation Test

Summary:
- Firmware version target is now `v1.8.00`.
- User still reported no tap response on v1.7.99, and monitor showed no `touch raw` / `touch down` during the test window.
- Temporarily disabled BMI270 initialization/task to isolate whether the six-axis sensor is interfering with the shared I2C touch path.
- Added once-per-second touch polling diagnostics logging INT, touch info, and first point bytes even when no touch is detected.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`
- `docs/HANDOFF.md`

Validation:
- Pending build/flash in the current repair pass.

## 2026-07-08 Handoff: v1.7.99 Touch Hold + Raw Diagnostics

Summary:
- Firmware version target is now `v1.7.99`.
- v1.7.98 still allowed user-visible no-response taps, likely because the touch-state ack could clear a short touch before the UI consumed it.
- Removed the immediate touch-state clear after valid point reads.
- Added a short cached press hold window so one-frame touch samples still reach LVGL/card click handling.
- Added throttled raw touch diagnostics logging the INT pin, touch info byte, and first point bytes whenever the controller reports potential touch data.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`
- `docs/HANDOFF.md`

Validation:
- Pending build/flash in the current repair pass.

## 2026-07-08 Handoff: v1.7.98 Touch State Ack

Summary:
- Firmware version target is now `v1.7.98`.
- v1.7.97 confirmed the first menu tap could enter the apps page, but the touch controller kept reporting the same coordinate afterward until the firmware forced a release.
- Added an explicit touch-state clear after valid point reads so stale controller data is acknowledged before the next real tap or swipe.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`
- `docs/HANDOFF.md`

Validation:
- Built and merged successfully from the `/tmp/qdtech-touch-fix-src` staging path.
- `xiaozhi.bin` size was `0x646b90`; merged image size was `0x746b90`.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Serial monitor confirmed `App version: 1.7.98`, `Touch max points: 1`, LVGL touch input creation, and BMI270 detection.
- No touch event was observed during the short post-flash monitor window, so physical tap/card/right-swipe behavior still needs on-device confirmation.

## 2026-07-08 Handoff: v1.7.97 Stale Coordinate Filter

Summary:
- Firmware version target is now `v1.7.97`.
- v1.7.96 shortened stale-touch release, but old point data could be accepted again immediately and swallow real card taps.
- Added a stale-coordinate filter: after forced release, the same near-identical coordinate is ignored until the touch point moves significantly.
- A new valid press clears the stale marker.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`

Validation:
- Built and flashed from the `/tmp/qdtech-touch-fix-src` staging path.
- Serial monitor confirmed `App version: 1.7.97`, `Touch max points: 1`, LVGL touch input creation, and BMI270 detection.
- Runtime logs showed a menu tap entering the apps page, followed by a forced release at the same coordinate, confirming stale point data was still being held by the controller.

## 2026-07-08 Handoff: v1.7.96 Fast Stale Touch Release

Summary:
- Firmware version target is now `v1.7.96`.
- After v1.7.95 restored touch point reads, logs showed valid `touch down` events but stale point data held the press until the old 1.8 s forced-release timeout.
- Stationary stale touches now release after about 280 ms instead of 1.8 s.
- Forced stale release no longer resets the touch controller every time, avoiding repeated 250 ms suppression windows.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`

Validation:
- Build passed from `/tmp/qdtech-touch-fix-src`, build directory `/tmp/qdtech-touch-fix-build`.
- CMake reported `App "xiaozhi" version: 1.7.96`.
- App image size after build: `0x646a70`; smallest app partition: `0x700000`; free: `0xb9590`.
- `merge-bin` generated `merged-binary.bin`, size `0x746a70`.
- Flashed successfully to `/dev/cu.usbmodem212401`; all flashed sections passed hash verification.
- Serial monitor confirmed `App version: 1.7.96`, `Touch max points: 1`, LVGL touch input created, and BMI270 detected at `0x68`.

## 2026-07-08 Handoff: v1.7.95 Touch Point Data Fallback

Summary:
- Firmware version target is now `v1.7.95`.
- Touch reads no longer require the touch status bit or INT pin to report pressed before reading point data.
- The driver now reads the point block directly and treats a valid point flag as the source of truth.
- This targets the observed hardware behavior where no `touch down` log appeared even after the panel was tapped.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`

Validation:
- Pending build/flash in the current repair pass.

## 2026-07-08 Handoff: v1.7.94 Touch I2C Recovery Hotfix

Summary:
- Firmware version target is now `v1.7.94`.
- Fixed a regression where touch could stop responding because the touch read path abandoned the shared I2C mutex after only 5 ms.
- Touch mutex wait is now 30 ms, and mutex timeout participates in the touch recovery counter instead of silently returning no points.
- Touch detection now falls back to the touch INT pin when the controller status bit is unreliable.
- Touch I2C reset backoff is shorter after repeated failures, so the panel recovers faster.
- BMI270 periodic reads are now non-blocking and much slower, with a longer quiet window after any touch activity, so the six-axis sensor yields to touch input.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`

Validation:
- Pending build/flash in the current repair pass.

## 2026-07-08 Handoff: v1.7.93 Photo Swipe Exit Hotfix

Summary:
- Firmware version target is now `v1.7.93`.
- Fixed Photo page right-swipe exit being blocked by the v1.7.92 early-tap path.
- Photo page now skips early tap dispatch and uses a dedicated horizontal-drag exit path.
- Photo page swipe back is more tolerant: 35 px horizontal movement, horizontal-dominant, no 900 ms release-duration limit.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.h`
- `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`

Validation:
- Build passed from `/tmp/qdtech-touch-fix-src`, build directory `/tmp/qdtech-touch-fix-build`.
- CMake reported `App "xiaozhi" version: 1.7.93`.
- App image size after build: `0x6469a0`; smallest app partition: `0x700000`; free: `0xb9660`.
- `merge-bin` generated `merged-binary.bin`, size `0x7469a0`.
- Flashed successfully to `/dev/cu.usbmodem212401`; all flashed sections passed hash verification.
- Serial monitor confirmed `App version: 1.7.93`, `Touch max points: 1`, LVGL touch input created, and BMI270 detected at `0x68`.

## 2026-07-08 Handoff: v1.7.92 Touch Priority Hotfix

Summary:
- Firmware version target is now `v1.7.92`.
- Short taps now trigger after a stable 90 ms press instead of waiting for the touch controller's delayed release report.
- Horizontal swipes now trigger earlier with a 45 px threshold and a stronger horizontal-dominance check, improving right-swipe back detection.
- Apps page now treats either horizontal direction as back/home, avoiding direction-sign problems on the rotated touch panel.
- Touch I2C mutex wait is limited to 5 ms so BMI270 activity cannot stall touch reads for hundreds of milliseconds.
- Touch read retry backoff is shortened again to recover faster from transient shared-I2C contention.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`

Validation:
- Build passed from `/tmp/qdtech-touch-fix-src`, build directory `/tmp/qdtech-touch-fix-build`.
- CMake reported `App "xiaozhi" version: 1.7.92`.
- App image size after build: `0x646870`; smallest app partition: `0x700000`; free: `0xb9790`.
- `merge-bin` generated `merged-binary.bin`, size `0x746870`.
- Flashed successfully to `/dev/cu.usbmodem212401`; all flashed sections passed hash verification.
- Serial monitor confirmed `App version: 1.7.92`, `Touch max points: 1`, LVGL touch input created, and BMI270 detected at `0x68`.

## 2026-07-08 Handoff: v1.7.91 Touch Latency Hotfix

Summary:
- Firmware version target is now `v1.7.91` for the touch-latency hotfix after `v1.7.90`.
- Fixed slow tap response caused by touch I2C read failures backing off for 750 ms. Touch now uses a short 30 ms retry backoff, with 200 ms only after repeated failures.
- Made BMI270 reads non-blocking on the shared I2C mutex, so motion sensing skips a sample instead of delaying touch.
- Extended BMI270 quiet time after touch activity to 1000 ms so touch interaction has priority over flip sensing.
- Added early horizontal-swipe dispatch while the finger is still moving, so right-swipe back no longer waits for a release event that may arrive late.
- Made detail pages tolerant of horizontal sign reversal: non-main/non-Apps pages navigate back on either strong horizontal swipe; Apps still keeps left-swipe-to-Media and right-swipe-to-Home behavior.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`

Validation:
- Pending build/flash in the current repair pass.

## 2026-07-08 Handoff: v1.7.90 Touch Gesture + BMI270 Flip Tuning

Summary:
- Firmware version target is now `v1.7.90` so OTA clients can see the touch/flip fix after `v1.7.89`.
- Fixed the post-BMI270 touch regression where taps still worked but right-swipe back and slower vertical/horizontal swipes were unreliable.
- Relaxed manual gesture timing: taps now allow up to 350 ms and swipes up to 900 ms.
- Removed the aggressive 450 ms forced touch-controller reset from moving touches. The controller reset now only runs for long, almost-stationary stuck touches, so normal swipes are not cut off.
- Shortened the BMI270 quiet period after touch activity so the motion detector recovers sooner.
- Made flip-to-Focus Timer more sensitive by detecting 180-degree rotation in the X/Y plane first, with the old full 3D face-down flip kept as a fallback.
- Reduced BMI270 stable-count and cooldown thresholds so the Focus Timer enters/exits faster while still checking gyro stability.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`

Validation:
- Full no-space-path build passed from `/tmp/qdtech-touch-fix-src`, build directory `/tmp/qdtech-touch-fix-build`.
- CMake reported `App "xiaozhi" version: 1.7.90`.
- `idf.py -C /tmp/qdtech-touch-fix-src -B /tmp/qdtech-touch-fix-build -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" set-target esp32s3 build merge-bin` passed.
- App image size after build: `0x6466f0`; smallest app partition: `0x700000`; free: `0xb9910`.
- `merge-bin` generated `merged-binary.bin`, size `0x7466f0`.
- `releases/v1.7.90/qdtech-s3-touch-lcd-3.5-v1.7.90-app.bin`: `4b3ff4fc9b4e4bd3377ebb238f1fbed548eddd3f06e85e478d6fc286aad5b571`
- `releases/v1.7.90/qdtech-s3-touch-lcd-3.5-v1.7.90-full.bin`: `aff7ee1be349567ec3d729036bb2698d543818f7518c86de0ec14cfc237d9b31`
- `releases/v1.7.90/qdtech-s3-touch-lcd-3.5-v1.7.90-firmware.zip`: `62ae0a88d68e1735848a9d8ebbb0215850182f1091914340cadcc366bd5b79fd`

Follow-up test request:
- On hardware, verify: tap Menu, right-swipe back; open a page with vertical scrolling and do a slow up/down swipe; rotate 180 degrees into Focus Timer; tap Focus Timer controls; rotate back and confirm home touch still works.

## 2026-07-08 Handoff: v1.7.89 BMI270 Flip Focus + Touch Recovery

Summary:
- Firmware version target is now `v1.7.89` so OTA clients can see the BMI270 flip-focus and touch-recovery build.
- Added BMI270 six-axis sensor support on the shared touch I2C bus. The board probes `0x68/0x69`, validates chip ID `0x24`, uploads the BMI270 config blob, and starts a background orientation task.
- Horizontal 180-degree rotation now starts Focus Timer / Pomodoro mode. Rotating back to the normal horizontal position exits the Focus Timer page.
- When Focus Timer is opened by the flip gesture, the display output is rotated 180 degrees and touch coordinates are mapped to the inverted screen.
- Hardened the touch path after flip testing: touch and BMI270 now share an I2C mutex, BMI270 pauses briefly after touch activity, stale long presses synthesize release events, the FT6336 controller is reset after stuck touches, and repeated false taps are suppressed until real release.
- Focus Timer has a Back button and fallback tap handling for work/break/start/reset/back controls.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/bmi270_context_config.c`
- `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.h`
- `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`

Validation:
- `idf.py -B build-qdtech-v1.7.88-bmi270 ... build merge-bin` passed after bumping `PROJECT_VER` to `1.7.89`.
- CMake reported `App "xiaozhi" version: 1.7.89`.
- App image size after build: `0x6444f0`; smallest app partition: `0x700000`; free: `0xbbb10`.
- `merge-bin` generated `merged-binary.bin`, size `0x7444f0`.
- App-only flashed the connected QDTech ESP32-S3 board on `COM3`; esptool hash verification passed.
- `releases/v1.7.89/qdtech-s3-touch-lcd-3.5-v1.7.89-app.bin`: `86d4b720100093311eb4bc4252b6d203c8b1b46e94cc0afa9a7d6dc0fa905879`
- `releases/v1.7.89/qdtech-s3-touch-lcd-3.5-v1.7.89-full.bin`: `6bcb3176408d75cbe7d2cea7b94234ec27ee2950f2572587793058a2e44e8394`
- `releases/v1.7.89/qdtech-s3-touch-lcd-3.5-v1.7.89-firmware.zip`: `94eca5573d0e0f70a708894c644dbb255670ae386b3bf141429e1122402d407d`

Follow-up test request:
- On hardware, verify three paths after OTA/flash: normal home-page touch, flip into Focus Timer and tap its buttons, then flip back and tap the home page again. The latest touch-stuck suppression is intended to fix the post-flip no-touch issue, but it still needs a real board soak test.

## 2026-07-08 Handoff: v1.7.87 Music Controls Stability

Summary:
- Firmware version target is now `v1.7.87` so OTA clients can detect the music-control hotfix after v1.7.86.
- Stabilized the new Music page Play/Pause/Next controls: rapid taps are debounced, Music Next no longer falls through to radio station switching, and stale radio commands are dropped when the queue is full.
- Built and app-only flashed the connected QDTech ESP32-S3 board on `COM3`; esptool hash verification passed.
- Boot log confirmed `Ota: Current version: 1.7.87`, WiFi connected, MQTT connected, and the app reached `STATE: idle`.

Files touched:
- `CMakeLists.txt`
- `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.h`
- `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/radio_service.cc`

Validation:
- `idf.py -B build-qdtech-v1.7.87-musicfix ... set-target esp32s3 build merge-bin` passed.
- App image size after build: `0x640460`; smallest app partition: `0x700000`; free: `0xbfba0`.
- `releases/v1.7.87/qdtech-s3-touch-lcd-3.5-v1.7.87-app.bin`: `add6945dd8f9aa3060dfb699cb7a0fe93dc81512298feb0c21b556fab69115d1`
- `releases/v1.7.87/qdtech-s3-touch-lcd-3.5-v1.7.87-full.bin`: `68068d81a3dfc755abf731c120d46f43b94553292938b78a2b717cef559e2f22`
- `releases/v1.7.87/qdtech-s3-touch-lcd-3.5-v1.7.87-firmware.zip`: `161b2c89f3b1376bddb0c0720c3d6f3ecd89efab9d34aa177dbbfec14fe2b7b4`
- App-only flash path: `0x100000 build-qdtech-v1.7.87-musicfix/xiaozhi.bin`.

Follow-up test request:
- Ask XiaoZhi to play a song, tap Pause/Play several times, then tap Next. Next should either replay the next cached recent song or prompt for a fresh song instead of switching to radio.
# QDTech 3.5 XiaoZhi Project Handoff

> Future Codex note: read this file, `docs/PROJECT_STATUS.md`, `docs/NEXT_TASKS.md`, and `docs/CODEX_RULES.md` before changing code.

## 2026-07-07 Handoff: v1.7.86 OTA Version Correction

Current target:

- Firmware version target is now `v1.7.86`.
- This release is a version correction for remote OTA detection after the `v1.7.85` music controls and NAS private FM work.
- Built on macOS with ESP-IDF 5.5 from `/tmp/qdfw-release-186`, build directory `/tmp/qdfw-release-186-build`.
- Release assets were generated in `releases/v1.7.86/`.
- The connected board was not reflashed for this correction pass; the priority was publishing a newer GitHub Release so other boards can detect the remote upgrade.

What changed:

- Bumped `PROJECT_VER` from `1.7.85` to `1.7.86`.
- Kept the v1.7.85 firmware code path unchanged: Music page `Pause`, `Play`, and `Next` controls remain, NAS NetEase private FM continuous autoplay remains, and `self.music.play_url` behavior is unchanged.

Verification:

- Full `idf.py -C /tmp/qdfw-release-186 -B /tmp/qdfw-release-186-build -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" set-target esp32s3 build merge-bin` passed.
- CMake reported `App "xiaozhi" version: 1.7.86`.
- Final app image: `/tmp/qdfw-release-186-build/xiaozhi.bin`, size `0x642180`; QDTech 7 MB OTA app slot has `0xbde80` bytes free.
- `merge-bin` generated `merged-binary.bin`, size `0x742180`.
- The app image was checked and contains version string `1.7.86`.

Release asset SHA256:

- `releases/v1.7.86/qdtech-s3-touch-lcd-3.5-v1.7.86-app.bin`: `88c7292bdd97d6d9700ad10fa0310b6dad1b711842bad3699e8dae9b768aedbb`
- `releases/v1.7.86/qdtech-s3-touch-lcd-3.5-v1.7.86-full.bin`: `af446e333fac9090d1b0b65c0306c2ca227c2aadac6790a5d46e8444606a30a0`
- `releases/v1.7.86/qdtech-s3-touch-lcd-3.5-v1.7.86-firmware.zip`: `8ef3c262e9a0b7e1ef3b1a6c9e5c51368e4f72e5210c52bdc572c6e2e8a4b275`

## 2026-07-07 Handoff: v1.7.85 Music Controls and NAS Private FM

Current target:

- Firmware version target is now `v1.7.85`.
- Built on macOS with ESP-IDF 5.5 from `/tmp/qdtech-xiaozhi-buildcheck`, build directory `/tmp/qdtech-xiaozhi-buildcheck/build`.
- Release assets were generated in `releases/v1.7.85/`.
- The connected QDTech ESP32-S3 board on `/dev/cu.usbmodem212401` has been flashed with `v1.7.85`; esptool hash verification passed.

What changed:

- Kept the 2026-07-05 Music vinyl UI and `self.display.qrcode` NetEase QR-login prep.
- Removed the `NetEase` wordmark from the Music page left visual panel.
- Replaced the small coin/source icon with a black rotating vinyl GIF asset (`qd_music_vinyl.gif` / `qd_music_vinyl.c`).
- Enlarged the vinyl visual and waveform bars so the left panel reads as a music animation instead of a source badge.
- Added LVGL QR code support and a `self.display.qrcode` MCP tool so login URLs can be shown directly on the board.
- Added Music page `Pause`, `Play`, and `Next` buttons below `Back`.
- Added `RadioService::Pause()` so the Music page can pause the current custom URL stream without clearing the current song context; `Play` reopens/resumes the current stream and `Next` advances the URL playlist/NetEase private FM flow.
- Updated repository NAS NetEase copies in `tools/nas/`: `xiaozhi-ws-mcp.js`, `netease-mcp.js`, and `README-小智网易云MCP.md`.
- NetEase private FM now supports continuous autoplay. Repeated `music.netease_private_fm` calls while already playing return `private_fm_already_playing` and do not push another `self.music.play_url`, preventing the current song from being cut off.
- Private FM prefers `NETEASE_PRIVATE_FM_LEVELS=standard,higher,exhigh` by default for steadier ESP32 playback, while normal song search can still use `NETEASE_MUSIC_LEVELS`.

Verification so far:

- Local bridge syntax checks passed with `node --check tools/nas/xiaozhi-ws-mcp.js` and `node --check tools/nas/netease-mcp.js`.
- `idf.py build` and `idf.py merge-bin` passed from `/tmp/qdtech-xiaozhi-buildcheck`.
- CMake reported `App "xiaozhi" version: 1.7.85`.
- App image size: `0x642180`; QDTech 7 MB OTA app slot has `0xbde80` bytes free.
- `merge-bin` generated `merged-binary.bin`, size `0x742180`.
- Flash to `/dev/cu.usbmodem212401` completed with esptool hash verification.
- Boot monitor confirmed the flashed firmware was running, weather updated, battery was reported, the user could open Apps and the Music page, and the new Music page was live.
- NAS deployment succeeded through UGREEN Docker API using a temporary sync container. The temp container printed `NETEASE_PRIVATE_FM_STABLE_DEPLOY_OK`, then both `xiaozhi-netease-yuyu` and `xiaozhi-netease-xiaocanglan` were restarted successfully.
- NAS logs confirmed both NetEase containers were running and connected to XiaoZhi MCP. Yuyu logs showed private FM autoplay fetching the next song, `>> tools/call self.music.play_url`, `has_lyrics:true`, and local UDP lyric display.

Release asset SHA256:

- `releases/v1.7.85/qdtech-s3-touch-lcd-3.5-v1.7.85-app.bin`: `03739449055d875cb55052c42fa5fbab87eeeb38c9eefa2567d3e7facabbbbb1`
- `releases/v1.7.85/qdtech-s3-touch-lcd-3.5-v1.7.85-full.bin`: `7a63a39cae9833610b515f3f90985d363ff74447de1f225bdfbef882fddb2af4`
- `releases/v1.7.85/qdtech-s3-touch-lcd-3.5-v1.7.85-firmware.zip`: `66a9ad96555caa6c67b98603dd0c6539d41461a5f45374fa98398afe81e8f8fc`

Important NAS status:

- NAS shared path is `/volume1/docker/xiaozhi-mcp-services/netease-music`.
- Running containers are `xiaozhi-netease-yuyu` and `xiaozhi-netease-xiaocanglan`.
- Safe deployment path: copy the three repo files from `tools/nas/` into that NAS shared path, run `node --check netease-mcp.js` and `node --check xiaozhi-ws-mcp.js`, then restart both containers.
- Expected private FM logs: `private fm song selected`, `private fm autoplay scheduled`, `private fm autoplay next selected`, `>> tools/call self.music.play_url`, and lyric UDP lines.

## 2026-07-04 Handoff: v1.7.84 Music Playback Failure Feedback

Current target:

- Firmware version target is now `v1.7.84`.
- Built on macOS with ESP-IDF 5.5 from `/tmp/qdfw-release-184`, build directory `/tmp/qdfw-release-184-build`.
- Release assets were generated in `releases/v1.7.84/`.
- The connected QDTech ESP32-S3 board on `/dev/cu.usbmodem212401` has been flashed with `v1.7.84`; esptool hash verification passed.

What changed:

- Bumped `PROJECT_VER` to `1.7.84`.
- Fixed the most confusing music failure path: when XiaoZhi says it found a song but the music side receives no valid playable URL, a short preview URL, or a rejected/unavailable source, the UI now shows a clear Chinese failure line instead of silently staying idle.
- `self.music.play_url` and `self.music.play` now return stronger guidance to the model: if `Music URL was NOT started` comes back, it must not claim the song already played. It should retry with a complete direct MP3 URL or tell the user the source is unavailable.
- The Music page, XiaoZhi lyric overlay, and `Again` replay failure path now use the same failure feedback, so tapping replay on a bad/stale source also explains what happened.
- Short preview detection still rejects URLs below the full-song threshold, but the status now says `Need full song URL` and the lyric band explains that the current source is only a preview.

Verification:

- Full `idf.py -C /tmp/qdfw-release-184 -B /tmp/qdfw-release-184-build build merge-bin` passed.
- CMake reported `App "xiaozhi" version: 1.7.84`.
- Final app image: `/tmp/qdfw-release-184-build/xiaozhi.bin`, size `0x63c390` / `6538128` bytes; QDTech 7 MB OTA app slot has `0xc3c70` free.
- `merge-bin` generated `merged-binary.bin`, size `0x73c390` / `7586704` bytes.
- Final flash to `/dev/cu.usbmodem212401` completed and esptool hash verification passed.
- Boot log confirmed `App version: 1.7.84`, WiFi connected to `MERCURY_A59F`, IP `192.168.1.104`, MCP music tools registered, `QdEspMqtt: MQTT_EVENT_CONNECTED`, `MQTT: Connected to endpoint`, `Application: STATE: idle`, SNTP time sync, and weather update.
- Startup OTA HTTP check timed out once and firmware correctly continued to MQTT fallback; this is existing network behavior and did not block XiaoZhi service connection.
- Before the version bump, the same code path was tested live with `幸福摩天轮`: the provided URL was detected as a short preview (`len=481115`), playback was not started, and the UI displayed `这个来源只有试听片段，正在等小智换完整版本。`
- Runtime note: BLE phone-config startup still skipped under low internal SRAM, while HTTP config, WiFi, MQTT, weather, and idle state were normal.

Known limitation:

- This firmware does not bypass music copyright/source availability. Songs from popular artists may still return preview clips, empty URLs, 403/404 links, or web pages instead of direct MP3 files; the improvement is that the user now sees why playback did not start.
- Real album artwork is still not available until the music/MCP contract provides a reliable `cover_url` or image payload.
- `lyrics_json` is still kept in RAM only. Songs freshly played during the current boot can restart synced lyrics through `Again`; older persisted recents or post-reboot recents can replay audio but cannot restart exact lyric sync.

Release asset SHA256:

- `releases/v1.7.84/qdtech-s3-touch-lcd-3.5-v1.7.84-app.bin`: `8bb44159139880ae97e43b0418244f58b8794682d58d454eb3cb385210f3e399`
- `releases/v1.7.84/qdtech-s3-touch-lcd-3.5-v1.7.84-full.bin`: `af12f81dd96aeefb1e591f35f5e6883bcd87b8b8416eafd08ff1d2dc5db65232`
- `releases/v1.7.84/qdtech-s3-touch-lcd-3.5-v1.7.84-firmware.zip`: `697e9c6f2c42e366ef3d8028eea7a5fc9fd05e7071c3a3194894cdadbc130439`

## 2026-07-04 Handoff: v1.7.83 Music Page Premium Subtitle Layout

Current target:

- Firmware version target is now `v1.7.83`.
- Built on macOS with ESP-IDF 5.5 from `/Users/tupi/qdtech_release_183_src`, build directory `/Users/tupi/qdtech_release_183_build`.
- Release assets were generated in `releases/v1.7.83/`.
- The connected QDTech ESP32-S3 board on `/dev/cu.usbmodem212401` has been app-only flashed with `v1.7.83` at `0x100000`; esptool hash verification passed.

What changed:

- Bumped `PROJECT_VER` to `1.7.83`.
- Reworked the Music page again after hardware feedback that the large lyric panel looked empty when only one lyric line is available.
- Changed the Music page into a tighter premium player layout: cover visual at upper-left, song title/artist/status at upper-center, a single horizontal subtitle-style lyric band in the middle, recent songs at lower-left, and `Ask / Again / Stop` at lower-right.
- Removed duplicate lyric syncing from the small right-side status area. The small status now only shows short English states such as `Ready`, `Listening`, `Playing`, `Replaying`, or `Error`.
- This also removes the yellow square glyph issue caused by feeding Chinese lyric text into the small English-font status label.
- Kept Chinese lyric rendering only in the main lyric band using the QDTech Chinese font.

Verification:

- Quick `esp-idf/main/libmain.a` build passed from the main workspace.
- Full `idf.py -B /Users/tupi/qdtech_release_183_build build merge-bin` passed from `/Users/tupi/qdtech_release_183_src`.
- CMake reported `App "xiaozhi" version: 1.7.83`.
- Final app image: `/Users/tupi/qdtech_release_183_build/xiaozhi.bin`, size `0x63bf70` / `6537072` bytes; QDTech 7 MB OTA app slot has `0xc4090` free.
- `merge-bin` generated `merged-binary.bin`, size `0x73bf70` / `7585648` bytes.
- Final flash to `/dev/cu.usbmodem212401` completed and esptool hash verification passed.
- Boot log confirmed `App version: 1.7.83`, WiFi connected to `MERCURY_A59F`, IP `192.168.1.104`, MCP music tools registered, `QdEspMqtt: MQTT_EVENT_CONNECTED`, `MQTT: Connected to endpoint`, `Application: STATE: idle`, SNTP time sync, and weather update.
- Startup OTA HTTP check timed out once and firmware correctly continued to MQTT fallback; this is existing network behavior and did not block XiaoZhi service connection.
- Runtime note: BLE phone-config startup still skipped under low internal SRAM, while HTTP config, WiFi, MQTT, weather, and idle state were normal.

Known limitation:

- The firmware still only receives one active lyric line at a time, so the Music page is now designed around a subtitle band rather than a large lyric sheet.
- `lyrics_json` is still kept in RAM only. Songs freshly played during the current boot can restart synced lyrics through `Again`; older persisted recents or post-reboot recents can replay audio but cannot restart exact lyric sync.
- Real album artwork is still not available until the music/MCP contract provides a `cover_url` or image payload.

Release asset SHA256:

- `releases/v1.7.83/qdtech-s3-touch-lcd-3.5-v1.7.83-app.bin`: `282f1fd60e6f4e349a45d135972dad55eb52bf03e104976fe03d3377b1f5a286`
- `releases/v1.7.83/qdtech-s3-touch-lcd-3.5-v1.7.83-full.bin`: `e522bc7a9d2db21002027f268974834b9934e06c348ee88997fb3830fadea9ce`
- `releases/v1.7.83/qdtech-s3-touch-lcd-3.5-v1.7.83-firmware.zip`: `09ef09d277fe5dd679707dc73197543e5de3bd96ec6c8d0020bede9d837b1c28`

## 2026-07-04 Handoff: v1.7.82 Music Lyric Sync and Layout Polish

Current target:

- Firmware version target is now `v1.7.82`.
- Built on macOS with ESP-IDF 5.5 from the clean no-space release worktree `/Users/tupi/qdtech_release_182_src`, build directory `/Users/tupi/qdtech_release_182_build`.
- Release assets were generated in `releases/v1.7.82/`.
- The connected QDTech ESP32-S3 board on `/dev/cu.usbmodem212401` has been app-only flashed with `v1.7.82` at `0x100000`; esptool hash verification passed.

What changed:

- Bumped `PROJECT_VER` to `1.7.82`.
- Music lyric updates no longer force navigation to the XiaoZhi face page. `SetMusicLyric()` now refreshes both the XiaoZhi lyric overlay and the Music page lyric area while leaving the current page alone.
- `self.music.play` and `self.music.play_url` now remember the incoming `lyrics_json` in the Music recent-song RAM entry.
- `Again` / recent replay now passes the remembered `lyrics_json` back into the replay callback and restarts the lyric scheduler when lyrics are available, so replayed songs can keep lyric sync in the same boot session.
- Reworked the Music page again based on hardware feedback: removed the top title/WiFi/time/status clutter, moved the lyrics into a large left-side reading panel, and moved the cover visual plus song title/artist/status and `Ask / Again / Stop` controls to the right.
- The recent-song list is now compact and stays at the lower-left, leaving the main area for readable lyrics.

Verification:

- Quick `esp-idf/main/libmain.a` build passed from the main workspace.
- Full `idf.py -B /Users/tupi/qdtech_release_182_build build merge-bin` passed from `/Users/tupi/qdtech_release_182_src`.
- CMake reported `App "xiaozhi" version: 1.7.82`.
- Final app image: `/Users/tupi/qdtech_release_182_build/xiaozhi.bin`, size `0x63bf70` / `6537072` bytes; QDTech 7 MB OTA app slot has `0xc4090` free.
- `merge-bin` generated `merged-binary.bin`, size `0x73bf70` / `7585648` bytes.
- Final flash to `/dev/cu.usbmodem212401` completed and esptool hash verification passed.
- Boot log confirmed `App version: 1.7.82`, WiFi connected to `MERCURY_A59F`, IP `192.168.1.104`, MCP music tools registered, `QdEspMqtt: MQTT_EVENT_CONNECTED`, `MQTT: Connected to endpoint`, `Application: STATE: idle`, SNTP time sync, and weather update.
- Runtime note: BLE phone-config startup still skipped under low internal SRAM (`skip BLE init, not enough internal memory`), while HTTP config, WiFi, MQTT, weather, and idle state were normal.
- A user touch opened the Apps page and Music page after final flash. Tapping an older persisted recent song started playback and updated the Music page lyric/status path (`SetMusicLyric`), but `lyrics_json length=0`, so exact per-line lyric replay was skipped for that old recent entry as expected.
- End-to-end touch song request and `Again` lyric replay were not re-tested after the final flash because they require physical screen/voice interaction. The code path is wired and compiled; ask the user to test a fresh song after boot, then tap `Again` before reboot to verify the RAM-backed lyrics replay.
- Release asset SHA256:
  - `releases/v1.7.82/qdtech-s3-touch-lcd-3.5-v1.7.82-app.bin`: `04abaa3cb88562affd32220edc47b0762171e8ba86ff2f036506a2bfef2acc20`
  - `releases/v1.7.82/qdtech-s3-touch-lcd-3.5-v1.7.82-full.bin`: `133dfcdbe72622819ea394a1ce6a2f88843b75280b4d1448644a7891eea278ca`
  - `releases/v1.7.82/qdtech-s3-touch-lcd-3.5-v1.7.82-firmware.zip`: `c46560f30870ed2af5b496875120cffbca0a79d5f3e5c31bfae62a1f5839c1d6`

Known limitation:

- `lyrics_json` is kept in RAM only. Songs already saved in NVS before this firmware, or songs replayed after a reboot, still only have title/artist/url, so `Again` can replay audio but cannot restart exact lyric sync unless the song was freshly played during the current boot.
- Real album artwork is still not available. The music/MCP tool contract still only passes `title`, `artist`, `url`, and `lyrics_json`; firmware needs a reliable `cover_url` or image payload before it can show real song covers safely.
- Short/preview MP3 URLs are still rejected by firmware. The music-search side should retry for a full direct MP3 URL when preview rejection is returned.
- Internal SRAM is still tight while XiaoZhi, HTTP probing, music decoding, LVGL, and lyrics are active together.

## 2026-07-04 Handoff: v1.7.81 Music Ask and Playback Hotfix

Current target:

- Firmware version target is now `v1.7.81`.
- Built on macOS with ESP-IDF 5.5 from the clean no-space release worktree `/Users/tupi/qdtech_release_181_src`, build directory `/Users/tupi/qdtech_release_181_build`.
- Release assets were generated in `releases/v1.7.81/`.
- The connected QDTech ESP32-S3 board on `/dev/cu.usbmodem212401` has been app-only flashed with `v1.7.81` at `0x100000`; esptool hash verification passed.

What changed:

- Bumped `PROJECT_VER` to `1.7.81`.
- Fixed the Music page `Ask` action. `v1.7.80` used manual-stop listening from a one-tap button, so the device could keep listening and never submit the spoken song name. `Ask` now uses XiaoZhi's normal auto-stop/realtime chat toggle while staying on the Music page.
- Added an external-audio playback preparation path that stops voice capture/playback, clears voice audio queues, and closes only the voice audio channel before music URL probing/playback. This keeps the MQTT control channel online while freeing internal memory for music and lyrics.
- Music URL playback now uses that synchronous preparation path instead of a delayed state cleanup, so lyrics can start before internal SRAM gets squeezed by the old voice session.
- If a URL is rejected as a short preview, external-audio focus is released immediately.
- Tapping the animated Music cover no longer jumps to the XiaoZhi face page; it now starts the same Music `Ask` flow to avoid accidental page switches near the Ask area.

Verification:

- Quick `esp-idf/main/libmain.a` build passed from the main workspace.
- Full `idf.py -B /Users/tupi/qdtech_release_181_build build merge-bin` passed from `/Users/tupi/qdtech_release_181_src`.
- CMake reported `App "xiaozhi" version: 1.7.81`.
- Final app image: `/Users/tupi/qdtech_release_181_build/xiaozhi.bin`, size `0x63bbc0` / `6536128` bytes; QDTech 7 MB OTA app slot has `0xc4440` free.
- `merge-bin` generated `merged-binary.bin`, size `0x73bbc0` / `7584704` bytes.
- Final flash to `/dev/cu.usbmodem212401` completed and esptool hash verification passed.
- Boot log confirmed `App version: 1.7.81`, WiFi connected to `MERCURY_A59F`, IP `192.168.1.104`, MCP music tools registered, `QdEspMqtt: MQTT_EVENT_CONNECTED`, `MQTT: Connected to endpoint`, and `Application: STATE: idle`.
- Live Music Ask test succeeded: from the Music page, `Ask` entered listening, recognized a spoken song request, searched NetEase, called `self.music.play_url`, opened the stream, and played `莫愁�?- 亞細亞曠世奇才`.
- Lyrics test succeeded for that same playback: `ParseLyricsJson bytes=4213 lines=87`, `play_url lyrics started`, and repeated `SetMusicLyric` updates were observed while music played.
- A second test with `逝去的歌` reached `self.music.play_url` but the provided MP3 was rejected as a short preview (`len=480813`), which is expected behavior; the model/MCP side should retry with a full direct MP3 URL.
- Runtime note: startup OTA/weather HTTP requests can still time out on a slow network, but firmware continues to MQTT and weather retry later recovered.
- Runtime note: repeated touch I2C reset warnings were observed during a long playback test, and internal SRAM minimum can dip very low during music URL probing. Playback and lyric updates continued in the successful test, but this remains an area for future memory/touch-driver cleanup.
- Release asset SHA256:
  - `releases/v1.7.81/qdtech-s3-touch-lcd-3.5-v1.7.81-app.bin`: `8554e22548c1bea004cb841ec2ddf510d3654ce6fdd52211788783b072de17be`
  - `releases/v1.7.81/qdtech-s3-touch-lcd-3.5-v1.7.81-full.bin`: `1293be6c23a7d8036d26220d235ccf07cae276755f656f44481960a8f668a414`
  - `releases/v1.7.81/qdtech-s3-touch-lcd-3.5-v1.7.81-firmware.zip`: `712aa6eee2bec59b14f4bdd91a6ba9223d73072a46f1cafa6198330c1d3edfcb`

Known limitation:

- Real album artwork is still not available. The music/MCP tool contract still only passes `title`, `artist`, `url`, and `lyrics_json`; firmware needs a reliable `cover_url` or image payload before it can show real song covers safely.
- Short/preview MP3 URLs are still rejected by firmware. The next improvement should be on the music-search side: when `self.music.play_url` returns preview rejection, automatically search again for a full direct MP3 URL instead of stopping.
- Internal SRAM is still tight while XiaoZhi, HTTP probing, music decoding, LVGL, and lyrics are active together. Further work should reduce HTTP probe memory or move more transient allocations away from internal RAM.

## 2026-07-04 Handoff: v1.7.80 Music Player Interaction Polish

Current target:

- Firmware version target is now `v1.7.80`.
- Built on macOS with ESP-IDF 5.5 from the clean no-space release worktree `/Users/tupi/qdtech_release_180_src`, build directory `/Users/tupi/qdtech_release_180_build`.
- Release assets were generated in `releases/v1.7.80/`.
- The connected QDTech ESP32-S3 board on `/dev/cu.usbmodem212401` has been app-only flashed with `v1.7.80` at `0x100000`; esptool hash verification passed.

What changed:

- Bumped `PROJECT_VER` to `1.7.80`.
- Music page now has a dedicated right-side lyric panel, so the active lyric/status line is visible outside the center song-info card.
- The center Music card is now focused on title, artist, and current status; right side is for lyrics.
- The left music visual now has lightweight animation: a small disc wobble and bouncing level bars.
- `Ask` no longer forces navigation to the XiaoZhi page. It stays on the Music page, updates the UI to `Listening... say a song name.`, prepares voice interaction, and starts listening from the player page.
- Recent-song clear/remove/replay status updates now also refresh the right-side lyric panel.

Verification:

- Quick `esp-idf/main/libmain.a` build passed from the main workspace.
- Full `idf.py -B /Users/tupi/qdtech_release_180_build build merge-bin` passed from `/Users/tupi/qdtech_release_180_src`.
- CMake reported `App "xiaozhi" version: 1.7.80`.
- Final app image: `/Users/tupi/qdtech_release_180_build/xiaozhi.bin`, size `0x63bc60` / `6536288` bytes; QDTech 7 MB OTA app slot has `0xc43a0` free.
- `merge-bin` generated `merged-binary.bin`, size `0x73bc60` / `7584864` bytes.
- Final flash to `/dev/cu.usbmodem212401` completed and esptool hash verification passed.
- Boot log confirmed `App version: 1.7.80`, `Ota: Current version: 1.7.80`, WiFi connected to `MERCURY_A59F`, IP `192.168.1.104`, MCP music tools registered, `QdEspMqtt: MQTT_EVENT_CONNECTED`, `MQTT: Connected to endpoint`, and `Application: STATE: idle`.
- Runtime note: the first OTA check timed out and MQTT attempt 1 timed out, but the firmware continued with MQTT fallback and attempt 2 connected successfully. Weather fetch later succeeded: `weather ok 26 C 中山 毛毛雨`.
- Runtime note: low internal SRAM still causes BLE phone-config startup to be skipped (`skip BLE init, not enough internal memory`), while HTTP config remains available.
- Release asset SHA256:
  - `releases/v1.7.80/qdtech-s3-touch-lcd-3.5-v1.7.80-app.bin`: `52b9c90a125903826c32169c6534d3a3dfb4d0a69b2e0adeac5a60b729c02183`
  - `releases/v1.7.80/qdtech-s3-touch-lcd-3.5-v1.7.80-full.bin`: `52d511b1f360d96e9f1c0b00c5a79a5d208a821beb53acf0d6b39c7717ae8d7a`
  - `releases/v1.7.80/qdtech-s3-touch-lcd-3.5-v1.7.80-firmware.zip`: `c446e3d90e5bc5ad6ce798f365deee485e2a254b705453ca88e8745ac6858092`

Known limitation:

- The animated left music visual is still firmware-rendered, not real album artwork. The current music/MCP contract only provides `title`, `artist`, `url`, and `lyrics_json`; reliable real cover display needs the music service to pass a `cover_url` or decoded image data to the firmware.
- ESP32-side direct cover scraping by song title is technically possible but risky under the current memory budget because it would add network search, image download, JPEG decode, and cache eviction while music/network/MQTT are already active.
- `Ask` now stays on the Music page, but a full end-to-end song request from touch to playback was not soak-tested after the final flash; boot, network, MQTT, weather, and idle state were verified.

## 2026-07-04 Handoff: v1.7.79 Music Page UI Polish

Current target:

- Firmware version target is now `v1.7.79`.
- Built on macOS with ESP-IDF 5.5 from the clean no-space release worktree `/Users/tupi/qdtech_release_179_src`, build directory `/Users/tupi/qdtech_release_179_build`.
- Release assets were generated in `releases/v1.7.79/`.
- The connected QDTech ESP32-S3 board on `/dev/cu.usbmodem212401` has been app-only flashed with `v1.7.79` at `0x100000`; esptool hash verification passed.

What changed:

- Bumped `PROJECT_VER` to `1.7.79`.
- Reworked the Music page from a cramped button strip into a two-column layout.
- Added a cover-style visual panel on the left with the existing face-page shortcut on tap.
- Enlarged the song/artist/lyric panel so the active lyric line has more room and wraps cleanly.
- Moved recent songs into a compact lower-left list and kept the `Clear` action nearby.
- Replaced the old `Talk / Again / Face / Stop` row with a right-side `Ask / Again / Stop` action column.
- Removed the duplicate visible `Face` button; the cover visual now opens the XiaoZhi face page.
- Changed the default Music line to `Tap Ask and say a song name.`.

Verification:

- Quick `esp-idf/main/libmain.a` build passed from the main workspace.
- Full `idf.py -B /Users/tupi/qdtech_release_179_build build merge-bin` passed from `/Users/tupi/qdtech_release_179_src`.
- CMake reported `App "xiaozhi" version: 1.7.79`.
- Final app image: `/Users/tupi/qdtech_release_179_build/xiaozhi.bin`, size `0x63b1a0` / `6533536` bytes; QDTech 7 MB OTA app slot has `0xc4e60` free.
- `merge-bin` generated `merged-binary.bin`, size `0x73b1a0` / `7582112` bytes.
- Final flash to `/dev/cu.usbmodem212401` completed and esptool hash verification passed.
- Boot log confirmed `App version: 1.7.79`, `Ota: Current version: 1.7.79`, WiFi connected to `MERCURY_A59F`, IP `192.168.1.104`, OTA check succeeded as latest, `QdEspMqtt: connect start attempt=1/2`, `QdEspMqtt: MQTT_EVENT_CONNECTED`, `MQTT: Connected to endpoint`, and `Application: STATE: idle`.
- Weather/time sync also succeeded after boot: `weather ok 26 C 中山 毛毛雨`.
- Runtime note: low internal SRAM still causes BLE phone-config startup to be skipped (`skip BLE init, not enough internal memory`), while HTTP config remains available. This did not prevent WiFi, OTA check, MQTT, idle state, or weather sync.
- Release asset SHA256:
  - `releases/v1.7.79/qdtech-s3-touch-lcd-3.5-v1.7.79-app.bin`: `0d89dbd99c1666c6b289610f6e08764a506da2799895d6e4944925ab0f6e694d`
  - `releases/v1.7.79/qdtech-s3-touch-lcd-3.5-v1.7.79-full.bin`: `1c3a123b6db490b50c459da8ad5a09ee62855a15d73dc18f88805ac7576da520`
  - `releases/v1.7.79/qdtech-s3-touch-lcd-3.5-v1.7.79-firmware.zip`: `7c719f926844faada6b7c47ed8829de410c42de84a90033b43bbec44d57c5fff`

Known limitation:

- The Music cover is a firmware-rendered visual placeholder; the current music/MCP path does not pass real album artwork or related-image URLs into the firmware. Real dynamic covers would require adding image data or an image URL field to the music service/tool contract.
- This release verifies boot, network, OTA, MQTT, idle state, and weather sync, but does not include a long music playback soak test.

## 2026-07-04 Handoff: v1.7.78 QDTech MQTT and Music Retry Stability

Current target:

- Firmware version target is now `v1.7.78`.
- Built on macOS with ESP-IDF 5.5 from the no-space worktree `/Users/tupi/qdtech_worktree_nospace`, build directory `/Users/tupi/qdtech_worktree_build`.
- Release assets were generated in `releases/v1.7.78/`.
- The connected QDTech ESP32-S3 board on `/dev/cu.usbmodem212401` has been app-only flashed with `v1.7.78` at `0x100000`; esptool hash verification passed.

Why:

- `v1.7.77` improved music stream reconnect behavior, but the board still sometimes showed `无法连接服务，请稍后再试` after boot because MQTT could time out at the old 10 second wait even though TLS certificate validation had already succeeded.
- A second music retry issue remained: custom music URL retry attempts were reset after every first decoded frame, so a bad or unstable stream could restart more times than intended.

What changed:

- Bumped `PROJECT_VER` to `1.7.78`.
- Added tracked QDTech-board MQTT implementation files `qd_esp_mqtt.h` and `qd_esp_mqtt.cc` instead of relying on an edited managed component.
- QDTech now creates `QdEspMqtt` from `QdtechS3TouchLcd35Board::CreateMqtt()`.
- `QdEspMqtt` raises the connect wait to 25 seconds, tries startup MQTT connect up to 2 times, waits for connected/error instead of treating a disconnected event as the main result, and sets the underlying MQTT network timeout to the same 25 second window.
- MQTT startup now logs host/port, client/user id lengths, internal free/largest heap, TLS/MQTT error fields, and frees a half-open client on failure.
- Custom music URL retry accounting no longer resets on the first decoded frame; the 3-attempt retry cap now stays meaningful for custom music links.

Verification:

- Quick `esp-idf/main/libmain.a` build passed from the main workspace after adding `QdEspMqtt`.
- Full `idf.py -B /Users/tupi/qdtech_release_178_build build merge-bin` passed from the clean release worktree `/Users/tupi/qdtech_release_178_src`.
- CMake reported `App "xiaozhi" version: 1.7.78`.
- Final app image: `/Users/tupi/qdtech_release_178_build/xiaozhi.bin`, size `0x63b110` / `6533392` bytes; QDTech 7 MB OTA app slot has `0xc4ef0` free.
- `idf.py -B /Users/tupi/qdtech_release_178_build merge-bin` passed and generated `merged-binary.bin`, size `0x73b110` / `7581968` bytes.
- Final flash to `/dev/cu.usbmodem212401` completed and esptool hash verification passed.
- Boot log confirmed `App version: 1.7.78`, `Ota: Current version: 1.7.78`, WiFi connected to `MERCURY_A59F`, IP `192.168.1.104`, OTA check succeeded as latest, `QdEspMqtt: connect start attempt=1/2`, `QdEspMqtt: MQTT_EVENT_CONNECTED`, `MQTT: Connected to endpoint`, and `Application: STATE: idle`.
- Weather/time sync also succeeded after boot: `weather ok 26 C 中山 毛毛雨`.
- Runtime note: low internal SRAM still causes BLE phone-config startup to be skipped (`skip BLE init, not enough internal memory`), while HTTP config remains available. This did not prevent WiFi, OTA check, MQTT, idle state, or weather sync.
- Release asset SHA256:
  - `releases/v1.7.78/qdtech-s3-touch-lcd-3.5-v1.7.78-app.bin`: `c826fd1b7889cdd8bf5fc3ae77649313a56bef89da93b19eb1c09a96c6005255`
  - `releases/v1.7.78/qdtech-s3-touch-lcd-3.5-v1.7.78-full.bin`: `ebc28a76fa5bee1b8dc3e7a340bd2131511c575a5c50e71ea21b50ee0a9f99ea`
  - `releases/v1.7.78/qdtech-s3-touch-lcd-3.5-v1.7.78-firmware.zip`: `589f7e5a6d4026407cbfd309dabba5afc1a202b072233ee80ab6de1c9d041c9b`

Known limitation:

- The firmware is now online to XiaoZhi service in the verified boot path, but a real voice wake/listen conversation was not soak-tested after the final flash. If the screen is idle yet speech still gets no response, capture serial around wake/listen and check whether audio capture sends frames after MQTT is connected.
- Music UI polish is still pending: the Music page is still cramped, has overlapping intent with Talk/Face, and does not yet show a richer lyric plus cover/related-image layout.
- External local music HTTP health previously returned an empty reply in one Mac-side check. Revisit that service only if local HTTP music control is needed.

## 2026-07-04 Handoff: v1.7.77 Music Stream Retry Stabilization

Current target:

- Firmware version target is now `v1.7.77`.
- Built on macOS with ESP-IDF 5.5 from the no-space worktree `/Users/tupi/qdtech_worktree_nospace`, build directory `/Users/tupi/qdtech_worktree_build`.
- Release assets were generated in `releases/v1.7.77/`.

Why:

- After `v1.7.76`, custom music URL playback could still stop mid-song when the HTTP stream stalled or returned transient empty reads.
- The old custom-URL path treated several transient stream failures as final interruption, while radio stations had broader reconnect behavior.

What changed:

- Bumped `PROJECT_VER` to `1.7.77`.
- Custom music URLs now tolerate more empty reads before declaring a stall.
- Custom music URL HTTP read timeout is longer than radio stream timeout, so slow music CDNs have more time to respond.
- Mid-song stalls, read failures, and repeated MP3 decode errors now enter a limited reconnect path instead of immediately stopping playback.
- Custom music URL retries are capped at 3 attempts, with short backoff, so a dead URL does not loop forever.
- Explicit unavailable responses (`401`, `403`, `404`, `410`) still stop as fatal music URL errors.

Verification:

- Quick `esp-idf/main/libmain.a` build passed from the main workspace.
- Full `idf.py -B /Users/tupi/qdtech_worktree_build build merge-bin` passed on macOS ESP-IDF 5.5.
- CMake reported `App "xiaozhi" version: 1.7.77`.
- Final app image: `/Users/tupi/qdtech_worktree_build/xiaozhi.bin`, size `0x6398b0`; QDTech 7 MB OTA app slot has `0xc6750` free.
- Final full image: `/Users/tupi/qdtech_worktree_build/merged-binary.bin`, size `0x7398b0`.
- App-only flash to `/dev/cu.usbmodem212401` at `0x100000` completed and esptool hash verification passed.
- Boot log confirmed `App version: 1.7.77`, `Ota: Current version: 1.7.77`, WiFi connected to `MERCURY_A59F`, IP `192.168.1.104`, MCP music tools registered, and weather sync succeeded.
- Runtime issue still observed after flash: the startup OTA config check timed out, firmware correctly continued with MQTT fallback, but MQTT connection to `mqtt.xiaozhi.me:8883` failed after certificate validation. The device entered idle and showed `无法连接服务，请稍后再试`; this explains "speaking to XiaoZhi has no response" until MQTT connects.
- Mac-side network check from the same workspace could open TCP connections to `mqtt.xiaozhi.me` on ports `1883`, `8883`, `443`, and `80`, so the remaining XiaoZhi-response issue is not simple DNS/domain reachability. Next investigation should capture MQTT auth/credential state and device-side TLS/MQTT memory during `StartMqttClient()`.
- Release asset SHA256:
  - `releases/v1.7.77/qdtech-s3-touch-lcd-3.5-v1.7.77-app.bin`: `52bc42d511d6cd1646b67aa53a99b5c983300d0170caf80de1480ee8528e538c`
  - `releases/v1.7.77/qdtech-s3-touch-lcd-3.5-v1.7.77-full.bin`: `a71a1d2f1d1847d7aebba46cb1fa19a87ca67eb723146c4b7cd49d22d21b7a4c`
  - `releases/v1.7.77/qdtech-s3-touch-lcd-3.5-v1.7.77-firmware.zip`: `98a5c929d3cabedddffae5f1da7dcf425a79138134e15c49ec5eab1b3aa790f6`

Known limitation:

- This improves firmware-side tolerance for network/CDN stalls, but it still cannot turn a preview or expired music URL into a stable full-song URL. If a specific song still stops or refuses to play, capture whether the URL is short/expired or whether the server returns a fatal HTTP status.
- `v1.7.77` does not yet fix the observed MQTT service connection failure. When MQTT fails, wake word/listen can still try to reconnect, but XiaoZhi cannot answer until the protocol connects.

## 2026-07-04 Handoff: v1.7.76 QDTech Stability Hotfix

Current target:

- Firmware stays on `v1.7.76`, rebuilt on Windows with ESP-IDF 5.5 in `build-qdtech-v1.7.76`.
- The same source was flashed to the connected QDTech ESP32-S3 board on `COM3`; esptool verification passed and the board hard-reset cleanly.
- Release assets were regenerated in `releases/v1.7.76/` from the final build.

What changed:

- Weather now uses the HTTPS Open-Meteo endpoint with the ESP certificate bundle, larger HTTP buffers, longer timeout, retry backoff, and a memory guard so the main screen stops failing silently after transient network problems.
- XiaoZhi wake/listen now clears external audio focus and pending decode/send queues before entering voice interaction. This prevents leftover music/radio state from blocking wake or causing slow response.
- Music URL playback now buffers faster, uses larger stream reads, tolerates short empty-read stalls, and avoids restarting custom URLs from the beginning on ordinary status/speech transitions.
- NetEase-style music requests now send stronger HTTP headers and reject very small custom MP3 URLs as likely preview clips. This protects songs such as `Lai Zi Yun De Feng`, where the service returned short preview links that caused stop/replay loops.
- `self.music.play_url` and HTTP/UDP music entry points now start lyrics/recent-song state only after `RadioService::PlayUrlFromTool(...)` confirms the URL was accepted.
- Fixed the observed `LoadProhibited` crash when lyric task creation failed under very low internal SRAM: the fallback lyric line is copied before moving the vector into task args, and lyric display is skipped when internal SRAM is too low.
- Updated the music tool descriptions to ask the model/MCP side for full direct MP3 URLs instead of trial/preview URLs.

Verification:

- `idf.py -B build-qdtech-v1.7.76 build` passed on Windows ESP-IDF 5.5.
- Final app image: `build-qdtech-v1.7.76/xiaozhi.bin`, size `0x6382b0`; QDTech 7 MB OTA app slot has `0xc7d50` free.
- `idf.py -B build-qdtech-v1.7.76 merge-bin` passed and generated `merged-binary.bin`, size `0x7382b0`.
- App-only flash to `COM3` at `0x100000` completed and hash verification passed.
- Serial evidence before the last fix showed the crash root cause: short NetEase preview URL rejection followed by lyric task creation failure with `free_internal=695 largest_internal=288`, then `Guru Meditation Error: LoadProhibited`. The code path now avoids that invalid fallback access.

Known limitation:

- Firmware can reject preview/too-short URLs and ask for a full MP3, but it cannot turn a NetEase preview URL into a full song by itself. If the Mac/remote music service only returns preview links for a song, the correct next fix is on the MCP/music-service side: return a full playable URL or a different source.
## 2026-07-03 Handoff: v1.7.76 OTA Safety, Apps, Music Recent

Current target:

- Source baseline is `v1.7.75` (`origin/main` and tag `v1.7.75` at `3d3985671db9d3ca57cbcd0cd09d8820a3930e02`).
- Firmware version target: `v1.7.76`.
- Goal: ship the post-`v1.7.75` OTA hardening and touch-first Apps/Music polish as a new QDTech candidate release.

What changed:

- `FirmwareUpdateService` now reads the GitHub latest release asset size and, when present, `SHA256SUMS.txt`.
- OTA update checks now block an app asset that is larger than the next OTA partition and show `Need USB flash` instead of offering an impossible OTA install.
- `RunUpgrade()` repeats the partition-size check immediately before starting OTA, so stale UI state cannot bypass the guard.
- `Ota::StartUpgradeFromUrl(...)` accepts expected SHA256 and expected size.
- `Ota::Upgrade(...)` rejects mismatched `Content-Length`, prevents streams from exceeding the OTA partition, validates the final downloaded byte count, and verifies SHA256 before marking the new image bootable.
- During OTA download, the Settings update button and Apps `Settings` tile now show the live percentage instead of staying on `Wait`; the Apps tile keeps the last progress value across shared status refreshes, and the status line still shows percentage plus KB/s speed.
- Release asset selection now prefers the exact `qdtech-s3-touch-lcd-3.5-<tag>-app.bin` match over broader fallback matches.
- The Apps page was expanded from 8 tiles to 10 compact tiles by adding `Music` and `Podcast` entries.
- `Music` opens the XiaoZhi page and starts chat so the user can ask for a NetEase song by voice; `Podcast` opens the podcast list and activates the podcast service path.
- Long-pressing the Apps page `Settings` tile now opens a hidden `Diagnostics` page.
- Diagnostics shows firmware version, board name, running app slot, next OTA slot, OTA app file size/slot/margin, internal heap, PSRAM, WiFi/IP/RSSI, saved WiFi count, battery state, and the last OTA status. It has `Refresh` and `Back` buttons and swipes right back to Apps.
- The Apps `Music` tile now opens a dedicated Music request page instead of jumping straight to the XiaoZhi face.
- The Music page shows the latest song title, artist/source, and current lyric/playback line. It has `Talk`, `Again`, `Face`, and `Stop` buttons: `Talk` opens XiaoZhi listening for a song request, `Again` replays the newest saved recent song, `Face` opens the lyric face page, and `Stop` stops the current music URL/radio playback and clears lyric state.
- Apps tiles now show lightweight live status badges for Radio, Network, Settings/OTA, Music, and Podcast. The badge text/color follows playing/buffering/stopped states, WiFi online/offline state, OTA update/wait/latest state, current music title, and podcast playback state.
- The Apps `Photos` tile now preserves live photo-service state across Apps refreshes, showing refresh/ready/no-photo/SD/decode states while keeping the Photos page itself full-screen and text-free.
- The Apps `NES` tile now preserves live FC emulator state across Apps refreshes, showing scanning/loading/error/selected-ROM/playing states instead of staying on static `SD ROMs`.
- The Settings/OTA tile now keeps oversized firmware states visible as `USB` instead of falling back to a green `Check` badge when OTA is blocked.
- The Apps `Calendar` tile now refreshes through the shared Apps status path and shows the current synced date whenever available.
- The Apps `Focus` tile now shows the live timer state: ready duration, remaining minutes while running, `Paused`, or `Done`.
- The Focus page now supports a smoother pomodoro loop: when a work session finishes, the main button changes to `休息` and starts the 5 minute break; when a break finishes, it changes to `专注` and starts the next 25 minute work session.
- Focus `今日完成` is now date-aware. The completed pomodoro count is stored with a `YYYYMMDD` key and resets automatically when synced time moves to a new day; older saved counts without a date are preserved and stamped to the first synced day after upgrade.
- Startup OTA check failure no longer blocks normal XiaoZhi/MQTT startup; if `api.tenclass.net` times out, firmware logs the failure and continues with the default MQTT path.
- Focus date reconciliation no longer writes NVS from the PSRAM-stack time/weather task, avoiding the cache-disabled flash assert that looked like repeated network failure after WiFi connected.
- The Apps `Network` tile now preserves the latest WiFi status across Apps refreshes and shows the current IP address when connected.
- The Apps `Podcast` tile now preserves its latest playback/list state across Apps refreshes instead of resetting to `Episodes`.
- The Music page now keeps three recent direct-URL songs in NVS under `music_ui`. `self.music.play_url` / `self.music.play` record title, artist, and URL; tapping a recent row promotes it to the top and replays the same URL through `RadioService::PlayUrlFromTool(...)`. The `Again` button replays the newest recent song, the small `Clear` button clears all saved songs, and long-pressing a recent row removes only that song.
- Recent-song replay now has lightweight feedback: the selected row shows `Replaying...`, returns to the normal title once RadioService reaches `Buffering`/`Playing`, and keeps the short RadioService failure reason with an error-colored row if RadioService reports `Error` for that same song. The failure mark remains visible until the user replays, removes, clears, or records a new song.

Verification:

- macOS ESP-IDF 5.5 configured the project for `esp32s3` and generated a 7 MB / 7 MB QDTech partition table.
- The changed objects built successfully:
  - `main/ota.cc`
  - `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc`
  - `main/boards/qdtech-s3-touch-lcd-3.5/firmware_update_service.cc`
- `esp-idf/main/libmain.a` built successfully in the existing configured build directory.
- Rebuilt `esp-idf/main/libmain.a` after adding Diagnostics; it passed. The only warning observed was the existing unused `rgb565` helper in `fc_emulator_service.cc`.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after adding the Music request page; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after adding Apps tile status badges; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj`, `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc.obj`, and `esp-idf/main/libmain.a` after adding Music recent-song replay; all passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after adding Music recent clear; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after adding recent replay pending/failure feedback; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after adding the oversized OTA `USB` badge; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after changing Music recent long-press to remove one row; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after showing RadioService replay failure reasons inline; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after preserving Music recent failure marks across row refresh; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after adding live Focus tile status; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after moving Calendar tile date into the shared Apps status refresh path; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after preserving Network tile status and showing connected IP; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after preserving Podcast tile state across Apps refresh; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after adding Music `Again` one-tap recent replay; both passed.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after adding Focus work/break one-tap cycling; both passed.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after wiring OTA progress into Settings/App tile labels; both passed.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj`, `main/boards/qdtech-s3-touch-lcd-3.5/firmware_update_service.cc.obj`, and `esp-idf/main/libmain.a` after adding OTA file/slot/margin details to Diagnostics; all passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after making Focus `今日完成` date-aware; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after adding live NES tile status; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after adding live Photos tile status; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj` and `esp-idf/main/libmain.a` after preserving OTA progress across Apps tile refreshes; both passed with the same existing `rgb565` warning.
- Rebuilt `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc.obj`, `main/application.cc.obj`, and `esp-idf/main/libmain.a` after the startup-connectivity hotfix; all passed with the same existing `rgb565` warning.
- Full no-space-path build passed from `/Users/tupi/qdtech_worktree_nospace` with build dir `/Users/tupi/qdtech_worktree_build`.
  - Configure command used `-DSDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults'` so QDTech's 7 MB / 7 MB OTA partition table was selected.
  - A fresh component-manager download of `78__esp-opus-encoder` lacked the local `REQUIRES 78__esp-opus` CMake fix; the verification worktree was patched to match the existing local managed component before the final build.
  - Final artifacts were generated: `xiaozhi.elf`, `xiaozhi.bin`, bootloader, partition table, OTA data, and srmodels.
  - Final `v1.7.76` build confirmed `App "xiaozhi" version: 1.7.76`.
  - `xiaozhi.bin` size was `0x639120`; smallest app partition was `0x700000`; free app space was `0xc6ee0` (11%).
  - `idf.py merge-bin` generated `merged-binary.bin` at `0x739120`.
- Local `v1.7.76` release candidate assets were generated in `releases/v1.7.76/`:
  - `releases/v1.7.76/qdtech-s3-touch-lcd-3.5-v1.7.76-app.bin`: `48eef6a10e44dbaa1e9d9b495feac4ca9501792efd9efeb642704879ea20bbc3`
  - `releases/v1.7.76/qdtech-s3-touch-lcd-3.5-v1.7.76-full.bin`: `411fa3a642d1fe9df837924ef7eda75adb1838a62bb96ee2aa60f688944c0e76`
  - `releases/v1.7.76/qdtech-s3-touch-lcd-3.5-v1.7.76-firmware.zip`: `207c302fa006a2fc05596b3bf86baebd12e75b2a7d78305e94055f7420b010d9`
  - `releases/v1.7.76/SHA256SUMS.txt` records the same hashes for OTA verification.
- `PROJECT_VER` was bumped to `1.7.76`.
- `git diff --check` passed.
- Full final link is still blocked only when the repo lives under `/Users/tupi/Documents/带小�?3.5 �?...`: ESP-SR prebuilt library paths are split at the `3.5 寸` directory space, causing `ld: cannot find 3.5`. The no-space worktree build proves the current source changes link successfully.
- A symlink path does not fix the original path because CMake resolves the real source directory. For future full local macOS builds, use a real no-space source path or a Git worktree such as `/Users/tupi/qdtech_worktree_nospace`.

Next recommended work:

- If publishing to GitHub, upload the four `releases/v1.7.76/` assets above and test a real OTA from an older 7 MB-partition build.
- After OTA hardening is shipped, continue the touch-first app work: consider whether Music needs direct search/play queue controls beyond the current voice request and recent-song replay flow.

## 2026-07-01 Handoff: v1.7.75 NetEase Lyric Overlay and Cutover Stability

Current target:

- Firmware version target: `v1.7.75`.
- Branch: `main`.
- Remote: `origin` -> `https://github.com/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware.git`.
- Windows build directory: `build-qdtech-v1.7.62`.
- Hardware flashed and verified on `COM13` with app-only flash at `0x100000`.
- QDTech continues to use `partitions/v1/16m_qdtech_7m_ota.csv` with two 7 MB OTA app slots.

What changed:

- Added a dedicated XiaoZhi music lyric overlay at the bottom of the XiaoZhi page, separate from the ordinary chat/status text path.
- `DesktopUI::SetMusicLyric(...)` now updates the existing XiaoZhi message label and the dedicated overlay, invalidates the objects, and logs `SetMusicLyric overlay line=...`.
- `DesktopUI::ClearMusicLyric()` hides the overlay and is called by `self.music.stop`.
- `self.music.play_url` still preserves the existing playback path, but now clears/replaces old lyric state on every song cutover.
- Scheduled lyrics use a generation guard so the previous song's lyric task exits cleanly when a new song starts.
- External `self.music.show_lyric` / UDP lyric updates are filtered if they belong to a stale song, or if the same song already has scheduled `lyrics_json`.
- If the Mac MCP sends no lyrics for a song, the firmware clears old lyrics and shows `Playing: <title>` instead of leaving the previous song line stuck.
- The lyric parser now handles nested lyric JSON recursively, object arrays, `[time,text]`, `[time,duration,text]`, wrapped LRC, and common alternate field names.
- Fixed a release-blocking crash risk in `ParseLyricsJson`: the recursive fallback no longer reads `lyric_source` after `cJSON_Delete(root)`.
- NetEase direct MP3 URLs now use browser-like headers, and custom URL HTTP `401/403/404/410` stops retries with a UI error instead of looping forever.
- Increased `play_lyrics` and `lyric_udp` task stacks to 8192 bytes in PSRAM.
- Kept the no-`play_music` rule; do not expose that Mac-side tool name.

Verification:

- Built on Windows with ESP-IDF from `C:\Users\Administrator\esp-idf`.
- `ninja -C build-qdtech-v1.7.62 -j 1 all` passed.
- `idf.py -B build-qdtech-v1.7.62 merge-bin` regenerated `merged-binary.bin`.
- `xiaozhi.bin` size is `0x630fb0`; smallest app partition is `0x700000`; free app space is `0xcf050` (12%).
- Full merged image size is `0x730fb0`.
- App-only flashed `build-qdtech-v1.7.62\xiaozhi.bin` to `COM13` at `0x100000`; esptool hash verification passed.
- Boot verification confirmed `App version: 1.7.75`, MQTT connected, and music MCP tools registered.
- Real Mac MCP tests observed:
  - `self.music.play_url title=... lyrics_json length=1945`, `ParseLyricsJson bytes=1945 lines=41`, HTTP `200`, playback frames, and overlay lyric updates.
  - A second song with `lyrics_json length=0` cleared the old lyric task and showed `Playing: <title>` instead of stale lyrics.
  - `self.music.play_url title=... lyrics_json length=4203`, `ParseLyricsJson bytes=4203 lines=87`, HTTP `200`, playback frames, and overlay lyric updates.
  - `self.music.stop` logged `DesktopUI: ClearMusicLyric` and hid the overlay.
- One user-reported second-song crash was not captured because the monitor was interrupted; later monitored song cutovers after the parser lifetime fix did not reproduce it.

Release assets:

- `releases/v1.7.75/qdtech-s3-touch-lcd-3.5-v1.7.75-app.bin`: `bac3a9ef7a7c879a327f9370725879b0385cecc4667cf635c43af0ae81e8a77e`
- `releases/v1.7.75/qdtech-s3-touch-lcd-3.5-v1.7.75-full.bin`: `4a9a0b3ee5cf5aaa50323cecb8881f7a639308a6ff9787e2eee71ef68d2393bd`
- `releases/v1.7.75/qdtech-s3-touch-lcd-3.5-v1.7.75-firmware.zip`: `f7853d03ddc2080338bb76c2e71888389b7e099dc521fa2a2439c0df63134ca2`
- `releases/v1.7.75/SHA256SUMS.txt` records the same hashes.

Important future warning:

- If lyrics do not show, check serial in this order: `lyrics_json length`, `ParseLyricsJson ... lines`, `play_url lyrics started`, `SetMusicLyric overlay line`.
- If audio says "playing" but there is no sound, check `stream open ... status=...`; a `403` means the direct NetEase URL/header or server URL freshness has regressed.
- If another second-song crash appears, capture the full panic/backtrace first; this release specifically removed one suspected use-after-free path in lyric parsing.

## 2026-07-01 Handoff: v1.7.74 NetEase Lyric Display Fix

Current target:

- Firmware version target: `v1.7.74`.
- Branch: `main`.
- Remote: `origin` -> `https://github.com/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware.git`.
- Windows build directory: `build-qdtech-v1.7.62`.
- Hardware burned and verified on `COM14`.
- QDTech continues to use `partitions/v1/16m_qdtech_7m_ota.csv` with two 7 MB OTA app slots.

Root cause:

- The Mac NetEase MCP was already sending `self.music.play_url(title, artist, url, lyrics_json)` with non-empty lyrics and also calling `self.music.show_lyric`.
- Firmware `v1.7.73` received `lyrics_json`, but the lyric scheduler used a normal FreeRTOS task with a 6144 byte internal-RAM stack. During real music playback internal SRAM was around 3-4 KB, so `play_lyrics` task creation failed.
- A second issue appeared after moving past task creation: the `Application::Schedule` UI callback could lag behind music/protocol work, so later lyric lines were logged by the lyric scheduler but did not reliably reach the XiaoZhi bottom label.
- The original parser only accepted one narrow JSON shape, so some valid Mac-side lyric payload variants could parse as zero lines.

What changed:

- Bumped firmware version to `1.7.74`.
- Added a dedicated `DesktopUI::SetMusicLyric(title, artist, line)` path for XiaoZhi music lyrics.
- Music lyric updates now write the XiaoZhi bottom message label directly under the display lock instead of relying on the main application schedule queue.
- Added a short music-lyric hold window so ordinary `SetXiaozhiState` / `SetChatMessage` updates do not immediately clear the lyric line.
- Moved the scheduled lyric task stack to PSRAM using `xTaskCreateWithCaps(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` and `vTaskDeleteWithCaps`.
- Kept `self.music.play_url` playback unchanged and kept the no-`play_music` rule.
- Made lyric parsing tolerant of:
  - arrays of objects with `time_ms` / `timeMs` / `start_ms` and `text` / `line` / `lyric` / `content`;
  - arrays of `[time, text]`;
  - wrapped objects such as `{lyrics:[...]}`, `{lines:[...]}`, or `{lrc:{lyric:"..."}}`;
  - raw LRC strings and nested JSON strings.
- Added logs for:
  - `self.music.play_url ... lyrics_json length=...`;
  - `ParseLyricsJson ... lines=...`;
  - `StartLyricsFromPlayUrl ... lines=...`;
  - `play_url lyrics started ... stack=psram`;
  - `self.music.show_lyric ...`;
  - `ShowMusicLyric request/applied`;
  - `DesktopUI: SetMusicLyric ...`;
  - `SetXiaozhiState skipped during music lyric hold ...`.

Verification:

- Built on Windows with ESP-IDF from `C:\Users\Administrator\esp-idf`.
- `ninja -C build-qdtech-v1.7.62 -j 1 all` passed.
- `idf.py -B build-qdtech-v1.7.62 merge-bin` regenerated `merged-binary.bin`.
- `xiaozhi.bin` size is `0x6307f0`; smallest app partition is `0x700000`; free app space is `0xcf810` (12%).
- Full merged image size is `0x7307f0`.
- App-only flashed `build-qdtech-v1.7.62\xiaozhi.bin` to `COM14` at `0x100000`; esptool hash verification passed.
- Boot verification confirmed `Ota: Current version: 1.7.74`, MQTT connected, and `self.music.play_url`, `self.music.play`, `self.music.stop`, `self.music.show_lyric` registered.
- UDP lyric smoke test confirmed `lyric udp line`, `ShowMusicLyric request`, `DesktopUI: SetMusicLyric`, and `ShowMusicLyric applied`.
- Real Mac MCP point-song test confirmed:
  - board received `% self.music.play_url...`;
  - board logged `self.music.play_url ... lyrics_json length=1800`;
  - `ParseLyricsJson bytes=1800 lines=25`;
  - `play_url lyrics started ... lines=25 stack=psram`;
  - stream opened with HTTP `200` and continuous MP3 frames;
  - repeated `DesktopUI: SetMusicLyric line=...`;
  - `SetXiaozhiState skipped during music lyric hold ...`, proving ordinary dialogue refresh did not clear the lyrics.
- No panic/assert/Guru Meditation was observed during the final lyric playback capture.

Release assets:

- `releases/v1.7.74/qdtech-s3-touch-lcd-3.5-v1.7.74-app.bin`: `4e279b5356aa8bc0b80b8af270cb29d7fd032cb8da7abd9b15fec3d79b428115`
- `releases/v1.7.74/qdtech-s3-touch-lcd-3.5-v1.7.74-firmware.zip`: `ae64b993b7e926ffed89f7f10f3025ff3f98cfa74b21d552cba230d40cb6a11b`
- `releases/v1.7.74/qdtech-s3-touch-lcd-3.5-v1.7.74-full.bin`: `ba2288a300fae5463126363549711a79a0b7615ab45547885d85d8b4c66ce963`

Important future warning:

- Do not expose a Mac-side tool named `play_music`; it conflicts with XiaoZhi's internal tool name.
- If lyrics disappear again, first check board serial for `lyrics_json length > 0`, then `ParseLyricsJson ... lines > 0`, then `play_url lyrics started ... stack=psram`, then `DesktopUI: SetMusicLyric`.

## 2026-07-01 Handoff: v1.7.73 NetEase Second-Song Chain Fix

Current target:

- Firmware version target: `v1.7.73`.
- Hardware burned and verified on `/dev/cu.usbmodem212401`.
- Mac MCP services live in `/Users/tupi/xiaozhi-mcp-services/netease-music`; that directory is not a git repo, so the operational details are mirrored here and in `docs/XIAOZHI_MCP_HANDOFF.md`.

Root cause:

- The Mac NetEase bridge had exposed a compatibility tool named `play_music`.
- XiaoZhi also had an internal `play_music`, so the board logged `Duplicate tool names: play_music`.
- With the duplicated name, the first model turn could detect the song, but later tool routing could stop before a reliable device call.

What changed:

- Removed the public `play_music` compatibility tool from the Mac NetEase bridge; only `music.netease_play` and `music.netease_search` should be listed.
- Kept the required playback chain: after `music.netease_play` returns a fresh NetEase direct MP3 URL, the bridge and/or model must continue with `tools/call self.music.play_url(title, artist, url)`.
- The NetEase result text is now explicit JSON with `next_tool: "self.music.play_url"` and `play_url_arguments`.
- Firmware version was bumped to `1.7.73`.
- Firmware now has a lightweight UDP listener on port `45678` that can display lyric lines and can also accept a JSON `{title, artist, url}` playback packet as a local fallback. On the current Mac network, cloud MCP delivery was the reliable path; local UDP was logged as sent by the Mac but did not reach the board through the active route.
- An experimental HTTP `/music/play_url` helper exists in code but is not started, because starting it consumed scarce internal SRAM and previously caused MQTT/OTA connection failures.

Verification:

- Built on macOS with ESP-IDF from `/Users/tupi/esp/esp-idf-v5.5`; CMake reported `App "xiaozhi" version: 1.7.73`.
- `xiaozhi.bin` size is `0x631ea0`; smallest app partition is `0x700000`; free app space is `0xce160` (12%).
- Flashed to `/dev/cu.usbmodem212401`; esptool hash verification passed.
- Boot log confirmed `App version: 1.7.73`, Wi-Fi IP `192.168.1.114`, `lyric udp listening port=45678`, MQTT connected, and `self.music.play_url` registered.
- First song test: `播放道别是一件难事`.
  - Mac log showed `tool call music.netease_play {"song":"道别是一件难�?}`.
  - Mac log showed `>> tools/call self.music.play_url` with a direct NetEase MP3 URL.
  - Board serial showed `% self.music.play_url...`, `stream open ... status=200`, and continuous `playing station=道别是一件难�?- 上海彩虹室内合唱团` frames.
- Second song test: `播放绿光`.
  - Mac log showed `tool call music.netease_play {"song":"绿光","artist":"孙燕�?}`.
  - Mac log showed a new `>> tools/call self.music.play_url` URL for `绿光`.
  - Board serial showed `% self.music.play_url...`, `stream open station=绿光 - 孙燕�?... status=200`, and continuous playback frames.
- Both NetEase LaunchAgents were restarted after the bridge fix:
  - `com.tupi.xiaozhi.netease.yuyu`
  - `com.tupi.xiaozhi.netease.xiaocanglan`

Release assets:

- `releases/v1.7.73/qdtech-s3-touch-lcd-3.5-v1.7.73-app.bin`: `75b1ef1fd58dfc3e000087405d07dc7e62b497c0a9633900158e8da5dfefb2bb`
- `releases/v1.7.73/qdtech-s3-touch-lcd-3.5-v1.7.73-firmware.zip`: `8eb6a84cb2b707cb7a9e262c1df1518fb4524b5d72c22978d1b119bbaf39c8e9`
- `releases/v1.7.73/qdtech-s3-touch-lcd-3.5-v1.7.73-full.bin`: `387e14e258d78b1bce5c895c030169e57cc2b17aecdf8ee07c72df0399805a71`

Important future warning:

- Do not expose a Mac-side tool named `play_music`; it conflicts with XiaoZhi's internal tool name. Use `music.netease_play` for NetEase search/play and `self.music.play_url` for the board playback tool.
- If playback fails again, first check for `Duplicate tool names: play_music` in board logs, then verify the Mac log has `>> tools/call self.music.play_url`, and finally verify board serial has `% self.music.play_url...` plus `stream open ... status=200`.

## 2026-07-01 Handoff: v1.7.67 Autonomous Goodbye Speech Guard

Current target:

- Firmware version target: `v1.7.67`.
- `v1.7.66` was burned and boot-verified, but hardware logs showed it was incomplete: after about a minute, XiaoZhi said "拜拜啦，下次再聊�?, audio focus paused the NetEase MP3, and the same URL reopened from the beginning.

What changed:

- Keep the 4 second startup grace from `v1.7.66`.
- Add an autonomous-speaking guard: when custom URL music is playing, `kDeviceStateSpeaking` only steals focus if it follows real user interaction states such as listening/connecting/audio-testing.
- If `speaking` appears autonomously while custom music is playing, schedule `AbortSpeaking` and restore idle without stopping the MP3 stream.

Verification:

- Build passed on macOS with ESP-IDF v5.5; CMake reported `App "xiaozhi" version: 1.7.67`.
- `xiaozhi.bin` size is `0x62ee20`; the 7 MB OTA app slot has `0xd11e0` bytes (12%) free.
- Flashed to `/dev/cu.usbmodem212401` at 460800 baud; esptool hash verification passed.
- Boot log confirmed `App version: 1.7.67`, Wi-Fi connected, MQTT connected, idle state, and registered tools including `self.music.play_url`.
- Hardware test with `宇宇` verified first song `道别是一件难�?- 上海彩虹室内合唱团` and second song `暮色森林 - 欧阳娜娜`.
- Mac-side NetEase log showed `music.netease_play` followed by `>> tools/call self.music.play_url` with a fresh NetEase MP3 URL.
- Board serial showed `% self.music.play_url...`, `stream open ... status=200`, and continuous MP3 frames for both songs.
- Autonomous goodbye lines were caught and suppressed:
  - `audio focus speaking ignored during music url playback previous=3`
  - `Abort speaking`
  - MP3 frames continued after `那先不打扰啦，拜拜！`, with no restart.
- Real user taps/listening still pause and resume music; this is expected and was observed during testing.

Release assets:

- `releases/v1.7.67/qdtech-s3-touch-lcd-3.5-v1.7.67-app.bin`: `9791772b54190ec0c0a6de14dace9e87f103e7ce3defbddc1f3024d8579d710f`
- `releases/v1.7.67/qdtech-s3-touch-lcd-3.5-v1.7.67-firmware.zip`: `70adb5a3a0bee4b4d980581beef5652c643fa805de61697a4bf95984291ad303`
- `releases/v1.7.67/qdtech-s3-touch-lcd-3.5-v1.7.67-full.bin`: `3a243f1b0b8c5473fe883e7f336c0a03e841fdc8802b332173406160b5fda529`

## 2026-07-01 Handoff: v1.7.66 Music Startup Speech-Focus Guard

Current target:

- Firmware version target: `v1.7.66`.
- Goal: prevent NetEase direct MP3 playback from restarting when XiaoZhi says a short post-tool line such as "我先退下了" right after music begins.

What changed:

- `RadioService::PlayUrlFromTool(...)` now opens a 4 second custom-URL speaking grace window.
- During that window, `kDeviceStateSpeaking` does not block music audio focus, so the MP3 stream keeps playing instead of stopping and reopening from the beginning.
- `kDeviceStateListening`, `kDeviceStateConnecting`, and `kDeviceStateAudioTesting` still yield focus, so real user interaction can still interrupt music.
- `self.music.play_url` and `self.music.play` tool descriptions now ask the server not to speak extra confirmation after the tool succeeds.

Verification:

- Build passed on macOS with ESP-IDF v5.5; CMake reported `App "xiaozhi" version: 1.7.66`.
- `xiaozhi.bin` size is `0x62ec90`; the 7 MB OTA app slot has `0xd1370` bytes (12%) free.
- Release assets are in `releases/v1.7.66/`.
- 待确�? flash/OTA and hardware test with the user phrase "让多多播放网易云音乐".

Release assets:

- `releases/v1.7.66/qdtech-s3-touch-lcd-3.5-v1.7.66-app.bin`: `dba77d32bca511b539c078f8dee5f76078967a6f4a5b5e34be0c7a2449dd02ef`
- `releases/v1.7.66/qdtech-s3-touch-lcd-3.5-v1.7.66-firmware.zip`: `fb9899d36bfd388c5a0318c01de206d3eafe6d9eed195853ac46e4440f3f5514`
- `releases/v1.7.66/qdtech-s3-touch-lcd-3.5-v1.7.66-full.bin`: `d6799fdfa31922e8a0f35e7c27b82d76d68b7f5e58dffb8101e50eb92a75f306`

## 2026-06-30 Handoff: Mac-Side XiaoZhi MCP Services

New detailed operations handoff:

- `docs/XIAOZHI_MCP_HANDOFF.md`

What is captured there:

- Two simultaneous NetEase MCP services:
  - `宇宇`: LaunchAgent `com.tupi.xiaozhi.netease.yuyu`, local API port `3099`
  - `小苍兰`: LaunchAgent `com.tupi.xiaozhi.netease.xiaocanglan`, local API port `3100`
- Required music chain: every successful `music.netease_play` URL lookup must continue to device tool `self.music.play_url(title, artist, url)`.
- The Mac NetEase bridge now logs the device call result or failure after `self.music.play_url`, so future debugging can distinguish Mac-side lookup success from board-side playback failure.
- OpenClaw/Dodo MCP bridge for `宇宇`, including lightweight `openclaw.ask_dodo`, app opening, service checks, folder creation, and two-step file move/organize confirmation.
- XiaoZhi backend role-prompt rules and the easy-to-miss requirement to restart/reconnect the physical board after saving console configuration.

Important security note:

- The MCP handoff is secret-free. Do not commit `current-endpoint*.txt`, raw endpoint URLs, API keys, or logs that contain tokens.

## 2026-06-30 Handoff: v1.7.65 Final NetEase MCP Music Chain Hardening

Current repo state:

- Branch base: `origin/main` at `v1.7.64`.
- Firmware version target: `v1.7.65`.
- Remote: `origin` -> `https://github.com/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware.git`.
- Local macOS build uses ESP-IDF from `/Users/tupi/esp/esp-idf-v5.5`.
- Build/flash directory: `build-qdtech-s3-final`.
- Hardware flash/serial port used locally: `/dev/cu.usbmodem212401`.
- External NetEase MCP server directory: `/Users/tupi/Documents/论文中期考核/xiaozhi-netease-mcp`.

What changed:

- Kept the remote `v1.7.64` device tool model:
  - `self.music.play_url(title, artist, url)`
  - `self.music.play(title, artist, url)` as a non-conflicting alias
  - `self.music.stop`
  - playback routed through `RadioService::PlayUrlFromTool(...)`
- Hardened the firmware for the field failure observed on macOS hardware:
  - external music no longer stops/restarts the XiaoZhi protocol;
  - `Application::SetExternalAudioActive(true)` now keeps MQTT/MCP online so second-song requests can arrive while the first MP3 stream is active;
  - low-memory `self.music.*` MCP tools can fall back to inline execution instead of being dropped when `tool_call` thread creation fails.
- Non-music tools keep the existing low-internal-memory rejection behavior so heavy tools do not run inline in the MQTT callback path.
- The Mac-side NetEase bridge must continue to call `self.music.play_url` after `music.netease_play` returns a fresh direct MP3 URL. In local testing, `/Users/tupi/Documents/论文中期考核/xiaozhi-netease-mcp/xiaozhi-ws-mcp.js` was updated to log and send `>> tools/call self.music.play_url`.

Verification evidence from the local hardware session:

- Built and flashed from `build-qdtech-s3-final`; CMake reported `App "xiaozhi" version: 1.7.65`.
- App binary size: `0x62ea70`; smallest app partition: `0x700000`; free app space: `0xd1590` (12%).
- Flash to `/dev/cu.usbmodem212401` completed and esptool hash verification passed.
- The Mac-side service log showed `music.netease_play` followed by `>> tools/call self.music.play_url` with distinct NetEase MP3 URLs for consecutive songs.
- Serial logs showed the device receiving `% self.music.play_url...`.
- MP3 playback logs showed continuous decode frames and nonzero PCM peaks, for example `pcm write ... peak=7505`, `peak=10082`, and `peak=14840`, with `output_enabled=1`.
- The user confirmed music playback worked after the final music-chain pass.

Release assets:

- `releases/v1.7.65/qdtech-s3-touch-lcd-3.5-v1.7.65-app.bin`: `db0b2cf10d1d9f344a8df7b372e50e66a6076ed687f3e4f3bd5265ac8d35f639`
- `releases/v1.7.65/qdtech-s3-touch-lcd-3.5-v1.7.65-firmware.zip`: `e0cda4359209694c4124ad7e96bffce557d0cdb11c1642ba5e267da089355405`
- `releases/v1.7.65/qdtech-s3-touch-lcd-3.5-v1.7.65-full.bin`: `00d4b13de11b473a041002b0d37e631f8e9289bd6947ead14af8469206077f53`

Important follow-up:

- If a future report says "server found the song but no board audio", check three logs in order:
  1. Mac service: `>> tools/call self.music.play_url` with a new direct `http://` or `https://` MP3 URL.
  2. Board serial: `% self.music.play_url...` and `music url play requested`.
  3. Board serial: MP3 HTTP/decode logs and nonzero PCM peaks.
- `pthread: Failed to create task` can still appear under low internal SRAM. For `self.music.*` tools in `v1.7.65`, this should fall back inline and is not fatal by itself.

## 2026-06-30 Handoff: v1.7.63 Interruptible NetEase Music URL Playback

Current repo state:

- Branch: `main`.
- Firmware version target: `v1.7.63`.
- Remote: `origin` -> `https://github.com/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware.git`.
- Build directory used on Windows: `build-qdtech-v1.7.62` reused as the incremental v1.7.63 build directory.
- Hardware flash/serial verification: app-only flashed and verified on `COM13`.
- The build continues to use the QDTech 7 MB OTA partition table: `partitions/v1/16m_qdtech_7m_ota.csv`.

What changed:

- Bumped firmware version to `1.7.63`.
- `self.music.play_url` now interrupts any current URL/radio stream before starting the new MP3 URL.
- Added `self.music.stop`, mapped to the same RadioService stop path, so the XiaoZhi server can stop music URL playback explicitly.
- RadioService now uses a stream generation counter so stop/next/previous/new URL can break the HTTP/MP3 playback loop instead of waiting for the old stream.
- Custom music URL playback remembers the prior radio station and restores normal radio controls after stop/next/previous.
- The radio task stays on a PSRAM stack to preserve scarce internal SRAM for MCP calls, but station-index NVS saves are skipped from that PSRAM task to avoid `esp_task_stack_is_sane_cache_disabled()` asserts.
- Playback code copies URL/station data before opening a stream, avoiding dangling references when a second song replaces the transient music station.

Build evidence:

- Single-thread incremental Ninja build completed with `ninja -C build-qdtech-v1.7.62 -j 1 all`.
- CMake reported `App "xiaozhi" version: 1.7.63`.
- Final app binary: `build-qdtech-v1.7.62\xiaozhi.bin`.
- Final app binary size: `0x62c1c0`; smallest app partition: `0x700000`; free space: `0xd3e40` (about 12%).

Hardware evidence:

- App-only flashed `build-qdtech-v1.7.62\xiaozhi.bin` to `COM13` at `0x100000` with esptool `460800`; hash verified.
- Boot log confirmed `App version: 1.7.63`.
- Boot log confirmed `MCP: Add tool: self.music.play_url` and `MCP: Add tool: self.music.stop`.
- Serial test confirmed `self.music.play_url` accepted with internal SRAM available, opened a NetEase direct MP3 URL with HTTP `200`, and decoded continuous MP3 frames at `44100` Hz.
- Radio page test confirmed play, stop, and next station worked without the earlier `spi_flash_disable_interrupts_caches_and_other_cpu ... esp_task_stack_is_sane_cache_disabled()` reboot.

Known follow-up:

- The final post-flash capture confirmed boot stability; one earlier v1.7.63 capture confirmed NetEase MP3 frame output. If the server still answers without audio, capture serial for whether it calls `self.music.play_url` or only the Mac-side NetEase MCP.
- Some touch I2C timeout/reset logs can still appear under heavy audio/network activity; they did not cause this music playback failure.

## 2026-06-29 Handoff: v1.7.62 NetEase MCP URL Playback Bridge

Current repo state:

- Branch: `main`.
- Firmware version target: `v1.7.62`.
- Remote: `origin` -> `https://github.com/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware.git`.
- Build directory: `build-qdtech-v1.7.62`.
- Hardware flash/serial verification: flashed and boot-verified on `COM13`.
- The build uses the v1.7.61 QDTech 7 MB OTA partition table: `partitions/v1/16m_qdtech_7m_ota.csv`.

What changed:

- Added device MCP tool `self.music.play_url`.
- Tool arguments are `title`, `artist`, and `url`.
- The callback calls `RadioService::PlayUrlFromTool(title, artist, url)`.
- `RadioService::PlayUrlFromTool(...)` validates that the URL starts with `http://` or `https://`, creates/updates a transient MP3 station named `title - artist`, switches XiaoZhi back to idle when needed, and posts a custom play command to the existing radio/MP3 playback task.
- Local MCP tool calls now log internal heap, refuse calls when internal heap is too low, and create tool-call threads with internal-memory stacks so NVS/flash operations such as volume persistence do not run on PSRAM stacks.
- Output volume writes are de-duplicated and NVS persistence is throttled to reduce repeated `self.audio_speaker.set_volume` churn.
- Radio and Podcast task startup retain the low-memory guards from the local hardening pass.

Build evidence:

- `idf.py -B build-qdtech-v1.7.62 -D SDKCONFIG="build-qdtech-v1.7.62/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build merge-bin` completed.
- `build-qdtech-v1.7.62\project_description.json` reports `project_version: 1.7.62`.
- `idf.py -B build-qdtech-v1.7.62 size` passed.
- App binary size: `0x62b9b0`; smallest app partition: `0x700000`; free space: `0xd4650` (about 12%).
- Merged binary size: `7518640` bytes.

Hardware evidence:

- Flashed `COM13` at 921600 baud with bootloader, `xiaozhi.bin`, partition table, OTA data, and SR models; every esptool segment hash verified.
- Boot log confirmed `App version: 1.7.62`.
- Boot log confirmed `Ota: Current version: 1.7.62`.
- Boot log confirmed `MCP: Add tool: self.music.play_url`.
- Boot log confirmed WiFi connected to `liutupi`, MQTT connected, and `Application: STATE: idle`.
- Radio startup log showed `radio task create free_internal=63659 largest_internal=63488` and `radio task started stack=6144`.
- No panic, abort, Guru Meditation, or backtrace appeared in the 55 second boot capture.

Release assets:

- `releases/v1.7.62/qdtech-s3-touch-lcd-3.5-v1.7.62-app.bin`: `7B01CDCDE60BDBD9CAACC56526DA9371B8C649074E73C5F4757030A0B18A3EB3`
- `releases/v1.7.62/qdtech-s3-touch-lcd-3.5-v1.7.62-firmware.zip`: `A54638B2B7B8D1275E4B11271CF0E3E3BC94B767115BD08086F953E0694548E7`
- `releases/v1.7.62/qdtech-s3-touch-lcd-3.5-v1.7.62-full.bin`: `75EEB5E43E736D0530D2263D0EE592ACCF51C1690C121BA34BB70ADAAB48D9AC`

Important follow-up:

- On the Mac-side NetEase MCP path, confirm the tool result contains a direct playable `http://` or `https://` MP3 stream URL.
- Confirm the XiaoZhi server/LLM calls device tool `self.music.play_url` after the NetEase search/link tool returns. If logs show only the NetEase MCP call and no `self.music.play_url`, the remaining fix is server prompt/tool-routing, not firmware.

## 2026-06-29 Handoff: v1.7.61 New-Environment WiFi Fallback

Current repo state:

- Branch: `main`.
- Firmware version target: `v1.7.61`.
- Local macOS build used ESP-IDF from `/Users/tupi/esp/esp-idf-v5.5`.
- Build/flash directory: `build-qdtech-s3-final`.
- The QDTech board now uses a 7 MB OTA app partition table (`partitions/v1/16m_qdtech_7m_ota.csv`) so the FC emulator build fits while preserving the NVS offset.
- `build-qdtech-s3-final/xiaozhi.bin` size is `0x62bd30`; the smallest app partition is `0x700000`, leaving `0xd42d0` bytes (12%) free.
- Flashed successfully to `/dev/cu.usbmodem212401` at 921600 baud with bootloader, partition table, OTA data, SR models, and app image; all esptool hash checks passed.
- The local build needed an ignored temporary fix in downloaded `managed_components/78__esp-opus-encoder/CMakeLists.txt` to expose `78__esp-opus` as a public requirement; that dependency fix is not part of the tracked v1.7.61 source changes.

What changed:

- Startup WiFi now trims the saved WiFi list to the newest/default-first 5 entries before attempting to connect.
- All known new-credential paths now trim the saved list to 5 entries after saving:
  - normal `WifiBoard::SwitchToWifi(...)`;
  - provisioning web page `WifiConfigurationAp::Save(...)`;
  - component-level `WifiStation::AddAuth(...)`.
- Startup WiFi connection fallback is shortened from 60 seconds to 30 seconds. If none of the saved networks connect in the new environment, the board stops STA mode and enters the existing `Xiaozhi-xxxx` provisioning hotspot sooner.
- The provisioning scan event no longer attempts to restart a null `scan_timer_`, which keeps manual `/scan` safe in the current pure-SoftAP/manual-scan provisioning flow.
- Fixed the new-environment fallback reboot: `WifiStation::Stop()` now destroys the default STA netif before provisioning starts, and the provisioning AP only creates a STA netif when manual scanning or setup-time joining needs AP+STA mode. This avoids the ESP-IDF duplicate-netif assertion that previously reset the board before the hotspot became visible.

Expected behavior:

- In a known environment, the station scan still matches any visible saved SSID and connects by strongest scan result.
- In a new environment with none of the 5 saved WiFi networks visible/usable, the board should automatically fall back to WiFi provisioning after about 30 seconds.
- Adding a sixth WiFi keeps the newest one and drops the oldest saved entry.

Verification:

- `idf.py -B build-qdtech-s3-final ... set-target esp32s3 build` configured successfully and reported `App "xiaozhi" version: 1.7.61`.
- `idf.py -B build-qdtech-s3-final build` compiled the updated WiFi component files, QDTech board sources, and FC emulator service.
- Size check passed with 7 MB OTA partitions: `xiaozhi.bin` `0x62bd30`, app partition `0x700000`, free `0xd42d0`.
- Boot log after flash confirmed `App version: 1.7.61`, `SKU=qdtech-s3-touch-lcd-3.5`, desktop UI creation, touch max points 5, FC emulator service registration, and WiFi station scan retry logs.
- Hardware verification after the fallback hotfix: after about 30 seconds without a saved WiFi match, the board entered `STATE: configuring`, started SoftAP as `Xiaozhi-AAA9`, started DHCP/web server at `192.168.4.1`, and did not reboot. A client was assigned `192.168.4.2`.

Release assets:

- `releases/v1.7.61/qdtech-s3-touch-lcd-3.5-v1.7.61-app.bin`: `363A0B7FF8F790345636414E5019D563AB0AE8B04CDF244E04D15DA850127591`
- `releases/v1.7.61/qdtech-s3-touch-lcd-3.5-v1.7.61-firmware.zip`: `B6B3A98894B1CEC69F9A36BCE0CC80D0D76F0E39F036D7A40996CB0A954E4716`
- `releases/v1.7.61/qdtech-s3-touch-lcd-3.5-v1.7.61-full.bin`: `809E7081CDC42684C0222BD62CFBAA7EE69BE1887B764418D553A44F2472C98C`

## 2026-06-28 Handoff: v1.7.60 New-Board WiFi Provisioning Fix

Current repo state:

- Branch: `main`.
- Remote used for this handoff: `origin` -> `https://github.com/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware.git`.
- Firmware version target: `v1.7.60`.
- Build directory: `build-qdtech`.
- New board flash/monitor port used: `COM16`.

What changed:

- Changed the QDTech default WiFi BSSID memory to off. This avoids locking a new board to the wrong AP in same-SSID or mesh environments.
- Patched the local `78__esp-wifi-connect` component for this fork so provisioning starts in pure SoftAP mode instead of AP+STA scanning immediately.
- Fixed setup hotspot discoverability by using channel 1, limiting setup AP clients to 2, and setting max TX power.
- Changed WiFi list scanning to run only when the web page requests `/scan`; the handler now preserves the scan results populated by the WiFi scan event instead of reading and clearing them a second time.
- Added setup-time diagnostics for manual scan requests, scan result counts, and STA disconnect reason codes.

Hardware evidence:

- App-only flashed `build-qdtech\xiaozhi.bin` to the new board on `COM16`; esptool hash verification passed.
- Boot log confirmed `App version: 1.7.60`.
- Setup hotspot started as `Xiaozhi-85A1` with `wifi:mode : softAP` and web server at `192.168.4.1`.
- Refreshing the provisioning web page triggered `manual scan done ap_num=15`.
- The scan list included the user's WiFi `liutupi` with strong signal, `RSSI: -7`.

Release assets:

- `releases/v1.7.60/qdtech-s3-touch-lcd-3.5-v1.7.60-app.bin`: `88AB96580CA77073B72E1C66C4322F6AB34AF2244701AC09C158ADAB8368FC1E`
- `releases/v1.7.60/qdtech-s3-touch-lcd-3.5-v1.7.60-firmware.zip`: `9ED8E7DDF38B14758C1B88B6D2E0BBDA1FA62DC443CC65B12F2A35B60FCB04CA`
- `releases/v1.7.60/qdtech-s3-touch-lcd-3.5-v1.7.60-full.bin`: `7772DCF18FBB745F70BEB9EA2BC5D50BFF5584AA0A355CC1B11CEC39F23B286A`

Important follow-up:

- After selecting `liutupi` on the provisioning page, watch serial for `STA disconnected during setup reason=...` if the password still fails. The hotspot/listing problem is fixed; any remaining failure should be treated as credential/auth/router compatibility.
- The patched `managed_components/78__esp-wifi-connect` files are intentionally tracked with `git add -f` because the upstream managed component is normally ignored.

## 2026-06-28 Handoff: v1.7.56 FC/NES Mapper Compatibility Pass

Current repo state:

- Branch: `main`.
- Remote used for this handoff: `origin` -> `https://github.com/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware.git`.
- Firmware version target: `v1.7.56`.
- Build directory: `build-qdtech`.
- Hardware flash port used: `COM13`.
- Latest app-only flash path: `0x100000 build-qdtech\xiaozhi.bin`.

What changed:

- Added CRC-based mapper correction for known problematic FC/NES ROM dumps.
- Added mapper 83 (`Cony/Yoko`) scaffold for Cony/Final Fight style ROMs.
- Added mapper 198 (`TW MMC3+VRAM`) scaffold for the tested `Tun Shi Tian Di 2` dump.
- Added mapper 224 (`KT-008`) scaffold for tested `Xuan Yuan Jian` dumps.
- Added forced FC ROM diagnostic logs before nofrendo starts:
  `rom diag path=... prg=... chr=... header_mapper=... corrected_mapper=... crc=...`.
- Added mapper 198 CHR-RAM PPU write relaxation and 16KB SRAM setup.

Hardware evidence:

- `轩辕剑` dumps now run after CRC correction to mapper 224.
- `快打旋风 [Cony Soft].nes` is confirmed by serial log as `crc=fdec419f`, `header_mapper=4`, `corrected_mapper=83`; it still displays garbled graphics while CPU/audio continue running.
- `吞食天地2 [先锋卡通汉�?(laopix简体中文名字版)].nes` is confirmed by serial log as `crc=3963f12a`, `header_mapper=4`, `corrected_mapper=198`; it still shows black/solid-color output while CPU/audio continue running and frame samples stay effectively constant (`min=max=1084` after startup).

Important limitation:

- Do not report `快打旋风` or `吞食天地2` as fixed. They are identified and routed to candidate mappers, but the nofrendo mapper implementations are still incomplete.
- The next practical fix is to port more of FCEUmm's mapper83 and MMC3/mapper198 behavior, or replace the FC core with a better-supported NES core if flash/PSRAM budget allows.

Build/flash status:

- `idf.py -B build-qdtech build merge-bin` succeeded for v1.7.56 source.
- App partition is nearly full; latest observed free space is about `0x8400` bytes.
- Final v1.7.56 image was app-only flashed to `COM13` at `0x100000`; esptool hash verification passed.
- Post-flash boot log confirmed `App version: 1.7.56`, WiFi IP `192.168.4.177`, MQTT connected, `Application: STATE: idle`, and live Zhongshan weather sync.

Release assets:

- `releases/v1.7.56/qdtech-s3-touch-lcd-3.5-v1.7.56-app.bin`: `F9217510EB5701206158D5AC254D6F90D8B0EFEA6E2C0ADAE3F0756C559793A0`
- `releases/v1.7.56/qdtech-s3-touch-lcd-3.5-v1.7.56-firmware.zip`: `4BD15AAC17BCA83821125490B3C232E9B136DFE5C7D8C99E8BAA9BEEE70158DB`
- `releases/v1.7.56/qdtech-s3-touch-lcd-3.5-v1.7.56-full.bin`: `5E31B83C3F2AF1AFC4C45EC7D23CB344B44927D3E0C59C2757DE8A04D32E7FDC`

## What This Project Is

This repository is a long-term firmware fork based on official `xiaozhi-esp32`, adapted for the QDTech ESP32-S3 3.5 inch touch LCD board.

The current goal is not to rewrite XiaoZhi. The goal is to keep the official voice core working while turning this board port into a stable desktop-system base with display, touch, time/weather, network radio, and XiaoZhi state integration.

Main user GitHub remote:

- `qdtech-new`: `https://github.com/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware.git`

Official upstream remote:

- `origin`: `https://github.com/78/xiaozhi-esp32.git`

## Current Board Entry Point

The board-specific code lives here:

- `main/boards/qdtech-s3-touch-lcd-3.5/`

Important files:

- `config.h`: board pins, display size, audio codec pins, touch pins.
- `qdtech_s3_touch_lcd_3_5.cc`: board class, display init/flush, touch polling, MCP tools, service startup.
- `desktop_ui.cc` / `desktop_ui.h`: LVGL desktop UI.
- `photo_service.cc` / `photo_service.h`: MicroSD JPEG photo slideshow service.
- `radio_service.cc` / `radio_service.h`: MP3 network radio and audio focus behavior.
- `time_weather_service.cc` / `time_weather_service.h`: SNTP clock, weather fetch, cached weather display.
- `firmware_update_service.cc` / `firmware_update_service.h`: Settings-page GitHub release check and on-device OTA trigger.
- `sdkconfig.defaults`: board type defaults for this board.

## Current Verified State

Last verified on 2026-06-27 in the Windows workspace:

- Workspace: `D:\3.5inch_ESP32-S3\qdtech-s3-touch-lcd-3.5-xiaozhi-firmware`
- Branch: `main`
- User remote branch: `origin/main`
- Latest local update: 2026-06-27 v1.7.55 Classic XiaoZhi robot face pack plus daily-card robot avatar
- Last published GitHub release: 2026-06-27 v1.7.55 Classic XiaoZhi robot face pack plus daily-card robot avatar
- Build directory used for board verification: `build-qdtech`
- Serial port used during the last device flash: `COM13`

## Latest Runtime Notes: 2026-06-27 v1.7.55 Classic XiaoZhi Robot Face Pack + Daily Avatar

Scope:

- Bumped firmware version to `1.7.55`.
- Added Classic theme robot GIF faces for standby, listening, and speaking using the same complete-frame approach that was accepted for the Cat and Tupi Warm themes.
- Classic XiaoZhi now uses full robot frames instead of layered LVGL eyes/pupils/highlights/mouth objects, avoiding doubled mouths and misaligned highlights.
- Speaking frames animate the mouth at the correct face position inside the robot art.
- Classic XiaoZhi has a compact bottom subtitle/dialogue strip, and subtitle text is UTF-8-cleaned before display to avoid garbled mixed bytes.
- Board chat-message forwarding now sends only user and assistant text into the XiaoZhi subtitle strip, so system messages do not leak into the UI.
- The main daily-card quote area now includes a compact Classic robot avatar beside the quote, generated from the user-approved reference crop and blended into the dark Classic card background.

Verification:

- Built on Windows with `idf.py -B build-qdtech build merge-bin`.
- CMake/app version: `1.7.55`.
- Final `xiaozhi.bin` size: `0x5f6020`.
- Full merged image size: `0x6f6020`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x9fe0`, about 1%.
- App-only flashed successfully to `COM13` at 921600 baud so WiFi/NVS was preserved.
- Short serial verification confirmed:
  - `App version: 1.7.55`
  - `Ota: Current version: 1.7.55`
  - WiFi connected to `liutupi`, IP `192.168.4.177`
  - MQTT connected
  - `Application: STATE: idle`
  - `Daily card updated for 2026-06-28 kind=quote`
  - `weather ok 27 C 中山 �?00:00 H98% C98%`

Release assets:

- `releases/v1.7.55/qdtech-s3-touch-lcd-3.5-v1.7.55-app.bin`: `5174212A46460B1769475B6DD53E91EC6E4B8AC29424C15B9BC0D20B9263161A`
- `releases/v1.7.55/qdtech-s3-touch-lcd-3.5-v1.7.55-firmware.zip`: `485DE1216D45BC76800706C219426ADF31DA01FFBBEB8F3639467BCE70845E40`
- `releases/v1.7.55/qdtech-s3-touch-lcd-3.5-v1.7.55-full.bin`: `28BE202C60026D040FB9515FED01F5B028AF8E504C1D7DF75031DF1F7711CF7F`

## Latest Runtime Notes: 2026-06-27 v1.7.54 Podcast Cover Memory Recovery + Cat Daily Card Kitten

Scope:

- Bumped firmware version to `1.7.54`.
- Investigated the report that entering the Podcast card no longer showed episode cover images.
- Root cause is most likely memory pressure introduced by the new full-character Cat/Tupi Warm XiaoZhi GIF path:
  - `CreateXiaozhiPage()` runs during UI startup even though the page is hidden.
  - The themed XiaoZhi GIF object and LVGL GIF decoded-data cache could remain allocated while the user is on Media/Podcast.
  - `v1.7.53` boot logs showed only about 5-7 KB free internal SRAM after WiFi/MQTT/Phone Web startup, leaving poor margin for Podcast JPEG cover decode.
- Changed Cat/Tupi Warm XiaoZhi GIF lifecycle:
  - Create the themed GIF only when showing the XiaoZhi page.
  - Delete the themed GIF when leaving the XiaoZhi page.
  - Do not run themed GIF animation updates while another page is active.
- Added Podcast cover decode diagnostics for path/size/input allocation/output allocation/JPEG decode failures.
- Replaced the Cat theme daily-card geometric cat mark with a compact `64x52` RGB565 kitten image derived from the Cat standby face resource.
- The daily-card kitten is static on purpose: it matches the Cat XiaoZhi visual language while avoiding another small GIF allocation on the main page.

Verification:

- Built on Windows with `idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" reconfigure build merge-bin`.
- CMake/app version: `1.7.54`.
- Final `xiaozhi.bin` size: `0x59f910`.
- Full merged image size: `0x69f910`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x606f0`, about 6%.
- Flashed successfully to `COM13` at 921600 baud; flash logs verified every written region.
- Short filtered serial verification confirmed:
  - `App version: 1.7.54`
  - `Ota: Current version: 1.7.54`
  - MQTT connected
  - `Application: STATE: idle`
  - No panic, abort, or backtrace appeared in the captured startup window
- Podcast-card tap verification is still pending; the 90 second filtered serial capture did not include a Podcast page entry or cover decode attempt.

Release assets:

- `releases/v1.7.54/qdtech-s3-touch-lcd-3.5-v1.7.54-app.bin`: `2A4645511374F36D09985C64B1E3D9C7B5D2F0A78557CE3A96BA7CDCE44008D9`
- `releases/v1.7.54/qdtech-s3-touch-lcd-3.5-v1.7.54-firmware.zip`: `D240538F67CA8B60CA3EE7DEB9126FFA9540B515535CCE2DFA6117B020A74CB0`
- `releases/v1.7.54/qdtech-s3-touch-lcd-3.5-v1.7.54-full.bin`: `F2574B34C5BE583E0D9BC1EFF9EF7009A7E64F8CD48605AD29968537741AC048`

## Latest Runtime Notes: 2026-06-27 v1.7.53 Tupi Warm XiaoZhi Animated Robot Face Pack

Scope:

- Bumped firmware version to `1.7.53`.
- Added a Tupi Warm robot GIF pack based on the supplied warm robot reference image.
- Added state resources: standby, listening, speaking, thinking, happy, surprised, sad, angry, and sleepy.
- Connecting, upgrading, and fatal-error paths reuse the closest robot states to preserve app partition space.
- Reused the full-character GIF architecture from the Cat theme so the Tupi Warm XiaoZhi page no longer layers separate LVGL pupils, highlights, or mouth objects over a static face.
- Speaking animation uses complete robot mouth frames inside the GIF, avoiding duplicate mouths or incorrect mouth placement.
- Listening and idle states include small animated accents and breathing/bobbing motion to keep the character alive without adding heavy runtime animation objects.
- Tupi Warm shares the compact status-pill layout and hides the long runtime subtitle to avoid garbled mixed text in the XiaoZhi face area.

Verification:

- Local generated contact-sheet preview was visually checked for size, face placement, mouth placement, and warm-background blending.
- Built on Windows with `idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" reconfigure build merge-bin`.
- CMake/app version: `1.7.53`.
- Final `xiaozhi.bin` size: `0x59dc00`.
- Full merged image size: `0x69dc00`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x62400`, about 6%.
- Flashed successfully to `COM13` at 921600 baud.
- Flash logs confirmed ESP32-S3 with 8 MB PSRAM, app/bootloader/partition/OTA data/srmodels writes, hash verification for every region, and hard reset after flash.
- Short serial verification confirmed:
  - `App version: 1.7.53`
  - `Ota: Current version: 1.7.53`
  - `Desktop UI created`
  - WiFi connected to `liutupi`, IP `192.168.4.177`
  - MQTT connected and app reached `STATE: idle`
  - Weather synced: `weather ok 31 C 中山 多云 15:30 H78% C66%`
  - No panic, abort, or backtrace appeared in the captured startup window
- Touch I2C still logged intermittent transaction failures and a reset after repeated failures; this is the existing separate touch-driver/timing issue and did not stop boot, WiFi, MQTT, or weather.

Release assets:

- `releases/v1.7.53/qdtech-s3-touch-lcd-3.5-v1.7.53-app.bin`: `30931C7F906438ACED6C6DA58DB31156FF265E78556F7D0476A744709C2EA9FB`
- `releases/v1.7.53/qdtech-s3-touch-lcd-3.5-v1.7.53-firmware.zip`: `D47F596A57424F06B459FBA8E9FFB4603C45E712583195DADF2E5995C7604966`
- `releases/v1.7.53/qdtech-s3-touch-lcd-3.5-v1.7.53-full.bin`: `4DB3D8C93B6C1FA98CBB11956A2F7010B66AD0ACE9C8639E680121036A7AB4CD`

## Latest Runtime Notes: 2026-06-27 v1.7.52 Cat Theme XiaoZhi Animated Face Pack

Scope:

- Bumped firmware version to `1.7.52` so boards on `v1.7.51` can receive the Cat XiaoZhi face update through OTA.
- Replaced the Cat theme XiaoZhi vector face with a GIF pack derived from the supplied orange-and-white kitten expression grid.
- Added nine Cat XiaoZhi state resources: standby, listening, speaking, thinking, happy, surprised, sad, angry, and sleepy.
- Kept the original reference proportions, fur color, eye style, cheeks, ears, and soft illustrated finish instead of the simplified vector redraw.
- Speaking animation now switches between complete original-style closed-mouth and open-mouth frames, so the mouth does not overlap or appear twice.
- Removed extra eye highlight/pupil overlays after frame inspection showed they overlapped the reference eyes.
- Removed the Cat XiaoZhi long message subtitle so mixed Chinese/runtime text cannot render as garbled glyphs in the compact status area.

Verification:

- Built on Windows with `idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build merge-bin`.
- CMake/app version: `1.7.52`.
- Final `xiaozhi.bin` size: `0x579c40`.
- Full merged image size: `0x679c40`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x863c0`, about 9%.
- Flashed successfully to `COM13` at 921600 baud.
- Flash logs confirmed ESP32-S3 with 8 MB PSRAM, app/bootloader/partition/OTA data/srmodels writes, hash verification for every region, and hard reset after flash.
- Short serial verification confirmed:
  - `App version: 1.7.52`
  - `Ota: Current version: 1.7.52`
  - `Desktop UI created`
  - WiFi connected to `liutupi`, IP `192.168.4.177`
  - MQTT connected and app reached `STATE: idle`
  - Weather synced: `weather ok 30 C Zhongshan thunderstorm 14:45 H83% C100%`
  - No panic, abort, or backtrace appeared in the captured startup window

Release assets prepared in `releases/v1.7.52/`:

- `qdtech-s3-touch-lcd-3.5-v1.7.52-app.bin`, SHA256 `1A0FBA14020E9927AC7331453E0B26D0B94CBFA9F43D17AC7DC5068E0FC2A125`.
- `qdtech-s3-touch-lcd-3.5-v1.7.52-firmware.zip`, SHA256 `90403EDFC28A4DFD7C5C046CA3C2E0B897FE193387F224102FE4599CC6397A48`.
- `qdtech-s3-touch-lcd-3.5-v1.7.52-full.bin`, SHA256 `963CEE028AE949B70D1CB255CB0D35E24D1AD8EB162BAD49ED47CCCA774A179E`.

Known follow-up:

- Do a physical LCD taste pass in Cat theme XiaoZhi speaking/listening states and tune speaking frame cadence or expression crop placement from hardware feedback if needed.

## Latest Runtime Notes: 2026-06-26 v1.7.50 Radio And Podcast Audio Quality Polish

Scope:

- Bumped firmware version to `1.7.50`.
- Improved network radio output by adding the same style of lightweight automatic gain control and soft limiting used by Podcast playback.
- Removed the old mono-path `* 2` sample boost from both Radio and Podcast output. That boost made quiet mono streams louder, but it could clip voice/music peaks and make some stations or episodes sound harsh.
- Radio now resets its audio leveler whenever a new stream URL is opened, so gain history from one station does not carry into the next station.
- Kept the existing MP3 decode and linear resampling path to avoid adding CPU/memory pressure on the ESP32-S3. This release targets steadier loudness and less clipping, not lossless-quality reconstruction from low-bitrate sources.

Verification:

- Built with `idf.py -B build-qdtech build merge-bin`.
- CMake/app version: `1.7.50`.
- Final `xiaozhi.bin` size: `0x4ad9c0`.
- Full merged image size: `0x5ad9c0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x152640`, about 22%.
- Flashed successfully to `COM13`.
- Boot/runtime logs confirmed:
  - `App version: 1.7.50`.
  - WiFi connected to `liutupi`, IP `192.168.4.177`.
  - MQTT connected and app reached `STATE: idle`.
  - Weather synced: `weather ok 28 C 中山 毛毛�?17:30 H95% C100% raw=55 refined=55`.
  - No reboot, panic, or backtrace during the observed startup window.

Release assets prepared in `releases/v1.7.50/`:

- `qdtech-s3-touch-lcd-3.5-v1.7.50-app.bin`, SHA256 `7546111FD59625F9A3282C75D34AC730BFA4BD0A9289808BE7E96F9D41FB0569`.
- `qdtech-s3-touch-lcd-3.5-v1.7.50-firmware.zip`, SHA256 `60BD51ECDE396FA6197728AE75297641F9774D58426DCE84C0261BC51B46064F`.
- `qdtech-s3-touch-lcd-3.5-v1.7.50-full.bin`, SHA256 `3C218C1EC1930DF9FBE185C50B5E5358C3A7221A5E4895E93F96D47EEDBDAF35`.

Known follow-up:

- Listen-test both a network radio station and several local Podcast episodes on hardware. The expected improvement is less clipping and steadier loudness; low-bitrate 64 kbps radio sources will still sound limited by their source stream.
- For the next real audio-quality jump, prepare SD-card Podcast files with normalized loudness and replace low-bitrate `radio.json` station URLs with higher-bitrate MP3/AAC sources where available.

## Latest Runtime Notes: 2026-06-26 v1.7.49 Photos Portrait Fit And Blurred Background

Scope:

- Bumped firmware version to `1.7.49`.
- Photos still scans JPG/JPEG files from the existing SD-card photo directories; no SD format change is required.
- Landscape photos continue using the existing full-screen cover presentation.
- Portrait photos now fit fully and are centered, instead of being cropped by the previous full-screen cover scaling.
- Portrait photos automatically get a generated same-photo 480x320 background frame:
  - cover-scaled to the screen
  - lightly sampled/blurred
  - darkened for foreground contrast
  - allocated in PSRAM as part of the active photo frame
- The Photos page now uses two image layers per fade slot: background and foreground. Both layers cross-fade together.

Verification:

- Build passed with `idf.py -B build-qdtech build merge-bin`.
- CMake/app version: `1.7.49`.
- Final `xiaozhi.bin` size: `0x4ad8c0`.
- Full merged image size: `0x5ad8c0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x152740`, about 22%.
- Flashed successfully to `COM13`.
- Boot/runtime logs confirmed:
  - `App version: 1.7.49`.
  - WiFi connected to `liutupi`, IP `192.168.4.177`.
  - MQTT connected and app reached `STATE: idle`.
  - Weather synced: `weather ok 28 C 中山 雷雨 15:30 H94% C100% raw=96 refined=96`.
  - Deferred Phone Web services started after the memory guard check.
  - No reboot, panic, or backtrace during the observed startup window.

Release assets prepared in `releases/v1.7.49/`:

- `qdtech-s3-touch-lcd-3.5-v1.7.49-app.bin`, SHA256 `DAD8E4B31DC4E59291A445AEA6F0B3536C3F4BDB901984C7756EF0EFB567495C`.
- `qdtech-s3-touch-lcd-3.5-v1.7.49-firmware.zip`, SHA256 `FDC9556E3FB60F2E3CC52B7A45F5DC4FA1C46035CB94D5E083B0C30586EFCD9D`.
- `qdtech-s3-touch-lcd-3.5-v1.7.49-full.bin`, SHA256 `1012D6463DB5DDF1B09526B6B46FDBE06AC255933732F61E5872CED304608049`.

Known follow-up:

- Physically open Photos with both a 480x320 landscape photo and a 320x480 portrait photo. Confirm portrait people/faces are not cropped and the side background looks soft enough on the real LCD.
- If the generated background feels too sharp on hardware, increase the sampling radius or generate a lower-resolution background first, but watch decode time.

## Latest Runtime Notes: 2026-06-26 v1.7.48 System Smoothness For Podcast And FC

Scope:

- Bumped firmware version to `1.7.48`.
- Changed Podcast from boot-time startup to lazy startup when the Podcast card is opened.
- Reduced Podcast SD-card pressure:
  - `index.json` load no longer reads all episode summaries immediately.
  - list Up/Down no longer decodes JPG covers.
  - selected episode summary loads lazily; cover decoding is reserved for playback/detail work that actually needs it.
- Moved FC/NES ROM validation and startup out of the UI callback path. Play now requests `Loading ROM`, and the FC task performs the SD file validation/start work in the background.
- Added media mutual exclusion:
  - entering Podcast stops Radio and FC.
  - entering FC stops Radio and Podcast.
- Intent: reduce UI stalls, SD-card contention, audio contention, and freeze/deadlock risk when switching between heavy features such as podcasts and games.

Verification:

- Build passed with `idf.py -B build-qdtech build merge-bin`.
- CMake/app version: `1.7.48`.
- Final `xiaozhi.bin` size: `0x4ad510`.
- Full merged image size: `0x5ad510`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x152af0`, about 22%.
- Flashed successfully to `COM13`.
- Boot/runtime logs confirmed:
  - `App version: 1.7.48`.
  - WiFi connected to `liutupi`, IP `192.168.4.177`.
  - MQTT connected and app reached `STATE: idle`.
  - Weather synced without the earlier low-memory loop: `weather ok 27 C 中山 雷雨 14:00 H95% C100% raw=99 refined=99`.
  - Deferred Phone Web services started after memory check.
  - No reboot, panic, or backtrace during the observed startup window.
- Pre-version-bump interaction check opened Media -> Podcast and confirmed lazy task startup, SD mount, and `PodcastService: loaded podcast episodes=80`.

Release assets prepared in `releases/v1.7.48/`:

- `qdtech-s3-touch-lcd-3.5-v1.7.48-app.bin`, SHA256 `C28DD3E38439FA30FEF47EC6498A69CD95DB7FF89A85B6CE88001D36E236646C`.
- `qdtech-s3-touch-lcd-3.5-v1.7.48-firmware.zip`, SHA256 `352154D60BE9BEA082277F35F760FCFD5C3FAB9DEF11A30DE18C75D93E9E11BB`.
- `qdtech-s3-touch-lcd-3.5-v1.7.48-full.bin`, SHA256 `6C0A9E670D18F0E3ED6C4A9BBF56AECCFAAE4F901F3EDBD57EDE723709520A51`.

Known follow-up:

- Have the user stress test repeated Podcast list/detail/play/back cycles and FC ROM loading on the physical board.
- If freezes persist, add a shared SD/media operation guard or central media manager so all SD-heavy services serialize expensive operations explicitly.

## Latest Runtime Notes: 2026-06-26 v1.7.47 SD Podcast Player And Font Fix

Scope:

- Bumped firmware version to `1.7.47` so GitHub/OTA sees this as newer than the previous `v1.7.46` weather/UI release.
- Added a third `Media` page and a `Nothing Impossible` podcast card.
- Added `PodcastService` for local SD-card podcasts at `/sdcard/podcast`.
- Expected SD structure:
  - `/sdcard/podcast/index.json`
  - `/sdcard/podcast/epNNN.mp3`
  - `/sdcard/podcast/epNNN.jpg`
  - `/sdcard/podcast/epNNN.txt`
- `index.json` entries provide `vol`, `title`, `audio`, `cover`, and `desc`.
- The user SD card was prepared with 80 podcast episodes; source files remain outside the repo.
- Podcast UI now has two levels:
  - list view with 8 visible rows and Back / Up / Open / Down controls.
  - detail view with cover on the left, title/summary on the right, progress slider, and List / Prev / Play / Stop / Next controls.
- Removed the Podcast page top status/brand bar so podcast content can use almost the whole 480x320 screen.
- Added seek support by percent through the progress slider.
- Added a lightweight playback leveler to reduce large volume differences between MP3 files.
- Fixed the playback/list-navigation freeze path by not decoding covers or doing heavy UI work from inside the MP3 decode command loop.
- Fixed podcast title mojibake by replacing unsafe byte-count truncation with UTF-8-safe truncation.
- Normalized smart quotes and long dashes to embedded-font-safe punctuation for podcast display.
- Important font lesson: do not use the narrow `qd_font_lxgw_16/20` subset for arbitrary podcast titles. It misses many Chinese glyphs. Podcast dynamic text is back on the broader Puhui font helpers (`qd_cn_font_16/20`). If truly full Chinese coverage is required later, use an SD-card font/font-engine approach rather than small compiled subsets.

Verification:

- Build passed with `idf.py -B build-qdtech build merge-bin`.
- CMake/app version: `1.7.47`.
- Final `xiaozhi.bin` size: `0x4acfe0`.
- Full merged image size: `0x5acfe0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x153020`, about 22%.
- Flashed successfully to `COM13`.
- Boot/runtime logs confirmed:
  - `App version: 1.7.47`.
  - `RadioService: radio sd mounted width=4`.
  - `PodcastService: loaded podcast episodes=80`.
  - Strong WiFi AP selected: `bssid = fc:94:35:08:0a:e8`, RSSI about `-15`.
  - IP address: `192.168.4.177`.
  - MQTT connected and app reached `STATE: idle`.
  - Weather retried after an `ESP_ERR_HTTP_EAGAIN` and then succeeded: `weather ok 28 C 中山 雷雨 13:30 H90% C100% raw=99 refined=99`.

Release assets prepared in `releases/v1.7.47/`:

- `qdtech-s3-touch-lcd-3.5-v1.7.47-app.bin`, SHA256 `481522E7171428C274409EDC8628D6C88A25F4A051302702E0FC105885ECDCF6`.
- `qdtech-s3-touch-lcd-3.5-v1.7.47-firmware.zip`, SHA256 `9FEEEB16EA63AAD5AB5065162053FCD5BD1F0D43808AC8362CE1EBC7822C6481`.
- `qdtech-s3-touch-lcd-3.5-v1.7.47-full.bin`, SHA256 `C5BE37D2832CEE18BF02BB5A7F80D650931367922C0B01122B675C4464B654A0`.

Known issues / next podcast checks:

- The current embedded Puhui font is broader than the LXGW subset but still not a true all-CJK solution. If future podcast titles include rare characters, build a real full-font path, probably SD-card backed.
- The latest hardware check verified boot/services/logs. Visual confirmation of the corrected list text should be done on the physical screen after opening Media -> Podcast.

## Latest Runtime Notes: 2026-06-26 v1.7.46 Font, Phone Web, Brand Earth, And Weather Accuracy

Scope:

- Bumped firmware version to `1.7.46`.
- Improved Chinese text rendering by routing dynamic user/weather/calendar labels through the broader Puhui font instead of narrow Latin/LXGW subsets.
- Added a small transparent rotating earth GIF next to the top-left main-page brand mark. Final accepted direction is earth-only, `46x46`, no satellite, no latitude/longitude grid lines, and a clean blue-white rim to avoid dirty yellow edge artifacts.
- Stabilized slow WiFi reconnect behavior by enabling the remembered strongest BSSID path when absent, so the board prefers the strong `liutupi` AP instead of weak same-SSID APs.
- Delayed Phone Web startup after WiFi and added retry behavior when internal RAM is temporarily too low.
- Lowered guarded HTTP/weather internal-memory thresholds carefully enough to avoid the previous `weather low memory` loop while keeping the device from rebooting.
- Fixed Settings page Phone Web IP persistence by caching and restoring the displayed WiFi config status across UI refresh/theme rebuilds.
- Fixed a theme-rebuild crash by clearing cached status-bar time/battery label pointers before recreating LVGL pages.
- Improved weather accuracy on the existing Open-Meteo fallback path by fetching humidity, precipitation, rain, showers, cloud cover, and API timestamp in addition to temperature/weather code.
- Refined misleading Open-Meteo thunderstorm codes: if raw code is storm/rain but actual precipitation is `0.00`, the UI now downgrades to cloudy/overcast based on cloud cover instead of showing false `Storm`.

Verification:

- Build passed with `idf.py -B build-qdtech build`.
- Merge image passed with `idf.py -B build-qdtech merge-bin`.
- Final `xiaozhi.bin` size: `0x4a55b0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x15aa50`, about 23%.
- Flashed successfully to `COM13`.
- Boot/runtime logs confirmed:
  - `App "xiaozhi" version: 1.7.46` during CMake configure.
  - Strong WiFi AP selected: `bssid = fc:94:35:08:0a:e8`, RSSI about `-16`.
  - IP address: `192.168.4.177`.
  - MQTT connected and app reached `STATE: idle`.
  - Phone Web deferred startup succeeded: `Deferred phone config services started`.
  - Theme switch to Cat rebuilt UI without the previous status-bar pointer crash.
  - Weather refinement worked on live Zhongshan data:

```text
weather ok 28 C 中山 �?00:30 H95% C97% raw=95 refined=3 rain=0.00 cloud=97 humidity=95 updated=00:30
```

- Release assets prepared in `releases/v1.7.46/`:
  - `qdtech-s3-touch-lcd-3.5-v1.7.46-app.bin`, SHA256 `108CFA46D5E7C2EC8E79A835872B5A9F83E79F5713B950A4B33C8FCDE3B9AFCB`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.46-firmware.zip`, SHA256 `266BDA638EAD530B616D9EE74B60365CF0C34FE4D64BF10786648E723BE234F6`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.46-full.bin`, SHA256 `5857F4E0447481C6523079A9F073CABE6821E61F7C4933F707643E6F409A7C7B`.

Known issues and next weather direction:

- Open-Meteo remains the token-free fallback. It is stable and publish-safe, but China-local weather can still be less accurate than domestic providers.
- The preferred higher-accuracy China option is Caiyun realtime weather because it has finer local granularity, but its token must be provisioned locally and must not be committed to this public repository.
- A future safe design is provider priority: local Caiyun token present -> Caiyun realtime; no token or fetch failure -> Open-Meteo refined fallback.

## Latest Runtime Notes: 2026-06-25 v1.7.45 Brand Text Auto-Wrap And Weather Refresh

Scope:

- Bumped firmware version to `1.7.45`.
- Fixed brand text (logo) auto-wrap for long text in Tupi Warm theme.
- Fixed weather refresh when returning to main page from other pages.
- Added BLE config service header include fix for compilation.

Changes:

1. **Brand text auto-wrap (`desktop_ui.cc`)**
   - Changed `fit_brand_label()` from `LV_LABEL_LONG_DOT` (truncation) to `LV_LABEL_LONG_WRAP` (auto-wrap)
   - Added `max_height` limit of 2 lines to prevent overflow
   - Owner name now dynamically positions below brand text using `lv_obj_align_to()`

2. **Weather refresh on page switch (`time_weather_service.cc`)**
   - When returning to main page, `weather_ticks` is now set to `kWeatherRefreshSeconds` to force weather refresh
   - This ensures weather data is updated instead of just showing cached data

3. **BLE compilation fix (`qd_ble_config_service.cc`, `main/CMakeLists.txt`)**
   - Added conditional compilation for BLE headers with `#if CONFIG_BT_NIMBLE_ENABLED`
   - Added fallback `BLE_ATT_ERR_UNLIKELY` definition when BLE is not enabled
   - Added NimBLE include paths in CMakeLists.txt for proper compilation

Verification:

- Build passed with `idf.py -B build-qdtech build`
- `xiaozhi.bin` size: `0x4eae70`
- Smallest app partition: `0x600000`
- Free app partition space: `0x115190`, about 18%
- Flashed successfully to `/dev/cu.usbmodem212401` at 921600 baud
- Post-flash logs confirmed:
  - `App version: 1.7.45`
  - WiFi connected
  - Desktop UI created
  - Weather fetch attempted on page switch
  - Brand text wraps correctly for long text

Known issues:

- Weather fetch may timeout (`ESP_ERR_HTTP_EAGAIN`) when AFE audio processing is active
- This is a resource contention issue, not a code bug

## Latest Runtime Notes: 2026-06-25 v1.7.44 Phone WiFi Profile And Weather Config Sync

- Latest merged firmware base is `v1.7.43`; release target is `v1.7.44`.
- A local WiFi phone config page/API is now the verified sync path because always-on BLE does not fit the current runtime internal-RAM budget.
- Settings now includes a `Phone Web` row showing the URL. On the last verified board run it was `http://192.168.1.111/`.
- Endpoints:
  - `GET /` serves a small phone-friendly HTML form.
  - `GET /config` returns current JSON config.
  - `POST /config` accepts form or JSON fields: `logo`, `name`, `owner`, `city`, `latitude`, `longitude`.
- The phone form now only asks for `Weather City`; latitude/longitude are resolved automatically through Open-Meteo geocoding when the user does not provide manual coordinates.
- Saving profile values now refreshes visible brand labels across Main, Apps, Radio, Network, and Settings directly, without rebuilding the full LVGL UI.
- Brand labels use theme-aware colors again and clamp long text with ellipsis.
- The main-page top-left logo/name overlap was fixed by giving each brand label an explicit single-line height and aligning the owner label directly below the logo label instead of using fragile absolute y offsets.
- Weather summary text now uses a Chinese-capable font so Chinese city names do not render as mojibake or missing glyphs.
- Verified JSON response:

```json
{"logo":"nothing impossible","name":"tupi","city":"Zhongshan","latitude":"22.5176","longitude":"113.3928"}
```

- New WiFi config files:
  - `qd_wifi_config_server.cc/h`: local HTTP page/API for phone sync.
  - `qd_user_config.cc/h`: shared persistent profile/weather config and RAM cache.
- The HTTP server task stack is allocated from PSRAM, while config reads are cached in RAM before server startup. This avoids ESP-IDF flash/NVS cache-disable assertions from PSRAM-stack tasks.
- Runtime POST writes are scheduled onto the existing application/background path, then lightly refresh Settings controls. Do not rebuild the whole LVGL UI from this path; hardware testing showed that can race with status-bar refresh.
- Weather location reads are cached in `TimeWeatherService` before the PSRAM-stack weather task starts, so weather refresh no longer reads NVS from a PSRAM stack.
- Build verification passed from `/tmp/qdtech_firmware_copy` because the original macOS workspace path contains spaces and the ESP-SR component can emit unquoted linker `-L` paths.
- Latest verified build result before the release version bump: `xiaozhi.bin` `0x4ce350`, app partition `0x600000`, free `0x131cb0` (20%).
- Hardware flash passed on `/dev/cu.usbmodem212401`.
- Runtime verification passed after the `v1.7.44` flash: boot log confirmed `App version: 1.7.44`; display/touch/audio codec started; WiFi reconnected to `MERCURY_A59F`; IP was `192.168.1.111`; HTTP config server started; MQTT connected; app reached `STATE: idle`; live weather returned `30 C 深圳 Storm`; Main -> Apps -> Settings navigation worked; no reboot/backtrace occurred during the monitor window.
- Release assets prepared for `v1.7.44`:
  - `qdtech-s3-touch-lcd-3.5-v1.7.44-app.bin`, SHA256 `AAF698866909DB0A6EBB7B35B906D6B3B96D1FFBAACF8F05705513019B4E31B5`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.44-firmware.zip`, SHA256 `BCF321D3F990C7C0DD45C6DF9484C3DDEB4D28CF5C2DD9B4EFBD00347C11C19C`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.44-full.bin`, SHA256 `7109A141405D828D2889A73E811C39EFFCA6860E612B7FC750DFF3F71D52DCC0`.
- HTTP verification passed while the device was idle online:
  - `GET /config` returned the expected JSON.
  - `GET /` returned the HTML form without latitude/longitude input fields.
  - `POST /config` returned `Saving...`, logged `wifi config synced profile=1 weather=1`, refreshed weather, and did not reboot.
- City-only POST verification passed: posting `city=中山` without coordinates resolved to `22.5231,113.3791`, updated `/config`, refreshed weather, and did not reboot.
- English city verification passed: posting `city=dongguan` resolved to `东莞�?23.0180,113.7487`, `/config` updated, weather fetched `31 C 东莞�?Storm`, and no reboot occurred.
- BLE code remains present as a guarded fallback/prototype, but the guard still skips NimBLE startup on current hardware memory (`BLE low memory`) to avoid reboot loops.

## Runtime Notes: 2026-06-25 Phone BLE Profile And Weather Config Sync

- Latest merged firmware base is `v1.7.43`.
- Settings now includes a phone-sync/profile area that shows BLE sync status, the current text logo, owner name, and weather city.
- The top-left brand mark now reads profile values from NVS instead of hardcoded `nothing impossible / tupi`.
- New files:
  - `qd_user_config.cc/h`: persistent profile and weather config wrappers.
  - `qd_ble_config_service.cc/h`: NimBLE GATT service for phone read/write sync.
- BLE peripheral device name is `QDTech-Config` when the low-memory guard allows NimBLE to start.
- BLE characteristic accepts JSON such as:

```json
{"logo":"nothing impossible","name":"tupi","city":"Zhongshan","latitude":"22.5176","longitude":"113.3928"}
```

- Supported write fields: `logo`, `name`, `owner`, `city`, `latitude`, `longitude`.
- Weather sync requires latitude and longitude because the current stable weather backend is Open-Meteo, not a city-name geocoder.
- BLE is enabled through the QDTech board `sdkconfig.defaults`, but startup is guarded by an internal-RAM check to avoid reboot loops.
- Build verification passed from `/tmp/qdtech_firmware_copy` because the original macOS workspace path contains spaces and the ESP-SR component can emit unquoted linker `-L` paths.
- Verification result after the memory guard: `xiaozhi.bin` `0x4c8080`, app partition `0x600000`, free `0x137f80` (20%).
- Hardware flash passed on `/dev/cu.usbmodem212401`; esptool verified bootloader, app, partition table, OTA data, and srmodels hashes.
- Runtime verification passed: display, touch, audio codec, WiFi reconnect to `MERCURY_A59F`, IP `192.168.1.111`, MQTT, live Zhongshan weather, and `Application: STATE: idle`.
- Hardware BLE result: after WiFi startup the guard logged `free_internal=14743` and `largest_internal=14336`, then skipped BLE init with `BLE low memory`. This keeps the firmware stable, but phone BLE read/write sync is not available on the current runtime memory budget.
- Failed BLE placements captured during validation:
  - Starting BLE before WiFi made BLE advertise but WiFi failed with `ESP_ERR_NO_MEM`.
  - Starting BLE after WiFi without the guard hit a BLE controller allocation/assert reboot.
  - Starting BLE before audio caused internal memory pressure in audio startup.

## Runtime Notes: 2026-06-24 v1.7.43 Caiyun Rollback

- Latest release target is `v1.7.43`.
- This release intentionally removes the `v1.7.42` Caiyun weather integration.
- Reason: the Caiyun token had to live in each device's local config, so a fleet of boards could not receive the token purely through OTA, and live testing showed weather sync became unstable on the current network path.
- Weather should use the previous stable Open-Meteo HTTP fetch path in `time_weather_service.cc`.
- Do not reintroduce a private weather API token into the public firmware release unless there is a repo-safe, multi-board provisioning plan.
- `v1.7.42` GitHub Release assets should be removed so new boards do not install the withdrawn Caiyun firmware.
- `v1.7.43` remains numerically newer than `v1.7.42` so boards already flashed with Caiyun firmware can OTA forward to the no-Caiyun stable line.
- Build passed on Windows with `idf.py -B build-qdtech build`: `xiaozhi.bin` `0x492650`, smallest app partition `0x600000`, free `0x16d9b0`.
- GitHub Release `v1.7.42` and tag were deleted after the rollback because that release contained the withdrawn Caiyun firmware.
- GitHub Release `v1.7.43` was published with app, full, and zip assets.
- Hardware verification on `COM13` after app-only flashing:
  - Boot log confirmed `App version: 1.7.43`.
  - WiFi reconnected to IP `192.168.4.177`.
  - SNTP synchronized.
  - Weather returned through the restored Open-Meteo path: `weather ok 34 C Zhongshan Storm 15:34 code=95 updated=15:34`.
  - MQTT connected and the application reached `STATE: idle`.
  - No assert, backtrace, or reboot was observed during the post-flash monitor window.
- Release assets prepared in `releases/v1.7.43/`:
  - `qdtech-s3-touch-lcd-3.5-v1.7.43-app.bin`, SHA256 `0AB583C0B731A373B6E66D25D066AC14C2D1B96F7D3CF625F63139D360F2EA08`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.43-firmware.zip`, SHA256 `F74A81A7866724288E9A24A646497540F7B576FA88E9BC76BABF6BF07742D30A`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.43-full.bin`, SHA256 `36A5D0C43D20804F03E2FEBA55806C6D45A4D7976883AFDF0489DE44B7913EAA`.

## Latest Runtime Notes: 2026-06-23 Code Quality & SD Card Radio

### Code Quality Optimizations (P0-P2)

1. **RadioService thread safety**
   - `play_requested_` and `stop_requested_` changed from `bool` to `std::atomic<bool>`
   - Prevents race conditions on dual-core ESP32-S3

2. **last_success_url_ array overflow fix**
   - Changed from fixed `[16]` to `std::vector<int>`
   - Resized dynamically based on actual station count (37+)

3. **emotion_ dangling pointer fix**
   - Changed from `const char*` to `std::string` in DesktopUI
   - Prevents dangling pointer from temporary strings

4. **FC emulator dead code cleanup**
   - Removed `LoadNesRom()`, `RunNesFrame()`, `RenderFrame()` functions
   - Removed `nes_bus.cc/h`, `nes_cpu.cc/h`, `nes_ppu.cc/h` files
   - Saved ~5-10KB in compiled binary

5. **RadioService PSRAM fallback**
   - Task stack now uses internal RAM (not PSRAM) to avoid cache-disable crash during NVS access

6. **WritePcm persistent buffer**
   - Added `pcm_mono_buf_` and `pcm_output_buf_` member variables
   - Avoids per-frame heap allocation/deallocation

7. **Weather particle limit**
   - Added `kMaxParticles = 12` limit in `WeatherParticleCb`
   - Prevents internal RAM exhaustion from too many particle objects

8. **Theme switch without restart**
   - `CycleTheme()` now clears and rebuilds UI instead of calling `esp_restart()`

### SD Card Radio Configuration

RadioService now supports loading stations from `/sdcard/radio.json`:

```json
{
  "stations": [
    {
      "name": "Station Name",
      "url": "http://stream.url/mp3",
      "url2": "backup_url",
      "url3": "third_backup",
      "codec": "MP3",
      "bitrate": 64,
      "category": "national"
    }
  ]
}
```

Supported categories: `national`, `beijing`, `shanghai`, `guangdong`, `zhejiang`, `jiangsu`, `sichuan`, `hunan`, `hubei`, `shandong`, `music`, `traffic`, `other`

If `radio.json` is not found, firmware uses built-in 37 stations.

### FC Emulator Stability Fixes

1. **selected_index_ thread safety**
   - Changed from `size_t` to `std::atomic<size_t>`
   - All accesses use `load()`/`store()` with proper memory ordering

2. **Boundary checks added**
   - `HandleCommand()`, `NextStation()`, `PlayCurrentStation()` all validate indices
   - Prevents crash from out-of-bounds vector access

3. **FC display optimization**
   - Palette pre-swaps bytes in `qd_video_set_palette()`
   - Added `DrawLandscapeRgb565PreSwapped()` fast path
   - Removed per-frame 256->320 scaling (now uses native 256x240)

### Radio Page Chinese Font Fix

- Radio station label now uses `font_puhui_16_4` instead of `lv_font_montserrat_20`
- Supports Chinese characters in station names

Observed boot/runtime facts after flashing:

- Firmware logs `SKU=qdtech-s3-touch-lcd-3.5`.
- Desktop UI is created.
- Touch controller reports `Touch max points: 5`.
- WiFi connected during the last run.
- OTA check completed and reported latest firmware.
- MQTT connected.
- AFE/wake word started.
- Application reached `STATE: idle`.
- XiaoZhi AI chat was tested after the stability repair: the device entered `STATE: listening`, opened the audio channel, reached `STATE: speaking`, and did not reboot during the verified test.
- SNTP time synchronization completed.
- Daily card updated after SNTP sync and rendered date-linked Chinese content with embedded LXGW WenKai subset fonts.
- Weather visual mapping updated after weather fetch; captured log showed cloudy visual mapping for Open-Meteo code `1`.
- Apps tile `FOC / Focus / 25 min` opens the local Focus Timer page instead of starting a XiaoZhi quote chat.
- Focus Timer was reworked into a sparse three-column layout after the first version visually overlapped on the 480x320 display.
- Settings now uses a scrollable layout that fits the 480x320 display.
- Settings brightness and volume sliders read the current hardware values when the page opens and persist changes when the user releases the slider.
- The current manual touch path has an explicit Settings adapter for slider release, vertical scrolling, and Back navigation until standard LVGL input migration.
- Navigation now follows Main -> Apps -> feature pages, with right-swipe back; Photos preserves left-or-right swipe exit.
- Leaving the Radio page does not stop playback. Radio Stop is an explicit user action.
- Calendar tile opens the local Calendar page.
- Calendar Next button changed the displayed month during hardware testing.
- Photos page exists and the photo task is lazy-started only when the Photos page is opened.
- Photos were verified on hardware after the SD scan and display fixes: the slideshow reads the prepared MicroSD photos, colors render normally, images fill the 480x320 screen, no text controls are shown, and left/right swipe exits back to Apps.
- PhotoService now allocates its 6144-byte task stack from PSRAM first and logs internal-memory diagnostics; a boot self-test confirmed SD mount and repeated 480x320 JPEG decode after the black-screen repair.
- Weather API may return 429 or 502; the firmware should keep running and retain cached data when available.

## Latest Runtime Notes: 2026-06-23 v1.7.39 OTA Verified

- Latest release target is `v1.7.39`.
- Current GitHub head after the OTA work: `4e30dcb` on `main`.
- Current board used for validation: `COM13`.
- `v1.7.38` is the real OTA updater fix: a persistent internal-RAM `ota_flash` worker is allocated before the firmware HTTPS/proxy connection opens; it owns `esp_ota_begin()`, `esp_ota_write()`, `esp_ota_end()`, and `esp_ota_set_boot_partition()`.
- The HTTPS/proxy download path uses proxy candidates first for GitHub Release assets. The verified attempt logged `Firmware download attempt 1/3: proxy` and `Firmware download proxy selected`.
- The `v1.7.39` release is a version-only validation target for the `v1.7.38` updater.
- Hardware OTA verification succeeded from `v1.7.38` to `v1.7.39`:
  - Settings updater selected `qdtech-s3-touch-lcd-3.5-v1.7.39-app.bin`.
  - OTA wrote to `ota_1` at `0x700000`.
  - Progress reached `100% (4784848/4784848)`.
  - Runtime logged `Firmware upgrade successful, rebooting in 3 seconds...`.
  - Rebooted firmware logged `App version: 1.7.39`.
  - Boot validation logged `Running partition: ota_1` and `Marking firmware as valid`.
- Important test note: do not reopen the USB serial monitor while OTA is in progress. Opening the serial port resets this board and interrupts the OTA. Start one monitor before tapping Update and leave it open until reboot completes.
- Still observed during long OTA: repeated touch I2C timeout/reset logs. They did not stop OTA, but touch stability should be investigated separately if the user reports missed taps.

## Previous Runtime Notes: 2026-06-23 v1.7.32 OTA Low-Internal-RAM Upgrade Fit

- Release target was `v1.7.32`.
- Follow-up OTA testing proved `v1.7.31` could check GitHub and create the upgrade task, but the 6144-byte internal upgrade stack left too little internal heap for the OTA image header and download buffer.
- The new fit is a 4096-byte internal upgrade stack plus 1024-byte HTTP firmware download buffer.
- `COM14` was USB-flashed with a `v1.7.31` bootstrap build containing these reductions before preparing `v1.7.32`.
- `v1.7.32` build passed with `idf.py -B build-qdtech build`; `xiaozhi.bin` size is `0x48f980`, smallest app partition is `0x600000`, and free app space is `0x170680`, about 24%.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.32-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.32-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.32-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.32-app.bin`: `2a5cb26b3587c79356a8ac033a7a3d07acfd136d879eb11919336d468801c248`
  - `qdtech-s3-touch-lcd-3.5-v1.7.32-firmware.zip`: `a94bace312395293d5b96c3395a9e4fa1e741145af7000e5eb4e03521902e10a`
  - `qdtech-s3-touch-lcd-3.5-v1.7.32-full.bin`: `e3f1230680314d43ff71f5e1b79a4197293cf8380e1a1964672da722d1524e73`

## Previous Runtime Notes: 2026-06-23 v1.7.31 OTA Check/Upgrade Task Split

- Release target was `v1.7.31`.
- Follow-up testing found that `v1.7.30` was still too strict: it forced the 10KB firmware check task into internal RAM, but the live board usually had only about 7KB largest internal block, so check task creation failed before it could find the release.
- The corrected split is: firmware check task uses PSRAM stack because it only fetches/parses GitHub release metadata; firmware upgrade task uses internal stack because it writes flash.
- If an upgrade task cannot get an internal stack, the updater shows `No internal RAM` and refuses the unsafe fallback.
- `COM14` was USB-flashed with the fixed `v1.7.30` bootstrap build before preparing `v1.7.31`.
- `v1.7.31` build passed with `idf.py -B build-qdtech build`; `xiaozhi.bin` size is `0x48f980`, smallest app partition is `0x600000`, and free app space is `0x170680`, about 24%.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.31-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.31-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.31-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.31-app.bin`: `a49538f4186d84cbaf049d5c29c444addce142320306bd0bbb498d93a6f1d5fd`
  - `qdtech-s3-touch-lcd-3.5-v1.7.31-firmware.zip`: `d6f84cfcb9103f3df0859007c35163e8764edbd457e6dcc0c82f283163318608`
  - `qdtech-s3-touch-lcd-3.5-v1.7.31-full.bin`: `c8b7bbd5ad5e5b863da67f70f4273a452bd12b0590b3e3c725832a32d62e0f0c`

## Previous Runtime Notes: 2026-06-23 v1.7.30 OTA Internal-RAM Write Fix

- Release target was `v1.7.30`.
- A new board running `v1.7.28` reproduced the Settings OTA failure on `COM14`.
- Captured log sequence: the board selected the GitHub Release app asset, direct GitHub download timed out, proxy download was selected, the image header was read as the newer version, and then ESP-IDF asserted in `esp_cache_freeze_caches_disable_interrupts` during OTA flash-write handling.
- This proves the blocker was not simply lack of GitHub access. The proxy path was already able to reach the app image.
- Root cause: the firmware upgrade task used a PSRAM stack first. Flash erase/write disables cache, so code running with a PSRAM task stack can trip `s_task_stack_is_sane_when_cache_frozen()`.
- Fix: create firmware update tasks with an internal-RAM stack first, reduce the upgrade stack to fit the observed internal largest block, move the OTA HTTP download buffer from stack to internal heap, and replace the OTA image-header `std::string` accumulation with an internal-RAM fixed header buffer.
- Bootstrap limitation: boards already on `v1.7.28` need one USB flash because the old updater crashes before it can install the fixed updater. After flashing `v1.7.29` or newer once, future OTA updates use the fixed path.
- `COM14` was USB-flashed with the fixed `v1.7.29` bootstrap build before preparing `v1.7.30`.
- `v1.7.30` build passed with `idf.py -B build-qdtech build`; `xiaozhi.bin` size is `0x48f850`, smallest app partition is `0x600000`, and free app space is `0x1707b0`, about 24%.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.30-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.30-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.30-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.30-app.bin`: `476ebaa1ea9afa1fad285e815a754d4815c94bd4e3fc552006fa54e9331ed620`
  - `qdtech-s3-touch-lcd-3.5-v1.7.30-firmware.zip`: `0e780c5bfdeb7fd3e490af80e82cffe7152380f8e2ae26ef4731409646cab58a`
  - `qdtech-s3-touch-lcd-3.5-v1.7.30-full.bin`: `b3a33e954bde02dec6cd96297dcbd544f692e6c8e4cdcab076effb015723a598`

## Previous Runtime Notes: 2026-06-23 v1.7.29 Weather Scene GIF Pack

- Latest release target is `v1.7.29`.
- User rejected the LVGL primitive weather animation quality, especially the storm effect, and asked for a richer/惊艳 visual approach.
- Implemented the replacement route as a GIF scene player rather than more LVGL circles/bars/lines.
- QDTech board type now selects `LV_USE_GIF` and `LV_GIF_CACHE_DECODE_DATA`.
- Added six generated weather scene resources in `main/boards/qdtech-s3-touch-lcd-3.5/`:
  - `qd_weather_clear_scene.c`
  - `qd_weather_cloudy_scene.c`
  - `qd_weather_rain_scene.c`
  - `qd_weather_snow_scene.c`
  - `qd_weather_fog_scene.c`
  - `qd_weather_storm_scene.c`
- `desktop_ui.cc` declares all six `lv_img_dsc_t` resources and creates one 142x84 `lv_gif` object in the weather card. The bottom temperature and summary labels remain outside the animation area.
- Weather scene mapping:
  - `scene=0`: clear, weather code `0`
  - `scene=1`: cloudy/pending, weather codes `1`, `2`, `3`, and default/pending
  - `scene=2`: rain, weather codes `51-67` and `80-82`
  - `scene=3`: snow, weather codes `71-77`
  - `scene=4`: fog, weather codes `45` and `48`
  - `scene=5`: storm, weather codes `95+`
- Older LVGL shape-based weather objects still exist as fallback code, but normal QDTech runtime hides them while the GIF scene player is available.
- Build verification passed with `idf.py -B build-qdtech build`; `xiaozhi.bin` size is `0x48f920`, smallest app partition is `0x600000`, and free app space is `0x1706e0`, about 24%.
- Flashed successfully to the currently connected board on `COM13` at 921600 baud.
- Captured boot logs confirmed `App version: 1.7.29`, QDTech startup, `Desktop UI created`, saved WiFi reconnect, MQTT connection, and `Application: STATE: idle`.
- Runtime weather logs confirmed pending/default `scene=1`, then live Zhongshan storm weather `code=95` switching to `scene=5`.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.29-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.29-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.29-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.29-app.bin`: `483dd14a036b6b66861729889a6525286be102287b47d48a555e3679d85f06e1`
  - `qdtech-s3-touch-lcd-3.5-v1.7.29-firmware.zip`: `9c7170cc4e8a73beb1a73b094a6dae216ae2558b6ed48746b1d821fe2e446557`
  - `qdtech-s3-touch-lcd-3.5-v1.7.29-full.bin`: `e5d99dab73860a5834c6c9f7d560f02a50846dc4fc0b968aba501913046063ed`
- Follow-up: storm was verified by live weather on COM13. Clear, cloudy, rain, snow, and fog still need a visual hardware pass by forcing weather codes or waiting for real weather conditions. App partition free space is now 24%, so be careful with additional embedded bitmap/GIF assets.

## Previous Runtime Notes: 2026-06-23 v1.7.28 GitHub OTA Proxy Fallback

- Latest release target is `v1.7.28`.
- User reported that a board on `v1.7.26` could detect `v1.7.27`, but pressing Update failed and appeared to restart/return without updating.
- Serial capture on `COM14` showed the exact failure:
  - `FirmwareUpdate: firmware update ready current=1.7.26 latest=1.7.27`
  - `Ota: Upgrading firmware from https://github.com/Liutupi/.../qdtech-s3-touch-lcd-3.5-v1.7.27-app.bin`
  - `Ota: Writing to partition ota_1 at offset 0x700000`
  - `esp-tls: Failed to open new connection in specified timeout`
  - `HTTP_CLIENT: Connection failed, sock < 0`
  - `Ota: Failed to open firmware HTTP connection: ESP_ERR_HTTP_CONNECT`
  - The app returned to `STATE: idle`; no panic or partition-write failure was captured.
- Root cause: the Settings updater could read GitHub Release metadata intermittently, but the ESP32 could not reliably open the GitHub Release asset download URL from this network. This is a GitHub connectivity/proxy problem, not a bad app image or OTA partition problem.
- Fix: firmware downloads now try the original GitHub asset URL first, then fall back to proxy-prefixed URLs using `https://ghfast.top/` and `https://gh-proxy.com/` for `https://github.com/...` assets. The log prints direct/proxy attempt numbers.
- Release metadata fetch now retries once before reporting Check failed.
- Important bootstrap limitation: boards already running `v1.7.27` or older do not contain this fallback logic, so they may still need one USB flash to `v1.7.28`. OTA releases after this baseline should be able to use the proxy fallback.
- Build verification passed with `idf.py -B build-qdtech build`; `xiaozhi.bin` size is `0x3d7e10`, smallest app partition is `0x600000`, and free app space is `0x2281f0`, about 36%.
- Flashed successfully to the currently connected board on `COM14` at 921600 baud.
- Captured boot logs confirmed `App version: 1.7.28`, QDTech startup, `Desktop UI created`, saved WiFi reconnect, `Ota: Current version: 1.7.28`, MQTT connection, weather update, and `Application: STATE: idle`.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.28-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.28-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.28-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.28-app.bin`: `8cddf5b367c5dfb108840127743ae208bdcc2f34e20b3e44e3d0a5468cd1a029`
  - `qdtech-s3-touch-lcd-3.5-v1.7.28-firmware.zip`: `777be58c569dce64035b66363da943e97109b082eafed0a2bd34f1d2906a7806`
  - `qdtech-s3-touch-lcd-3.5-v1.7.28-full.bin`: `5e6b9fb0df401a7a4ddc65f110624dbab3917467eca3a260bd86db884f75821f`
- Follow-up: publish a later test release from `v1.7.28` or newer and verify the serial log reaches `Firmware download attempt 2/3: proxy` when direct GitHub asset download times out.

## Previous Runtime Notes: 2026-06-23 v1.7.27 Tupi Warm Theme And Weather Layout Reuse

- Latest release target is `v1.7.27`.
- User approved a third warm paper theme direction built around the `nothing impossible / tupi` brand mark.
- `desktop_ui.cc` now includes a third persisted theme enum value, `Tupi Warm`, with an ivory/warm-paper palette, graphite text, muted olive, amber accents, and a four-dot Tupi brand mark.
- The main Tupi Warm page keeps the earlier accepted large time treatment while moving the top status items apart, hiding the daily-card network footer in Tupi mode, and keeping the FC/NES surfaces readable instead of black-on-black.
- The compact Tupi-specific weather layout was rejected on hardware. The final `v1.7.27` code intentionally reuses the previous Classic weather animation geometry for sun, clouds, rain, snow, storm, and gold particles, while retaining the Tupi Warm card colors.
- Build verification passed with `idf.py -B build-qdtech build`; `xiaozhi.bin` size is `0x3d75d0`, smallest app partition is `0x600000`, and free app space is `0x228a30`, about 36%.
- Flash verification passed on `COM13` at 921600 baud.
- Captured boot logs after the weather-layout correction confirmed QDTech startup, `Desktop UI created`, `Touch max points: 5`, battery ADC readings, saved WiFi reconnect, GitHub OTA latest check, MQTT connection, and `Application: STATE: idle`.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.27-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.27-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.27-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.27-app.bin`: `88a347cb3700185db6dcb17b6b590c28f8da0309446f7bc9dbb81c399a74a82a`
  - `qdtech-s3-touch-lcd-3.5-v1.7.27-firmware.zip`: `126b45d88d6e50c1a102c81e9db39559593d963c79e61d54fcb2d4637320961c`
  - `qdtech-s3-touch-lcd-3.5-v1.7.27-full.bin`: `56f506ef9f7dd255c6b114fcdde78c6327839b4dbff5436af1e192427f9c7b4e`
- Follow-up: visually inspect Tupi Warm on the actual 480x320 LCD after release, especially the big-clock colon alignment, top-right status spacing, quote wrapping, and weather contrast.

## Previous Runtime Notes: 2026-06-22 v1.7.26 GitHub OTA Redirect Buffer Fix

- Latest release target is `v1.7.26`.
- User reported another Settings OTA update failure on a newly inserted board. Serial capture on `COM13` showed the exact failure:
  - user pressed the Settings OTA button
  - `FirmwareUpdate: firmware task create action=1`
  - `Ota: Upgrading firmware from https://github.com/Liutupi/.../qdtech-s3-touch-lcd-3.5-v1.7.25-app.bin`
  - GitHub returned `302` to a long signed `release-assets.githubusercontent.com` URL
  - `HTTP_CLIENT: Out of buffer`
  - `Ota: Failed to open firmware HTTP connection: ESP_FAIL`
- Root cause: the OTA firmware-download HTTP client used 1024-byte RX/TX buffers, too small for GitHub's long signed release-asset redirect URL/request line.
- Fix: `main/ota.cc` now uses a 2048-byte RX and TX buffer for firmware downloads and logs only redirect status plus Location length, not the full signed URL.
- Build verification passed:
  - `xiaozhi.bin` size: `0x3d6a10`.
  - Smallest app partition: `0x600000`.
  - Free app partition space: `0x2295f0`, about 36%.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.26-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.26-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.26-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.26-app.bin`: `1b2b33dc5cef448fbe8d2142f9f5151e9145c3a25f797c7d765aee1ae192a36e`
  - `qdtech-s3-touch-lcd-3.5-v1.7.26-firmware.zip`: `51a426a752542126916f512d5a960c9b40e66ebce12a43af58fd267e8a6698df`
  - `qdtech-s3-touch-lcd-3.5-v1.7.26-full.bin`: `276c0f80e0c30f06db08a546d9a811e539d129e8e852e23663b62d0e6df42fbf`
- Follow-up: after publishing `v1.7.26`, run a real Settings OTA from a board still on `v1.7.25` and confirm it progresses past the GitHub redirect.

## Previous Runtime Notes: 2026-06-21 v1.7.25 FC ROM Name Font Coverage Fix

- Latest release target is `v1.7.25`.
- FC/NES ROM list and selected-game detail labels were restored to `font_puhui_16_4` after user feedback that game names became garbled/missing-glyph-heavy after the shared LXGW WenKai UI font pass.
- Keep this distinction: fixed UI copy can use the board-local LXGW WenKai subset, but dynamic SD-card ROM names must use a broad Chinese-coverage font such as `font_puhui_16_4` because ROM filenames are not known when generating the subset.
- The rest of the `v1.7.24` font direction remains: Cat/Classic fixed Chinese UI text uses the shared LXGW WenKai subset, including daily-card and Calendar labels.
- Build verification passed:
  - `xiaozhi.bin` size: `0x3d6a00`.
  - Smallest app partition: `0x600000`.
  - Free app partition space: `0x229600`, about 36%.
- Flashed successfully to `COM14` at 921600 baud. Esptool verified hashes for bootloader, app, partition table, OTA data, and srmodels, then hard reset the board.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.25-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.25-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.25-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.25-app.bin`: `c6aded5a555049e414718fe84705fc8f4ceb84a1ade9bbdf0e00744c4a0db6c3`
  - `qdtech-s3-touch-lcd-3.5-v1.7.25-firmware.zip`: `abd27e9bc1b1e243dc99206e0b23bdfb977843f2792c0625e34297b239b1d80f`
  - `qdtech-s3-touch-lcd-3.5-v1.7.25-full.bin`: `ea2a2a3afda4e8c286ae6d1b8a947fe5e0c7c801ac62fd09dcde871434671845`
- Follow-up: visually re-open FC/NES on the physical LCD and confirm Chinese ROM names render normally.

## Previous Runtime Notes: 2026-06-21 v1.7.24 Shared LXGW WenKai UI Font Release

- Latest release target is `v1.7.24`.
- The QDTech embedded `qd_font_lxgw_16` and `qd_font_lxgw_20` LVGL font subsets were regenerated from LXGW WenKai and are now the shared Chinese UI font path for both Classic and Cat themes.
- The regenerated subset includes the newer Cat-theme and desktop glyphs such as `小苍兰`, `端午`, cat/theme labels, daily-card text, Calendar labels, and related UI copy. This fixes the missing-glyph boxes that appeared when the Cat brand mark used an incomplete subset.
- Desktop Chinese labels that previously used `font_puhui_16_4` were switched to the shared LXGW WenKai subset where appropriate: daily-card title/body, Calendar today/weekday labels, and FC/NES list/detail labels.
- Classic theme daily-card title and body now use the 20 px LXGW WenKai font to make the quote area less thin; Cat theme keeps using the same 20 px shared font.
- Do not link the full `font_puhui_20_4` or `font_puhui_30_4` fonts for this UI pass: a test build with those full fonts overflowed the OTA app partition by about 2 MB. Regenerate the board-local font subset instead when adding new Chinese UI text.
- Build verification passed:
  - `xiaozhi.bin` size: `0x3d6a00`.
  - Smallest app partition: `0x600000`.
  - Free app partition space: `0x229600`, about 36%.
- Flashed successfully to `COM14` at 921600 baud. Esptool verified hashes for bootloader, app, partition table, OTA data, and srmodels, then hard reset the board.
- `idf.py monitor` could not be captured from this non-interactive shell because it requires stdin attached to a TTY; the build output confirmed `App "xiaozhi" version: 1.7.24`.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.24-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.24-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.24-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.24-app.bin`: `b3d4f95b449db34cb28bb42c70169bcaa960bc4547efb446dc579b839fa17ae3`
  - `qdtech-s3-touch-lcd-3.5-v1.7.24-firmware.zip`: `13c2d4042ed9d96bb0c5bbbf5d4330884abfa4257f6d70c5b79e6a2653035158`
  - `qdtech-s3-touch-lcd-3.5-v1.7.24-full.bin`: `91958bcf02c64000e81b86fe907f30695e45b2f5778709a36efbdbbaaa58e597`
- Follow-up: visually inspect both Classic and Cat themes on the physical LCD after release, especially the 20 px daily-card line wrapping.

## Previous Runtime Notes: 2026-06-21 v1.7.23 Cat Theme Switcher And Layout Polish

- Latest release target is `v1.7.23`.
- Added a second persisted UI theme named `Cat` while preserving the existing `Classic` black/gold theme.
- Settings now includes an `Appearance / Theme` row. Tapping `Switch` cycles `Classic` <-> `Cat`, saves the choice in NVS namespace `qd_ui`, and restarts so the whole LVGL desktop is recreated with the selected palette.
- The Cat theme intentionally keeps the existing page architecture: Main, Apps, Radio, FC/NES, Calendar, Focus, Network, Settings, Photos, and XiaoZhi navigation and callbacks are unchanged. Only palette, card styling, brand mark, icon decoration, and selected Cat-theme layout offsets change.
- Cat theme visual updates include a pinker background, pink/white cards, pink-orange high-contrast time digits, Chinese brand mark `小苍�?/ 端午`, a small cat on the daily-card panel, and a cat-style XiaoZhi face page.
- The main-page Cat time card was moved below the top-left brand mark after hardware feedback showed the first version overlapped the logo. In Cat mode the time group now starts lower and uses a shorter card; Classic mode keeps the previous layout.
- The Chinese brand mark uses the already-linked `font_puhui_16_4` font path to avoid adding new glyph subset requirements for this theme pass.
- Build verification passed after the final layout fix:
  - `xiaozhi.bin` size: `0x3d4f00`.
  - Smallest app partition: `0x600000`.
  - Free app partition space: `0x22b100`, about 36%.
- Flashed successfully to `COM13` at 921600 baud. Esptool verified hashes for bootloader, app, partition table, OTA data, and srmodels, then hard reset the board.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.23-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.23-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.23-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.23-app.bin`: `51f84d5d354c5959a81b93a2766ab7e00551f3e579d3999c5d54c0f94bfc0634`
  - `qdtech-s3-touch-lcd-3.5-v1.7.23-firmware.zip`: `360cd8bff4ad320f633ea15633a5b22159483da08437a922382f9c37178eb494`
  - `qdtech-s3-touch-lcd-3.5-v1.7.23-full.bin`: `aff237067c6e55e9b6ff94fa6a82ddea6d9dca70d696e13f23463d6dbe6d1df8`
- Remaining visual follow-up: inspect the Cat theme on hardware after this release and fine-tune exact spacing/colors from the physical screen rather than screenshots alone.

## Previous Runtime Notes: 2026-06-21 v1.7.22 More Robust GitHub OTA Download

- Latest release target is `v1.7.22`.
- User reported a freshly flashed board can detect the newest GitHub firmware in the Settings OTA row, but pressing `Update` repeatedly fails.
- Root-cause candidate from code review: the old OTA writer required a positive `Content-Length` and could write before `esp_ota_begin()` if the first HTTP/TLS read returned less than the ESP image header size. Both are plausible with GitHub release-asset redirects and small TLS chunks.
- `v1.7.22` hardens `Ota::Upgrade()` so unknown content length continues with byte-count progress, app size is checked against the target OTA partition when length is known, the image header is accumulated before beginning OTA, and clearer OTA begin/header-write errors are logged.
- Final `v1.7.22` build passed from the Windows checkout: `xiaozhi.bin` `0x3d2f40`, smallest app partition `0x600000`, free `0x22d0c0`.
- Final `v1.7.22` build was flashed successfully to the newly connected board on `COM14` at 921600 baud. The serial port was present after flash but did not emit a captured boot log in the short post-flash monitor window.
- Important compatibility note: boards already running the older updater still use that older updater to fetch `v1.7.22`. If an older board still cannot cross-upgrade from GitHub, install `v1.7.22-full.bin` once over USB; OTA from `v1.7.22` forward uses the hardened path.
- Release assets prepared locally:
  - `qdtech-s3-touch-lcd-3.5-v1.7.22-app.bin`, SHA256 `6a700d79478094792ce175542a02a9e1520e69cf161db656dfb3980e6e12e4a6`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.22-firmware.zip`, SHA256 `413c7a89172c634869d03fa61f606def0af32201ed166b1de0a09533cbbfc635`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.22-full.bin`, SHA256 `857ad9ab97c059d7afdbfee3a6d0785826be876ce685b6c390598d5cf400b859`.
## Previous Runtime Notes: 2026-06-20 v1.7.21 Preserve WiFi After BOOT Wake

- Latest release target is `v1.7.21`.
- User feedback after `v1.7.20`: BOOT could power the board off/on, but after BOOT wake the saved WiFi was not reused and the board entered the pairing/config flow again.
- Root cause: the QDTech BOOT single-click handler still contained the upstream startup shortcut: if device state was `starting` and WiFi was not connected yet, it called `ResetWifiConfiguration()`. A BOOT wake/release can arrive before WiFi reconnects, so it could clear credentials immediately after wake.
- `v1.7.21` removes that reset path for QDTech. BOOT clicks during `starting` or `wifi configuring` are ignored with log `BOOT click ignored during startup/config to keep saved WiFi`; after normal idle state, BOOT still toggles chat state.
- Final `v1.7.21` build passed from the Windows checkout: `xiaozhi.bin` `0x3d2bd0`, smallest app partition `0x600000`, free `0x22d430`.
- Final `v1.7.21` build was flashed to `COM13`.
- Post-flash logs confirmed `App version: 1.7.21`, saved WiFi `liutupi` reconnect, IP `192.168.4.92`, `Ota: Current version: 1.7.21`, idle state, and live battery readings. No WiFi reset/config path appeared.
- BOOT deep-sleep cycle was not recaptured in the final monitor window; the board remained awake and continued battery logs. The WiFi regression fix is still valid because the startup reset path has been removed.
- Battery hardware follow-up on 2026-06-20: local schematic `D:\3.5inch_ESP32-S3\5-_Schematic\ESP32-S3原理�?pdf` shows a USB-C powered battery charge/discharge circuit with `U2 TP4054`, `VBUS/+5V`, `JP1 BAT+`, and `CHRG`. USB-C should charge a connected single-cell Li-ion/LiPo battery automatically in hardware; firmware does not need to control charging.
- Battery capacity note: replacing the current 1000mAh pack with a larger 2800mAh pack is acceptable if it is still a single-cell 3.7V nominal / 4.2V full Li-ion/LiPo battery, the connector polarity matches `BAT+`/`GND`, and the pack has protection. Larger capacity only extends runtime and increases charge time.
- Firmware battery status caveat: the QDTech firmware reads battery voltage through `IO8 / ADC1_CH7` and maps voltage to percentage. The schematic's `CHRG` signal was not found on an ESP32 GPIO in the IO allocation table, so the UI currently reports real battery level but not a hardware-proven charging/charged state.
- Release assets prepared locally:
  - `qdtech-s3-touch-lcd-3.5-v1.7.21-app.bin`, SHA256 `91d227d0a8b3d1031d8362317e9237263d0edd971c8c3630589bddd6325c63d2`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.21-firmware.zip`, SHA256 `63f1ef9152ed390a5ef0a20a24a24c41da23cb53f611237282b3a8e3d71bff0a`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.21-full.bin`, SHA256 `317a280aa1bc8916dc031203051dacc9f7c50461fd24042b5ceb7cf105a2f730`.
## Previous Runtime Notes: 2026-06-20 v1.7.20 BOOT Soft Power Release-Gated Sleep

- Latest release target is `v1.7.20`.
- `v1.7.19` added the battery monitor and first BOOT deep-sleep path, but entering sleep while BOOT/GPIO0 was still held low could immediately trigger the low-level wake condition. User feedback confirmed long-press BOOT did not feel like power-off.
- `v1.7.20` changes the soft-power sequence: long press turns off the backlight, stops the BOOT poll timer, waits until BOOT is released, waits another 250 ms, then enables GPIO0 low-level wake and enters deep sleep. This makes the same long press power off, while the next BOOT press wakes the board.
- This is still firmware soft power/deep sleep, not physical battery rail cutoff. A real hard-off switch still needs power-latch/PMIC hardware.
- Final `v1.7.20` build passed from the Windows checkout: `xiaozhi.bin` `0x3d2b70`, smallest app partition `0x600000`, free `0x22d490`.
- Final `v1.7.20` build was flashed to `COM13`.
- Post-flash logs confirmed `App version: 1.7.20`, `Ota: Current version: 1.7.20`, WiFi, idle state, and repeated real battery ADC readings around `4166-4174mV / 97%`.
- BOOT long-press test behavior: serial monitor saw normal battery logs until the COM port closed/aborted, matching expected USB serial drop during deep sleep; after pressing BOOT again, COM13 reopened and printed a fresh `App version: 1.7.20` boot log.
- Release assets prepared locally:
  - `qdtech-s3-touch-lcd-3.5-v1.7.20-app.bin`, SHA256 `fb80a2806dfaa11d9603e754dabecd0e7bd40c267237c6b9c25e77c61c44e4ec`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.20-firmware.zip`, SHA256 `62de97c20e442ac5b7646a544a5b42571bdb23904cbc1106d8b1abb4d8247707`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.20-full.bin`, SHA256 `1eb83bb34fa72f7113fb4078865396dc0ee112e07c1a83905e9aaf9cbab447d6`.
## Previous Runtime Notes: 2026-06-20 v1.7.19 Battery Monitor And BOOT Soft Power

- Latest release target is `v1.7.19`.
- Live GitHub release check before this work showed the newest published release was `v1.7.17`; `main` already contained the pushed `v1.7.18` FC-audio commit, but `v1.7.18` had not been published as a GitHub Release.
- Battery monitor is now QDTech-specific in `qdtech_s3_touch_lcd_3_5.cc`. It reads ADC1 channel 7 / GPIO8 with 12 dB attenuation and 12-bit width, uses ESP-IDF curve-fitting calibration when available, multiplies ADC millivolts by the board's x2 divider, maps the resulting Li-ion voltage to 0-100%, and publishes to `DesktopUI::SetBatteryStatus()`.
- The desktop status bar no longer hardcodes `80%`; it starts as `--%` and updates to the sampled battery percentage. Hardware logs after flashing showed stable real readings around `4076-4082mV` and `89-90%` with calibration enabled.
- `GetBatteryLevel()` now reports the QDTech monitor value through the common board API.
- BOOT/GPIO0 long press now attempts soft power-off by dimming the backlight, entering deep sleep, and enabling GPIO0 low-level wake. A direct 50 ms GPIO0 polling path was added in addition to the button component callback.
- BOOT soft-power caveat: this is firmware deep sleep, not true battery power cut. It depends on the board wiring BOOT to GPIO0 during runtime and GPIO0 being usable as the wake source. Two serial-monitor verification windows did not observe `BOOT press down` or `BOOT gpio down`, so deep-sleep entry/wake remains implemented but not hardware-confirmed on this physical unit.
- Final `v1.7.19` build passed from the Windows checkout: `xiaozhi.bin` `0x3d29b0`, smallest app partition `0x600000`, free `0x22d650`.
- Final `v1.7.19` build was flashed to `COM13`.
- Post-flash logs confirmed `App version: 1.7.19`, `Ota: Current version: 1.7.19`, WiFi, idle state, and repeated real battery ADC readings.
- Release assets prepared locally:
  - `qdtech-s3-touch-lcd-3.5-v1.7.19-app.bin`, SHA256 `5a65a3ecf01e77d8c10b274d78d4e02fb81afc093a5d52245b9dc4c94182f434`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.19-firmware.zip`, SHA256 `5a8c3abfcea5e88c90afcf42d5f3af1648b3023778a8bf12e64f68af45689473`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.19-full.bin`, SHA256 `5406b5a0fe3f9b58966d82c1a3e38d02293d5cc4863bd39ab098eca1cd2259c9`.
## Current FC/NES Emulator Handoff

Last worked on 2026-06-18 in this Windows workspace:

- Workspace: `D:\3.5inch_ESP32-S3\xiaozhi-esp32`
- Branch used locally: `codex/qdtech-landscape-v176`
- Target user remote: `qdtech-new/main`
- Build directory: `build-qdtech`
- Serial port: `COM13`

Latest 2026-06-19 macOS hardware pass for `v1.7.19`:

- Workspace: `/Users/tupi/Documents/带小�?3.5 �?qdtech-s3-touch-lcd-3.5-xiaozhi-firmware`
- Build staging directory used to avoid workspace build churn: `/private/tmp/qdtech_s3_build_src`
- Serial port: `/dev/cu.usbmodem212401`
- Release target is `v1.7.19`; this bump is intentional so the on-device GitHub Release updater sees the build as newer than `v1.7.18`.
- Current flashed candidate is the "playability-first" rollback build, not the failed PSRAM static SRAM/VRAM experiment.
- Kept FC fixes:
  - FC task priority lowered from `5` to `1`.
  - FC audio callback path reuses `audio_output_buffer_` and emits low-rate audio/internal-SRAM diagnostics.
  - Nofrendo SRAM allocation keeps the 16 KB padding guard and safe nulling in `rom_free()`.
  - Entering the FC page calls `Application::SetExternalAudioActive(true)`, suspending XiaoZhi protocol/MQTT work. Staying on the FC page after LIST keeps background protocol suspended so selecting another ROM does not compete with reconnects. Leaving the FC page resumes protocol work.
  - OTA retry alerts/sounds and weather HTTP fetches are suppressed while FC/external audio mode is active.
- Hardware evidence from the final candidate:
  - Final `v1.7.19` build reported `xiaozhi.bin` size `0x3cf380`, smallest app partition `0x600000`, free `0x230c80`.
  - Monitor logs confirmed `App version: 1.7.19` and `Ota: Current version: 1.7.19`.
  - Boot and apps navigation were normal after flashing.
  - When MQTT was connected, entering FC suspended the protocol and internal SRAM recovered from about `7 KB` to about `15 KB`.
  - SD mounted and `/sdcard/FC` scanned successfully with `fc scan found 192 nes files`.
- Release assets:
  - `qdtech-s3-touch-lcd-3.5-v1.7.19-app.bin`, SHA256 `da477d4d9c7b9ce98aa5f88f9a61b26d4c31b5cff79704aae4bd183435530e0c`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.19-firmware.zip`, SHA256 `bcc5bf4d01dfe1bce56e0f335b5b9cfc200e6729f086ecd4cb3a6f33fecea4b6`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.19-full.bin`, SHA256 `0aade4d670afa8e6651dd94469c6550492bf3409cb8f8374a1821bc502519720`.
- Known remaining FC problem:
  - LIST/Stop after gameplay can still crash on some ROMs. A captured panic showed `StoreProhibited` inside TLSF free, with the backtrace reaching `rom_free()` at `components/nofrendo/nes/nes_rom.c` while freeing SRAM after `main_eject()`.
  - An attempted fix that moved SRAM/VRAM to large static PSRAM buffers (`64 KB` SRAM / `32 KB` VRAM) made gameplay worse: the user reported a complete freeze soon after entering gameplay. That experiment was reverted before `v1.7.19`. Do not reintroduce it as-is.
- Recommended next FC step:
  - Keep `v1.7.19` as the usable baseline. For the LIST/Stop issue, test one small nofrendo cleanup change at a time with hardware logs. Prefer a ROM-specific or mapper-specific guard, or a safer nofrendo quit sequence, over moving active mapper memory to PSRAM.

Current user-visible FC/NES state:

- Apps contains an `FC / NES / SD ROMs` tile.
- Entering FC now shows a ROM list first; it no longer auto-loads or auto-runs a ROM.
- Prev/Next move the selected ROM in the list.
- Start loads the selected ROM and switches to the game view.
- Game view is top screen plus bottom virtual controller.
- Game view has a `LIST` button to stop and return to the ROM list.
- Right-swipe exit is disabled while in game view to avoid accidental exits during D-pad swipes.
- The SD card is detected. FC scans the first populated ROM directory and now keeps up to 192 `.nes` files in the on-device list.
- The active emulator path is the Nofrendo adapter under `components/nofrendo`. Start now pre-validates the selected ROM and reports bad iNES headers, NES 2.0 headers, ROMs over 2 MB, and mappers not supported by the tracked Nofrendo mapper list.

Important FC/NES files:

- `main/boards/qdtech-s3-touch-lcd-3.5/fc_emulator_service.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/fc_emulator_service.h`
- `main/boards/qdtech-s3-touch-lcd-3.5/nes_bus.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/nes_bus.h`
- `main/boards/qdtech-s3-touch-lcd-3.5/nes_cpu.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/nes_cpu.h`
- `main/boards/qdtech-s3-touch-lcd-3.5/nes_ppu.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/nes_ppu.h`
- `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc`
- `main/boards/qdtech-s3-touch-lcd-3.5/qdtech_s3_touch_lcd_3_5.cc`

Current FC/NES verification evidence:

- Build passed with `ninja -C build-qdtech all`.
- Flash passed with `idf.py -B build-qdtech -p COM13 -b 921600 flash`.
- Latest app size: `xiaozhi.bin` `0x3dd8b0`; smallest app partition `0x600000`; free `0x222750`.
- Serial confirmed ROM selection and load, for example `NES ROM loaded: 144??~1.NES`.
- Serial confirmed virtual buttons enter the service as `FcEmulator: controller=0x..`.
- Serial confirmed the NES bus can latch controller states, for example `NesBus: controller latch=0x40`, `0x20`, `0x08`, `0x02`, and `0x01`.

2026-06-18 macOS continuation:

- Pulled `https://github.com/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware.git` into `/Users/tupi/Documents/带小�?3.5 �?qdtech-s3-touch-lcd-3.5-xiaozhi-firmware`.
- Improved the in-repo minimal NES core timing and sprite correctness: CPU base-cycle accounting, `CPU cycles * 3` PPU advancement, NMI-cycle PPU advancement, Mapper 2 bank wrapping, sprite bitplane/X/Y fixes, and background fetch timing.
- Changed FC startup to be fully lazy: normal boot only registers callbacks; the FC task, SD mount, and ROM scan start when the FC page or FC controls request them. Leaving the FC page stops playback and releases the ROM list.
- Full ESP32-S3 build passed from `/private/tmp/qdtech_s3_build_src` because the original macOS workspace path contains spaces/Chinese text that split an ESP-SR linker search path at link time.
- Final build result from the no-space validation path: `xiaozhi.bin` `0x3df3b0`; smallest app partition `0x600000`; free `0x220c50` bytes, about 35%.
- Flashed successfully to `/dev/cu.usbmodem212401` on macOS.
- Boot verification after the lazy-start change reached desktop UI, touch max points `5`, WiFi, OTA latest, MQTT, AFE load, and `Application: STATE: idle`.
- The first pre-lazy-start flash proved FC boot-time SD/ROM scan could create internal SRAM pressure, including `esp-aes: Failed to allocate memory`. The final lazy-start flash removed the FC task/SD scan from boot.
- Residual runtime risk remains: AFE `Ringbuffer of AFE(FEED) is full` warnings still appeared in the captured monitor output, and internal SRAM remains tight. Do not treat this as a finished XiaoZhi/audio stability pass.
- Do not declare FC playable from this build alone; ROM display/gameplay still needs a controlled hardware test with known-good Mapper 0/NROM and Mapper 2 ROMs.
- User then reported FC remained unplayable: very low frame rate and virtual controls felt unresponsive.
- Added an FC game-mode direct touch-poll fallback, shortened controller release hold to `120ms`, reduced FC task priority to `1`, shortened emulator execution slices, and added `fc perf` logging.
- A brief LVGL partial-render experiment corrupted the screen on hardware. It was reverted; keep `LV_DISPLAY_RENDER_MODE_FULL` and `timer_period_ms = 50` unless the rotated flush path is redesigned and tested.
- Responsiveness hotfix build passed and was flashed to `/dev/cu.usbmodem212401`; final app size `0x3df5a0`, free `0x220a60`.
- Added a QDTech direct RGB565 frame path for FC gameplay. It draws the native `256x240` NES frame into the top-center screen area through the existing stable rotated transfer routine, avoiding per-frame LVGL full-screen image refresh.
- FC frame publishing now skips stale frames and only redraws when `NesBus` reports a new complete frame. The emulation slice was raised to `12000` instructions or about `12ms` per loop.
- Hardware logs from the second hotfix build proved the bottom virtual controller reaches both the FC service and the NES bus during gameplay: `controller=0x01/0x02/0x10/0x20/0x40/0x80` and `NesBus: controller latch=0x..` were captured.
- Before the stale-frame skip and larger emulation slice, `fc perf` still showed only about `3-5` produced NES frames per second on the tested ROM. This established that the remaining "no reaction" symptom is mostly low emulator throughput/core behavior, not a missing touch event.
- Latest final build passed and was flashed to `/dev/cu.usbmodem212401`: `xiaozhi.bin` `0x3df920`, smallest app partition `0x600000`, free `0x2206e0`. A fresh in-game `fc perf` sample from this exact final build was not captured because there was no new FC interaction during the final monitor window.
- Later 2026-06-18 macOS continuation replaced the in-repo minimal FC core path with a Nofrendo adapter under `components/nofrendo` and switched board defaults to performance optimization (`CONFIG_COMPILER_OPTIMIZATION_PERF=y`).
- The board's internal SRAM was too tight with the local AFE wake word/noise processor enabled. The QDTech board defaults now intentionally disable local wake word and audio processor (`CONFIG_USE_AFE_WAKE_WORD`, `CONFIG_USE_ESP_WAKE_WORD`, and `CONFIG_USE_AUDIO_PROCESSOR` are unset) while keeping audio capture/playback. A clean build verified `no_wake_word.cc` and `no_audio_processor.cc` are compiled.
- SD startup was retested on `/dev/cu.usbmodem212401`. Logs confirmed early SDMMC mount succeeds after LCD init: `FcEmulator: fc sd card mounted width=4` and `QDtech35: SD card prepared early`.
- The user's "SD card contents disappeared" symptom was reproduced as an FC-page scanning/runtime problem, not a missing SD mount. Serial logs showed `/sdcard/FC` entries being discovered.
- FC ROM scanning was reduced to avoid freezing the device or exhausting internal SRAM: it now scans preferred directories in order (`/sdcard/FC`, `/sdcard/nes`, `/sdcard/roms`, `/sdcard`), stops after the first directory with ROMs, caps the list at 64 `.nes` files, and validates the selected ROM only when Start is pressed.
- The latest build after this scan-limit fix passed and was flashed to `/dev/cu.usbmodem212401`: `xiaozhi.bin` `0x39a630`, smallest app partition `0x600000`, free `0x2659d0`.
- Post-flash boot stayed stable for several minutes, SD mounted successfully, WiFi/weather continued, and no `SR_RINGBUF Memory exhausted` / AFE reboot loop appeared. No final FC-page touch test was captured after this last scan-limit flash because no new touch input occurred during the final monitor window.
- Follow-up crash fix: Nofrendo initially crashed in `vid_findmode()` because the QDTech video adapter returned `NULL` from `lock_write()`. The adapter now creates a small PSRAM-backed hardware bitmap so Nofrendo can read screen dimensions while still using the direct RGB565 custom blit for real frames.
- Follow-up crash fix: Nofrendo then crashed in `ppu_renderbg()` because the primary bitmap was allocated as `256x224` while the PPU renders 240 scanlines. The QDTech port now uses `NES_SCREEN_HEIGHT` (`240`) for both OSD video info and `vid_setmode()`.
- Latest hardware result after both fixes: Start no longer crashes. Logs showed `vid_init done`, `rgb frame buffer ready 256x240 pixels=61440`, and stable `nofrendo fps=35-39`.
- Virtual FC buttons were verified again in the Nofrendo path: logs showed Start/A/B/D-pad values such as `controller=0x08`, `0x01`, `0x02`, `0x80`, `0xa0`, and `0x60`. Leaving the FC page stopped Nofrendo cleanly with `nofrendo stopped result=0`.
- Follow-up playability pass for the user's "bigger screen, no multitouch, key crash" report:
  - QDTech touch now reads all active touch points from the controller instead of only the first point.
  - FC game mode ORs multiple touch points into one controller state, so holding a D-pad direction while pressing A/B can produce combined masks.
  - FC game mode makes LVGL report touch released and uses the direct manual touch path only, preventing gameplay touches from also triggering LVGL gestures/buttons.
  - Normal LVGL pages now read cached touch coordinates from the manual poller instead of reading the I2C touch chip a second time. This removed the observed repeated `i2c.master: I2C software timeout` spam during touch-heavy testing.
  - The D-pad hit map was widened and given a center dead zone; A/B hit zones were separated further to reduce accidental hits.
  - The direct FC frame path now scales standard `256x240` NES frames to `320x240` before drawing, centered at the top of the `480x320` screen.
- Latest flashed build after this pass: `xiaozhi.bin` `0x39abb0`, smallest app partition `0x600000`, free `0x265450`.
- Latest hardware monitor result: boot stable, `Touch max points: 5`, SD mounted, `/sdcard/FC` scanned, Nofrendo entered multiple ROMs, and gameplay held about `29-31` FPS with the larger `320x240` draw. LIST stopped Nofrendo cleanly. No repeated I2C timeout spam appeared in the final monitor window.
- Follow-up smoothness/layout pass after the user asked whether the game screen could be larger without hurting smoothness:
  - Tried `336x240` and `328x240` scaled FC draw widths. Both entered Nofrendo, but hardware logs stayed around `24-26` FPS, so they were rejected as defaults.
  - Final default is back to `320x240`, which remains the best tested balance for readability and smoothness on the current direct-draw path.
  - Bottom controls were rearranged: D-pad visual keys are less crowded, the actual D-pad hit region is a wider left-side joystick-style area with a center dead zone, A/B are farther apart, and LIST/Select/Start are centered more cleanly.
  - Controller serial logging was removed from the hot path to avoid losing frame time while rapidly pressing A/B/D-pad.
  - Nofrendo quit handling was changed so LIST/Stop first asks the NES loop to power off and only releases Nofrendo memory after the loop exits. This fixed the heap crash seen when pressing LIST during gameplay.
  - Touch I2C failures now use a conservative backoff and low-rate `i2c_master_bus_reset()` instead of continuous reads. Rapid FC list tapping no longer produced the previous continuous I2C timeout flood in the captured final boot/list test.
  - Latest final build passed and was flashed to `/dev/cu.usbmodem212401`: `xiaozhi.bin` `0x39ae30`, smallest app partition `0x600000`, free `0x2651d0`.
  - Latest final boot monitor confirmed `Touch max points: 5`, early SD mount, WiFi, MQTT, and idle state. No final in-game FPS sample was captured after returning from `328x240` to `320x240`, but `320x240` had already been measured at about `29-31` FPS earlier in this same pass.
- Follow-up ROM directory/name pass after the user reported garbled, numeric, and hard-to-identify game names:
  - QDTech board defaults now select FATFS CP936 with UTF-8 API encoding instead of CP437/ANSI-OEM, so Chinese long filenames should reach LVGL as UTF-8.
  - FC list rows now display a stable 3-digit index plus a cleaned title: no directory path, no `.nes` suffix, trimmed whitespace, and UTF-8-safe truncation.
  - Numeric ROM sets can place `roms.txt`, `roms.csv`, or `games.txt` in the active ROM directory. Lines may be `001.nes=Super Mario`, `001.nes,Super Mario`, or tab-separated. Aliases load once during scan and are not used in the gameplay frame loop.
  - Validation regenerated `build-qdtech/sdkconfig` in `/private/tmp/qdtech_s3_build_src`; generated config confirmed `CONFIG_FATFS_CODEPAGE_936=y`, `CONFIG_FATFS_CODEPAGE=936`, and `CONFIG_FATFS_API_ENCODING_UTF_8=y`.
  - Latest build was flashed to `/dev/cu.usbmodem212401`: `xiaozhi.bin` `0x3c89f0`, smallest app partition `0x600000`, free `0x237610`.
  - Hardware monitor confirmed `/sdcard/FC` scan and readable Chinese ROM names in logs, for example `10.能源战士2(无限HP+无限生命)` and `100.恶魔�?无敌版`.
  - Entering Nofrendo after the ROM-name pass stayed stable and logged about `27-32` FPS, so this directory/name fix did not regress the smoothness-first `320x240` path.
  - After the user reported that later ROM names still looked garbled on the panel, the FC list/detail labels were switched from Montserrat to the existing `font_puhui_16_4` Chinese font. This addresses the separate display-font layer: decoded UTF-8 names need a font with Chinese glyph coverage.
  - The font-only pass built and flashed successfully to `/dev/cu.usbmodem212401` with the same `xiaozhi.bin` size `0x3c89f0`; boot was verified. The monitor session did not capture a fresh FC page entry after this last flash, so the user still needs to visually confirm the FC list on the panel.
  - User confirmed the FC ROM list became readable after the font pass.
  - Release prep bumped `PROJECT_VER` to `1.7.9` and changed `.gitignore` so `components/nofrendo` is tracked while other generated `components/` content remains ignored. Do not drop the Nofrendo component from future commits; `FcEmulatorService` includes `qdtech_nofrendo.h`.
  - Final `v1.7.9` release build passed from `/private/tmp/qdtech_s3_build_src`: `xiaozhi.bin` `0x3c89f0`, smallest app partition `0x600000`, free `0x237610`.
  - Final `v1.7.9` build was flashed to `/dev/cu.usbmodem212401`; monitor logs confirmed `App version: 1.7.9`, OTA current version `1.7.9`, early SD mount, FC page activation, `/sdcard/FC` scan, and readable Chinese list navigation through names such as `10.能源战士2(无限HP+无限生命)`, `100.恶魔�?无敌版`, and `115.未来战士HACK`.
  - Follow-up system stability pass after the user reported slow time/weather sync and occasional crashes after flashing FC:
    - Removed boot-time FC SD prepare; SD now mounts lazily when FC/Photos needs it.
    - FC now reuses an existing `/sdcard` mount instead of trying a second mount.
    - Time/Weather task now allocates its 6144-byte stack from PSRAM first.
    - SNTP starts immediately after WiFi connects instead of waiting for application/MQTT readiness.
    - Weather fetch was shortened to one 5-second initial attempt; failures keep cached UI state and retry later.
  - Latest stability build passed and was flashed to `/dev/cu.usbmodem212401`: `xiaozhi.bin` `0x3c8a70`, smallest app partition `0x600000`, free `0x237590`.
  - Latest stability monitor result: boot stable, WiFi initialized normally, SNTP synchronized immediately after IP acquisition, `time weather task started stack=6144 memory=psram`, free internal SRAM around `20KB` with minimum around `17KB`, FC page entered, FC task started in PSRAM, SD mounted on demand, and `/sdcard/FC` scanned 64 capped ROMs.
  - Rejected stability experiment: enabling `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y` caused WiFi boot failure (`wifi:malloc buffer fail`, `Expected to init 6 rx buffer, actual is 2`, `ESP_ERR_NO_MEM` from `esp_wifi_init`). It was reverted. Do not re-enable it casually; it made the board reboot-loop.

Unresolved FC/NES problem:

- User still needs to test actual play feel on hardware after the multi-touch/cache/scale pass.
- User also needs to verify real ROM list rendering on the panel after the FATFS/alias/font build. If any names are still unreadable, check whether the serial log shows correct Chinese; if serial is correct but the panel is wrong, extend the UI font coverage. If serial is wrong too, use `roms.txt`/`roms.csv` aliases or inspect mixed-encoding filenames on the SD card.
- The `320x240` draw is currently the largest smoothness-safe default tested. `328x240` and `336x240` were visibly more ambitious but measured only `24-26` FPS on hardware.
- If the user still wants a bigger view without losing FPS, do not just increase `target_width` again. The next real path is a deeper display/game mode optimization: reduce competing LVGL/network redraws during gameplay, make the direct display path more asynchronous, or expose a user-selectable display-size/performance mode.
- The Nofrendo adapter is now the active path. The older in-repo minimal CPU/PPU core remains in the tree but should no longer be treated as the primary path unless Nofrendo is deliberately removed.

Recommended next FC/NES step:

- Have the user test combinations explicitly: hold Left/Right/Up/Down and tap A/B, then try Start/Select and LIST.
- If multi-touch still fails on the physical panel, capture raw point counts in `QdtechTouch::ReadPoints()` for one monitor run to distinguish firmware mapping from panel/driver behavior.
- If FPS/latency is still unacceptable, first add a fast display option such as native `256x240` or intermediate `300x240`, then evaluate Bluetooth gamepad support.

Important 2026-06-05 stability finding:

- A previous radio smoothing change (`834d55a`) was reverted by `76c1541` because the firmware became unstable.
- The XiaoZhi crash when entering chat was traced to internal SRAM pressure during audio-channel setup: `pthread: Failed to create task!` in `EspUdp::Connect()`.
- Commit `5b58b22` keeps photo/radio tasks smaller or lazy, catches UDP thread creation failure instead of aborting, shortens MCP tool descriptions, reduces MCP `tools/list` payload size, and treats MCP publish failures as warnings instead of user-facing network alerts.
- Do not reintroduce large always-running tasks, large MCP descriptions, or broad radio buffering changes without first proving XiaoZhi listen/speak on hardware.

## What Is Already Implemented

- XiaoZhi voice core from the upstream project.
- QDTech 3.5 inch landscape display adaptation.
- CST9217/TDDI touch polling with tap/swipe dispatch.
- LVGL touch input device for button clicks and slider interactions.
- Hybrid touch mode: LVGL for buttons/sliders, manual polling for gestures.
- LVGL desktop UI with main/app/XiaoZhi/radio/settings pages.
- Radio page with audio spectrum visualization (16 animated bars).
- Local Calendar page with month grid, Today, Prev, and Next controls.
- Local Focus Timer page with 25 minute focus mode, 5 minute break mode, start/pause, reset, and NVS-persisted completion count.
- Local Photos page replacing the previous Weather app tile, reading JPEG photos from common SD directories and playing them as a pure full-screen slideshow.
- Time and weather service.
- Main-page daily card with date-linked festival (19), history-on-this-day (31), and daily quote (32) content.
- Embedded LXGW WenKai LVGL subset fonts (499 Chinese characters).
- MP3 network radio service with 37 Chinese stations.
- Radio station categories: National (CNR), Beijing, Shanghai, Guangdong, Zhejiang, Jiangsu, Sichuan, Hunan, Hubei, Shandong, Music, Traffic, Other.
- Radio station favorites with NVS storage persistence.
- Radio station index persistence (last played station remembered).
- MCP tools for weather location and radio controls.
- Lightweight audio focus behavior so XiaoZhi states can pause the radio.
- Radio reconnect/fallback handling and remembered last successful station URL.
- Radio error state display after 5 consecutive failures.
- Main page visual enhancements:
  - Daily card breathing animation (opacity 0.92-1.0) for subtle visual depth.
  - Clock digit shadow pulse effect (opacity 10-40) for time display ceremony.
  - Weather particle animation (golden particles floating up from sun) for clear/cloudy weather.
- Optimized time/weather sync speed:
  - SNTP wait time reduced from 30s to 15s (500ms polling).
  - Weather API timeout reduced from 20s to 10s.
  - Weather retry attempts reduced from 3 to 2.

## Latest Runtime Notes: 2026-06-19 v1.7.18 FC Emulator Audio Output

- Latest release target is `v1.7.18`.
- Root cause of FC/NES silence: Nofrendo registered `nes.apu->process` through `osd_setsound()`, but the QDTech `qdtech_osd.c` implementation discarded the callback, so the emulator never generated or emitted PCM to the board codec.
- `components/nofrendo/qdtech_osd.c` now stores the sound callback, generates 22050 Hz mono PCM on each 60 Hz emulator input/video tick, and forwards samples through a QDTech audio callback.
- `FcEmulatorService` now receives Nofrendo PCM, resamples it to the ES8311 output rate when needed, enables codec output, and writes via the same `AudioCodec::OutputData()` path used by other board audio features.
- `RunNofrendoRom()` marks external audio active while the emulator is running so XiaoZhi's normal audio loop does not disable output underneath FC gameplay.
- Final `v1.7.18` local build passed from the Windows checkout: `xiaozhi.bin` `0x3cdf20`, smallest app partition `0x600000`, free `0x2320e0`.
- Final `v1.7.18` build was flashed to `COM13`.
- Post-flash logs confirmed `App version: 1.7.18`, `Ota: Current version: 1.7.18`, QDTech boot, WiFi/MQTT, SNTP/weather, FC service registration, FC page activation, SD mount, `/sdcard/FC` selection, and `fc scan found 192 nes files`.
- The serial monitor session did not capture a ROM actually starting, so the remaining hardware confirmation is to start any ROM and check for `NofrendoQD: audio frames=...` plus audible output.
- Release assets:
  - `qdtech-s3-touch-lcd-3.5-v1.7.18-app.bin`, SHA256 `f63c8383a2bbccaa0fbb852b1723329d7d390cee361ca3baa71e4515d021955e`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.18-firmware.zip`, SHA256 `aad007cfb27cafac979e542e5a98a60bd46a4ef7d7aff160e6c73d7f260fe404`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.18-full.bin`, SHA256 `2f447b012b14390adc85b6b9e75611eff55553417720646ab3c1e956cb0f1b75`.

2026-06-19 macOS follow-up after the user reported frequent freezes when entering FC:

- Pulled `origin/main` to `a234e01` / `v1.7.18` before changing code.
- High-probability freeze cause: the new FC audio path allocated a temporary `std::vector<int16_t>` on every Nofrendo audio tick while the board still has very tight internal SRAM margins.
- `FcEmulatorService` now reuses one service-owned audio output buffer instead of constructing a fresh vector for every audio callback.
- FC task priority was reduced from `5` back to `1` so the emulator yields more naturally to UI/touch/audio system tasks while entering and running games.
- Added a low-rate `fc audio frames=...` log with internal SRAM/largest-block diagnostics every two seconds during gameplay.
- Validation build passed from `/private/tmp/qdtech_s3_build_src` with explicit `esp32s3` target: `xiaozhi.bin` `0x3cec40`, smallest app partition `0x600000`, free `0x2313c0`.
- Hardware flash/runtime test is still needed. On the board, start a ROM and check for `nofrendo fps=...`, `NofrendoQD: audio frames=...`, and `FcEmulator: fc audio frames=...` while confirming FC no longer hangs on entry.

## Previous Runtime Notes: 2026-06-19 v1.7.17 Daily Card Lunar Festival Support

- Latest release target is `v1.7.17`.
- Main-page daily card now treats lunar festivals as first-class festival matches before history-on-this-day and daily quote fallback.
- Added a compact built-in 2024-2030 lunar festival date table for Spring Festival, Lantern Festival, Dragon Boat Festival, Qixi, Mid-Autumn Festival, Double Ninth Festival, Laba Festival, and Chinese New Year's Eve.
- The immediate user-facing bug is fixed: `2026-06-19` now matches `端午节` and shows `粽叶飘香，愿你安康顺遂。` instead of the daily quote.
- The daily-card update guard now uses full `YYYYMMDD` instead of day-of-year, so time sync and year changes cannot leave stale daily-card text behind.
- Daily-card title/body labels now use the already-linked `font_puhui_16_4` Chinese font for broader glyph coverage without adding the much larger `font_puhui_20_4` to the binary.
- Final `v1.7.17` release build passed from the Windows checkout: `xiaozhi.bin` `0x3cdac0`, smallest app partition `0x600000`, free `0x232540`.
- Final `v1.7.17` build was flashed to `COM13`. Boot logs confirmed `App version: 1.7.17`, `Ota: Current version: 1.7.17`, WiFi, MQTT, SNTP time sync, weather update, `Daily card updated for 2026-06-19 kind=festival`, and `Application: STATE: idle`.
- Release assets:
  - `qdtech-s3-touch-lcd-3.5-v1.7.17-app.bin`, SHA256 `6c7a63de0863de6de4cfa5cbed2aef369ab3afe55a049026cdc2c7adfa6842ad`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.17-firmware.zip`, SHA256 `3271900dd7adbd3a7a7df6950332ddb3051db7914298f64db940c3f631fb68d1`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.17-full.bin`, SHA256 `10f04be0887a09b377ca61b007f97d590d7cb584d7b7422da88340dcbf5a6af0`.
- Follow-up: visually confirm the daily card on the LCD reads `端午节`; runtime logs already prove the festival branch wins.
- Board-initiated OTA should be verified from a device running `1.7.16` or older: Settings -> Firmware -> Check -> Update -> reboot -> `1.7.17`.

## Previous Runtime Notes: 2026-06-19 v1.7.16 Apps Page Visual Polish And WiFi Icon Fix

- Latest release target is `v1.7.16`.
- Apps page was restyled to follow the supplied dark brown app-center reference:
  - black/brown background
  - main-screen `Nothing impossible` brand mark at the upper-left
  - two-column horizontal app cards with outlined app-code badges and status dots
  - warm brown card borders and a matching pill-style `Back` button
  - right-side decorative glyphs for Radio, Photos, XiaoZhi, NES, Calendar, Focus, Network, and Settings
- Network card WiFi glyph was corrected after hardware feedback that the dot was off-center. The new glyph is drawn inside one fixed 54x36 transparent container so the three arcs and bottom dot share a single center axis.
- Final `v1.7.16` release build passed from the Windows checkout: `xiaozhi.bin` `0x3cd5e0`, smallest app partition `0x600000`, free `0x232a20`.
- Final `v1.7.16` build was flashed to `COM13`. Boot logs confirmed `App version: 1.7.16`, `Ota: Current version: 1.7.16`, WiFi, MQTT, UI clock update, weather update, and `Application: STATE: idle`.
- Release assets:
  - `qdtech-s3-touch-lcd-3.5-v1.7.16-app.bin`, SHA256 `ebd91e9d87b0dbf6825a0f82b743243cc16e650da3a2983f0aa75d41da702605`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.16-firmware.zip`, SHA256 `cf0197d470f996a690a2e3f264181a9e8a283e2e209223ca40f6f8a073887565`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.16-full.bin`, SHA256 `4fc5797309c24f868675002c632e33118f50966a812af8fbb6a4fd904207e1a1`.
- Follow-up: inspect the Apps page on the LCD and confirm the Network WiFi glyph reads centered; the final visual judgment is hardware-authoritative.
- Board-initiated OTA should be verified from a device running `1.7.15` or older: Settings -> Firmware -> Check -> Update -> reboot -> `1.7.16`.

## Previous Runtime Notes: 2026-06-19 v1.7.15 Calendar UI Redesign And Brand Polish

- Release target was `v1.7.15`.
- Calendar page was redesigned to follow the supplied warm brown/gold reference:
  - left light today card with `今日`, `TODAY`, large day number, Chinese weekday, and `YYYY / MM`
  - right dark month panel with `Month YYYY`, `Minimal monthly view`, weekday headers, rounded date pills, orange weekend pills, and gold today highlight
  - bottom `Prev` / `Today` / `Next` controls
- The left card's extra `温暖的一天` label was removed after user feedback.
- The Calendar bottom controls were enlarged to 96x34 and given 12 px LVGL extended click padding. `Prev` was moved inward and upward to improve the bottom-left touch target.
- Secondary pages no longer use `XiaoZhi AI` as the top-left brand mark. Apps, Radio, Network, and Settings now use the same `Nothing impossible` treatment as the main page. Calendar intentionally uses the reference-style layout without the secondary brand mark.
- Final `v1.7.15` release build passed from the Windows checkout: `xiaozhi.bin` `0x3ccfc0`, smallest app partition `0x600000`, free `0x233040`.
- Final `v1.7.15` build was flashed to `COM13`. Boot logs confirmed `App version: 1.7.15`, `Ota: Current version: 1.7.15`, WiFi, MQTT, time sync, weather update, and `Application: STATE: idle`.
- Release assets:
  - `qdtech-s3-touch-lcd-3.5-v1.7.15-app.bin`, SHA256 `f038e0950131f347f155a2b763208a8959b1801464e75c15784c16d2888b7f82`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.15-firmware.zip`, SHA256 `3cd1ac5602678c77d5774c090ce06b761bbd304c2f2bb8180f3b125fcfd06935`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.15-full.bin`, SHA256 `4cf7c0b7ac20966d9b16adce17497da9cb5d5f5a22847f98c4ce2add71e80cab`.
- Follow-up: physically press Calendar `Prev`, `Today`, and `Next` on the panel after OTA/USB install; the code-side touch target is larger, but button feel is still hardware-authoritative.
- Board-initiated OTA should be verified from a device running `1.7.14` or older: Settings -> Firmware -> Check -> Update -> reboot -> `1.7.15`.

## Previous Runtime Notes: 2026-06-19 v1.7.14 FC ROM Scan Cap And Load Diagnostics

- Release target was `v1.7.14`.
- FC ROM scanning now keeps up to 192 `.nes` files from the first populated ROM directory instead of stopping at 64. The old `fc scan found 64 nes files` result was the firmware's protective cap, not missing SD contents.
- FC still scans conservatively in this order: `/sdcard/FC`, `/sdcard/nes`, `/sdcard/roms`, `/sdcard`.
- Start now validates the selected ROM before entering Nofrendo and reports clearer failure reasons:
  - invalid or missing iNES header
  - NES 2.0 header
  - ROM file larger than the current 2 MB Nofrendo PSRAM load guard
  - mapper not present in `components/nofrendo/nes/mmclist.c`
- The active emulator path remains the Nofrendo adapter under `components/nofrendo`; do not use the older in-repo minimal CPU/PPU core as the current compatibility reference.
- Final `v1.7.14` release build passed from the Windows checkout: `xiaozhi.bin` `0x3cc8e0`, smallest app partition `0x600000`, free `0x233720`.
- Final `v1.7.14` build was flashed to `COM13`. Boot logs confirmed `App version: 1.7.14`, `Ota: Current version: 1.7.14`, WiFi, MQTT, time sync, weather update, and `Application: STATE: idle`.
- Release assets:
  - `qdtech-s3-touch-lcd-3.5-v1.7.14-app.bin`, SHA256 `d08ce99d118d456439f43bad6acec751d36a6ae2b7967c38d040e289edea4b17`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.14-firmware.zip`, SHA256 `b99afb216cc6e1dcf8ef294a39b80a667b367b339989f9b793a2a2448ce02a8e`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.14-full.bin`, SHA256 `951f7cec9904d2b25781ef57774c28c4867d0c9b94fb0cc91c0bf6d03ea8b96b`.
- FC page scan verification still needs a tap into the FC page with the user's ROM SD card inserted.
- After the GitHub Release exists, the board-initiated OTA path should be verified from a device running `1.7.13` or older: Settings -> Firmware -> Check -> Update -> reboot -> `1.7.14`.

## Previous Runtime Notes: 2026-06-18 v1.7.13 On-Device Firmware Update Bootstrap

- Latest release target is `v1.7.13`.
- `SET / Settings / Firmware` now has a real `Check` / `Update` button, not just reserved status text.
- New QDTech-only files:
  - `main/boards/qdtech-s3-touch-lcd-3.5/firmware_update_service.h`
  - `main/boards/qdtech-s3-touch-lcd-3.5/firmware_update_service.cc`
- The firmware update service checks the latest GitHub Release:
  - `https://api.github.com/repos/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware/releases/latest`
  - It compares the release tag against `esp_app_get_description()->version`.
  - It looks for `qdtech-s3-touch-lcd-3.5-vX.Y.Z-app.bin` and uses that URL for OTA.
- `Ota` now exposes `StartUpgradeFromUrl()` and the app-image download path handles GitHub redirects directly through ESP-IDF HTTP.
- The update flow blocks unsafe starts:
  - no WiFi -> `WiFi needed`
  - radio/external audio active -> `Stop audio first`
  - XiaoZhi not idle -> `Wait idle`
- Release packaging now needs three assets:
  - `qdtech-s3-touch-lcd-3.5-vX.Y.Z-full.bin` for full USB flashing.
  - `qdtech-s3-touch-lcd-3.5-vX.Y.Z-firmware.zip` for manual download/archival.
  - `qdtech-s3-touch-lcd-3.5-vX.Y.Z-app.bin` for on-device OTA.
- Final `v1.7.13` release build passed from `/private/tmp/qdtech_s3_build_src`: `xiaozhi.bin` `0x3cd140`, smallest app partition `0x600000`, free `0x232ec0`.
- Hardware monitor after flashing:
  - App version and OTA current version both reported `1.7.13`.
  - ESP-IDF runtime reported `v5.5.2`.
  - WiFi initialized normally and obtained IP `192.168.1.104`.
  - Time synchronized successfully: `2026-06-18 17:49`.
  - Weather completed during boot: `weather ok 27 C Zhongshan Storm 17:49 code=96 updated=17:49`.
  - MQTT connected and the app reached `Application: STATE: idle`.
  - Internal SRAM remained stable in the observed idle window, with `free sram` around `13KB` and minimum around `8KB`.
- Release assets:
  - `qdtech-s3-touch-lcd-3.5-v1.7.13-app.bin`, SHA256 `f0ad472babf25b0c5fcde313bc53c302396f22a13b2761c0d472c3d77819593d`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.13-firmware.zip`, SHA256 `a49a4290c8718a66fd6f270f701fdf7a53ed053db2cc0ffa074ead5d8785a94a`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.13-full.bin`, SHA256 `64d914e33bbd3d41c123b9726e0129dfa68f8bbf099ed395b0f19270b411be08`.
- Important limitation: `v1.7.13` is the bootstrap firmware that adds the on-device updater. A full board-initiated download/write/reboot should be verified with the next higher GitHub Release. A board already running `1.7.13` should report `Latest` when the latest release is also `v1.7.13`.

## Previous Runtime Notes: 2026-06-18 v1.7.12 Network/System Settings Split

- Latest release target is `v1.7.12`.
- `NET` and `SET` are no longer duplicate entry points:
  - `NET / Network / WiFi Hub` opens a dedicated Network page.
  - `SET / Settings / System` opens the System Settings page.
- The Network page contains:
  - Connection status, mirrored from `DesktopUI::SetNetworkStatus()`.
  - Saved WiFi count.
  - A scrollable saved WiFi list from `SsidManager::GetSsidList()`.
  - Tap-to-set-default behavior through `SsidManager::SetDefaultSsid(index)`.
- Destructive WiFi actions are intentionally not exposed as touch buttons yet. Use the existing MCP tools for remove/switch until the on-device confirmation flow is designed.
- The Settings page now focuses on Display & Sound, Weather, and Firmware:
  - Brightness and volume sliders retain the existing hardware-backed behavior.
  - Weather location remains a status row tied to the existing XiaoZhi/MCP configuration path.
  - Firmware row reads the running app version through `esp_app_get_description()`.
  - OTA status text is a reserved anchor for the next real on-device firmware-update workflow.
- Radio page spectrum bars changed from one yellow color to `RADIO_BAR_COLORS[16]`, a soft rainbow palette. Bars are dim at idle and full opacity during playback.
- Final `v1.7.12` release build passed from `/private/tmp/qdtech_s3_build_src`: `xiaozhi.bin` `0x3ca6a0`, smallest app partition `0x600000`, free `0x235960`.
- Hardware monitor after flashing:
  - App version and OTA current version both reported `1.7.12`.
  - WiFi initialized normally and obtained IP `192.168.1.104`.
  - Time synchronized successfully: `2026-06-18 17:16`.
  - Weather completed during boot: `weather ok 28 C Zhongshan Storm 17:16 code=96 updated=17:16`.
  - MQTT connected and the app reached `Application: STATE: idle`.
  - Internal SRAM remained stable in the observed window, with `free sram` around `10-21KB` and minimum around `9KB`.
- Release assets:
  - `qdtech-s3-touch-lcd-3.5-v1.7.12-full.bin`, SHA256 `6ce97bb6b910420e0fd52c5b6185c8c5512e8c52f91cf7050dfbecb9276dbb65`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.12-firmware.zip`, SHA256 `b50b5510007093a9c90ba0c2accf4abf3580ed92d35d43d323e08f967102adcb`.

## Previous Runtime Notes: 2026-06-18 v1.7.11 Clock Visual Polish

- Latest release target is `v1.7.11`.
- Main page layout is intentionally unchanged. Only the large clock rendering was adjusted.
- Replaced the old four-card `lv_font_montserrat_48` digit layout with two complete labels using a small QDTech-only `qd_font_clock_72` generated from Montserrat Bold and limited to `0123456789:`.
- The clock keeps the warmer visual direction: hour digits use `COLOR_CREAM`, minute digits use `COLOR_GOLD`, and the colon is rendered as two soft gray round dots.
- The block/segment clock prototype was rejected on hardware because it looked blurry, visually broken, and too cold in color. Do not revive that path unless the design direction changes.
- The accepted clock labels are vertically lowered and use a shorter label box so the large digits visually align with the two colon dots and do not crowd the date row.
- `FlipDigit()` and `ClockShadowCb()` are now retained as no-op compatibility methods because other animation helpers still use the shared opacity/Y callbacks; the current clock path updates hour/minute labels directly in `RenderBigTime()`.
- Final `v1.7.11` release build passed from `/private/tmp/qdtech_s3_build_src`: `xiaozhi.bin` `0x3c9d80`, smallest app partition `0x600000`, free `0x236280`.
- Hardware monitor after flashing:
  - App version and OTA current version both reported `1.7.11`.
  - WiFi initialized normally and obtained IP `192.168.1.104`.
  - Time synchronized successfully: `2026-06-18 16:43`.
  - Weather completed during boot: `weather ok 28 C Zhongshan Storm 16:43 code=95 updated=16:43`.
  - Internal SRAM was stable in the observed window, around `25KB` free with minimum around `20KB`.
- Release assets:
  - `qdtech-s3-touch-lcd-3.5-v1.7.11-full.bin`, SHA256 `971fb48e6516bb5263db061e67e39512f245e853de22d72f049182bc5d10b710`.
  - `qdtech-s3-touch-lcd-3.5-v1.7.11-firmware.zip`, SHA256 `49bd8e1b2c62cdb5daa8da7f8b85940b2d1d12747333514c4aca72488ab26622`.

## Previous Runtime Notes: 2026-06-18 v1.7.10 Time/Weather Recovery

- Latest release target is `v1.7.10`.
- FC and Photos no longer force SD-card mounting during board construction. SD is mounted lazily when a feature needs it, and FC reuses an existing `/sdcard` mount.
- The Time/Weather task is created in PSRAM first, with an internal-memory fallback and diagnostic logging.
- Exiting FC now notifies the Time/Weather service to refresh the clock immediately and retry weather if the last weather fetch failed.
- Returning from any secondary page to the main page now notifies the Time/Weather service to refresh the clock immediately.
- The Time/Weather loop refreshes the clock every 5 seconds, but `DesktopUI::SetTime()` skips redraws when the displayed minute and date are unchanged.
- Time is accepted for display only after SNTP reports a real sync at least once, so stale valid-looking local time is less likely to be shown as corrected time.
- While the main page is visible, `DesktopUI::SetTime()` rewrites the big clock digits every 5 seconds even when the minute is unchanged. This is deliberate to self-repair stale LVGL label state.
- OTA server-time parsing no longer adds `timezone_offset` before `settimeofday()`. That offset caused a later +8 hour jump after OTA/MQTT startup; the system clock should stay UTC and `TZ=CST-8` handles display time.
- Weather uses Open-Meteo over HTTP instead of HTTPS, with a 3-second timeout and one quick retry, to avoid TLS contention with OTA/MQTT during boot.
- Desktop page switches invalidate the active screen, and main-page time/weather label updates invalidate the main page. This is intended to clear stale FC direct-draw pixels after leaving the emulator.
- Weather retry timing is now real 10-second retry behavior after a failed fetch. Previously the retry was delayed by the 60-second outer wait loop.
- Latest flashed local build: `xiaozhi.bin` `0x3c8f30`, smallest app partition `0x600000`, free `0x2370d0`.
- Hardware monitor after flashing:
  - WiFi initialized normally with static RX `6`, dynamic RX `8`, RX BA `6`.
  - Time stayed on the correct local hour after OTA/MQTT startup; the prior +8 hour jump did not recur.
  - Weather completed quickly over HTTP during boot: `weather ok 28 C Zhongshan Storm 15:51 code=95 updated=15:51`.
  - Main clock advanced normally from `15:51` to `15:52` in the monitor session.
  - Internal SRAM stayed around `13-14KB` free after startup, minimum around `10051` bytes during the observed window.
- Release assets should follow the existing two-asset pattern: `qdtech-s3-touch-lcd-3.5-v1.7.10-full.bin` and `qdtech-s3-touch-lcd-3.5-v1.7.10-firmware.zip`.

## What Still Needs Work

Highest priority areas are listed in `docs/NEXT_TASKS.md`.

Short version:

- Keep proving XiaoZhi voice, radio, touch, and display together on hardware after every meaningful firmware change.
- Improve display flush performance without rewriting the display path all at once.
- Move radio station configuration toward SPIFFS/SD/NVS while keeping built-in fallback stations.
- Expand settings UI only after the stability base is proven.

## Touch Architecture Note

The current touch system uses a hybrid approach:

### 2026-07-08 v1.8.17 Photo Segmented Swipe Recovery

- v1.8.16 monitor confirmed second-level taps are now alive: Menu opened Apps, Apps tiles opened FC/Photos, and each tap produced fresh touch packets after reset.
- Photo right-swipe still did not exit because the post-tap reset split a drag into short zero-delta releases.
- Added photo-page segmented swipe recovery: consecutive short touch segments within a small time window are accumulated into a horizontal swipe and routed through `HandleSwipe`.

### 2026-07-08 v1.8.16 Reset Latched Touch After Tap

- Monitor on v1.8.15 showed the touch controller kept replaying the first Menu tap (`p0=81 1a 01 a5`) after Apps opened, so second-level taps never produced new coordinates.
- Reset the touch controller immediately after a completed synthetic or natural tap release to clear the latched point and allow the next tap to report fresh coordinates.
- Reduced raw touch log spam by logging repeated legacy point packets once per second unless the packet changes.

### 2026-07-08 v1.8.15 Top Button Hit Zone Widening

- Widened manual hit zones for the main Menu and top-right Back buttons after monitor showed Menu landing at `x=416 y=43`, just outside the old `y < 42` range.
- Keeps v1.8.14 latched-coordinate suppression unchanged.

### 2026-07-08 v1.8.14 Latched Coordinate Suppression

- Changed synthetic-release suppression from a short timer to same-coordinate suppression until the touch controller reports a different point or no touch.
- Monitor on v1.8.13 showed the controller repeatedly replaying the first Menu point (`x=406 y=35`) for many seconds, causing endless Apps-page taps.

### 2026-07-08 v1.8.13 Touch Soft Release

- Removed the `WriteReg(0x0010, 0x00)` clear after reading a touch point; monitor showed the first tap worked but subsequent taps stopped reporting.
- Added a software synthetic release after a short stable press so manual page buttons receive a tap even when the controller keeps the same status latched.
- Added a short same-coordinate suppress window to avoid replaying a latched point as many repeated taps.

### 2026-07-08 v1.8.12 Manual Secondary Button Fallback

- Added manual tap fallback hit zones for secondary-page buttons because QDTech touch is manually polled and LVGL click callbacks are not reliable on this board.
- Covers Apps tiles/back plus Radio, Music, Media, Podcast, Calendar, Network, and Diagnostics primary buttons.
- This is intentionally scoped to `DesktopUI::HandleTap`; touch parsing from v1.8.11 is unchanged.

### 2026-07-08 v1.8.11 Touch Coordinate Flag Fix

- Fixed legacy touch coordinate parsing for hardware data like `p0=81 29 01 aa`: byte 0 bit `0x80` is the valid-touch flag and must be masked out before calculating X.
- Restored the old valid-point guard (`point_data[0] & 0x80`) while keeping the observed status-bit check at `0x0010`.
- Clears touch status after consuming a point so stale coordinates are not replayed as a permanent press.

### 2026-07-08 v1.8.10 Touch Idle Read Fix

- Matched pre-BMI270 touch idle behavior more closely: if `0x0010` does not report touch bit `0x08`, the poller now returns immediately instead of also reading the point block.
- Removed the once-per-second idle touch log/read path to reduce UI stalls while no finger is touching the panel.
- Touch point parsing remains the hardware-observed raw high-low format from `0x0014`.

### 2026-07-08 v1.8.09 Touch Restore Attempt

- Restored touch polling behavior closer to pre-BMI270 firmware `v1.7.87`: direct press/move/release flow, no synthetic early tap, no cached hold, no forced release, no stale-touch filter.
- Restored touch I2C device speed to 400 kHz and stopped sharing the board I2C mutex with the touch reader while BMI270 is temporarily disabled.
- Kept the corrected legacy point parser observed on hardware (`0x0010 = 0x08`, point bytes at `0x0014` as raw x/y high-low pairs).
- BMI270 remains disabled in this build so touch can be validated independently before re-enabling flip-to-focus.

- **LVGL input device** (`lv_indev_t`): Handles button clicks and slider interactions automatically.
- **Manual touch polling** (20ms timer): Handles gesture detection (swipe navigation) because LVGL 9.x does not expose gesture sensitivity tuning APIs.

This hybrid approach was adopted after discovering that:
1. LVGL gesture detection requires 50px minimum swipe distance (too insensitive).
2. LVGL 9.x does not provide `lv_indev_set_gesture_limit()` or similar APIs.
3. Direct struct member access is blocked by opaque type definitions.

Do not remove the manual touch polling without providing an alternative gesture detection mechanism.

## Do Not Randomly Change These Areas

- Do not delete or bypass the official XiaoZhi `Application` voice flow.
- Do not broadly refactor official upstream core code just to support QDTech UI.
- Do not commit generated `sdkconfig`, build output, WiFi credentials, activation data, or NVS dumps.
- Do not assume another 3.5 inch ESP32-S3 board uses the same pins, touch chip, or display controller.
- Do not declare radio or touch fixed from a compile result alone. Hardware logs matter.
- Do not let optional features such as Photos, Radio, or MCP tools consume enough internal SRAM to break XiaoZhi audio-channel creation.
- Do not create and hide duplicate LVGL layouts as a migration shortcut; remove obsolete objects after the replacement layout is proven.
- Do not remove the tracked `managed_components/78__esp-ml307/esp_udp.cc` hotfix unless the equivalent behavior is moved into a maintained component patch or upstream update.
- Do not add new Chinese daily-card strings without regenerating both tracked LXGW WenKai subset fonts.
- Do not add new Chinese Focus Timer strings without regenerating both tracked LXGW WenKai subset fonts.
- Do not let the Focus Timer page become visually dense again; verify spacing on the 480x320 hardware screen.
- Do not read `Board::GetInstance()` while `DesktopUI::Create()` is running inside the board constructor. Defer hardware-backed control synchronization until the page is opened.
- Do not assume LVGL widget callbacks receive touch input on this board; current touch is manually polled and dispatched.
- Do not commit `.codex_tmp/` or the full LXGW WenKai TTF used only for local font generation.

## How To Continue Development

1. Read this handoff and the other files in this handoff set.
2. Confirm you are in the intended checkout and branch.
3. Run `git status --short --branch` and protect any user changes.
4. Use the QDTech build command from `docs/PROJECT_STATUS.md`.
5. Make small scoped changes.
6. Build before finishing source changes.
7. Flash and capture boot logs when the task affects runtime behavior.
8. Commit with a message that states the actual firmware behavior changed.
9. Push to `qdtech-new/main` only when the user asked to update GitHub.
