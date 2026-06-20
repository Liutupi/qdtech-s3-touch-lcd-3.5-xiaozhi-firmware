# QDTech Next Tasks

This list is intentionally ordered. Future work should start at the top unless the user gives a more specific request.

## Current Active Task: OTA Follow-Up Verification

Current state:

- Firmware `v1.7.13` added the first real on-device update flow inside `SET / Settings / Firmware`.
- Firmware `v1.7.21` is the current higher-version release target after that bootstrap. It includes real QDTech battery ADC reporting, release-gated BOOT/GPIO0 deep-sleep soft power, and the saved-WiFi preservation fix after BOOT wake.
- The row has a `Check` / `Update` button with busy, latest, failed, no-WiFi, no-asset, and progress states.
- The board checks GitHub Releases at `Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware/releases/latest`.
- The updater compares release tags against `esp_app_get_description()->version`.
- `Ota::StartUpgradeFromUrl()` can now write a direct app image URL to the next OTA app partition.
- GitHub releases must now publish three assets per QDTech release:
  - `qdtech-s3-touch-lcd-3.5-vX.Y.Z-full.bin`
  - `qdtech-s3-touch-lcd-3.5-vX.Y.Z-firmware.zip`
  - `qdtech-s3-touch-lcd-3.5-vX.Y.Z-app.bin`

Next work:

- Verify a full board-initiated OTA from `1.7.17` or older to `1.7.21`: check -> update -> download -> partition write -> reboot -> new version.
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
