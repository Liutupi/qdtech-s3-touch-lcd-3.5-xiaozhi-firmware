# QDTech Next Tasks

This list is intentionally ordered. Future work should start at the top unless the user gives a more specific request.

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
- Record the COM port, commit hash, and important logs in future changelog entries.

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

## Priority 3: Audio Focus Hardening

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

## Priority 4: Radio Stability

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

## Priority 5: Touch Input Migration

Current state:

- `QdtechTouch::ReadFirstPoint()` reads and maps the first touch point.
- Board code performs tap/swipe detection manually.
- Logs exist for touch tap/swipe events without logging every poll.

Next work:

- Wrap raw touch reading, coordinate mapping, and gesture dispatch into clearer boundaries.
- Create a standard LVGL `lv_indev_t` read callback when safe.
- Keep old tap/swipe path available until the LVGL input path is proven on hardware.
- Test edge coordinates in all four corners of the landscape display.
- Check release handling and long press behavior before removing manual dispatch.

## Priority 6: Display Refresh

Current state:

- LVGL logical landscape buffer is rotated into the physical 320x480 panel path.
- Slow flush performance logs are already present.

Next work:

- Measure flush areas and elapsed time under idle, radio page, and XiaoZhi animation.
- Reduce unnecessary redraws before rewriting the display driver.
- Consider smaller dirty-region updates if they do not break orientation.
- Avoid large display-driver rewrites until hardware-visible behavior is captured.

## Priority 7: Settings And UI Growth

Only start after the stability base is boring and repeatable.

Possible additions:

- Visual weather city editor backed by NVS.
- Radio station management from file storage.
- Volume, brightness, and startup radio preference in NVS.
- Better WiFi settings page.
- Pomodoro module.
- UI polish for clock/weather/radio cards.
