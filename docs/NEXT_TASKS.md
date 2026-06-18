# QDTech Next Tasks

This list is intentionally ordered. Future work should start at the top unless the user gives a more specific request.

## Current Active Task: On-Device Firmware Update

Current state:

- Firmware `v1.7.12` added a dedicated Firmware row inside `SET / Settings / System`.
- The row reads the running app version through `esp_app_get_description()`.
- The UI currently exposes status text only: `Update path ready`.
- The existing upstream `Ota` class already handles version checks, firmware URLs, and `StartUpgrade()` once the OTA server reports a newer firmware.
- Boot-time OTA check currently runs during application startup and reports `Ota: Current is the latest version`.
- GitHub releases now publish two assets per QDTech release:
  - `qdtech-s3-touch-lcd-3.5-vX.Y.Z-full.bin`
  - `qdtech-s3-touch-lcd-3.5-vX.Y.Z-firmware.zip`

Next work:

- Decide the update source for the on-device flow:
  - Prefer the existing XiaoZhi OTA service if it can serve this custom QDTech build.
  - If using GitHub release assets, add a small manifest layer with version, URL, SHA256, and minimum version.
- Add a real touch action only after the source is known: check update, show current/new version, require confirmation, then start upgrade.
- Reuse `Ota::CheckVersion()` and `Ota::StartUpgrade()` if the existing service path is used; avoid a parallel updater unless the release-asset source requires it.
- Add a progress state to the Firmware row or a lightweight modal: checking, available, downloading, verifying, rebooting, failed.
- Block or defer OTA while FC gameplay, radio playback, or XiaoZhi listening/speaking is active.
- Keep the UI defensive: no fake update button, no destructive restart without confirmation, and no update attempt if battery/power assumptions are unclear.
- Verify by publishing a test version newer than the installed one, then confirm download, partition write, reboot, and rollback safety.

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
- Re-verify Photos opens visibly after any UI growth; watch PhotoService task-allocation logs as an internal SRAM regression signal.
- Record the COM port, commit hash, and important logs in future changelog entries.
- Complete a user-visible pass of Main -> Apps -> Radio -> Apps -> Main and confirm Radio keeps playing after leaving its page.

## Priority 2: Daily Card Content Growth

Current state:

- Main-page daily card is date-linked and local/offline.
- Content priority is festival, then history-on-this-day, then daily quote.
- Current Chinese rendering uses generated LXGW WenKai 16 px and 20 px subset fonts.

Next work:

- Expand the festival and history tables gradually.
- Decide whether lunar festivals should be calculated locally or loaded from a data file.
- Regenerate `qd_font_lxgw_16.c` and `qd_font_lxgw_20.c` whenever new Chinese glyphs are introduced.
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
