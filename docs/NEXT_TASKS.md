# QDTech Next Tasks

This list is intentionally ordered. Future work should start at the top unless the user gives a more specific request.

## Current Active Task: v1.7.44 WiFi Phone Config Sync Follow-Up

Current state:

- Firmware base is merged through `v1.7.43`.
- 2026-06-25 local build/flash with WiFi phone profile/weather config sync and the main-page brand overlap fix passed from a temporary no-space project copy.
- Latest pre-release hardware build result: `xiaozhi.bin` `0x4ce350`, smallest app partition `0x600000`, free `0x131cb0` (20%).
- Settings now shows phone web sync status, BLE sync status, the current custom text logo, owner name, and weather city.
- The verified phone sync path is now the local WiFi page/API at `http://<device-ip>/`; last hardware IP was `http://192.168.1.111/`.
- Verified endpoints: `GET /`, `GET /config`, and `POST /config` for `logo`, `name`/`owner`, `city`, `latitude`, and `longitude`.
- The phone page only asks for city; if latitude/longitude are omitted, firmware resolves the city through Open-Meteo geocoding. `city=中山` resolved to `22.5231,113.3791` during hardware validation.
- Brand labels update in place across Main, Apps, Radio, Network, and Settings after saving profile text. Avoid full UI reconstruction from the config apply path.
- Long brand text is clamped with ellipsis to prevent overlap. The main-page `logo/name` pair now uses fixed single-line heights and places `name` below `logo`; if the user wants full multiline display, design a larger dedicated profile area instead of forcing wraps into the top-left mark.
- Weather summary uses a Chinese-capable font; `dongguan` resolved to `东莞市 23.0180,113.7487` and rendered/fetched as Chinese during hardware validation.
- Runtime verification after POST logged `wifi config synced profile=1 weather=1`, refreshed weather, and did not reboot.
- The HTTP server task stack is in PSRAM; config values are preloaded/cached so HTTP handlers do not read NVS from a PSRAM stack.
- Weather location is cached before the PSRAM-stack weather task starts, avoiding NVS reads during weather fetch.
- BLE service code can advertise as `QDTech-Config` and accepts JSON writes for `logo`, `name`/`owner`, `city`, `latitude`, and `longitude`.
- Hardware flash verification passed on `/dev/cu.usbmodem212401`; boot reached stable WiFi, MQTT, weather, UI, and idle state.
- Current hardware memory result: after WiFi starts, internal RAM is too low for NimBLE (`free_internal=14743`, `largest_internal=14336`), so the guard skips BLE startup and shows `BLE low memory` instead of rebooting.
- Direct always-on BLE phone sync is therefore not yet usable on this board/runtime budget; keep the guard.
- The original macOS workspace path contains spaces; ESP-SR's generated linker path handling can fail there. Use a no-space project path for full local builds until that upstream/build-system issue is avoided.
- Firmware `v1.7.39` was the last fully verified Settings OTA release.
- Settings OTA from `v1.7.38` to `v1.7.39` was verified end-to-end on `COM13`.
- Verified OTA sequence: GitHub Release metadata fetch, app asset selection, proxy-first firmware download, image header validation, full app image write to `ota_1`, reboot, `App version: 1.7.39`, and `Marking firmware as valid`.
- The final OTA architecture is a persistent internal-RAM `ota_flash` worker plus internal staging buffer, allocated before the firmware HTTPS/proxy stream opens.
- The updater still uses the GitHub Release app asset, but for firmware downloads it tries proxy URLs before direct GitHub to avoid long signed redirect URLs.
- Important bootstrap note: boards already running older broken updaters may still need one USB flash to `v1.7.38` or newer, because the old updater code is the code that fails before it can install the fixed updater.
- During long OTA verification, repeated touch I2C timeout/reset logs appeared. They did not stop OTA, but they can make taps unreliable and should be treated as a separate touch-driver/hardware-timing issue.
- Firmware `v1.7.29` replaces the main-page weather module's primitive LVGL shape animation with a six-scene GIF animation pack for clear, cloudy, rain, snow, fog, and storm.
- QDTech now selects `LV_USE_GIF` and `LV_GIF_CACHE_DECODE_DATA`; the main weather card uses one 142x84 GIF scene area plus separate bottom text labels.
- Current build size after the full weather scene pack is `0x48f920`, with app partition free space `0x1706e0` (about 24%). Watch this if adding more bitmap/GIF assets.
- Firmware `v1.7.27` adds a third persisted Settings -> Appearance -> Theme option named `Tupi Warm`.
- `Tupi Warm` follows the warm paper `nothing impossible / tupi` direction: ivory background, graphite text, muted olive, and amber accents.
- The main page keeps the earlier accepted large time style, adds the Tupi brand mark, warms the Apps/FC surfaces, and hides the main daily-card network footer so the quote does not collide.
- The Tupi weather card deliberately reuses the prior Classic weather animation geometry after the new compact weather layout was rejected on hardware.
- Firmware `v1.7.23` previously added the persisted `Classic` and `Cat` themes.
- The Cat theme keeps the existing desktop architecture but changes palette, cards, brand mark, main clock styling, daily-card decoration, and the XiaoZhi face visual style.
- Cat theme was built and flashed to `COM13`; the final time-card layout no longer overlaps the top-left brand mark.
- Firmware `v1.7.13` added the first real on-device update flow inside `SET / Settings / Firmware`.
- Firmware `v1.7.22` is the current higher-version release target after that bootstrap. It includes the v1.7.21 battery/BOOT/WiFi fixes plus a hardened GitHub OTA download/write path.
- The row has a `Check` / `Update` button with busy, latest, failed, no-WiFi, no-asset, and progress states.
- The board checks GitHub Releases at `Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware/releases/latest`.
- The updater compares release tags against `esp_app_get_description()->version`.
- `Ota::StartUpgradeFromUrl()` can now write a direct app image URL to the next OTA app partition, and v1.7.22 tolerates missing `Content-Length` plus short first HTTP/TLS chunks.
- GitHub releases must now publish three assets per QDTech release:
  - `qdtech-s3-touch-lcd-3.5-vX.Y.Z-full.bin`
  - `qdtech-s3-touch-lcd-3.5-vX.Y.Z-firmware.zip`
  - `qdtech-s3-touch-lcd-3.5-vX.Y.Z-app.bin`

Next work:

- Polish the WiFi phone page UX and optionally add a QR code or clearer on-device URL display.
- Consider adding a city search preview/confirmation if users often enter ambiguous city names.
- Consider a detail/edit screen for full long logo/name text if ellipsis is not sufficient.
- If keeping BLE, prototype config mode only: enter from Settings, release enough services, start NimBLE, sync values, stop BLE, then restart normal network/audio flow.
- Keep the current BLE memory guard even if another path is chosen; it prevents the board from rebooting under low internal SRAM.
- Re-run hardware flash/monitor after any sync change and confirm boot, WiFi reconnect, MQTT idle, touch navigation, weather refresh, GET/POST config, and no reset/backtrace.
- Investigate touch I2C stability if missed taps continue. Capture whether failures correlate with OTA/network load, display flushes, or normal idle. Avoid mixing this with OTA unless the user reports that Update taps cannot be recognized.
- Inspect all six weather scenes on the physical 480x320 screen. Storm has been runtime-verified through live Zhongshan weather; clear, cloudy, rain, snow, and fog still need visual inspection under forced or real weather codes.
- Inspect `Tupi Warm` on the physical 480x320 screen and tune exact time colon alignment, top-right status spacing, daily-card line wrapping, and weather-card contrast from hardware feedback.
- Inspect the Cat theme on the actual 480x320 screen and tune exact pink strength, time-card spacing, daily-card cat position, and Chinese brand readability from hardware feedback.
- Consider replacing the restart-to-apply theme flow with a safe immediate UI recreation only after LVGL timer/object lifecycle risk is designed and tested.
- Keep OTA validation practice: start serial monitor once before tapping Update and do not reopen the port until reboot completes, because reopening COM13 resets the board.
- Verify BOOT physical-key wiring: confirm the user-visible flow on battery only: long-press BOOT until the screen turns off, release, then press BOOT once to wake. Confirm it reconnects to the saved WiFi without pairing/config mode. Record whether USB-connected and battery-only behavior differ.
- Add a second-tap confirmation or a tiny modal before starting `Update`; the current bootstrap intentionally avoids auto-update but still starts update on the available-state button.
- Consider adding SHA256 verification by release manifest or asset sidecar before writing, because GitHub's latest-release JSON does not provide the asset checksum.
- Keep blocking OTA while FC gameplay, radio playback, or XiaoZhi listening/speaking is active.
- Keep the UI defensive: no destructive restart without a clear user action, and no update attempt when WiFi is disconnected.

## Priority 0: Keep The Base Reproducible

- Keep `build-qdtech` as the known QDTech build directory.
- Keep build commands in documentation current.
- Avoid committing generated `sdkconfig` or build artifacts.
- After any runtime-affecting change, build, flash, and capture logs.
- Before pushing, verify the remote head if the push command times out.

## Priority 1: Hardware Runtime Validation

- Re-verify XiaoZhi wake/listen/speak on hardware after radio or audio changes.
- Re-verify radio playback through the actual board speaker.
- Re-verify touch tap/swipe after display or UI coordinate changes.
- Re-verify weather failure behavior with network disconnected or API failure.
- Re-verify Chinese daily-card wrapping after adding any new festival, history, or quote text.
- Re-verify Focus Timer visual spacing and touch zones after any layout change.
- Re-verify Calendar Prev / Today / Next on hardware after the `v1.7.15` touch-target enlargement; the code now uses 96x34 buttons plus 12 px LVGL extended click padding.
- Re-verify the Apps page Network card visually on hardware after the `v1.7.16` WiFi glyph fix; the icon now uses one 54x36 container so the arcs and dot share a center axis.
- Re-verify Photos opens visibly after any UI growth; watch PhotoService task-allocation logs as an internal SRAM regression signal.
- Record the COM port, commit hash, and important logs in future changelog entries.
- Complete a user-visible pass of Main -> Apps -> Radio -> Apps -> Main and confirm Radio keeps playing after leaving its page.

## Priority 2: Daily Card Content Growth

Current state:

- Main-page daily card is date-linked and local/offline.
- Content priority is lunar/fixed festival, then history-on-this-day, then daily quote.
- Current daily-card Chinese rendering uses the already-linked `font_puhui_16_4` font so added lunar festival text can display without regenerating LXGW subsets.

Next work:

- Expand the festival and history tables gradually.
- Extend the compact built-in lunar festival date table beyond 2030 if the firmware is expected to run unchanged after that range.
- Regenerate `qd_font_lxgw_16.c` and `qd_font_lxgw_20.c` whenever new Chinese glyphs are introduced outside the daily-card Puhui-font path.
- Keep body text short enough for the 438x94 main-page card.

## Priority 3: Focus Timer Hardening

Current state:

- Apps tile `FOC / Focus / 25 min` opens a local Focus Timer page.
- The page supports 25 minute focus mode, 5 minute break mode, start/pause, reset, and an in-memory completed-session counter.
- The layout was simplified into three columns after the first visual version overlapped on the 480x320 screen.

Next work:

- Verify the final spacing on hardware after user feedback.
- Add optional sound/vibration or visual completion indication only if it does not interfere with XiaoZhi audio.
- Persist completed count and user-selected durations in NVS if the user wants this to survive reboot.
- Consider adding a short/long break cycle after several completed focus sessions.
- Regenerate the LXGW WenKai subset fonts whenever new Chinese labels are added.

## Priority 4: Audio Focus Hardening

Current state:

- `RadioService` has lightweight audio focus blocking based on XiaoZhi application states.
- XiaoZhi states such as connecting/listening/speaking/audio testing have priority over radio.
- Radio remembers user playback intent and can resume after idle.

Next work:

- Consider extracting a small `AudioFocus` or `AudioSession` abstraction only after current behavior is proven stable.
- Keep the abstraction simple and non-blocking.
- Avoid holding locks while calling `AudioCodec` or UI update paths.
- Add logs for every focus owner transition.
- Verify repeated play/pause/stop calls remain idempotent.

## Priority 5: Radio Stability

Current state:

- Built-in MP3 station list.
- Multiple URL fallback.
- Last successful URL is remembered per station while service is running.
- Reconnect behavior exists for stream/read/decode failures.

Next work:

- Persist station list outside code, preferably SPIFFS/SD `radio.json`, while keeping built-in fallback stations.
- Keep direct MP3 working before adding HLS/AAC.
- Add diagnostics for transport type and content type when a station fails.
- Add a simple user-visible error state for repeated reconnect failure.

## Priority 6: Touch Input Migration

Current state:

- `QdtechTouch::ReadFirstPoint()` reads and maps the first touch point.
- Board code performs tap/swipe detection manually.
- Logs exist for touch tap/swipe events without logging every poll.
- `DesktopUI::HandleTouchRelease()` now centralizes release classification.
- Settings sliders and scrolling have a scoped manual-touch bridge.

Next work:

- Wrap raw touch reading, coordinate mapping, and gesture dispatch into clearer boundaries.
- Create a standard LVGL `lv_indev_t` read callback when safe.
- Keep old tap/swipe path available until the LVGL input path is proven on hardware.
- Test edge coordinates in all four corners of the landscape display.
- Check release handling and long press behavior before removing manual dispatch.

## Priority 7: Display Refresh

Current state:

- LVGL logical landscape buffer is rotated into the physical 320x480 panel path.
- Slow flush performance logs are already present.

Next work:

- Measure flush areas and elapsed time under idle, radio page, and XiaoZhi animation.
- Reduce unnecessary redraws before rewriting the display driver.
- Consider smaller dirty-region updates if they do not break orientation.
- Avoid large display-driver rewrites until hardware-visible behavior is captured.

## Priority 8: Settings And UI Growth

Only start after the stability base is boring and repeatable.

Current state:

- The Settings page is scrollable and usable on the 480x320 display.
- Brightness and volume sliders are connected to hardware and persist changes.
- Network controls moved out of Settings. `NET / Network / WiFi Hub` now owns saved WiFi display and default-network selection.
- Settings now includes a Firmware row for current version and future OTA update status.
- Hardware values are deliberately read only when Settings opens, avoiding board-constructor re-entry during desktop creation.

Next work:

- Complete the on-device firmware update flow from the Firmware row.
- Visual weather city editor backed by NVS.
- Radio station management from file storage.
- Startup radio preference in NVS.
- Add safe WiFi remove/switch UI only after designing a confirmation flow.
- UI polish for clock/weather/radio cards.
