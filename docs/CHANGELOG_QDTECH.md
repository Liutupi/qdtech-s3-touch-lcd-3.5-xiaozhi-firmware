# QDTech Firmware Changelog

This changelog tracks QDTech-specific firmware maintenance. It is not a replacement for `git log`; it records the practical handoff facts that future maintainers need.

## 2026-07-03: v1.7.76 OTA Safety, Apps, Music Recent

Scope:

- Bumped firmware version to `1.7.76`.
- Added GitHub release asset size and `SHA256SUMS.txt` lookup to the QDTech firmware update service.
- Blocked OTA offers and OTA starts when the release app binary is larger than the next OTA partition.
- Added expected-size and SHA256 verification to direct URL OTA, including `Content-Length`, downloaded byte count, partition overflow, and final digest checks.
- OTA progress now appears directly on the Settings update button and the Apps `Settings` tile while downloading, and the Apps tile preserves the last percentage across shared status refreshes.
- Preferred the exact release app asset name before broad fallback app asset matches.
- Expanded the Apps page from 8 tiles to 10 compact tiles by adding `Music` and `Podcast`.
- The `Music` tile opens XiaoZhi chat for voice song requests; the `Podcast` tile opens and activates the podcast list.
- Added a hidden Diagnostics page opened by long-pressing the Apps page `Settings` tile.
- Diagnostics displays version, board, running/next OTA partitions, OTA app file size/slot/margin, heap/PSRAM, WiFi/IP/RSSI, saved WiFi count, battery, and last OTA status, with a manual refresh button.
- Added a dedicated Music request page behind the Apps `Music` tile.
- The Music page shows the latest song title, artist/source, and current lyric/playback line, with `Talk`, `Again`, `Face`, and `Stop` actions.
- Added lightweight live status badges on Apps tiles for Radio, Network, Settings/OTA, Music, and Podcast.
- Added live Apps tile status for Photos refresh, ready, no-photo, SD-card, and decode states without reintroducing overlay text on the full-screen slideshow.
- Added live Apps tile status for NES/FC emulator scanning, loading, error, selected-ROM, and playing states.
- Kept the Settings/OTA badge in an explicit `USB` state when the available app binary is too large for OTA.
- Calendar's Apps tile status now refreshes from the shared Apps status path and shows the synced current date when available.
- Added live Apps tile status for Focus, showing ready duration, running minutes, paused, or done state.
- Focus now supports one-tap work/break cycling after completion: finished work offers `休息`, and finished break offers `专注`.
- Focus `今日完成` now stores its count with a synced date and automatically resets on a new day.
- Startup OTA check failure no longer blocks normal XiaoZhi/MQTT startup; if the OTA API times out, firmware continues with the default MQTT path.
- Focus date reconciliation no longer writes NVS from the PSRAM-stack time/weather task, avoiding a cache-disabled flash assert after WiFi/time sync.
- Network's Apps tile now preserves live WiFi status across Apps refreshes and shows the current IP when connected.
- Podcast's Apps tile now preserves the latest playback/list state across Apps refreshes instead of resetting to `Episodes`.
- Added a three-item Music recent-song list persisted in NVS. Recent rows can replay the stored direct URL through the existing radio/music URL playback path. `Again` replays the newest saved song, `Clear` removes all saved songs, and long-pressing a row removes only that song.
- Recent-song replay rows now show `Replaying...` while waiting for the stream and keep a short RadioService failure reason when RadioService reports an error for that replayed song.

Verification:

- macOS ESP-IDF 5.5 configured for `esp32s3` with the QDTech 7 MB / 7 MB OTA partition table.
- `main/ota.cc`, `main/boards/qdtech-s3-touch-lcd-3.5/firmware_update_service.cc`, and `main/boards/qdtech-s3-touch-lcd-3.5/desktop_ui.cc` compiled to object files.
- `esp-idf/main/libmain.a` built successfully.
- After Diagnostics was added, `esp-idf/main/libmain.a` was rebuilt successfully again. The only warning observed was the existing unused `rgb565` helper in `fc_emulator_service.cc`.
- After the Music request page was added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After Apps tile status badges were added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After Music recent-song replay was added, `desktop_ui.cc.obj`, `qdtech_s3_touch_lcd_3_5.cc.obj`, and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After Music recent clear was added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After recent replay pending/failure feedback was added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After the oversized OTA `USB` badge was added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After Music recent single-row removal was added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After inline Music replay failure reasons were added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After persistent Music recent failure marks were added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After live Focus tile status was added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After shared Calendar tile date refresh was added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After persistent Network tile status and connected-IP display were added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After persistent Podcast tile status was added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After Music `Again` one-tap recent replay was added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again.
- After Focus one-tap work/break cycling was added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again.
- After OTA progress labels were added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again.
- After OTA file/slot/margin details were added to Diagnostics, `desktop_ui.cc.obj`, `firmware_update_service.cc.obj`, and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After Focus daily count reset was added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After live NES tile status was added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After live Photos tile status was added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After OTA progress preservation across Apps tile refreshes was added, `desktop_ui.cc.obj` and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- After the startup-connectivity hotfix was added, `desktop_ui.cc.obj`, `application.cc.obj`, and `esp-idf/main/libmain.a` rebuilt successfully again with the same existing `rgb565` warning.
- Full no-space-path build passed from `/Users/tupi/qdtech_worktree_nospace` using QDTech board defaults and the existing local `78__esp-opus-encoder` dependency fix. The final `xiaozhi.bin` was generated at `0x639120`; the 7 MB app partition has `0xc6ee0` free (11%).
- Final `v1.7.76` no-space-path build confirmed `App "xiaozhi" version: 1.7.76`.
- `idf.py merge-bin` generated `merged-binary.bin` at `0x739120`.
- Local release candidate assets:
  - `releases/v1.7.76/qdtech-s3-touch-lcd-3.5-v1.7.76-app.bin`: `48eef6a10e44dbaa1e9d9b495feac4ca9501792efd9efeb642704879ea20bbc3`
  - `releases/v1.7.76/qdtech-s3-touch-lcd-3.5-v1.7.76-full.bin`: `411fa3a642d1fe9df837924ef7eda75adb1838a62bb96ee2aa60f688944c0e76`
  - `releases/v1.7.76/qdtech-s3-touch-lcd-3.5-v1.7.76-firmware.zip`: `207c302fa006a2fc05596b3bf86baebd12e75b2a7d78305e94055f7420b010d9`
  - `releases/v1.7.76/SHA256SUMS.txt` records the same hashes.
- `git diff --check` passed.
- Full link remains blocked only while the repo path contains the space in `带小智 3.5 寸`; ESP-SR prebuilt library paths are not quoted correctly by the generated link line.

## 2026-07-01: v1.7.75 NetEase Lyric Overlay and Cutover Stability

Scope:

- Bumped firmware version to `1.7.75`.
- Added a dedicated XiaoZhi music lyric overlay and `DesktopUI::ClearMusicLyric()`.
- Kept the existing `self.music.play_url` playback chain intact while hardening lyric state changes around song cutover and stop.
- Added lyric generation guards so an old scheduled lyric task exits when a new song starts.
- Filtered stale external `self.music.show_lyric` / UDP lyric updates when they no longer match the current song.
- Cleared stale lyrics when a new song has empty `lyrics_json`, showing `Playing: <title>` instead.
- Expanded lyric parsing to recursive/nested payloads and additional NetEase-style field names.
- Fixed a suspected second-song crash source by avoiding a recursive parse after `cJSON_Delete(root)`.
- Added browser-like headers for NetEase direct MP3 URLs and stopped retry loops on custom URL `401/403/404/410`.
- Increased lyric scheduler and UDP listener stacks to 8192 bytes in PSRAM.

Verification:

- Built on Windows with ESP-IDF from `C:\Users\Administrator\esp-idf`; `ninja -C build-qdtech-v1.7.62 -j 1 all` passed.
- App binary size was `0x630fb0`; smallest app partition is `0x700000`; free app space is `0xcf050` (12%).
- `idf.py -B build-qdtech-v1.7.62 merge-bin` produced a full image of `0x730fb0`.
- App-only flashed to `COM13`; esptool hash verification passed.
- Boot/live tests confirmed `App version: 1.7.75`, `self.music.play_url ... lyrics_json length=1945`, `ParseLyricsJson ... lines=41`, HTTP `200`, playback frames, and `SetMusicLyric overlay line=...`.
- Cutover tests confirmed an empty-lyrics second song did not keep the first song lyrics, and a later `lyrics_json length=4203` song parsed to `87` lines and updated the overlay.
- `self.music.stop` cleared and hid the lyric overlay.

Release assets:

- `releases/v1.7.75/qdtech-s3-touch-lcd-3.5-v1.7.75-app.bin`: `bac3a9ef7a7c879a327f9370725879b0385cecc4667cf635c43af0ae81e8a77e`
- `releases/v1.7.75/qdtech-s3-touch-lcd-3.5-v1.7.75-full.bin`: `4a9a0b3ee5cf5aaa50323cecb8881f7a639308a6ff9787e2eee71ef68d2393bd`
- `releases/v1.7.75/qdtech-s3-touch-lcd-3.5-v1.7.75-firmware.zip`: `f7853d03ddc2080338bb76c2e71888389b7e099dc521fa2a2439c0df63134ca2`

## 2026-07-01: v1.7.74 NetEase Lyric Display Fix

Scope:

- Bumped firmware version to `1.7.74`.
- Fixed firmware-side lyric display for NetEase MCP playback after Mac-side logs confirmed `lyrics_json` and `self.music.show_lyric` were already being sent.
- Added `DesktopUI::SetMusicLyric(...)` to update the XiaoZhi bottom message label directly.
- Added a short lyric hold window so ordinary chat/status updates do not immediately clear displayed lyrics.
- Moved scheduled lyric playback to a PSRAM-stack task using `xTaskCreateWithCaps`.
- Made lyric parsing tolerant of wrapped JSON arrays, `[time,text]` arrays, alternate field names, raw LRC text, and nested JSON strings.
- Kept `self.music.play_url` playback behavior unchanged and did not expose any `play_music` tool name.

Verification:

- Built on Windows with ESP-IDF from `C:\Users\Administrator\esp-idf`; `ninja -C build-qdtech-v1.7.62 -j 1 all` passed.
- App binary size was `0x6307f0`; smallest app partition is `0x700000`; free app space is `0xcf810` (12%).
- `idf.py -B build-qdtech-v1.7.62 merge-bin` produced a full image of `0x7307f0`.
- App-only flashed to `COM14`; esptool hash verification passed.
- Boot and live MCP tests confirmed `Ota: Current version: 1.7.74`, `self.music.play_url ... lyrics_json length=1800`, `ParseLyricsJson bytes=1800 lines=25`, `play_url lyrics started ... stack=psram`, repeated `DesktopUI: SetMusicLyric`, HTTP `200` MP3 streaming, and continuous playback frames.

Release assets:

- `releases/v1.7.74/qdtech-s3-touch-lcd-3.5-v1.7.74-app.bin`: `4e279b5356aa8bc0b80b8af270cb29d7fd032cb8da7abd9b15fec3d79b428115`
- `releases/v1.7.74/qdtech-s3-touch-lcd-3.5-v1.7.74-firmware.zip`: `ae64b993b7e926ffed89f7f10f3025ff3f98cfa74b21d552cba230d40cb6a11b`
- `releases/v1.7.74/qdtech-s3-touch-lcd-3.5-v1.7.74-full.bin`: `ba2288a300fae5463126363549711a79a0b7615ab45547885d85d8b4c66ce963`

## 2026-07-01: v1.7.73 NetEase Second-Song Chain Fix

Scope:

- Bumped firmware version to `1.7.73`.
- Added a lightweight UDP listener on port `45678` for lyric display and JSON playback fallback packets containing `title`, `artist`, and direct HTTP/HTTPS `url`.
- Kept the HTTP music command helper disabled at runtime, because starting it consumed internal SRAM and previously interfered with MQTT/OTA connection setup.
- Documented the Mac-side root cause and fix: remove the public `play_music` compatibility tool because it conflicts with XiaoZhi's internal `play_music`; keep `music.netease_play` and device `self.music.play_url` as separate names.

Verification:

- Built on macOS with ESP-IDF from `/Users/tupi/esp/esp-idf-v5.5`; CMake reported `App "xiaozhi" version: 1.7.73`.
- App binary size was `0x631ea0`; smallest app partition is `0x700000`; free app space is `0xce160` (12%).
- Flashed to `/dev/cu.usbmodem212401`; esptool hash verification passed.
- Boot log confirmed `App version: 1.7.73`, Wi-Fi IP `192.168.1.114`, UDP lyric listener on `45678`, MQTT connected, and `self.music.play_url` registered.
- Hardware voice test verified two consecutive NetEase songs:
  - `道别是一件难事 - 上海彩虹室内合唱团`
  - `绿光 - 孙燕姿`
- For the second song, logs showed a fresh `music.netease_play`, a new `tools/call self.music.play_url`, board-side `% self.music.play_url...`, `stream open ... status=200`, and continuous playback frames.

Release assets:

- `releases/v1.7.73/qdtech-s3-touch-lcd-3.5-v1.7.73-app.bin`: `75b1ef1fd58dfc3e000087405d07dc7e62b497c0a9633900158e8da5dfefb2bb`
- `releases/v1.7.73/qdtech-s3-touch-lcd-3.5-v1.7.73-firmware.zip`: `8eb6a84cb2b707cb7a9e262c1df1518fb4524b5d72c22978d1b119bbaf39c8e9`
- `releases/v1.7.73/qdtech-s3-touch-lcd-3.5-v1.7.73-full.bin`: `387e14e258d78b1bce5c895c030169e57cc2b17aecdf8ee07c72df0399805a71`

## 2026-07-01: v1.7.67 Autonomous Goodbye Speech Guard

Scope:

- Bumped firmware version to `1.7.67`.
- Kept the `v1.7.66` startup speaking grace.
- Fixed the follow-up hardware finding that a later autonomous server line such as "拜拜啦，下次再聊！" could still enter `speaking`, pause NetEase custom URL playback, and reopen the MP3 from the beginning.
- While a custom direct MP3 URL is playing, autonomous `speaking` transitions that do not follow user `listening`/`connecting`/audio-testing are ignored for audio focus and scheduled for abort.
- Real user interruption still yields audio focus.

Verification:

- Built on macOS with ESP-IDF from `/Users/tupi/esp/esp-idf-v5.5`; CMake reported `App "xiaozhi" version: 1.7.67`.
- App binary size was `0x62ee20`; smallest app partition is `0x700000`; free app space is `0xd11e0` (12%).
- Flashed to `/dev/cu.usbmodem212401` at 460800 baud; esptool hash verification passed.
- Boot log confirmed `App version: 1.7.67`, Wi-Fi connected, MQTT connected, and `self.music.play_url` registered.
- Hardware serial and Mac NetEase logs verified two consecutive song requests:
  - `道别是一件难事 - 上海彩虹室内合唱团`
  - `暮色森林 - 欧阳娜娜`
- For both songs, the chain included `music.netease_play` followed by board-side `% self.music.play_url...`, fresh direct MP3 URLs, HTTP `200`, and continuous playback frames.
- During both songs, autonomous goodbye/closing speech was ignored for audio focus and aborted, while MP3 frames continued increasing and the song did not restart.

Release assets:

- `releases/v1.7.67/qdtech-s3-touch-lcd-3.5-v1.7.67-app.bin`: `9791772b54190ec0c0a6de14dace9e87f103e7ce3defbddc1f3024d8579d710f`
- `releases/v1.7.67/qdtech-s3-touch-lcd-3.5-v1.7.67-firmware.zip`: `70adb5a3a0bee4b4d980581beef5652c643fa805de61697a4bf95984291ad303`
- `releases/v1.7.67/qdtech-s3-touch-lcd-3.5-v1.7.67-full.bin`: `3a243f1b0b8c5473fe883e7f336c0a03e841fdc8802b332173406160b5fda529`

## 2026-07-01: v1.7.66 Music Startup Speech-Focus Guard

Scope:

- Bumped firmware version to `1.7.66`.
- Added a short 4 second speaking-focus grace window after `self.music.play_url` starts a custom direct MP3 URL.
- During that window, a XiaoZhi `speaking` state caused by a post-tool closing line such as "我先退下了" no longer pauses the just-started music stream and causes it to reopen from the beginning.
- Real user interruption states such as listening/connecting/audio-testing still yield audio focus.
- Updated `self.music.play_url` and `self.music.play` tool descriptions to tell the server not to speak extra confirmation after the tool succeeds.

Verification:

- Built on macOS with ESP-IDF from `/Users/tupi/esp/esp-idf-v5.5`; CMake reported `App "xiaozhi" version: 1.7.66`.
- App binary size was `0x62ec90`; smallest app partition is `0x700000`; free app space is `0xd1370` (12%).
- `idf.py -B build-qdtech-s3-final ... build merge-bin` completed successfully.
- 待确认: hardware OTA/flash verification for `v1.7.66`.

Release assets:

- `releases/v1.7.66/qdtech-s3-touch-lcd-3.5-v1.7.66-app.bin`: `dba77d32bca511b539c078f8dee5f76078967a6f4a5b5e34be0c7a2449dd02ef`
- `releases/v1.7.66/qdtech-s3-touch-lcd-3.5-v1.7.66-firmware.zip`: `fb9899d36bfd388c5a0318c01de206d3eafe6d9eed195853ac46e4440f3f5514`
- `releases/v1.7.66/qdtech-s3-touch-lcd-3.5-v1.7.66-full.bin`: `d6799fdfa31922e8a0f35e7c27b82d76d68b7f5e58dffb8101e50eb92a75f306`

## 2026-06-30: v1.7.65 Final NetEase MCP Music Chain Hardening

Scope:

- Bumped firmware version to `1.7.65`.
- Kept the remote `v1.7.64` `RadioService::PlayUrlFromTool(...)` music URL implementation and `self.music.play` alias.
- Changed external-audio handling so music playback keeps the XiaoZhi MQTT/MCP protocol online, allowing second-song requests to reach the board while the first direct MP3 URL is active.
- Let low-memory `self.music.*` MCP calls run inline when thread creation is not possible, while preserving low-memory rejection for heavier non-music tools.
- Documented the required Mac-side NetEase bridge behavior: every successful `music.netease_play` must be followed by `tools/call self.music.play_url` with a fresh direct MP3 URL.

Verification:

- Built and flashed from `build-qdtech-s3-final`; CMake reported `App "xiaozhi" version: 1.7.65`.
- App binary size was `0x62ea70`; smallest app partition is `0x700000`; free app space is `0xd1590` (12%).
- Flash to `/dev/cu.usbmodem212401` completed and esptool hash verification passed.
- Local hardware session on `/dev/cu.usbmodem212401` showed the Mac-side bridge calling `self.music.play_url` after `music.netease_play`.
- Board serial showed `% self.music.play_url...`, continuous MP3 decode frames, and nonzero PCM write peaks with output enabled.
- User confirmed music playback worked after this final pass.

Release assets:

- `releases/v1.7.65/qdtech-s3-touch-lcd-3.5-v1.7.65-app.bin`: `db0b2cf10d1d9f344a8df7b372e50e66a6076ed687f3e4f3bd5265ac8d35f639`
- `releases/v1.7.65/qdtech-s3-touch-lcd-3.5-v1.7.65-firmware.zip`: `e0cda4359209694c4124ad7e96bffce557d0cdb11c1642ba5e267da089355405`
- `releases/v1.7.65/qdtech-s3-touch-lcd-3.5-v1.7.65-full.bin`: `00d4b13de11b473a041002b0d37e631f8e9289bd6947ead14af8469206077f53`

## 2026-06-30: v1.7.63 Interruptible Music URL Playback

Scope:

- Bumped firmware version to `1.7.63`.
- Made `self.music.play_url` interrupt the active radio/custom URL stream before opening the new MP3 URL.
- Added `self.music.stop` as an explicit MCP stop tool for music URL playback.
- Added a RadioService stream-generation guard so stop/next/previous/new URL requests break the HTTP/MP3 decode loop promptly.
- Restored normal radio station state after stopping or changing away from a transient music URL.
- Kept the radio task on a PSRAM stack for internal-SRAM headroom, but skipped station-index NVS writes from that PSRAM task to avoid the ESP-IDF cache-disabled stack assert.
- Copied URL/station data before playback so a second song cannot invalidate the currently-open stream's references.

Verification:

- Built on Windows with `ninja -C build-qdtech-v1.7.62 -j 1 all`; CMake reported `App "xiaozhi" version: 1.7.63`.
- App binary size was `0x62c1c0`; smallest app partition is `0x700000`; free app space is `0xd3e40` (about 12%).
- App-only flashed to `COM13` at `0x100000`; esptool hash verification passed.
- Serial confirmed `self.music.play_url` and `self.music.stop` are registered.
- Serial confirmed a NetEase direct MP3 URL opened with HTTP `200` and decoded continuous `44100` Hz MP3 frames.
- Radio play/stop/next was tested after URL playback and did not reproduce the earlier `esp_task_stack_is_sane_cache_disabled()` reboot.

Known limitations:

- `self.music.play_url` still requires a direct playable HTTP/HTTPS MP3 URL from the Mac-side NetEase MCP.
- Touch I2C timeout/reset logs were observed during heavy audio/network use but recovered without reboot.

## 2026-06-29: v1.7.62 NetEase MCP URL Playback Bridge

Scope:

- Bumped firmware version to `1.7.62`.
- Added device MCP tool `self.music.play_url` with `title`, `artist`, and `url` parameters.
- Routed `self.music.play_url` into the existing QDTech radio/MP3 playback service through `RadioService::PlayUrlFromTool(...)`.
- Reused the network radio MP3 path by creating a transient music station from the supplied URL and displaying it as `title - artist`.
- Hardened local MCP tool execution under low internal SRAM by checking internal heap before thread creation, using internal task stacks for flash/NVS-safe tool calls, and reporting a recoverable error instead of aborting when memory is too low.
- Reduced repeated volume-tool NVS writes by ignoring unchanged output volume and throttling volume persistence.
- Kept the v1.7.61 7 MB QDTech OTA partition layout.

Verification:

- Built on Windows with `idf.py -B build-qdtech-v1.7.62 -D SDKCONFIG="build-qdtech-v1.7.62/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build merge-bin`.
- `build-qdtech-v1.7.62\project_description.json` reports `project_version: 1.7.62`.
- `idf.py -B build-qdtech-v1.7.62 size` passed.
- Final app binary size was `0x62b9b0`; smallest app partition is `0x700000`; free app space is `0xd4650` (about 12%).
- Full merged image size was `7518640` bytes.
- Flashed `v1.7.62` to `COM13` with bootloader, app, partition table, OTA data, and SR models; all esptool hash checks passed.
- Serial capture confirmed `App version: 1.7.62`, `Ota: Current version: 1.7.62`, `MCP: Add tool: self.music.play_url`, MQTT connected, `Application: STATE: idle`, and no panic/backtrace during boot.

Release assets:

- `releases/v1.7.62/qdtech-s3-touch-lcd-3.5-v1.7.62-app.bin`: `7B01CDCDE60BDBD9CAACC56526DA9371B8C649074E73C5F4757030A0B18A3EB3`
- `releases/v1.7.62/qdtech-s3-touch-lcd-3.5-v1.7.62-firmware.zip`: `A54638B2B7B8D1275E4B11271CF0E3E3BC94B767115BD08086F953E0694548E7`
- `releases/v1.7.62/qdtech-s3-touch-lcd-3.5-v1.7.62-full.bin`: `75EEB5E43E736D0530D2263D0EE592ACCF51C1690C121BA34BB70ADAAB48D9AC`

Known limitations:

- The Mac-side NetEase MCP must return a direct HTTP/HTTPS MP3 stream URL and the server/LLM must call `self.music.play_url` after search. The firmware now provides the device playback target, but it cannot synthesize a missing playable URL.

## 2026-06-29: v1.7.61 New-Environment WiFi Fallback

Scope:

- Bumped firmware version to `1.7.61`.
- Added a QDTech-specific 16 MB flash partition table with two 7 MB OTA app slots so the current FC emulator firmware image fits without moving NVS.
- Limited saved WiFi credentials to 5 entries across startup cleanup, normal WiFi switching, provisioning-page saves, and component-level auth saves.
- Shortened startup WiFi fallback from 60 seconds to 30 seconds so a board moved into a new environment enters provisioning sooner when none of the saved networks can connect.
- Guarded the provisioning scan event against a null scan timer in the current manual-scan SoftAP flow.
- Fixed the reboot that happened during new-environment fallback by destroying the station netif before entering provisioning and creating AP+STA station netif only when manual scan or setup-time join needs it.

Verification:

- Configured locally with ESP-IDF from `/Users/tupi/esp/esp-idf-v5.5`; CMake reported `App "xiaozhi" version: 1.7.61`.
- Build compiled the updated WiFi component, QDTech board sources, and FC emulator service and generated `build-qdtech-s3-final/xiaozhi.bin`.
- Size check passed with 7 MB OTA partitions: app size `0x62bd30`, partition `0x700000`, free `0xd42d0` (12%).
- Flashed to `/dev/cu.usbmodem212401` at 921600 baud; bootloader, partition table, OTA data, SR models, and app image hash verification passed.
- Boot log confirmed `App version: 1.7.61`, `SKU=qdtech-s3-touch-lcd-3.5`, desktop UI creation, touch max points 5, FC emulator service registration, and WiFi station scan retry logs.
- Post-fix serial verification confirmed automatic fallback after about 30 seconds: `STATE: configuring`, SoftAP `Xiaozhi-AAA9`, DHCP/web server at `192.168.4.1`, no duplicate-netif assert, and no reboot.

Release assets:

- `releases/v1.7.61/qdtech-s3-touch-lcd-3.5-v1.7.61-app.bin`: `363A0B7FF8F790345636414E5019D563AB0AE8B04CDF244E04D15DA850127591`
- `releases/v1.7.61/qdtech-s3-touch-lcd-3.5-v1.7.61-firmware.zip`: `B6B3A98894B1CEC69F9A36BCE0CC80D0D76F0E39F036D7A40996CB0A954E4716`
- `releases/v1.7.61/qdtech-s3-touch-lcd-3.5-v1.7.61-full.bin`: `809E7081CDC42684C0222BD62CFBAA7EE69BE1887B764418D553A44F2472C98C`

## 2026-06-28: v1.7.60 New-Board WiFi Provisioning Fix

Scope:

- Bumped firmware version to `1.7.60`.
- Disabled QDTech's default BSSID memory for WiFi joins so same-SSID AP/mesh networks do not pin the board to a bad BSSID.
- Patched the local `78__esp-wifi-connect` component so provisioning starts in pure SoftAP mode and the phone can reliably see `Xiaozhi-85A1`.
- Moved WiFi list scanning to manual `/scan` requests from the provisioning page.
- Fixed the manual scan handler so it no longer reads and clears the scan results after the event handler has already populated the list.
- Added serial diagnostics for manual scan and STA disconnect reasons.

Verification:

- Built on Windows with `idf.py -B build-qdtech build merge-bin`.
- App-only flashed `build-qdtech\xiaozhi.bin` to the new board on `COM16` at `0x100000`; esptool hash verification passed.
- Boot log confirmed `App version: 1.7.60`.
- Setup hotspot log confirmed `wifi:mode : softAP`, `Access Point started with SSID Xiaozhi-85A1`, and `Web server started`.
- Provisioning page scan log confirmed `manual scan done ap_num=15`.
- Serial scan results included `SSID: liutupi, RSSI: -7, Authmode: 3`.

Release assets:

- `releases/v1.7.60/qdtech-s3-touch-lcd-3.5-v1.7.60-app.bin`: `88AB96580CA77073B72E1C66C4322F6AB34AF2244701AC09C158ADAB8368FC1E`
- `releases/v1.7.60/qdtech-s3-touch-lcd-3.5-v1.7.60-firmware.zip`: `9ED8E7DDF38B14758C1B88B6D2E0BBDA1FA62DC443CC65B12F2A35B60FCB04CA`
- `releases/v1.7.60/qdtech-s3-touch-lcd-3.5-v1.7.60-full.bin`: `7772DCF18FBB745F70BEB9EA2BC5D50BFF5584AA0A355CC1B11CEC39F23B286A`

Known limitations:

- App partition remains very tight at about `0x80d0` bytes free.
- If WiFi join still fails after selecting the visible SSID, use the new `STA disconnected during setup reason=...` log to separate wrong password/auth/router rejection from hotspot/list scan issues.

## 2026-06-28: v1.7.56 FC/NES Mapper Diagnostics

Scope:

- Bumped firmware version to `1.7.56`.
- Added CRC-based mapper correction for known FC/NES problem ROMs.
- Added mapper 83, mapper 198, and mapper 224 scaffolds to nofrendo.
- Improved `轩辕剑` compatibility; tested dumps now run through mapper 224.
- Added ROM diagnostics logging path, PRG/CHR counts, header mapper, corrected mapper, CRC, and size before launching nofrendo.

Verification:

- Built on Windows with `idf.py -B build-qdtech build merge-bin`.
- App-only flashed `build-qdtech\xiaozhi.bin` to `COM13` at `0x100000`; esptool hash verification passed.
- Post-flash boot log confirmed `App version: 1.7.56`, WiFi, MQTT, `Application: STATE: idle`, and live Zhongshan weather sync.
- Serial evidence:
  - `快打旋风 [Cony Soft].nes`: `crc=fdec419f`, `header_mapper=4`, `corrected_mapper=83`.
  - `吞食天地2 [先锋卡通汉化 (laopix简体中文名字版)].nes`: `crc=3963f12a`, `header_mapper=4`, `corrected_mapper=198`.

Release assets:

- `releases/v1.7.56/qdtech-s3-touch-lcd-3.5-v1.7.56-app.bin`: `F9217510EB5701206158D5AC254D6F90D8B0EFEA6E2C0ADAE3F0756C559793A0`
- `releases/v1.7.56/qdtech-s3-touch-lcd-3.5-v1.7.56-firmware.zip`: `4BD15AAC17BCA83821125490B3C232E9B136DFE5C7D8C99E8BAA9BEEE70158DB`
- `releases/v1.7.56/qdtech-s3-touch-lcd-3.5-v1.7.56-full.bin`: `5E31B83C3F2AF1AFC4C45EC7D23CB344B44927D3E0C59C2757DE8A04D32E7FDC`

Known limitations:

- `快打旋风 [Cony Soft]` still flower-screens after mapper 83 routing.
- `吞食天地2` still renders black/solid-color frames after mapper 198 routing.
- App partition is now very tight; future FC mapper work should budget flash size carefully.

## 2026-06-27: v1.7.55 Classic XiaoZhi Robot Face Pack + Daily Avatar

Scope:

- Bumped firmware version to `1.7.55` for the Classic theme XiaoZhi visual update.
- Added Classic theme full-character robot GIF resources for standby, listening, and speaking states, based on the supplied black/gold cat-ear robot reference.
- Reused the proven Cat/Tupi Warm GIF face pipeline for Classic so the robot face is drawn as complete frames instead of separate LVGL eye, pupil, highlight, and mouth overlays.
- Speaking now swaps complete mouth frames inside the GIF, avoiding duplicate mouths or a mouth animated at the wrong position.
- Added a compact XiaoZhi bottom subtitle bar for Classic and sanitized assistant/user subtitle text with UTF-8 validation, control-character filtering, trimming, and a codepoint cap to avoid garbled text.
- Limited XiaoZhi subtitle updates to user/assistant chat content so system messages do not appear in the on-screen dialogue strip.
- Added a Classic daily-card robot avatar beside the daily quote, compressed from the user-approved reference crop and blended into the Classic dark card background.
- Kept the daily-card avatar static and RGB565 to avoid adding another GIF/cache allocation on the main page.

Verification:

- Built on Windows with `idf.py -B build-qdtech build merge-bin`.
- Build passed; CMake/app version was `1.7.55`.
- Final `xiaozhi.bin` size was `0x5f6020`, smallest app partition `0x600000`, free app space `0x9fe0` (about 1%).
- Full merged image size was `0x6f6020`.
- Flashed app-only successfully to `COM13` at 921600 baud so WiFi/NVS stayed intact.
- Short serial verification confirmed `App version: 1.7.55`, `Ota: Current version: 1.7.55`, WiFi IP `192.168.4.177`, MQTT connected, `STATE: idle`, daily card updated for `2026-06-28`, and weather displayed `27 C 中山 阴`.

Release assets:

- `releases/v1.7.55/qdtech-s3-touch-lcd-3.5-v1.7.55-app.bin`: `5174212A46460B1769475B6DD53E91EC6E4B8AC29424C15B9BC0D20B9263161A`
- `releases/v1.7.55/qdtech-s3-touch-lcd-3.5-v1.7.55-firmware.zip`: `485DE1216D45BC76800706C219426ADF31DA01FFBBEB8F3639467BCE70845E40`
- `releases/v1.7.55/qdtech-s3-touch-lcd-3.5-v1.7.55-full.bin`: `28BE202C60026D040FB9515FED01F5B028AF8E504C1D7DF75031DF1F7711CF7F`

## 2026-06-27: v1.7.54 Podcast Cover Memory Recovery + Cat Daily Card Kitten

Scope:

- Bumped firmware version to `1.7.54` for the podcast-cover recovery build.
- Root cause: after adding Cat and Tupi Warm full-character GIF faces, the hidden XiaoZhi page could still keep a themed GIF object and decoded GIF cache alive even while the user was on Media/Podcast.
- The board was already running with very low internal SRAM after WiFi/MQTT/Phone Web startup, so entering Podcast left too little margin for JPEG cover decode work.
- Changed Cat/Tupi Warm XiaoZhi GIF lifecycle to create the GIF only when the XiaoZhi page is shown, and delete it when leaving the XiaoZhi page.
- Added Podcast cover decode diagnostics so future failures log whether the cover path, JPG input size, PSRAM allocation, scaled output size, or JPEG decoder failed.
- Replaced the Cat theme daily-card geometric cat mark with a compact `64x52` RGB565 kitten image derived from the Cat standby face resource, keeping the daily card visually consistent without adding another GIF decoder/cache user.

Verification:

- Built on Windows with `idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" reconfigure build merge-bin`.
- Build passed; CMake/app version was `1.7.54`.
- Final `xiaozhi.bin` size was `0x59f910`, smallest app partition `0x600000`, free app space `0x606f0` (about 6%).
- Full merged image size was `0x69f910`.
- Flashed successfully to `COM13` at 921600 baud; flash logs verified every written region.
- Short serial verification confirmed `App version: 1.7.54`, `Ota: Current version: 1.7.54`, MQTT connected, and app reached `STATE: idle` with no panic/backtrace.
- Podcast-card tap verification is still pending; the 90 second filtered serial capture did not include a Podcast page entry or cover decode attempt.

Release assets:

- `releases/v1.7.54/qdtech-s3-touch-lcd-3.5-v1.7.54-app.bin`: `2A4645511374F36D09985C64B1E3D9C7B5D2F0A78557CE3A96BA7CDCE44008D9`
- `releases/v1.7.54/qdtech-s3-touch-lcd-3.5-v1.7.54-firmware.zip`: `D240538F67CA8B60CA3EE7DEB9126FFA9540B515535CCE2DFA6117B020A74CB0`
- `releases/v1.7.54/qdtech-s3-touch-lcd-3.5-v1.7.54-full.bin`: `F2574B34C5BE583E0D9BC1EFF9EF7009A7E64F8CD48605AD29968537741AC048`

## 2026-06-27: Tupi Warm XiaoZhi Animated Robot Face Pack

Scope:

- Bumped firmware version to `1.7.53` so the Tupi Warm XiaoZhi face update can be distinguished from the `v1.7.52` Cat release.
- Added a compact Tupi Warm robot GIF pack derived from the supplied warm robot reference: standby, listening, speaking, thinking, happy, surprised, sad, angry, and sleepy.
- Reused the closest robot states for connecting, upgrading, and fatal-error paths to keep the app partition from becoming dangerously full.
- Reused the proven full-character GIF path from the Cat theme instead of layering LVGL eyes, pupils, highlights, and mouths on top of a static face.
- Kept the robot at `300x238` so the XiaoZhi page remains visually full on the 480x320 display.
- Speaking uses complete mouth-frame swaps inside the robot face, avoiding duplicate mouths or misplaced mouth overlays.
- Listening animates with ear/signal accents; idle and emotion states keep subtle breathing/bobbing for a more alive feel.
- Tupi Warm now hides the long XiaoZhi runtime subtitle in the GIF face layout, matching the Cat fix that avoided garbled mixed text in the compact status area.

Verification:

- Generated a local contact-sheet preview and visually checked size, face placement, mouth placement, and warm-background blending.
- Built on Windows with `idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" reconfigure build merge-bin`.
- Build passed; CMake/app version was `1.7.53`.
- Final `xiaozhi.bin` size was `0x59dc00`, smallest app partition `0x600000`, free app space `0x62400` (about 6%).
- Full merged image size was `0x69dc00`.
- Flashed successfully to `COM13` at 921600 baud.
- Short serial verification confirmed:
  - `App version: 1.7.53`
  - `Ota: Current version: 1.7.53`
  - `Desktop UI created`
  - WiFi connected to `liutupi`, IP `192.168.4.177`
  - MQTT connected and app reached `STATE: idle`
  - Weather synced: `weather ok 31 C 中山 多云 15:30 H78% C66%`
  - No panic, abort, or backtrace appeared in the captured startup window
- Touch I2C still logged intermittent transaction failures and a reset after repeated failures; this matches the existing separate touch-driver/timing issue and did not stop boot, WiFi, MQTT, or weather.

Release assets:

- `releases/v1.7.53/qdtech-s3-touch-lcd-3.5-v1.7.53-app.bin`: `30931C7F906438ACED6C6DA58DB31156FF265E78556F7D0476A744709C2EA9FB`
- `releases/v1.7.53/qdtech-s3-touch-lcd-3.5-v1.7.53-firmware.zip`: `D47F596A57424F06B459FBA8E9FFB4603C45E712583195DADF2E5995C7604966`
- `releases/v1.7.53/qdtech-s3-touch-lcd-3.5-v1.7.53-full.bin`: `4DB3D8C93B6C1FA98CBB11956A2F7010B66AD0ACE9C8639E680121036A7AB4CD`

## 2026-06-27: Cat Theme XiaoZhi Animated Face Pack

Scope:

- Replaced the Cat theme XiaoZhi vector face with an animated kitten GIF pack derived from the supplied orange-and-white kitten expression grid.
- Bumped firmware version to `1.7.52` so boards on `v1.7.51` can receive the Cat XiaoZhi face update through OTA.
- Added nine state resources: standby, listening, speaking, thinking, happy, surprised, sad, angry, and sleepy.
- Sized the Cat XiaoZhi character at `300x238` so it fills the 480x320 XiaoZhi page instead of leaving the screen visually empty.
- Each GIF keeps the original reference proportions, fur color, eye style, cheeks, ears, and soft illustrated finish instead of using the simplified vector redraw.
- Added 8-frame breathing loops, listening pulse accents, and speaking animation that switches between complete original-style closed-mouth/open-mouth frames so highlights and mouths do not overlap.
- Removed extra eye highlight/pupil overlays after frame inspection showed they overlapped the reference eyes.
- Removed the Cat XiaoZhi long message subtitle so mixed Chinese/runtime text cannot render as garbled glyphs in the compact status area.

Verification:

- Built on Windows with `idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build merge-bin`.
- Build passed; `xiaozhi.bin` size was `0x579c40`, smallest app partition `0x600000`, free app space `0x863c0` (about 9%).
- Full merged image size was `0x679c40`.
- Flashed successfully to `COM13` at 921600 baud.
- Short serial verification confirmed:
  - `App version: 1.7.52`
  - `Ota: Current version: 1.7.52`
  - `Desktop UI created`
  - WiFi connected to `liutupi`, IP `192.168.4.177`
  - MQTT connected and app reached `STATE: idle`
  - Weather synced: `weather ok 30 C Zhongshan thunderstorm 14:45 H83% C100%`
  - No panic, abort, or backtrace appeared in the captured startup window

Release assets:

- `releases/v1.7.52/qdtech-s3-touch-lcd-3.5-v1.7.52-app.bin`: `1A0FBA14020E9927AC7331453E0B26D0B94CBFA9F43D17AC7DC5068E0FC2A125`
- `releases/v1.7.52/qdtech-s3-touch-lcd-3.5-v1.7.52-firmware.zip`: `90403EDFC28A4DFD7C5C046CA3C2E0B897FE193387F224102FE4599CC6397A48`
- `releases/v1.7.52/qdtech-s3-touch-lcd-3.5-v1.7.52-full.bin`: `963CEE028AE949B70D1CB255CB0D35E24D1AD8EB162BAD49ED47CCCA774A179E`

Follow-up:

- Do a physical screen taste pass on the 480x320 LCD and tune speaking frame cadence or expression crop placement from hardware feedback if needed.

## 2026-06-26: v1.7.51 Theme-specific XiaoZhi Face Animation Bounds

Scope:

- Bumped firmware version to `1.7.51` so boards on `v1.7.50` can receive the theme-animation fix through OTA.
- Added theme-specific XiaoZhi face metrics for eye, pupil, highlight, eyebrow, and mouth layout.
- Fixed the Cat theme regression where `UpdateFaceAnimation()` reapplied the default large Classic eye positions/sizes every frame, pushing Cat eyes outside the face frame.
- Kept Classic and Tupi Warm on the previous large face proportions.
- Hid pupil/highlight layers during closed-blink frames, then restored them as the eye opens.

Verification:

- Built with `idf.py -B build-qdtech build merge-bin` from `/tmp/qdtech_flash_src`; app version logged as `1.7.51`.
- Final `xiaozhi.bin` size: `0x4e0a80`.
- Full merged image size: `0x5e0a80`.
- Smallest app partition: `0x600000`; free app space `0x11f580` (about 19%).
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Flash logs confirmed ESP32-S3 with 8 MB PSRAM, successful writes for all regions, hash verification, and hard reset.

Release asset SHA256:

- `qdtech-s3-touch-lcd-3.5-v1.7.51-app.bin`: `E126BE2C68AEACF0E30D8AD72243BEC86B84A11F06FB3EA6F34F43676FEB578E`
- `qdtech-s3-touch-lcd-3.5-v1.7.51-firmware.zip`: `9A63F4AC1E342560EA8517C357F2090FE30EA380F2E87E150B8678A43DC74937`
- `qdtech-s3-touch-lcd-3.5-v1.7.51-full.bin`: `51B4157FBDF7CF7F109ED3B46A3AC93CAB4467E43F8D15BB1A2A4FC8D4C9EDBB`

## 2026-06-26: v1.7.50 Radio And Podcast Audio Quality Polish

Scope:

- Bumped firmware version to `1.7.50` so boards on `v1.7.49` can receive the audio-quality update through OTA.
- Added a lightweight automatic gain and soft-limiter stage to network radio playback.
- Removed the previous mono-path `* 2` sample boost from both Radio and Podcast playback. The old boost raised quiet mono streams but also increased clipping risk on voice/music peaks.
- Reset radio gain whenever opening a stream URL so station-to-station changes do not inherit old gain state.
- Kept the MP3 decoder and resampling path unchanged to avoid increasing memory and CPU pressure in the heavy SD/network audio paths.

Verification:

- Built with `idf.py -B build-qdtech build merge-bin`; app version logged as `1.7.50`.
- Final `xiaozhi.bin` size: `0x4ad9c0`.
- Full merged image size: `0x5ad9c0`.
- Smallest app partition: `0x600000`; free app space `0x152640` (about 22%).
- Flashed successfully to `COM13`.
- Runtime logs confirmed:
  - `App version: 1.7.50`
  - WiFi connected to `liutupi`, IP `192.168.4.177`
  - MQTT connected and app reached `STATE: idle`
  - Weather synced: `weather ok 28 C 中山 毛毛雨 17:30 H95% C100%`
  - No reboot, panic, or backtrace during the observed startup window

Release asset SHA256:

- `qdtech-s3-touch-lcd-3.5-v1.7.50-app.bin`: `7546111FD59625F9A3282C75D34AC730BFA4BD0A9289808BE7E96F9D41FB0569`
- `qdtech-s3-touch-lcd-3.5-v1.7.50-firmware.zip`: `60BD51ECDE396FA6197728AE75297641F9774D58426DCE84C0261BC51B46064F`
- `qdtech-s3-touch-lcd-3.5-v1.7.50-full.bin`: `3C218C1EC1930DF9FBE185C50B5E5358C3A7221A5E4895E93F96D47EEDBDAF35`

## 2026-06-26: v1.7.49 Photos Portrait Fit And Blurred Background

Scope:

- Bumped firmware version to `1.7.49` so boards on `v1.7.48` can receive the Photos portrait-display update through OTA.
- Changed the Photos page from one image layer per fade slot to two image layers per slot: a background image and a foreground image.
- Kept landscape photos in the previous full-screen cover mode so 480x320 / 800x480 style images still fill the screen.
- Added portrait-photo detection. Portrait photos now fit fully and remain centered instead of being enlarged and cropped.
- For portrait photos, `PhotoService` generates a 480x320 same-photo background frame in PSRAM, using cover scaling, lightweight sampled blur, and darkening.
- The generated background and the centered foreground cross-fade together, preserving the existing slideshow feel.
- This does not require extra files on the SD card. Users can keep placing JPG/JPEG files under `/sdcard/photos`; recommended portrait preparation remains `320x480`, JPG quality 80-85, preferably under 500 KB.

Verification:

- Built with `idf.py -B build-qdtech build merge-bin`; app version logged as `1.7.49`.
- Final `xiaozhi.bin` size: `0x4ad8c0`.
- Full merged image size: `0x5ad8c0`.
- Smallest app partition: `0x600000`; free app space `0x152740` (about 22%).
- Flashed successfully to `COM13`.
- Runtime logs confirmed:
  - `App version: 1.7.49`
  - WiFi connected to `liutupi`, IP `192.168.4.177`
  - MQTT connected and app reached `STATE: idle`
  - Weather synced: `weather ok 28 C 中山 雷雨 15:30 H94% C100%`
  - Deferred Phone Web services started after the memory guard check
  - No reboot, panic, or backtrace during the observed startup window

Release asset SHA256:

- `qdtech-s3-touch-lcd-3.5-v1.7.49-app.bin`: `DAD8E4B31DC4E59291A445AEA6F0B3536C3F4BDB901984C7756EF0EFB567495C`
- `qdtech-s3-touch-lcd-3.5-v1.7.49-firmware.zip`: `FDC9556E3FB60F2E3CC52B7A45F5DC4FA1C46035CB94D5E083B0C30586EFCD9D`
- `qdtech-s3-touch-lcd-3.5-v1.7.49-full.bin`: `1012D6463DB5DDF1B09526B6B46FDBE06AC255933732F61E5872CED304608049`

## 2026-06-26: v1.7.48 System Smoothness For Podcast And FC

Scope:

- Bumped firmware version to `1.7.48` so boards on `v1.7.47` can receive the system smoothness update through OTA.
- Changed `PodcastService` from boot-time startup to lazy startup when the Podcast card is opened.
- Stopped the podcast index load from reading every episode summary up front.
- Stopped podcast list Up/Down selection from decoding JPG covers; cover decoding now happens when entering playback/detail work that actually needs it.
- Moved FC/NES ROM validation and start work out of the UI callback path and into the FC background task, so tapping Play shows `Loading ROM` without blocking LVGL/touch on SD file reads.
- Added media mutual exclusion: entering Podcast stops Radio and FC; entering FC stops Radio and Podcast. This reduces SD-card, decoder, and audio-output contention when switching between heavy features.
- Kept the release scoped to runtime stability and did not change podcast SD-card format or game ROM format.

Verification:

- Built with `idf.py -B build-qdtech build merge-bin`; app version logged as `1.7.48`.
- Final `xiaozhi.bin` size: `0x4ad510`.
- Full merged image size: `0x5ad510`.
- Smallest app partition: `0x600000`; free app space `0x152af0` (about 22%).
- Flashed successfully to `COM13`.
- Runtime logs confirmed:
  - `App version: 1.7.48`
  - WiFi connected to `liutupi`, IP `192.168.4.177`
  - MQTT connected and app reached `STATE: idle`
  - Weather synced without `weather low memory`: `weather ok 27 C 中山 雷雨 14:00 H95% C100%`
  - Deferred Phone Web services started after the memory guard check
  - No reboot, panic, or backtrace during the observed startup window
- A pre-version-bump hardware interaction check opened Media -> Podcast and confirmed lazy podcast startup, SD mount, and `PodcastService: loaded podcast episodes=80`.

Release asset SHA256:

- `qdtech-s3-touch-lcd-3.5-v1.7.48-app.bin`: `C28DD3E38439FA30FEF47EC6498A69CD95DB7FF89A85B6CE88001D36E236646C`
- `qdtech-s3-touch-lcd-3.5-v1.7.48-firmware.zip`: `352154D60BE9BEA082277F35F760FCFD5C3FAB9DEF11A30DE18C75D93E9E11BB`
- `qdtech-s3-touch-lcd-3.5-v1.7.48-full.bin`: `6C0A9E670D18F0E3ED6C4A9BBF56AECCFAAE4F901F3EDBD57EDE723709520A51`

## 2026-06-26: v1.7.47 Podcast SD Player And List Font Fix

Scope:

- Bumped firmware version to `1.7.47` so boards already on `v1.7.46` can receive the podcast update through OTA.
- Added a third `Media` page with a `Nothing Impossible` podcast card.
- Added `PodcastService` for SD-card podcasts under `/sdcard/podcast`, using `index.json` plus per-episode MP3, JPG cover, and TXT summary files.
- Added a two-level podcast UI: episode list first, then detail view with cover, summary, play/stop/prev/next controls, and a seekable progress slider.
- Removed the Podcast page top status/brand bar so the content can use almost the full 480x320 screen.
- Normalized podcast audio loudness during playback with a lightweight peak/average leveler.
- Fixed playback/list switching stability by keeping list navigation lightweight while MP3 decoding is active; switching playback exits the decode loop cleanly before opening the next file.
- Expanded the podcast list from 5 visible rows to 8 visible rows.
- Fixed podcast title mojibake by replacing unsafe byte-count truncation with UTF-8-safe truncation, and by normalizing high-risk punctuation such as smart quotes and long dashes for the current embedded font.
- Reverted podcast labels from the narrow LXGW UI subset back to the broader Puhui Chinese font after hardware feedback showed the LXGW subset missed many podcast-title Chinese glyphs.

Verification:

- Built with `idf.py -B build-qdtech build merge-bin`; app version logged as `1.7.47`.
- Final `xiaozhi.bin` size: `0x4acfe0`.
- Full merged image size: `0x5acfe0`.
- Smallest app partition: `0x600000`; free app space `0x153020` (about 22%).
- Flashed successfully to `COM13`.
- Runtime logs confirmed:
  - `App version: 1.7.47`
  - `PodcastService: loaded podcast episodes=80`
  - WiFi connected to `liutupi`, IP `192.168.4.177`
  - MQTT connected and app reached `STATE: idle`
  - Weather retry succeeded: `weather ok 28 C 中山 雷雨 13:30 H90% C100%`

Release asset SHA256:

- `qdtech-s3-touch-lcd-3.5-v1.7.47-app.bin`: `481522E7171428C274409EDC8628D6C88A25F4A051302702E0FC105885ECDCF6`
- `qdtech-s3-touch-lcd-3.5-v1.7.47-firmware.zip`: `9FEEEB16EA63AAD5AB5065162053FCD5BD1F0D43808AC8362CE1EBC7822C6481`
- `qdtech-s3-touch-lcd-3.5-v1.7.47-full.bin`: `C5BE37D2832CEE18BF02BB5A7F80D650931367922C0B01122B675C4464B654A0`

## 2026-06-26: v1.7.46 Font, Phone Web, Brand Earth, And Weather Accuracy

Scope:

- Bumped firmware version to `1.7.46`.
- Routed dynamic Chinese text through broader Puhui font coverage where previous Montserrat/LXGW-subset usage could miss glyphs.
- Added the final accepted main-page brand earth GIF: `46x46`, transparent background, earth-only, no satellite, no grid lines, clean blue-white rim.
- Stabilized WiFi reconnect by preferring the strongest saved BSSID when the remembered-BSSID flag is absent.
- Delayed Phone Web startup after WiFi connect and added retry behavior when internal memory is temporarily low.
- Cached Phone Web/BLE status strings so Settings continues showing the IP/status after refresh or theme rebuild.
- Lowered weather/HTTP low-memory guards carefully enough to avoid the repeated `weather low memory` loop while preserving reboot safety.
- Fixed a theme-switch crash by clearing cached status-bar label pointers before recreating LVGL pages.
- Improved weather accuracy on the Open-Meteo fallback by requesting precipitation, rain, showers, cloud cover, humidity, and API time, then refining raw weather codes before display.

Verification:

- Built with `idf.py -B build-qdtech build`; final `xiaozhi.bin` size `0x4a55b0`, app partition free `0x15aa50` (about 23%).
- Built merged firmware with `idf.py -B build-qdtech merge-bin`; merged image size `0x5a55b0`.
- Flashed to `COM13`; boot reached WiFi, MQTT, idle state, weather, and Phone Web.
- Live weather validation corrected a misleading thunderstorm code:

```text
weather ok 28 C 中山 阴 00:30 H95% C97% raw=95 refined=3 rain=0.00 cloud=97 humidity=95 updated=00:30
```

- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.46-app.bin`: `108CFA46D5E7C2EC8E79A835872B5A9F83E79F5713B950A4B33C8FCDE3B9AFCB`
  - `qdtech-s3-touch-lcd-3.5-v1.7.46-firmware.zip`: `266BDA638EAD530B616D9EE74B60365CF0C34FE4D64BF10786648E723BE234F6`
  - `qdtech-s3-touch-lcd-3.5-v1.7.46-full.bin`: `5857F4E0447481C6523079A9F073CABE6821E61F7C4933F707643E6F409A7C7B`

## 2026-06-25: v1.7.44 Phone WiFi Profile, Weather Config, And Brand Layout Fix

Scope:

- Bumped firmware version to `1.7.44` so devices on `1.7.43` can see the new GitHub Release as an OTA update.
- Added a local WiFi phone config page/API at `http://<device-ip>/` as the verified sync path for custom logo, owner name, and weather location.
- Simplified the phone form so normal users only enter a city name; latitude/longitude are resolved automatically through Open-Meteo geocoding when omitted.
- Brand labels update in place across Main, Apps, Radio, Network, and Settings after profile saves.
- Fixed the main-page top-left `logo/name` overlap by using explicit single-line label heights and aligning `name` directly below `logo`.
- Restored theme-aware brand colors and clamped long brand text with ellipsis.
- Weather summary text now uses a Chinese-capable font so Chinese city names render correctly.
- BLE config service code remains present, but current runtime memory still skips NimBLE startup safely instead of rebooting.

Verification:

- Built the ESP32-S3 QDTech target from `/tmp/qdtech_firmware_copy` with ESP-IDF `v5.5.2`.
- Pre-release hardware flash of the brand-layout fix passed on `/dev/cu.usbmodem212401`.
- Boot monitor showed `App version: 1.7.43` before the version bump, display/UI creation, touch navigation Main -> Apps -> Main, WiFi IP `192.168.1.111`, HTTP config server startup, and no reboot/backtrace.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.44-app.bin`: `AAF698866909DB0A6EBB7B35B906D6B3B96D1FFBAACF8F05705513019B4E31B5`
  - `qdtech-s3-touch-lcd-3.5-v1.7.44-firmware.zip`: `BCF321D3F990C7C0DD45C6DF9484C3DDEB4D28CF5C2DD9B4EFBD00347C11C19C`
  - `qdtech-s3-touch-lcd-3.5-v1.7.44-full.bin`: `7109A141405D828D2889A73E811C39EFFCA6860E612B7FC750DFF3F71D52DCC0`

## 2026-06-25: Phone WiFi Profile And Weather Config Sync

Scope:

- Added a local WiFi phone config page/API at `http://<device-ip>/` as the verified sync path for custom logo, owner name, and weather location.
- Added `Phone Web` status in Settings next to the existing BLE status/profile/weather rows.
- Simplified the phone form so normal users only enter a city name; latitude/longitude are resolved automatically when omitted.
- Added Open-Meteo geocoding lookup for city-only weather configuration.
- Brand labels now update in place across Main, Apps, Radio, Network, and Settings after profile saves, avoiding full LVGL UI reconstruction.
- Restored theme-aware brand colors and clamped long brand text with ellipsis to prevent overlap.
- Weather summary text now uses a Chinese-capable font so Chinese city names render correctly.
- Added `qd_wifi_config_server.cc/h` using ESP-IDF `esp_http_server`.
- Moved the HTTP server task stack to PSRAM and cached config values in RAM before startup so HTTP handlers do not read NVS from a PSRAM stack.
- Shared JSON parsing/storage between BLE and WiFi config through `QdApplyConfigJson`.
- Cached weather location in `TimeWeatherService` before the PSRAM-stack weather task starts.
- Changed WiFi config apply to lightly refresh Settings controls instead of rebuilding the full LVGL UI after POST.

Verification:

- Built from `/tmp/qdtech_firmware_copy` with ESP-IDF `v5.5.2`.
- Build result: `xiaozhi.bin` size `0x4ccc80`, smallest app partition `0x600000`, free `0x133380` (20%).
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Runtime reached WiFi IP `192.168.1.111`, live Zhongshan weather, MQTT connected, and `Application: STATE: idle`.
- Verified `GET http://192.168.1.111/config` returned:

```json
{"logo":"nothing impossible","name":"tupi","city":"Zhongshan","latitude":"22.5176","longitude":"113.3928"}
```

- Verified `GET /` returned the mobile HTML form.
- Verified `POST /config` returned `Saving...`, then `GET /config` returned the same saved values.
- Verified city-only POST with `city=中山` resolved to `22.5231,113.3791`.
- Verified `city=dongguan` resolved to `东莞市 23.0180,113.7487`; weather refreshed as `31 C 东莞市 Storm`.
- Serial monitor confirmed `wifi config synced profile=1 weather=1`, weather refreshed, and no reboot/backtrace occurred after saving.

## 2026-06-25: Phone BLE Profile And Weather Config Sync

Scope:

- Merged the latest `origin/main` line through firmware version `1.7.43`.
- Added persistent user profile settings for the top-left text logo and owner name, defaulting to `nothing impossible` and `tupi`.
- Added a QDTech BLE configuration service named `QDTech-Config` so a phone app can read/write profile and weather settings when enough internal RAM is available.
- Added Settings-page "Phone Sync" rows showing BLE sync status, current profile logo/name, and weather city.
- Weather city sync reuses the existing Open-Meteo weather path by writing city, latitude, and longitude to the weather-location storage/service.
- Enabled trimmed NimBLE BLE peripheral defaults for the QDTech board and added a low-memory startup guard to prevent BLE controller reboot loops.
- Updated the QDTech main component dependencies required by ESP-IDF 5.5 explicit dependency checks.

BLE JSON payload:

```json
{"logo":"nothing impossible","name":"tupi","city":"Zhongshan","latitude":"22.5176","longitude":"113.3928"}
```

Verification:

- Built from a temporary no-space copy of the project because the macOS workspace path contains spaces and ESP-SR's generated `-L` paths are not fully quoted at link time.
- Build command target: ESP32-S3 QDTech board defaults with ESP-IDF `v5.5.2`.
- Result after the BLE memory guard: `xiaozhi.bin` size `0x4c8080`, smallest app partition `0x600000`, free `0x137f80` (20%).
- Flashed successfully to `/dev/cu.usbmodem212401`; esptool verified bootloader, app, partition table, OTA data, and srmodels hashes.
- Runtime monitor reached stable UI/audio/WiFi/MQTT/weather idle. Weather refreshed live for Zhongshan at `32 C` with storm code `95`.
- BLE hardware result: after WiFi startup the board had only `14743` bytes free internal RAM and `14336` bytes largest internal block, so the guard skipped NimBLE startup and surfaced `BLE low memory`.
- Follow-up: direct always-on BLE phone sync is deferred until a lower-memory strategy is chosen, such as WiFi local config, MQTT/MCP phone sync, or an explicit BLE-only config mode.

## 2026-06-24: v1.7.43 Roll Back Caiyun Weather

Scope:

- Reverted the `v1.7.42` configurable Caiyun weather source.
- Restored the stable weather path to the previous Open-Meteo HTTP implementation.
- Removed the on-device Caiyun token MCP/config workflow from the firmware surface so multi-board OTA does not depend on per-device private weather tokens.
- Bumped firmware version to `1.7.43` so boards already running `1.7.42` can OTA forward to the no-Caiyun stable line.
- The GitHub Release `v1.7.42` should be deleted because it contains the withdrawn Caiyun firmware assets.

Verification:

- Build the QDTech target. Result: `xiaozhi.bin` size `0x492650`, smallest app partition `0x600000`, free `0x16d9b0`.
- Published GitHub Release `v1.7.43` with app, full, and zip assets.
- Deleted the withdrawn GitHub Release `v1.7.42` and its tag.
- USB app-only flashed `v1.7.43` to `COM13`, preserving WiFi/NVS.
- Boot log confirmed `App version: 1.7.43`.
- Weather returned through the restored Open-Meteo path: `weather ok 34 C Zhongshan Storm 15:34 code=95 updated=15:34`.
- MQTT connected and the application reached `STATE: idle`.
- No assert, backtrace, or reboot was observed during the post-flash monitor window.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.43-app.bin`: `0AB583C0B731A373B6E66D25D066AC14C2D1B96F7D3CF625F63139D360F2EA08`
  - `qdtech-s3-touch-lcd-3.5-v1.7.43-firmware.zip`: `F74A81A7866724288E9A24A646497540F7B576FA88E9BC76BABF6BF07742D30A`
  - `qdtech-s3-touch-lcd-3.5-v1.7.43-full.bin`: `36A5D0C43D20804F03E2FEBA55806C6D45A4D7976883AFDF0489DE44B7913EAA`

## 2026-06-23: v1.7.39 Settings OTA Verified End-to-End

Scope:

- Bumped firmware version to `1.7.39`.
- This is the validation release for the `v1.7.38` OTA updater; code is unchanged except the version bump.
- Published GitHub Release `v1.7.39` with app, full, and zip assets.

Verification:

- Board on `COM13` was running `v1.7.38` after USB app-only bootstrap.
- Settings updater selected `qdtech-s3-touch-lcd-3.5-v1.7.39-app.bin`.
- Firmware download used proxy candidate first: `Firmware download attempt 1/3: proxy`, then `Firmware download proxy selected`.
- OTA read the image header: `New firmware version: 1.7.39`.
- OTA wrote the full app image to `ota_1` with progress reaching `100% (4784848/4784848)`.
- Runtime logged `Firmware upgrade successful, rebooting in 3 seconds...`.
- After reboot, firmware logged `App version: 1.7.39`, `Running partition: ota_1`, and `Marking firmware as valid`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.39-app.bin`: `BAB265FF1F70263C8CC1596878209CCB4D416172107A3B121A80137F56BF8CD1`
  - `qdtech-s3-touch-lcd-3.5-v1.7.39-firmware.zip`: `ED46D36418BFFB98B4AC844215A346C48699A18AAAF5ABE03D3801B076C8A01A`
  - `qdtech-s3-touch-lcd-3.5-v1.7.39-full.bin`: `0349A8AB8B0694FE85B118675CC7DB489DBCC299A42276BBD12BFC720DBCD01F`

Notes:

- The exact failure was not "network cannot reach GitHub" alone. Earlier logs proved GitHub release metadata could be fetched and the proxy asset stream could start.
- Final root cause was a combination of TLS/internal-RAM pressure and ESP-IDF's flash-write requirement that cache-freeze work run from an internal-RAM stack.
- Serial monitor must remain open for the whole OTA test. Reopening the USB serial port resets this board and interrupts an in-progress OTA.

## 2026-06-23: v1.7.38 Persistent Internal OTA Flash Worker

Scope:

- Bumped firmware version to `1.7.38`.
- Replaced the per-chunk OTA flash task creation with one persistent internal-RAM `ota_flash` worker.
- The worker is created before the firmware HTTP connection opens and owns the `esp_ota_*` calls.
- OTA data is downloaded on the normal updater path, then copied through a 1024-byte internal staging buffer before `esp_ota_write()`.
- This avoids both previous failure modes: post-TLS task allocation failure and `s_task_stack_is_sane_when_cache_frozen()` assertion from PSRAM task stacks.

Verification:

- Built successfully with `idf.py -B build-qdtech build`.
- `xiaozhi.bin` size: `0x4902d0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x16fd30`, about 24%.
- USB app-only flashed `v1.7.38` to the current `COM13` board at `0x100000`, preserving WiFi/NVS.
- Boot logs confirmed `App version: 1.7.38`, WiFi reconnect, MQTT idle, and Settings OTA readiness.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.38-app.bin`: `772EF16098ABB2F2927501A119A54BB005EFFBA6F779C4438FC1B6FC0DEC3A93`
  - `qdtech-s3-touch-lcd-3.5-v1.7.38-firmware.zip`: `6DF259376BEA59B64CC8C240FBD30B842A9C6042003D90BE9698B4F519A3D03E`
  - `qdtech-s3-touch-lcd-3.5-v1.7.38-full.bin`: `48DBE8496E0B2BEEEEB9F20ADA9D39F7B075DB0ADBD4A7A21E66BEF47E4A1B3C`

## 2026-06-23: v1.7.36-v1.7.37 OTA Debug Attempts

Scope:

- `v1.7.36` tried to avoid internal task allocation by writing OTA data directly with a preallocated internal staging buffer.
- `v1.7.37` was a version-only validation target for that updater.

Verification:

- `v1.7.36` could detect `v1.7.37`, select the GitHub Release app asset, and open the proxy stream.
- It then asserted at `esp_cache_freeze_caches_disable_interrupts` because the updater task stack was in PSRAM while calling `esp_ota_write()`.
- Conclusion: the write path must stay on an internal-RAM worker task, but that worker must be persistent and allocated before TLS download pressure.

## 2026-06-23: v1.7.33 OTA Flash Worker Split

Scope:

- Bumped firmware version to `1.7.33`.
- Split OTA into a PSRAM-stack HTTPS download task plus a small internal-RAM flash worker task.
- The HTTPS task can keep the larger stack and TLS/AES memory needs outside the scarce internal task stack budget.
- The flash worker is the only task that calls `esp_ota_begin()`, `esp_ota_write()`, `esp_ota_end()`, and `esp_ota_set_boot_partition()`, so flash cache-disable operations no longer run on a PSRAM stack.
- OTA header and HTTP download buffers are allocated from PSRAM; each write is copied into the flash worker's internal staging buffer before `esp_ota_write()`.

Verification:

- Built and USB-flashed the `v1.7.32` bootstrap build with the flash-worker split to `COM14`.
- Built successfully with `idf.py -B build-qdtech build`.
- `xiaozhi.bin` size: `0x48fd80`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x170280`, about 24%.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.33-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.33-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.33-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.33-app.bin`: `c5b910059161cfbeeb31fe036bf8890d90b16c656be04696406103c89c7c1bd0`
  - `qdtech-s3-touch-lcd-3.5-v1.7.33-firmware.zip`: `bb5e40f3bc0391a4c6c5323dc08bfe3c08b891d72c0acbed4dd2549f1e6b6b65`
  - `qdtech-s3-touch-lcd-3.5-v1.7.33-full.bin`: `be346268722b679b9157601667e9e2cfc3c0ba3a8c7862d0b8b1764554f22125`
- Board-initiated OTA from the bootstrap build to `v1.7.33` still did not complete. It no longer hit the old cache assertion or stack overflow, but the TLS/proxy read path failed with `esp-aes: Failed to allocate memory` and then `Failed to read HTTP data: ESP_FAIL`.
- Follow-up target: free more internal RAM before opening the firmware HTTP connection, or replace HTTPS OTA asset download with a lower-memory transport/manifest path.

## 2026-06-23: v1.7.32 OTA Low-Internal-RAM Upgrade Fit

Scope:

- Bumped firmware version to `1.7.32`.
- Follow-up fix after `v1.7.31` OTA testing: the internal upgrade task was created, but its 6144-byte internal stack left too little internal heap for the OTA image header and download buffer.
- Reduced the firmware upgrade task stack to 4096 bytes.
- Reduced the firmware HTTP download buffer from 2048 bytes to 1024 bytes so OTA can fit in the observed low-internal-RAM state.

Verification:

- Built and USB-flashed the `v1.7.31` bootstrap build with these memory reductions to `COM14`.
- Built successfully with `idf.py -B build-qdtech build`.
- `xiaozhi.bin` size: `0x48f980`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x170680`, about 24%.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.32-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.32-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.32-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.32-app.bin`: `2a5cb26b3587c79356a8ac033a7a3d07acfd136d879eb11919336d468801c248`
  - `qdtech-s3-touch-lcd-3.5-v1.7.32-firmware.zip`: `a94bace312395293d5b96c3395a9e4fa1e741145af7000e5eb4e03521902e10a`
  - `qdtech-s3-touch-lcd-3.5-v1.7.32-full.bin`: `e3f1230680314d43ff71f5e1b79a4197293cf8380e1a1964672da722d1524e73`
- Follow-up target: publish `v1.7.32` and verify board-initiated OTA from the bootstrap build to `v1.7.32`.

## 2026-06-23: v1.7.31 OTA Check/Upgrade Task Split

Scope:

- Bumped firmware version to `1.7.31`.
- Follow-up fix after `v1.7.30` testing: update-check tasks still need the larger 10KB stack but do not write flash, so they can safely use PSRAM.
- Firmware upgrade tasks still require an internal-RAM stack because they call `esp_ota_write()` while flash cache may be disabled.
- If the internal upgrade stack cannot be created, the updater now reports `No internal RAM` instead of falling back to an unsafe default stack.

Verification:

- `v1.7.30` bootstrap with this task split was USB-flashed to `COM14` after the task-create failure was captured.
- Built successfully with `idf.py -B build-qdtech build`.
- `xiaozhi.bin` size: `0x48f980`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x170680`, about 24%.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.31-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.31-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.31-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.31-app.bin`: `a49538f4186d84cbaf049d5c29c444addce142320306bd0bbb498d93a6f1d5fd`
  - `qdtech-s3-touch-lcd-3.5-v1.7.31-firmware.zip`: `d6f84cfcb9103f3df0859007c35163e8764edbd457e6dcc0c82f283163318608`
  - `qdtech-s3-touch-lcd-3.5-v1.7.31-full.bin`: `c8b7bbd5ad5e5b863da67f70f4273a452bd12b0590b3e3c725832a32d62e0f0c`
- Follow-up target: publish `v1.7.31` and verify board-initiated OTA from the bootstrap build to `v1.7.31`.

## 2026-06-23: v1.7.30 OTA Internal-RAM Write Fix

Scope:

- Bumped firmware version to `1.7.30`.
- Fixed the Settings OTA crash captured on a new board running `v1.7.28`.
- Runtime logs proved this was not a GitHub/proxy reachability problem: direct GitHub timed out, proxy download succeeded, and the crash happened after `New firmware version: 1.7.29` while OTA flash writing began.
- Root cause: the firmware upgrade task was created in PSRAM first. ESP-IDF disables cache during flash erase/write, so a PSRAM task stack is unsafe and triggers `s_task_stack_is_sane_when_cache_frozen()`.
- The firmware upgrade task now uses an internal-RAM stack first.
- OTA image header accumulation no longer uses `std::string`; the header buffer is allocated from internal RAM.
- OTA HTTP download buffer moved from task stack to internal heap, so `esp_ota_write()` always receives internal-RAM data.
- Bootstrap note: boards already running `v1.7.28` cannot fix this exact OTA crash by OTA, because the crashing updater is the old app. They need one USB flash to `v1.7.29` or newer before future OTA updates are reliable.

Verification:

- Captured failing board logs on `COM14`: current app `1.7.28`, release asset selected, direct GitHub connection timed out, proxy selected, `New firmware version: 1.7.29`, then ESP-IDF cache-freeze stack assertion and reboot.
- Built successfully with `idf.py -B build-qdtech build`.
- `xiaozhi.bin` size: `0x48f850`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x1707b0`, about 24%.
- USB-flashed the fixed `1.7.29` bootstrap build to the new board on `COM14`.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.30-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.30-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.30-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.30-app.bin`: `476ebaa1ea9afa1fad285e815a754d4815c94bd4e3fc552006fa54e9331ed620`
  - `qdtech-s3-touch-lcd-3.5-v1.7.30-firmware.zip`: `0e780c5bfdeb7fd3e490af80e82cffe7152380f8e2ae26ef4731409646cab58a`
  - `qdtech-s3-touch-lcd-3.5-v1.7.30-full.bin`: `b3a33e954bde02dec6cd96297dcbd544f692e6c8e4cdcab076effb015723a598`
- Follow-up verification target: publish `v1.7.30`, then test on-device OTA from the fixed `1.7.29` bootstrap build to `v1.7.30`.

## 2026-06-23: v1.7.29 Weather Scene GIF Pack

Scope:

- Bumped firmware version to `1.7.29`.
- Replaced the main-page weather module's low-quality LVGL primitive animation path with a GIF scene player for the QDTech board.
- Enabled `LV_USE_GIF` and `LV_GIF_CACHE_DECODE_DATA` for `BOARD_TYPE_QDTECH_S3_TOUCH_LCD_3_5`.
- Added six warm-paper weather scene assets: clear, cloudy, rain, snow, fog, and storm.
- Weather code mapping now selects a full animated scene: clear `0`, cloudy/pending `1`, rain `2`, snow `3`, fog `4`, storm `5`.
- The bottom temperature and weather-summary labels remain separate from the 142x84 animated scene area so the animation does not cover text.
- The older LVGL shape-based weather objects remain as a fallback path, but the normal QDTech build now uses the GIF scene player.

Verification:

- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech build`.
- `xiaozhi.bin` size: `0x48f920`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x1706e0`, about 24%.
- Flashed successfully to `COM13` at 921600 baud.
- Boot logs confirmed `App version: 1.7.29`, QDTech startup, `Desktop UI created`, saved WiFi reconnect, MQTT connection, and `Application: STATE: idle`.
- Weather logs confirmed default/pending `scene=1` and live Zhongshan storm weather switching to `scene=5`.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.29-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.29-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.29-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.29-app.bin`: `483dd14a036b6b66861729889a6525286be102287b47d48a555e3679d85f06e1`
  - `qdtech-s3-touch-lcd-3.5-v1.7.29-firmware.zip`: `9c7170cc4e8a73beb1a73b094a6dae216ae2558b6ed48746b1d821fe2e446557`
  - `qdtech-s3-touch-lcd-3.5-v1.7.29-full.bin`: `e5d99dab73860a5834c6c9f7d560f02a50846dc4fc0b968aba501913046063ed`
- Follow-up: visually inspect all six scenes on the physical LCD when the weather code changes or by forcing weather codes in test builds; if the asset pack grows again, watch app partition space.

## 2026-06-23: v1.7.28 GitHub OTA Proxy Fallback

Scope:

- Bumped firmware version to `1.7.28`.
- Fixed the observed Settings OTA failure where the board could detect the latest GitHub Release but timed out when opening the GitHub Release asset download URL.
- Serial logs proved the failure was `ESP_ERR_HTTP_CONNECT` before any image data was downloaded or written, so the app image and OTA partition were not the root cause.
- Added firmware-download URL candidates: direct GitHub first, then `https://ghfast.top/` plus the original GitHub URL, then `https://gh-proxy.com/` plus the original GitHub URL.
- Added attempt logging so future captures show whether the updater is using direct or proxy download.
- Added one retry to GitHub Release metadata fetch before showing Check failed.
- Bootstrap note: firmware already running `v1.7.27` or older does not have this fallback and may still require one USB flash to `v1.7.28`.

Verification:

- `curl -L -I` from the development PC confirmed `ghfast.top` and `gh-proxy.com` returned `200 OK` with the correct `Content-Length` for the `v1.7.27` app asset; `gh.llkk.cc` returned `403` and was not added.
- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech build`.
- `xiaozhi.bin` size: `0x3d7e10`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x2281f0`, about 36%.
- Flashed successfully to `COM14` at 921600 baud.
- Boot logs confirmed `App version: 1.7.28`, QDTech board startup, `Desktop UI created`, saved WiFi reconnect, `Ota: Current version: 1.7.28`, MQTT connection, weather update, and `Application: STATE: idle`.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.28-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.28-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.28-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.28-app.bin`: `8cddf5b367c5dfb108840127743ae208bdcc2f34e20b3e44e3d0a5468cd1a029`
  - `qdtech-s3-touch-lcd-3.5-v1.7.28-firmware.zip`: `777be58c569dce64035b66363da943e97109b082eafed0a2bd34f1d2906a7806`
  - `qdtech-s3-touch-lcd-3.5-v1.7.28-full.bin`: `5e6b9fb0df401a7a4ddc65f110624dbab3917467eca3a260bd86db884f75821f`
- Follow-up: verify a real on-device OTA from `v1.7.28` to a later release and confirm the proxy fallback path succeeds when direct GitHub asset download fails.

## 2026-06-23: v1.7.27 Tupi Warm Theme And Weather Layout Reuse

Scope:

- Bumped firmware version to `1.7.27`.
- Added a third persisted desktop theme, `Tupi Warm`, after the user approved the warm paper `nothing impossible / tupi` direction.
- Added the Tupi Warm palette, four-dot brand mark, warm main clock card, warm daily-card treatment, and warm Apps/FC/NES surface colors without changing the overall desktop page architecture.
- Removed the rejected compact Tupi weather-card layout. Tupi Warm now reuses the proven Classic weather animation geometry for sun, cloud, rain, snow, storm, and gold particles while retaining the warm-paper card colors.
- Kept the weather card free of the added city label because it collided with the animation on the 480x320 panel.

Verification:

- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech build`.
- `xiaozhi.bin` size: `0x3d75d0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x228a30`, about 36%.
- Flashed successfully to `COM13` at 921600 baud.
- Boot logs confirmed `App version: 1.7.27`, QDTech board startup, `Desktop UI created`, touch input creation, battery ADC readings, saved WiFi reconnect, `Ota: Current version: 1.7.27`, GitHub OTA latest check, MQTT connection, and `Application: STATE: idle`.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.27-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.27-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.27-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.27-app.bin`: `88a347cb3700185db6dcb17b6b590c28f8da0309446f7bc9dbb81c399a74a82a`
  - `qdtech-s3-touch-lcd-3.5-v1.7.27-firmware.zip`: `126b45d88d6e50c1a102c81e9db39559593d963c79e61d54fcb2d4637320961c`
  - `qdtech-s3-touch-lcd-3.5-v1.7.27-full.bin`: `56f506ef9f7dd255c6b114fcdde78c6327839b4dbff5436af1e192427f9c7b4e`
- Follow-up: after publishing `v1.7.27`, install or OTA it on the board and visually confirm the Tupi Warm weather card now matches the older accepted weather animation behavior.

## 2026-06-22: v1.7.26 GitHub OTA Redirect Buffer Fix

Scope:

- Bumped firmware version to `1.7.26`.
- Fixed the Settings GitHub OTA update failure seen on a board updating to `v1.7.25`.
- Serial capture showed the failing path: GitHub `browser_download_url` returned a `302` redirect to a very long signed `release-assets.githubusercontent.com` URL, then `HTTP_CLIENT: Out of buffer`, followed by `Ota: Failed to open firmware HTTP connection: ESP_FAIL`.
- Increased the firmware-download HTTP client RX/TX buffers from 1024 to 2048 bytes so the long GitHub release-asset redirect request can be opened.
- Changed redirect logging to print only the status and Location length, instead of logging the whole signed URL.

Verification:

- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech ... build`.
- `xiaozhi.bin` size: `0x3d6a10`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x2295f0`, about 36%.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.26-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.26-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.26-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.26-app.bin`: `1b2b33dc5cef448fbe8d2142f9f5151e9145c3a25f797c7d765aee1ae192a36e`
  - `qdtech-s3-touch-lcd-3.5-v1.7.26-firmware.zip`: `51a426a752542126916f512d5a960c9b40e66ebce12a43af58fd267e8a6698df`
  - `qdtech-s3-touch-lcd-3.5-v1.7.26-full.bin`: `276c0f80e0c30f06db08a546d9a811e539d129e8e852e23663b62d0e6df42fbf`
- Follow-up: after publishing, run a real Settings OTA update from a board on `v1.7.25` to `v1.7.26` and confirm progress passes the GitHub redirect step.

## 2026-06-21: v1.7.25 FC ROM Name Font Coverage Fix

Scope:

- Bumped firmware version to `1.7.25`.
- Restored FC/NES ROM list and selected-game detail labels to `font_puhui_16_4`.
- Kept the shared LXGW WenKai subset for fixed UI copy, but avoided using that subset for dynamic SD-card ROM names because the subset cannot cover arbitrary Chinese game titles.
- Fixed the regression where FC game names showed missing glyph boxes or garbled-looking text after the shared LXGW WenKai UI font pass.

Verification:

- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech ... build`.
- `xiaozhi.bin` size: `0x3d6a00`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x229600`, about 36%.
- Flashed successfully to `COM14` at 921600 baud.
- Esptool verified hashes for bootloader, app, partition table, OTA data, and srmodels, then hard reset the board.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.25-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.25-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.25-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.25-app.bin`: `c6aded5a555049e414718fe84705fc8f4ceb84a1ade9bbdf0e00744c4a0db6c3`
  - `qdtech-s3-touch-lcd-3.5-v1.7.25-firmware.zip`: `abd27e9bc1b1e243dc99206e0b23bdfb977843f2792c0625e34297b239b1d80f`
  - `qdtech-s3-touch-lcd-3.5-v1.7.25-full.bin`: `ea2a2a3afda4e8c286ae6d1b8a947fe5e0c7c801ac62fd09dcde871434671845`
- Follow-up: visually re-open FC/NES on the physical LCD and confirm Chinese ROM names render normally with the restored full-coverage font.

## 2026-06-21: v1.7.24 Shared LXGW WenKai UI Font Release

Scope:

- Bumped firmware version to `1.7.24`.
- Regenerated the QDTech embedded `qd_font_lxgw_16` and `qd_font_lxgw_20` LVGL font subsets from LXGW WenKai so the Cat and Classic themes share the same Chinese UI font style.
- Added the new Cat-theme and desktop Chinese glyphs to the subset, including `小苍兰`, `端午`, cat/theme labels, daily-card text, and related UI copy, avoiding missing-glyph boxes without linking the much larger full Puhui 20/30 px fonts.
- Switched desktop Chinese labels that previously used `font_puhui_16_4` to the shared LXGW WenKai subset where appropriate, including the daily card, Calendar today/weekday labels, and FC/NES ROM list/detail labels.
- Increased the Classic theme daily-card title and body text to the 20 px LXGW WenKai font so the quote area reads less thin while keeping the existing layout.

Verification:

- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech ... build`.
- `xiaozhi.bin` size: `0x3d6a00`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x229600`, about 36%.
- Flashed successfully to `COM14` at 921600 baud.
- Esptool verified hashes for bootloader, app, partition table, OTA data, and srmodels, then hard reset the board.
- `idf.py monitor` could not be captured from this non-interactive shell because it requires stdin attached to a TTY; the build output confirmed `App "xiaozhi" version: 1.7.24`.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.24-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.24-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.24-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.24-app.bin`: `b3d4f95b449db34cb28bb42c70169bcaa960bc4547efb446dc579b839fa17ae3`
  - `qdtech-s3-touch-lcd-3.5-v1.7.24-firmware.zip`: `13c2d4042ed9d96bb0c5bbbf5d4330884abfa4257f6d70c5b79e6a2653035158`
  - `qdtech-s3-touch-lcd-3.5-v1.7.24-full.bin`: `91958bcf02c64000e81b86fe907f30695e45b2f5778709a36efbdbbaaa58e597`
- Follow-up: visually inspect both Classic and Cat themes on the physical LCD after release, especially the 20 px daily-card line wrapping.

## 2026-06-21: v1.7.23 Cat Theme Switcher And Layout Polish

Scope:

- Bumped firmware version to `1.7.23`.
- Added a persisted `Cat` UI theme alongside the existing `Classic` theme.
- Added `Appearance / Theme` to Settings so the user can switch themes with one button. The setting is stored in NVS and applied after restart.
- Kept the existing desktop/page architecture intact; the Cat theme changes colors, cards, brand styling, page decorations, and selected Cat-only layout offsets.
- Updated Cat theme palette toward a stronger pink background and pink/white card system.
- Updated the main clock card for Cat theme with better contrast: pink hour digits, orange minute digits, and a dedicated card positioned below the top-left brand mark.
- Added Cat-theme Chinese brand mark `小苍兰 / 端午` using the existing Puhui Chinese font path.
- Added a small Cat-theme cat icon to the main daily-card panel while preserving the existing festival/history/daily-quote content priority.
- Added Cat-style XiaoZhi face decoration using LVGL primitives.

Verification:

- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech ... build`.
- `xiaozhi.bin` size: `0x3d4f00`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x22b100`, about 36%.
- Flashed successfully to `COM13` at 921600 baud.
- Esptool verified hashes for bootloader, app, partition table, OTA data, and srmodels, then hard reset the board.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.23-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.23-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.23-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.23-app.bin`: `51f84d5d354c5959a81b93a2766ab7e00551f3e579d3999c5d54c0f94bfc0634`
  - `qdtech-s3-touch-lcd-3.5-v1.7.23-firmware.zip`: `360cd8bff4ad320f633ea15633a5b22159483da08437a922382f9c37178eb494`
  - `qdtech-s3-touch-lcd-3.5-v1.7.23-full.bin`: `aff237067c6e55e9b6ff94fa6a82ddea6d9dca70d696e13f23463d6dbe6d1df8`
- Follow-up: perform one more visual pass on the physical display after the release if the Cat theme still needs tighter spacing or stronger contrast.

## 2026-06-21: v1.7.22 More Robust GitHub OTA Download

Scope:

- Bumped firmware version to `1.7.22`.
- Hardened the shared OTA download/write path used by the QDTech Settings firmware updater.
- GitHub firmware downloads can now continue when the final asset response has no usable `Content-Length`; progress falls back to byte-count logging instead of failing immediately.
- The OTA writer now waits until enough image header bytes have been received before calling `esp_ota_begin()` and writing data, avoiding failures when the first TLS/HTTP read returns a short chunk.
- Added explicit partition-size checking and clearer OTA begin/header-write error logs.

Verification:

- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech ... build`.
- `xiaozhi.bin` size: `0x3d2f40`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x22d0c0`, about 36%.
- Flashed successfully to the newly connected board on `COM14` at 921600 baud.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.22-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.22-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.22-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.22-app.bin`: `6a700d79478094792ce175542a02a9e1520e69cf161db656dfb3980e6e12e4a6`
  - `qdtech-s3-touch-lcd-3.5-v1.7.22-firmware.zip`: `413c7a89172c634869d03fa61f606def0af32201ed166b1de0a09533cbbfc635`
  - `qdtech-s3-touch-lcd-3.5-v1.7.22-full.bin`: `857ad9ab97c059d7afdbfee3a6d0785826be876ce685b6c390598d5cf400b859`
- Follow-up: verify a real older-board Settings -> Firmware -> Update path. If a board running the older updater still cannot cross-upgrade from GitHub, flash `v1.7.22-full.bin` once over USB and use `v1.7.22+` as the stable OTA baseline.

## 2026-06-20: v1.7.21 Preserve WiFi After BOOT Wake

Scope:

- Bumped firmware version to `1.7.21`.
- Fixed a BOOT-power regression from the new soft-power flow: the existing QDTech BOOT single-click handler could call `ResetWifiConfiguration()` while the app was still starting and WiFi had not reconnected yet.
- Changed QDTech BOOT single-click handling so clicks during `starting` or `wifi configuring` are ignored and never clear saved WiFi credentials. Once the device is idle, BOOT can still toggle chat state.

Verification:

- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech ... build`.
- `xiaozhi.bin` size: `0x3d2bd0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x22d430`, about 36%.
- Flashed successfully to `COM13` at 921600 baud.
- Boot logs confirmed `App version: 1.7.21`, direct reconnection to saved WiFi `liutupi`, IP `192.168.4.92`, `Ota: Current version: 1.7.21`, and `Application: STATE: idle`.
- No `ResetWifiConfiguration()`/配网 reset path appeared in the verified boot log.
- BOOT deep-sleep cycle was not recaptured during this monitor window; the board stayed awake and continued battery logs. The key regression fix is the removal of the startup-click WiFi reset path.
- Hardware follow-up confirmed from the local QDTech schematic: USB-C `VBUS/+5V` feeds a `TP4054` battery charge circuit for `JP1 BAT+`, so a connected single-cell 3.7V/4.2V Li-ion/LiPo pack should charge automatically from USB-C.
- A larger 2800mAh pack is acceptable if it remains a protected single-cell 3.7V/4.2V Li-ion/LiPo battery and connector polarity matches the board; charge time will be longer.
- The firmware reads real battery voltage through `IO8 / ADC1_CH7`, but no ESP32 GPIO mapping for the charger `CHRG` signal was found, so the UI does not yet show a hardware charger-state indicator.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.21-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.21-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.21-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.21-app.bin`: `91d227d0a8b3d1031d8362317e9237263d0edd971c8c3630589bddd6325c63d2`
  - `qdtech-s3-touch-lcd-3.5-v1.7.21-firmware.zip`: `63f1ef9152ed390a5ef0a20a24a24c41da23cb53f611237282b3a8e3d71bff0a`
  - `qdtech-s3-touch-lcd-3.5-v1.7.21-full.bin`: `317a280aa1bc8916dc031203051dacc9f7c50461fd24042b5ceb7cf105a2f730`

## 2026-06-20: v1.7.20 BOOT Soft Power Release-Gated Sleep

Scope:

- Bumped firmware version to `1.7.20`.
- Fixed the BOOT soft-power timing from `v1.7.19`: entering deep sleep while GPIO0 is still held low can immediately satisfy the wake condition, making the board look like it never powered off.
- Changed `EnterDeepSleep()` to turn the backlight off, stop the BOOT polling timer, wait for the BOOT key to be released, debounce for 250 ms, and only then enable GPIO0 low-level wake and enter deep sleep.
- Added an `entering_deep_sleep_` guard so the button callback and GPIO polling path cannot re-enter the sleep sequence.

Verification:

- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech ... build`.
- `xiaozhi.bin` size: `0x3d2b70`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x22d490`, about 36%.
- Flashed successfully to `COM13` at 921600 baud.
- Boot logs confirmed `App version: 1.7.20`, `Ota: Current version: 1.7.20`, WiFi, idle state, and live battery readings around `4166-4174mV / 97%`.
- During BOOT long-press testing, the serial monitor aborted with the COM port closing after the board had been running normally, which matches the expected USB serial drop when the board enters deep sleep. Reopening COM13 after pressing BOOT again produced a fresh `App version: 1.7.20` boot log and normal battery readings.
- This remains firmware soft power: it enters ESP32-S3 deep sleep rather than physically cutting the battery rail. True zero-power off still requires a power latch or PMIC.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.20-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.20-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.20-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.20-app.bin`: `fb80a2806dfaa11d9603e754dabecd0e7bd40c267237c6b9c25e77c61c44e4ec`
  - `qdtech-s3-touch-lcd-3.5-v1.7.20-firmware.zip`: `62de97c20e442ac5b7646a544a5b42571bdb23904cbc1106d8b1abb4d8247707`
  - `qdtech-s3-touch-lcd-3.5-v1.7.20-full.bin`: `1eb83bb34fa72f7113fb4078865396dc0ee112e07c1a83905e9aaf9cbab447d6`

## 2026-06-20: v1.7.19 Battery Monitor And BOOT Soft Power

Scope:

- Bumped firmware version to `1.7.19`.
- Replaced the QDTech desktop status-bar fake `80%` battery text with live battery percentage updates.
- Added QDTech battery ADC support using the vendor battery-voltage path: ADC1 channel 7 / GPIO8, 12 dB attenuation, 12-bit width, calibrated ADC millivolts multiplied by the board's x2 divider.
- Added a `Board::GetBatteryLevel()` implementation backed by the QDTech battery monitor.
- Added BOOT/GPIO0 long-press soft power behavior: long press enters ESP32-S3 deep sleep, configures GPIO0 as the wake source, and turns the backlight off first.
- Added direct GPIO0 polling in addition to the existing button component callback so the soft-power path does not depend only on the button helper event layer.

Verification:

- Checked live GitHub Releases before this work: latest published release was still `v1.7.17`; local `main` already had the pushed `v1.7.18` FC-audio commit but no GitHub Release.
- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech ... build`.
- `xiaozhi.bin` size: `0x3d29b0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x22d650`, about 36%.
- Flashed successfully to `COM13` at 921600 baud.
- Boot logs confirmed `App version: 1.7.19`, `Ota: Current version: 1.7.19`, WiFi connection, and `Application: STATE: idle`.
- Battery logs confirmed real ADC sampling on hardware: examples included `battery adc raw=2460 adc_mv=2041 battery_mv=4082 level=90% gpio=8 cal=1` and later stable `4076-4082mV / 90%` readings.
- BOOT soft-power code is compiled and flashed, but two 90-second serial-monitor windows did not capture `BOOT press down` or `BOOT gpio down`; either the physical BOOT key was not pressed during the window, the pressed key is not wired to GPIO0, or this board exposes BOOT differently from the expected GPIO0 path. Treat deep-sleep entry/wake as implemented but not hardware-confirmed until a GPIO0 edge is observed.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.19-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.19-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.19-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.19-app.bin`: `5a65a3ecf01e77d8c10b274d78d4e02fb81afc093a5d52245b9dc4c94182f434`
  - `qdtech-s3-touch-lcd-3.5-v1.7.19-firmware.zip`: `5a8c3abfcea5e88c90afcf42d5f3af1648b3023778a8bf12e64f68af45689473`
  - `qdtech-s3-touch-lcd-3.5-v1.7.19-full.bin`: `5406b5a0fe3f9b58966d82c1a3e38d02293d5cc4863bd39ab098eca1cd2259c9`

## 2026-06-19: v1.7.18 FC Emulator Audio Output

Scope:

- Bumped firmware version to `1.7.18`.
- Fixed FC/NES in-game silence in the Nofrendo path. Root cause: `nes.c` registered the APU `process` function through `osd_setsound()`, but the QDTech OSD adapter discarded that callback, so no PCM ever reached the ES8311 codec.
- Added a QDTech Nofrendo audio callback, stores the APU play function, generates 22050 Hz mono PCM once per 60 Hz emulator tick, and forwards it to `FcEmulatorService`.
- Added FC-side PCM output through the existing board `AudioCodec` path, including linear resampling to the codec output rate and `Application::SetExternalAudioActive()` while Nofrendo is running.

Verification:

- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech ... build`.
- `xiaozhi.bin` size: `0x3cdf20`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x2320e0`, about 37%.
- Flashed successfully to `COM13` at 921600 baud.
- Boot/FC logs confirmed `App version: 1.7.18`, `Ota: Current version: 1.7.18`, QDTech board startup, WiFi/MQTT, SNTP/weather, FC service registration, FC page activation, SDMMC mount, `/sdcard/FC` scan, and `fc scan found 192 nes files`.
- Runtime log capture did not enter a ROM during the monitor window, so the final `NofrendoQD: audio frames=...` line still needs a quick physical ROM-start confirmation on the panel.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.18-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.18-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.18-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.18-app.bin`: `f63c8383a2bbccaa0fbb852b1723329d7d390cee361ca3baa71e4515d021955e`
  - `qdtech-s3-touch-lcd-3.5-v1.7.18-firmware.zip`: `aad007cfb27cafac979e542e5a98a60bd46a4ef7d7aff160e6c73d7f260fe404`
  - `qdtech-s3-touch-lcd-3.5-v1.7.18-full.bin`: `2f447b012b14390adc85b6b9e75611eff55553417720646ab3c1e956cb0f1b75`

## 2026-06-19: v1.7.17 Daily Card Lunar Festival Support

Scope:

- Bumped firmware version to `1.7.17`.
- Added a compact built-in lunar festival date table for 2024-2030, covering Spring Festival, Lantern Festival, Dragon Boat Festival, Qixi, Mid-Autumn Festival, Double Ninth Festival, Laba Festival, and Chinese New Year's Eve.
- Fixed the 2026-06-19 daily card so it resolves to `端午节` instead of falling through to the daily quote.
- Kept the daily-card priority as festival first, then history-on-this-day, then daily quote fallback; festival now includes lunar and fixed Gregorian festivals.
- Changed the main-page daily-card Chinese title/body labels to the already-linked `font_puhui_16_4` font so new lunar festival text such as `端午节` and `粽叶飘香` can render without regenerating the LXGW subset or pulling in the much larger 20 px Puhui font.
- Replaced the update guard from day-of-year to full `YYYYMMDD`, avoiding stale daily-card content across time sync or year changes.

Verification:

- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech ... build`.
- `xiaozhi.bin` size: `0x3cdac0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x232540`, about 37%.
- Flashed successfully to `COM13` at 921600 baud.
- Boot logs confirmed `App version: 1.7.17`, `Ota: Current version: 1.7.17`, QDTech board startup, WiFi connection, MQTT connection, SNTP time sync, weather update, and `Application: STATE: idle`.
- Runtime logs confirmed `Daily card updated for 2026-06-19 kind=festival`, proving the Dragon Boat Festival path now wins over the quote fallback.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.17-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.17-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.17-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.17-app.bin`: `6c7a63de0863de6de4cfa5cbed2aef369ab3afe55a049026cdc2c7adfa6842ad`
  - `qdtech-s3-touch-lcd-3.5-v1.7.17-firmware.zip`: `3271900dd7adbd3a7a7df6950332ddb3051db7914298f64db940c3f631fb68d1`
  - `qdtech-s3-touch-lcd-3.5-v1.7.17-full.bin`: `10f04be0887a09b377ca61b007f97d590d7cb584d7b7422da88340dcbf5a6af0`
- Follow-up: visually confirm the daily card on the physical panel reads `端午节` and does not show missing glyph boxes.

## 2026-06-19: v1.7.16 Apps Page Visual Polish And WiFi Icon Fix

Scope:

- Bumped firmware version to `1.7.16`.
- Reworked the secondary Apps page toward the supplied dark brown reference style:
  - dark warm background
  - main-screen `Nothing impossible` brand mark in the upper-left
  - warm brown pill-style `Back` button
  - two-column horizontal app cards with small outlined app-code badges, status dots, and right-side decorative glyphs
- Fixed the Network card WiFi glyph after hardware feedback that the dot was visually off-center. The icon now uses one fixed 54x36 transparent container, with all three arcs and the dot positioned on the same center axis.

Verification:

- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech ... build`.
- `xiaozhi.bin` size: `0x3cd5e0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x232a20`, about 37%.
- Flashed successfully to `COM13` at 921600 baud.
- Boot logs confirmed `App version: 1.7.16`, `Ota: Current version: 1.7.16`, QDTech board startup, touch max points `5`, WiFi connection, MQTT connection, UI clock update, weather update, and `Application: STATE: idle`.
- Runtime logs did not include a final post-flash touch gesture capture; the Apps page change should still be judged visually on the LCD.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.16-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.16-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.16-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.16-app.bin`: `ebd91e9d87b0dbf6825a0f82b743243cc16e650da3a2983f0aa75d41da702605`
  - `qdtech-s3-touch-lcd-3.5-v1.7.16-firmware.zip`: `cf0197d470f996a690a2e3f264181a9e8a283e2e209223ca40f6f8a073887565`
  - `qdtech-s3-touch-lcd-3.5-v1.7.16-full.bin`: `4fc5797309c24f868675002c632e33118f50966a812af8fbb6a4fd904207e1a1`
- Follow-up: visually inspect the Apps page Network card on the physical panel; the code now centers the glyph mathematically, but the final look should still be judged from the LCD.

## 2026-06-19: v1.7.15 Calendar UI Redesign And Brand Polish

Scope:

- Bumped firmware version to `1.7.15`.
- Replaced the old compact Calendar page with a reference-inspired two-column layout:
  - left light "today" card with Chinese today label, large day number, weekday, and year/month
  - right warm dark monthly grid with `Month YYYY`, weekday headers, rounded date pills, weekend orange highlighting, and today gold highlighting
  - bottom `Prev` / `Today` / `Next` controls matching the warm brown/gold style
- Removed the extra "温暖的一天" label from the left Calendar card after hardware feedback.
- Enlarged the Calendar bottom controls from 82x30 to 96x34 and added 12 px LVGL extended click padding so `Prev` is easier to press near the bottom-left edge.
- Changed the left-top secondary-page brand text from `XiaoZhi AI` to the main-screen style `Nothing impossible` on Apps, Radio, Network, and Settings. Calendar now uses the reference-style page instead of a top-left brand mark.

Verification:

- Build completed successfully from the Windows checkout with `idf.py -B build-qdtech ... build`.
- `xiaozhi.bin` size: `0x3ccfc0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x233040`, about 37%.
- Flashed successfully to `COM13` at 921600 baud.
- Boot logs confirmed `App version: 1.7.15`, `Ota: Current version: 1.7.15`, QDTech board startup, touch max points `5`, WiFi connection, MQTT connection, SNTP time sync, weather update, and `Application: STATE: idle`.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.15-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.15-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.15-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.15-app.bin`: `f038e0950131f347f155a2b763208a8959b1801464e75c15784c16d2888b7f82`
  - `qdtech-s3-touch-lcd-3.5-v1.7.15-firmware.zip`: `3cd1ac5602678c77d5774c090ce06b761bbd304c2f2bb8180f3b125fcfd06935`
  - `qdtech-s3-touch-lcd-3.5-v1.7.15-full.bin`: `4cf7c0b7ac20966d9b16adce17497da9cb5d5f5a22847f98c4ce2add71e80cab`
- Follow-up: physically press Calendar `Prev` after the release build because the code-side fix increases the touch target, but the final button feel should still be judged on the panel.

## 2026-06-19: v1.7.14 FC ROM Scan Cap And Load Diagnostics

Scope:

- Bumped firmware version to `1.7.14`.
- Raised the FC ROM list cap from 64 to 192 entries. The previous `fc scan found 64 nes files` log was caused by the firmware's protective scan cap, not by a missing SD card or missing files.
- Kept the scan conservative: FC still uses the first populated ROM directory in priority order (`/sdcard/FC`, `/sdcard/nes`, `/sdcard/roms`, `/sdcard`) instead of recursively walking the whole card.
- Added pre-start ROM validation so unsupported files fail with a clearer status before entering Nofrendo:
  - invalid or missing iNES header
  - NES 2.0 header
  - ROM file larger than the current 2 MB Nofrendo PSRAM load guard
  - mapper not present in the tracked Nofrendo mapper list
- Updated handoff/status docs so they no longer describe the old minimal Mapper 0/2 core as the active FC path.

Verification:

- Build completed successfully from the Windows checkout with `cmake --build build-qdtech`.
- `xiaozhi.bin` size: `0x3cc8e0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x233720`, about 37%.
- Flashed successfully to `COM13` at 921600 baud.
- Boot logs confirmed `App version: 1.7.14`, `Ota: Current version: 1.7.14`, QDTech board startup, touch max points `5`, WiFi connection, MQTT connection, `Application: STATE: idle`, SNTP time sync, and weather update.
- FC ROM-list verification still needs an on-device tap into the FC page to trigger scanning.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.14-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.14-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.14-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.14-app.bin`: `d08ce99d118d456439f43bad6acec751d36a6ae2b7967c38d040e289edea4b17`
  - `qdtech-s3-touch-lcd-3.5-v1.7.14-firmware.zip`: `b99afb216cc6e1dcf8ef294a39b80a667b367b339989f9b793a2a2448ce02a8e`
  - `qdtech-s3-touch-lcd-3.5-v1.7.14-full.bin`: `951f7cec9904d2b25781ef57774c28c4867d0c9b94fb0cc91c0bf6d03ea8b96b`

## 2026-06-18: v1.7.13 On-Device Firmware Update Bootstrap

Scope:

- Bumped firmware version to `1.7.13`.
- Added a QDTech-only `FirmwareUpdateService` behind `SET / Settings / Firmware`.
- Changed the Firmware row from passive text to a real `Check` / `Update` touch action:
  - `Check` fetches the latest GitHub Release.
  - A newer release with an OTA app asset changes the button to `Update`.
  - Busy, no-WiFi, no-asset, latest, and progress states are shown in the row.
- Used GitHub Releases as the first on-device update source:
  - API: `https://api.github.com/repos/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware/releases/latest`.
  - Required OTA asset name: `qdtech-s3-touch-lcd-3.5-vX.Y.Z-app.bin`.
- Extended the existing `Ota` class with `StartUpgradeFromUrl()` so the board can upgrade from a direct app image URL.
- Reworked the OTA firmware download path to use ESP-IDF HTTP directly for app-image downloads, including GitHub redirect handling.
- Updated release packaging so `xiaozhi.bin` is included in the firmware zip and copied as a standalone app OTA binary.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x3cd140`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x232ec0`, about 37%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Boot logs confirmed `App version: 1.7.13`, `ESP-IDF: v5.5.2`, and `Ota: Current version: 1.7.13`.
- WiFi connected and obtained IP `192.168.1.104`.
- Time synchronized during boot: `2026-06-18 17:49`.
- Weather completed during boot: `weather ok 27 C Zhongshan Storm 17:49 code=96 updated=17:49`.
- MQTT connected and the application reached `STATE: idle`.
- Observed idle SRAM after boot stayed around `13KB` free with minimum around `8KB`.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.13-full.bin`, `qdtech-s3-touch-lcd-3.5-v1.7.13-firmware.zip`, and `qdtech-s3-touch-lcd-3.5-v1.7.13-app.bin`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.13-app.bin`: `f0ad472babf25b0c5fcde313bc53c302396f22a13b2761c0d472c3d77819593d`
  - `qdtech-s3-touch-lcd-3.5-v1.7.13-firmware.zip`: `a49a4290c8718a66fd6f270f701fdf7a53ed053db2cc0ffa074ead5d8785a94a`
  - `qdtech-s3-touch-lcd-3.5-v1.7.13-full.bin`: `64d914e33bbd3d41c123b9726e0129dfa68f8bbf099ed395b0f19270b411be08`

Known follow-up:

- `v1.7.13` is the bootstrap firmware that adds the on-device update UI. A full on-device write/reboot upgrade should be verified with the next higher GitHub Release, because a device already running `1.7.13` should correctly report `Latest` for `v1.7.13`.
- Keep publishing the standalone `*-app.bin` asset for every future release; `full.bin` is for USB/full-flash use and must not be written to an OTA app partition.

## 2026-06-18: v1.7.12 Network/System Settings Split Release

Scope:

- Bumped firmware version to `1.7.12`.
- Split the duplicated `NET` and `SET` app entries into two independent pages.
- Added a dedicated `Network / WiFi Hub` page with connection status, saved WiFi count, a scrollable saved-network list, and tap-to-set-default behavior.
- Kept destructive WiFi actions off the touch UI for now; WiFi remove/switch MCP tools remain available, and the on-device page only changes the default network.
- Reworked `Settings / System` so it now focuses on brightness, volume, weather location, firmware version, and OTA status.
- Added a firmware section that reads the running app version from `esp_app_get_description()` and reserves a stable OTA status area for the next on-device firmware-update pass.
- Polished the Radio page spectrum bars from single-color yellow to a soft rainbow palette with dim idle state and full-opacity playback state.
- Shortened small labels and added ellipsis handling for long SSIDs and firmware status text to protect the 480x320 layout.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x3ca6a0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x235960`, about 37%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Boot logs confirmed `App version: 1.7.12` and `Ota: Current version: 1.7.12`.
- WiFi connected and obtained IP `192.168.1.104`.
- Time synchronized during boot: `2026-06-18 17:16`.
- Weather completed during boot: `weather ok 28 C Zhongshan Storm 17:16 code=96 updated=17:16`.
- MQTT connected and the application reached `STATE: idle`.
- Internal SRAM stayed stable in the observed idle window, with `free sram` around `10-21KB` and observed minimum around `9KB`.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.12-full.bin` and `qdtech-s3-touch-lcd-3.5-v1.7.12-firmware.zip`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.12-full.bin`: `6ce97bb6b910420e0fd52c5b6185c8c5512e8c52f91cf7050dfbecb9276dbb65`
  - `qdtech-s3-touch-lcd-3.5-v1.7.12-firmware.zip`: `b50b5510007093a9c90ba0c2accf4abf3580ed92d35d43d323e08f967102adcb`

Next OTA work:

- Keep the current firmware row as the UI anchor.
- Add a real on-device "check update" action only after choosing the update source and confirmation flow.
- Reuse the existing `Ota` implementation where possible instead of adding a parallel updater.
- Avoid running OTA downloads concurrently with FC gameplay, radio streaming, or active XiaoZhi audio.

## 2026-06-18: v1.7.11 Clock Visual Polish Release

Scope:

- Bumped firmware version to `1.7.11`.
- Kept the main page layout unchanged and focused only on the large time display.
- Added `qd_font_clock_72`, a QDTech-only Montserrat Bold numeric font containing only `0123456789:` to make the time digits fuller without adding a broad font payload.
- Replaced the old four small flip-card digit labels with two complete clock labels: cream hours and gold minutes.
- Replaced the text colon with two soft gray round dots.
- Lowered the hour/minute labels and shortened their label boxes so the digits align visually with the colon dots and no longer appear too high.
- Rejected the intermediate segmented/block clock approach after hardware review because it looked blurry, broken, and visually colder than the existing warm UI.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x3c9d80`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x236280`, about 37%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Boot logs confirmed `App version: 1.7.11` and `Ota: Current version: 1.7.11`.
- Time synchronized successfully during boot: `2026-06-18 16:43`.
- Weather completed successfully during boot: `weather ok 28 C Zhongshan Storm 16:43 code=95 updated=16:43`.
- Internal SRAM stayed stable in the observed idle window, around `25KB` free with minimum around `20KB`.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.11-full.bin` and `qdtech-s3-touch-lcd-3.5-v1.7.11-firmware.zip`.
- Release asset SHA256:
  - `qdtech-s3-touch-lcd-3.5-v1.7.11-full.bin`: `971fb48e6516bb5263db061e67e39512f245e853de22d72f049182bc5d10b710`
  - `qdtech-s3-touch-lcd-3.5-v1.7.11-firmware.zip`: `49bd8e1b2c62cdb5daa8da7f8b85940b2d1d12747333514c4aca72488ab26622`

## 2026-06-18: v1.7.10 Time/Weather Recovery Release

Scope:

- Bumped firmware version to `1.7.10` for the time/weather recovery release.
- Added an FC-exit callback from the desktop UI to the time/weather service. Leaving FC now immediately requests a time refresh and a weather retry if the last weather fetch failed.
- Added a general main-page callback. Returning to the main page from any secondary page now requests an immediate clock refresh, not only the FC path.
- Increased background clock refresh cadence to 5 seconds while avoiding unnecessary redraws when the displayed minute has not changed.
- Time is now treated as trustworthy only after SNTP reports a real sync at least once, avoiding stale but valid-looking local time being displayed as corrected time.
- While the main page is visible, the big clock digits are rewritten every 5 seconds even if the minute is unchanged, so stale LVGL label state can self-repair quickly.
- Fixed OTA server-time handling so `timezone_offset` is not added before `settimeofday()`. System time remains UTC and the display layer applies `TZ=CST-8`, avoiding the later +8 hour jump after OTA/MQTT startup.
- Switched Open-Meteo weather fetch from HTTPS to HTTP with a shorter timeout and a quick second attempt. Weather data is public, and avoiding TLS reduces startup contention with OTA/MQTT TLS handshakes.
- Forced a full active-screen invalidation when switching pages, plus targeted main-page invalidation after time or weather labels change. This is intended to clear stale direct-drawn FC pixels and make the main page repaint immediately after exiting the emulator.
- Fixed weather retry cadence. The service previously checked the 10-second retry counter only after a 60-second wait loop; failed weather fetches now break out and retry on the intended 10-second cadence.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x3c8f30`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x2370d0`, about 37%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Boot logs confirmed WiFi initialized normally with static RX buffers `6`, dynamic RX buffers `8`, and RX BA window `6`.
- Time/Weather task still started with PSRAM stack allocation.
- Time display stayed on the correct local time after OTA/MQTT startup. The earlier +8 hour jump did not recur after removing `timezone_offset` from OTA `settimeofday()`.
- Weather completed quickly over HTTP during the boot window: `weather ok 28 C Zhongshan Storm 15:51 code=95 updated=15:51`, roughly 15 seconds after boot and before MQTT fully settled.
- Clock logs advanced normally from `15:51` to `15:52`, confirming the main-page clock continued refreshing after the initial sync.
- Internal SRAM stayed stable during the observed idle window after weather success, around `13-14KB` free with minimum around `10051` bytes.
- Release assets prepared as `qdtech-s3-touch-lcd-3.5-v1.7.10-full.bin` and `qdtech-s3-touch-lcd-3.5-v1.7.10-firmware.zip`.

## 2026-06-18: System Stability Pass After FC Release

Scope:

- Removed the QDTech boot-time FC SD-card prepare call. SD is now mounted lazily when Photos or FC actually needs it, avoiding boot-time internal SRAM pressure.
- Added FC SD mount reuse: if Photos or another service already mounted `/sdcard`, FC now reuses that mount instead of trying a second mount.
- Moved the Time/Weather task stack to PSRAM first, with an internal-memory fallback and diagnostic logging.
- Started SNTP immediately after WiFi connects instead of waiting for the full XiaoZhi application/MQTT readiness path.
- Shortened the first weather HTTP attempt from two 10-second attempts to one 5-second attempt; failed weather fetches keep cached UI state and retry later instead of tying up the task.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x3c8a70`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x237590`, about 37%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Boot logs confirmed no early FC SD mount during board construction; FC only mounted SD after entering the FC page.
- WiFi initialized normally with the original low-memory WiFi settings: static RX buffers `6`, dynamic RX buffers `8`, RX BA window `6`.
- Time/Weather logs confirmed PSRAM task stack allocation: `time weather task started stack=6144 memory=psram`.
- SNTP started immediately after WiFi IP acquisition and synchronized time in the same boot window.
- Internal SRAM improved substantially: after WiFi/time startup logs showed about `20KB` free internal SRAM with minimum around `17KB`, compared with earlier post-MQTT/FC pressure around `5-7KB`.
- FC page activation remained stable after the change. Logs showed FC task created in PSRAM, SD mounted on demand, `/sdcard/FC` scanned, and `fc scan found 64 nes files`.

Rejected experiment:

- `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y` was tested as a deeper network-memory optimization, but it caused WiFi initialization failure on this board: `wifi:malloc buffer fail`, `Expected to init 6 rx buffer, actual is 2`, then `ESP_ERR_NO_MEM` in `esp_wifi_init()`. This option was reverted and should not be reintroduced without a separate WiFi-driver memory plan.

## 2026-06-18: v1.7.9 FC Playability, ROM Names, and Firmware Release

Scope:

- Switched QDTech FATFS defaults from CP437/ANSI-OEM to CP936 with UTF-8 API encoding so Chinese long filenames on the MicroSD card are returned to the UI as UTF-8 instead of 8.3 aliases or garbled text.
- Changed the FC ROM list display to show a stable 3-digit row number plus a cleaned game title: directory path is removed, `.nes` is hidden, spaces are trimmed, and UTF-8 names are truncated without splitting Chinese characters.
- Added optional ROM alias files for numeric or messy ROM sets. The active ROM directory can contain `roms.txt`, `roms.csv`, or `games.txt`; each line may use `filename.nes=Display Name`, `filename.nes,Display Name`, or `filename.nes<Tab>Display Name`.
- ROM aliases are loaded once during the FC list scan, then games are sorted by the displayed name. This keeps gameplay rendering unchanged and does not add work to the frame loop.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src` after regenerating the temporary `build-qdtech/sdkconfig`.
- Generated config confirmed `CONFIG_FATFS_CODEPAGE_936=y`, `CONFIG_FATFS_CODEPAGE=936`, and `CONFIG_FATFS_API_ENCODING_UTF_8=y`.
- `xiaozhi.bin` size: `0x3c89f0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x237610`, about 37%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Hardware monitor confirmed stable boot, early SD mount, `/sdcard/FC` scan, and Chinese ROM names in FC logs such as `10.能源战士2(无限HP+无限生命)` and `100.恶魔城1无敌版`.
- Entered Nofrendo after the directory fix; gameplay remained around `27-32` FPS during the monitor window, so the ROM-name fix did not break the smoothness-first display path.
- Follow-up display fix: the FC list/detail labels were switched from Montserrat to the existing `font_puhui_16_4` Chinese font. The previous firmware could decode Chinese filenames correctly but still show missing glyphs on screen because Montserrat has no Chinese coverage.
- Follow-up font build completed and was flashed to `/dev/cu.usbmodem212401`; binary size remained `0x3c89f0`, with `0x237610` bytes free in the smallest app partition. Boot was verified after flashing; final FC-page visual inspection is left to the user because the monitor session did not capture a new FC page entry after this font-only pass.
- User visually confirmed the FC ROM list is readable after the font pass.
- Release prep bumped `PROJECT_VER` to `1.7.9` and allowed `components/nofrendo` to be tracked even though other generated `components/` content remains ignored.
- Final `v1.7.9` release build completed from `/private/tmp/qdtech_s3_build_src`: `xiaozhi.bin` `0x3c89f0`, smallest app partition `0x600000`, free `0x237610`.
- Final `v1.7.9` build was flashed to `/dev/cu.usbmodem212401`. Boot logs confirmed `App version: 1.7.9`, OTA current version `1.7.9`, early SD mount, `/sdcard/FC` scan, and readable Chinese FC list navigation through entries such as `10.能源战士2(无限HP+无限生命)`, `100.恶魔城1无敌版`, and `115.未来战士HACK`.

## 2026-06-18: FC Smoothness-First Layout and Stop Stability Pass

Scope:

- Reworked the FC bottom controls so the D-pad uses a wider left-side region with a larger center dead zone, A/B are farther apart, and LIST/Select/Start are more regular in the middle.
- Kept multi-touch controller combining for gameplay; direction plus A/B combinations are still supported through the manual touch path.
- Removed per-change controller serial logging from the hot path. Heavy A/B/D-pad testing previously flooded serial output and cost frame time.
- Changed Nofrendo quit handling so LIST/Stop requests power off the NES loop first and release Nofrendo state after the loop exits, avoiding the heap crash seen when stopping during active input polling.
- Added conservative touch I2C failure handling: after read failures the poller backs off, and repeated failures reset the I2C bus at a low rate. This avoids continuous timeout spam and CPU loss during touch-heavy FC list navigation.
- Tested enlarged draw widths `336x240` and `328x240`; both were rejected as defaults because they stayed around `24-26` FPS on hardware. The final default is back to `320x240` to prioritize smoothness.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x39ae30`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x2651d0`, about 40%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Boot logs confirmed `Touch max points: 5`, early SD mount, WiFi, MQTT, and normal idle state.
- `336x240` and `328x240` hardware tests entered Nofrendo but held only about `24-26` FPS, with occasional display-lock pressure. They are not smoothness-safe defaults.
- LIST/Stop was retested after the Nofrendo quit change: Nofrendo logged `nofrendo stopped result=0` and `nofrendo finished result=0` instead of crashing.
- Rapid FC list tapping after the I2C backoff/bus-reset change no longer produced the prior continuous I2C timeout flood during the captured boot/list test.

Current FC status:

- The practical smoothness ceiling for the current direct-draw path is `320x240`. Larger views need a deeper display strategy, such as a dedicated game mode that pauses competing LVGL/network redraws, a faster/asynchronous display path, or a controller/display-size setting exposed to the user.
- The latest flashed firmware is the smoothness-first `320x240` build with the improved layout and stop/touch stability fixes.

## 2026-06-18: FC Multi-Touch Controls and Larger Game View

Scope:

- Added multi-touch reading for the QDTech touch controller and changed FC game mode to combine all active touch points into one NES controller mask.
- Suppressed LVGL touch delivery while FC gameplay is active, so D-pad/A/B touches no longer also trigger page gestures or LVGL button events.
- Changed LVGL touch input to read cached coordinates from the manual touch poller instead of reading the I2C touch chip independently, removing repeated I2C timeout spam seen during touch-heavy testing.
- Widened and separated the FC virtual controller hit zones: D-pad uses a larger directional region with a center dead zone, and A/B are spaced farther apart.
- Enlarged standard NES frames from `256x240` to `320x240` before direct drawing, centered in the top `480x240` game area.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x39abb0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x265450`, about 40%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Boot logs confirmed `Touch max points: 5`, early SD mount, and `/sdcard/FC` ROM scanning.
- Nofrendo entered gameplay on multiple ROMs without reboot; logs showed `rgb frame buffer ready 256x240 pixels=61440`.
- Larger `320x240` direct draw held about `29-31` FPS in the final monitor window.
- LIST stopped Nofrendo cleanly. The final monitor window did not show the repeated touch I2C timeout spam seen before the cached-touch change.

Remaining FC status:

- User should test real play feel, especially hold-direction plus A/B combinations.
- If the larger view feels too slow, add a speed/display-size option or reduce the scaled draw width before deeper emulator changes.

## 2026-06-18: FC SD Scan Limit and AFE Memory Relief

Scope:

- Disabled QDTech local AFE wake word/audio processor defaults to preserve scarce internal SRAM for FC runtime while keeping audio capture/playback available.
- Kept SDMMC mount after LCD initialization and confirmed early SD mount succeeds on hardware.
- Reduced FC ROM scanning work: prefer `/sdcard/FC`, stop after the first populated ROM directory, cap the ROM list at 64 files, and defer ROM header validation until Start.
- This addresses the observed "SD contents missing" symptom, which was caused by FC scanning/runtime pressure rather than an actually missing SD mount.
- Fixed two Nofrendo startup crashes:
  - QDTech video adapter now returns a PSRAM-backed hardware bitmap from `lock_write()` so `vid_findmode()` has valid screen dimensions.
  - Nofrendo now allocates a `256x240` primary buffer instead of `256x224`, matching the PPU's 240 rendered scanlines.

Verification:

- Clean build completed from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size after the final crash fixes: `0x39a780`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x265880`, about 40%.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Boot logs confirmed `fc sd card mounted width=4` and `SD card prepared early`.
- Post-flash monitor stayed stable for several minutes without the previous AFE ringbuffer memory-exhausted reboot loop.
- FC Start entered Nofrendo without reboot. Logs showed `rgb frame buffer ready 256x240 pixels=61440`.
- Runtime held about `35-39` Nofrendo FPS on tested ROMs.
- Virtual controls were visible in logs during gameplay: Start/A/B/D-pad events reached `FcEmulator: controller=0x..`.

Remaining FC status:

- FC no longer instantly crashes on Start. Continue testing real gameplay compatibility across ROMs and mappers.
- One occasional display-lock warning appeared during heavy gameplay tapping, but it did not reboot the board.

## 2026-06-18: FC Direct Frame Path and Controller Verification

Scope:

- Added a QDTech display fast path that draws RGB565 landscape rectangles directly through the existing stable rotated transfer code.
- Wired FC gameplay frames to direct-draw the native `256x240` NES image at the top center of the `480x320` screen, instead of asking LVGL to refresh the whole screen for every game frame.
- Kept `LV_DISPLAY_RENDER_MODE_FULL` and `timer_period_ms = 50`; the earlier LVGL partial-render experiment still remains reverted because it corrupted the rotated display output.
- Changed FC frame publication so old/stale NES frames are not repeatedly converted and redrawn. The display is updated only when the NES core actually produces a new frame.
- Increased the FC emulation slice to `12000` instructions or about `12ms` per service loop after touch input was moved to the direct polling path.

Hardware observations:

- A second hotfix build proved bottom virtual buttons now reach both the FC service and the NES bus while the game is running.
- Captured examples included `FcEmulator: controller=0x01`, `0x02`, `0x10`, `0x20`, `0x40`, `0x80`, and `NesBus: controller latch=0x..`.
- This proves the remaining "no reaction" symptom is no longer a basic LVGL button event/layout failure.
- Before the stale-frame skip and larger emulation slice, `fc perf` still showed only about `3-5` produced NES frames per second on the tested ROM.
- The final third build was flashed successfully, but a fresh post-flash in-game `fc perf` sample was not captured because no new FC interaction occurred during the last monitor window.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x3df920`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x2206e0` bytes, about 35%.
- Flashed successfully to `/dev/cu.usbmodem212401`.

Current FC status:

- Touch/controller delivery is now confirmed on hardware.
- The display path no longer needs to use LVGL full-screen refresh for each game frame.
- FC should still be treated as not fully playable until the final build is tested in-game and `fc perf produced_fps=...` is captured again.
- If final `produced_fps` remains low, continue with emulator-core optimization/correctness or replace the minimal core with a known-good core.

## 2026-06-18: FC Responsiveness Hotfix and Display Rollback

Scope:

- Added a direct touch-poll fallback for FC game mode so bottom virtual controller touches feed `FcEmulatorService::SetController()` even when LVGL button events are delayed.
- Shortened FC controller release hold from `1500ms` to `120ms` to avoid sticky-feeling buttons.
- Reduced the FC emulator task priority from `3` to `1` so UI/touch and XiaoZhi tasks get scheduler time first.
- Split NES execution into shorter slices: `4500` instructions or about `6ms` per service loop, instead of a longer per-frame burst.
- Added `fc perf produced_fps=...` runtime logging for FC frame-rate and loop diagnostics.

Display note:

- A test change from LVGL full-screen render mode to partial render mode caused visible screen corruption on the QDTech rotated display path.
- That partial-render experiment was reverted immediately. Current code is back to `LV_DISPLAY_RENDER_MODE_FULL` and `timer_period_ms = 50`.
- Do not retry LVGL partial render mode without first rewriting and hardware-testing the custom rotated `Flush()` path.

Verification:

- Build completed successfully from `/private/tmp/qdtech_s3_build_src`.
- `xiaozhi.bin` size: `0x3df5a0`.
- Flashed successfully to `/dev/cu.usbmodem212401`.
- Post-rollback serial logs returned to the known full-screen flush path, for example `flush area=480x320 at 0,0`.

## 2026-06-18: FC/NES Core Timing and Sprite Correctness Pass

Scope:

- Replaced the previous fixed `18` PPU ticks per CPU instruction estimate with a 6502 base-cycle table and `CPU cycles * 3` PPU advancement.
- Advanced PPU time during NMI service cycles so NMI handling no longer consumes CPU cycles without corresponding PPU progress.
- Raised the NES frame instruction guard from `5200` to `12000` while keeping the existing per-call time budget.
- Fixed Mapper 2 bank switching to wrap selected PRG banks by the actual ROM bank count instead of masking to 16 banks.
- Fixed sprite rendering to keep low/high CHR bitplanes separate instead of OR-ing them into one byte.
- Fixed sprite X placement direction, OAM Y top-line offset, and sprite-0-hit detection against the original sprite index.
- Moved background tile fetch to the beginning of each 8-pixel group so visible pixels use the tile data for the current group.
- Changed `FcEmulatorService::Start()` to register callbacks only, so the FC task, SD mount, and ROM scan no longer start during normal boot.
- Releasing the FC page now stops playback, clears controller state, cancels scans, and releases the ROM list from the FC task.

Verification:

- Direct build in the original macOS workspace path reached link but failed because the parent path contains spaces/Chinese text and an ESP-SR linker search path was split into `3.5` and `寸/...`. This was treated as an environment/path issue, not an FC source compile error.
- Full validation build was run from a temporary no-space copy at `/private/tmp/qdtech_s3_build_src`.
- Build completed successfully for ESP32-S3/QDTech.
- Final `xiaozhi.bin` size after the FC lazy-start change: `0x3df3b0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x220c50` bytes, about 35%.

Hardware status:

- Flashed successfully from `/private/tmp/qdtech_s3_build_src` to `/dev/cu.usbmodem212401`.
- Boot log confirmed SKU `qdtech-s3-touch-lcd-3.5`, desktop UI creation, `Touch max points: 5`, WiFi, OTA latest, MQTT, AFE load, and `Application: STATE: idle`.
- Pre-lazy-start flash showed FC boot-time SD/ROM scan increased internal SRAM pressure and produced `esp-aes: Failed to allocate memory` during MQTT publish.
- Final lazy-start flash no longer starts the FC task, SD mount, or ROM scan at boot; captured boot logs showed only `fc emulator service registered` before normal XiaoZhi startup.
- Residual issue: AFE `Ringbuffer of AFE(FEED) is full` warnings still appear under the captured runtime, and internal SRAM remains tight. Treat this as a separate XiaoZhi/audio stability follow-up.
- FC/NES should still be treated as not yet proven playable until tested on the actual QDTech board with at least one known-good Mapper 0/NROM ROM and one Mapper 2 ROM.

## 2026-06-18: FC/NES App Card, ROM List, and Unfinished Emulator Core

Scope:

- Added an `FC NES` card to the desktop Apps page.
- Added a dedicated FC page with ROM status, ROM list text, preview/game screen, and touch actions.
- Removed the top FC page brand/title/status text to give the game screen more usable space.
- Added visible virtual controller buttons for Up/Down/Left/Right, A, B, Select, and Start.
- Added controller-state serial logging as `FcEmulator: controller=0x..` for touch debugging.
- Removed the right-side ROM information panel and Play/Stop/Prev/Next controls; FC now uses a handheld-style layout with the game screen on top and a classic controller on the bottom.
- Changed FC behavior to list-first: opening the FC page scans the SD card and shows a ROM list; Prev/Next only move selection; Start loads the selected game.
- Added a `LIST` button in game mode to stop emulation and return to the ROM list.
- Blocked right-swipe page exit while the game view is active to avoid accidental exits during D-pad swipes.
- Moved the NES PPU framebuffer from internal RAM to PSRAM after serial logs showed SDMMC mount failures with `sdmmc_read_sectors: not enough mem`.
- Reduced FC SD mount file handles and logged internal heap before mount attempts to make future SD failures diagnosable.
- Added `FcEmulatorService` for lazy task startup, SD card mount/reuse, `.nes` discovery, and iNES header validation.
- Scans these SD card locations for ROMs: `/sdcard/nes`, `/sdcard/NES`, `/sdcard/FC`, `/sdcard/fc`, `/sdcard/roms`, `/sdcard/ROMS`, and `/sdcard`.
- Reuses the existing SD mount when Photos already mounted `/sdcard`; otherwise mounts the QDTech SDMMC pins with 4-bit mode and 1-bit fallback.
- Added a minimal NES core:
  - 6502 CPU execution.
  - PPU background/sprite rendering into `256x240` frames.
  - Mapper 0/NROM and Mapper 2/UxROM PRG banking.
  - CHR ROM and CHR RAM handling.
  - Horizontal/vertical nametable mirroring.
  - Touch-screen virtual controller mapping for A/B/Select/Start/Up/Down/Left/Right.
- Optimized frame transfer by keeping the PPU framebuffer as 8-bit palette indices and converting to RGB565 only when publishing a frame to LVGL.
- Fixed one 6502 CPU bug found during the hardware session: `TSX (0xBA)` now copies `SP` to `X` instead of corrupting `SP`.
- Adjusted `RTI (0x40)` to restore the unused status bit.
- Mirrored controller reads on both `0x4016` and `0x4017` while debugging input compatibility.

Current limitation:

- This is not yet a playable emulator. It is a hardware-visible integration milestone with a ROM list, game view, and touch controller wiring.
- It only accepts Mapper 0 and Mapper 2 ROMs and has no APU/audio output yet.
- CPU/PPU timing and opcode compatibility are still incomplete. Hardware logs show the game can latch controller states, but tested ROMs still show little or no visible response to input.
- Unsupported mappers now fail explicitly instead of pretending to run as Mapper 0.
- The next implementation step should replace this minimal core with a more complete emulator core, or continue fixing CPU/PPU compatibility against a known-good small NROM ROM on the real board.
- A local reference checkout existed during this session at `C:\tmp\esp32-nesemu` using Nofrendo. It was not integrated because the original example reads a fixed Flash ROM and drives ILI9341 directly; integration would need a clean SD-ROM, LVGL-frame, and touch-input adapter. Check license obligations before importing it.

Verification:

- Build succeeded with the minimal emulator core.
- `xiaozhi.bin` size: `0x3dd8b0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x222750` bytes, about 36%.
- Flashed successfully to `COM13`.
- Serial logs confirmed SD mount, scan, and 146 supported `.nes` files found on the user's SD card.
- Serial logs confirmed list navigation, selected ROM load, and NES frame publication.
- Serial logs confirmed virtual buttons reach the emulator service as `controller=0x..`.
- Serial logs confirmed the NES bus latches controller states such as `0x40`, `0x20`, `0x08`, `0x02`, `0x01`.
- User-visible result at handoff: ROM list works and games can open, but gameplay remains effectively non-responsive and not ready to call playable.

## 2026-06-15: Radio Visual Enhancement - Audio Spectrum Bars

Scope:

- Added 16 audio spectrum bars at the bottom of Radio page
- Bars animate randomly when radio is playing (simulating audio frequency)
- Bars show static 5px height when radio is stopped
- Animation refresh rate: 100ms
- Uses COLOR_GOLD for visual consistency

Implementation:

- Added `radio_bars_[16]` array for bar objects
- Added `radio_playing_` state flag
- Added `RadioAnimTimerCb` callback for animation
- Modified `SetRadioState` to update playing state based on state string

Verification:

- Build: `idf.py -B build-qdtech build`
- Flash: `idf.py -B build-qdtech -p COM13 -b 921600 flash`
- Bars visible on Radio page
- Animation active during playback

## 2026-06-15: P0-P6 Optimization and LVGL Touch Migration

Scope:

- P0: Verified build environment and reproducible build process
- P1: Hardware runtime validation (boot log, WiFi, MQTT, SNTP, weather)
- P2: Expanded daily card content (festivals 19→19, history 8→31, quotes 16→32)
- P3: Focus timer NVS persistence for completion count
- P4: Enhanced radio audio focus logging
- P5: Radio station index NVS persistence, error state after 5 failures
- P6: LVGL touch input device migration (hybrid mode)

Font fix:

- Regenerated LXGW WenKai subset fonts (499 Chinese characters)
- Fixed garbled text on daily card and UI elements
- Adjusted daily card layout (title width 82→100px, divider 112→120px)

Touch architecture:

- Created LVGL input device for button clicks and slider interactions
- Kept manual touch polling for gesture detection (LVGL 9.x limitation)
- Disabled duplicate HandleTap coordinate detection for migrated pages
- Added Radio button event callbacks (prev/play/stop/next)

Verification:

- Build: `idf.py -B build-qdtech build`
- Flash: `idf.py -B build-qdtech -p COM13 -b 921600 flash`
- Boot: SKU=qdtech-s3-touch-lcd-3.5, Desktop UI created, WiFi connected
- Daily card: Festival/history/quote content displays correctly
- Focus timer: Completion count persists across reboots
- Radio: Station index persists, error state shows after failures

## 2026-06-13: Fix Black Photos Page After Settings Growth

Problem:

- Opening Photos showed a black screen.
- Runtime logs proved the Photos page activated, but the lazy photo task failed to start:
  - `PhotoService: photo service task create failed ret=-1`
- The SD card and JPEG files were not the cause.

Fix:

- Removed the obsolete Settings controls that were still being created and then hidden behind the new Settings layout.
- Moved the 6144-byte PhotoService task stack to PSRAM with an internal-memory fallback.
- Added internal-memory and largest-free-block diagnostics around photo task creation.
- Kept normal Photos behavior lazy: the task still starts only when Photos is opened.

Verification:

- Final firmware built successfully.
- `xiaozhi.bin` size: `0x3c3c30`.
- Final firmware flashed successfully to COM13 at 921600 baud.
- A temporary, uncommitted boot self-test activated PhotoService and confirmed:
  - task created with `memory=psram`
  - SD card mounted in 4-bit mode at 20 MHz
  - 53 JPG files found in the physical photos directory
  - repeated 480x320 JPEG decode succeeded
- The temporary auto-activation was removed before the final build and flash.
- No panic, abort, or Guru Meditation appeared during the self-test.

Maintainer notes:

- Do not restore hidden duplicate Settings object trees; hidden LVGL objects still consume memory.
- Keep optional service task stacks in PSRAM where the ESP-IDF configuration and workload permit it.
- If task creation fails again, inspect both total internal SRAM and the largest contiguous internal block.

## 2026-06-13: Unify Desktop Navigation Rules

Scope:

- Added a single `NavigateBack()` rule:
  - Apps returns to Main.
  - Feature pages return to Apps.
- Aligned manual swipe behavior with the visible navigation model:
  - Main left swipe opens Apps.
  - Apps right swipe returns to Main.
  - Feature pages right swipe return to Apps.
  - Photos keeps the previously accepted left-or-right swipe exit.
- Removed hidden tap-to-exit and refresh zones from the full-screen Photos page.
- Leaving the Radio page no longer implicitly stops playback; Stop remains an explicit transport action.
- Simplified the Apps footer hint to `Swipe right: Home`.

Verification:

- Build completed successfully.
- `xiaozhi.bin` size: `0x3c3cc0`.
- Firmware flashed successfully to COM13 at 921600 baud.
- Boot reached `Desktop UI created`, touch initialization, `STATE: idle`, and SNTP synchronization.
- Physical right-swipe exit from Photos logged `Navigate back page=2 target=1` and returned to Apps.
- No panic, abort, or Guru Meditation appeared in the captured logs.
- Main/Apps/Radio navigation and background-radio continuity still need a complete user-visible pass on the touchscreen.

## 2026-06-13: Make Settings Visible And Functional

Scope:

- Replaced the off-screen Settings content with a scrollable layout sized for the 480x320 display.
- Added working brightness and output-volume sliders.
- Added a manual-touch release bridge for the current non-`lv_indev_t` input path:
  - horizontal slider release applies the final brightness or volume value
  - vertical release scrolls the Settings content
  - hidden slider regions cannot receive touches outside the visible content viewport
  - the Settings Back control has a matching manual touch zone
- Hardware values are synchronized when Settings opens.
- Changes are persisted only when slider interaction finishes, avoiding repeated NVS writes while dragging.
- Kept WiFi scan results and weather-location status available in the page.
- Fixed an initialization deadlock found during hardware verification by deferring `Board::GetInstance()` access until after board construction.

Verification:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build
idf.py -B build-qdtech -p COM13 -b 921600 flash
```

Build result:

- Build completed successfully.
- `xiaozhi.bin` size: `0x3c3cd0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x23c330` bytes, about 37%.

Hardware/runtime verification on COM13:

- Firmware flashed successfully at 921600 baud.
- Boot reached `Desktop UI created`.
- Touch controller reported 5 points.
- WiFi and MQTT connected.
- Application reached `STATE: idle`.
- SNTP synchronized.
- No panic, abort, or Guru Meditation appeared in the captured startup log.
- Physical brightness dragging was confirmed with values from 16% through 100%.
- Physical vertical Settings scrolling was confirmed in both directions.
- Physical volume dragging, Back control, and persisted-value restoration still require user-visible confirmation on the touchscreen.

Maintainer notes:

- Do not call `Board::GetInstance()` from `DesktopUI::Create()` while the QDTech board singleton is still being constructed.
- Slider changes are applied on release to limit persistent-storage writes.
- Settings currently has an explicit manual-touch adapter because the board has not yet migrated to a standard LVGL input device.

## 2026-06-13: Replace QTE With Focus Timer And Reduce Layout Overlap

Scope:

- Replaced the Apps page `QTE / Quote / Daily` tile with `FOC / Focus / 25 min`.
- The tile now opens a local Focus Timer page instead of starting a XiaoZhi quote chat.
- Added a lightweight LVGL focus timer state machine:
  - 25 minute focus mode
  - 5 minute break mode
  - start/pause
  - reset
  - completed focus-session counter
- Reworked the Focus Timer page after hardware feedback that the first version had overlapping elements.
- Current layout is a small-screen three-column dashboard:
  - left status/quote card
  - center timer ring and time readout
  - right focus/break controls and completed count
  - bottom start/reset buttons
- Updated manual touch hot zones for the new Focus Timer button positions.
- Regenerated embedded LXGW WenKai 16 px and 20 px subset fonts for the added Chinese UI text.

Verification:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build
idf.py -B build-qdtech -p COM13 -b 921600 flash
```

Build result:

- Build completed successfully. The outer `idf.py` command timed out once while Ninja continued in the background; the background build finished successfully.
- `xiaozhi.bin` size: `0x3c2ff0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x23d010` bytes, about 37%.

Hardware/runtime verification on COM13:

- Firmware flashed successfully at 921600 baud.
- Boot reached `Desktop UI created`.
- Application reached `STATE: idle`.
- SNTP synchronized.
- Daily card updated for `2026-06-13`.
- Weather fetch completed for Zhongshan.
- No panic, abort, or Guru Meditation was observed in the captured startup log.

Maintainer notes:

- Focus Timer is currently local UI state only; completed count is not persisted in NVS.
- If Focus Timer Chinese labels change, regenerate both `qd_font_lxgw_16.c` and `qd_font_lxgw_20.c`.
- Keep the Focus page sparse on the 480x320 screen; avoid re-adding large decorative elements unless hardware-visible spacing is checked.

## 2026-06-06: Weather Visuals And Chinese Daily Card

Scope:

- Improved the main-page weather card so Open-Meteo weather codes select clear, cloudy, rain, snow, and storm visuals instead of always showing the sunny icon.
- Adjusted weather temperature and detail label positions to avoid overlap.
- Replaced the English quote card content flow with a date-linked daily card.
- Daily card priority is now fixed Gregorian festival first, history-on-this-day second, and local daily quote fallback third.
- Added embedded LXGW WenKai 16 px and 20 px LVGL subset fonts for the current Chinese daily-card text.
- Laid out the daily card as left-side date/category and right-side larger Chinese body text to use the full card width.
- Kept the font source TTF out of Git; only generated LVGL subset C files are tracked.

Verification:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" reconfigure build
idf.py -B build-qdtech -p COM13 -b 921600 flash
```

Build result:

- Build completed successfully after reconfigure so the new generated font C files were included.
- `xiaozhi.bin` size: `0x3c0c10`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x23f3f0` bytes, about 37%.

Hardware/runtime verification on COM13:

- Firmware flashed successfully at 921600 baud.
- Boot reached `Desktop UI created`.
- Application reached `STATE: idle`.
- SNTP synchronized.
- Daily card logged `Daily card updated for 2026-06-06`.
- Weather card logged a cloudy visual mapping for weather code `1`.
- Weather fetch completed for Zhongshan with temperature and cached update time.
- No panic, abort, or Guru Meditation was observed in the captured startup log.

Maintainer notes:

- If new Chinese daily-card text is added, regenerate both `qd_font_lxgw_16.c` and `qd_font_lxgw_20.c` so all glyphs exist.
- Do not commit `.codex_tmp/`; it only held the temporary LXGW WenKai TTF used to generate the subset fonts.
- The historical-events table is intentionally small for this phase. Expand it gradually and verify card wrapping on the real 480x320 screen.

## 2026-06-05: Finish MicroSD Full-Screen Photo Slideshow

Scope:

- Made the Photos page a pure full-screen slideshow with no title, status bar, Back button, Refresh button, hint text, or status text overlay.
- Added left-swipe and right-swipe exit from Photos back to Apps.
- Expanded photo scanning to common SD directory names: `/photos`, `/PHOTOS`, `/Photos`, `/PHOTOS_READY`, `/photos_ready`, and the SD root.
- Skips hidden/macOS resource files such as `._001.jpg`.
- Added SDMMC mount fallback from 4-bit default speed to 1-bit 10 MHz when 4-bit mount fails.
- Added clearer SD mount and photo directory scan logs.
- Fixed JPEG RGB565 color output by disabling the extra JPEG byte swap.
- Changed photo scaling to cover the full 480x320 landscape screen.

Verification:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build
idf.py -B build-qdtech -p COM13 -b 921600 flash
```

Build result:

- Build completed successfully.
- `xiaozhi.bin` size: `0x3af950`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x2506b0` bytes, about 39%.

Hardware/user verification:

- SD card mounted and photos displayed.
- Photo colors were confirmed normal after disabling JPEG byte swap.
- Photos were confirmed full-screen.
- Visible Photos page text and buttons were removed.
- Photos page exits with left or right swipe.

## 2026-06-04: Project Handoff Notebook

Scope:

- Added a documentation-only handoff set under `docs/`.
- No source code changed.
- No firmware behavior changed.

Reason:

- The project is becoming a long-term maintained firmware fork.
- Future Codex sessions and new computers need a reliable first-read path before touching code.

Verification:

- Documentation-only change. Firmware build/flash not required for this entry.

## 2026-06-04: Add MicroSD Photo Slideshow Entry

Scope:

- Replaced the Apps page Weather tile with a Photos tile.
- Added a dedicated Photos page with SD slideshow status, Refresh control, and fade transition image area.
- Added `PhotoService` to mount the MicroSD card, scan `/sdcard/photos`, decode JPEG files, and loop playback.
- Added QDTech MicroSD SDMMC pins from the product specification:
  - CLK GPIO5
  - CMD GPIO4
  - D0 GPIO6
  - D1 GPIO7
  - D2 GPIO2
  - D3 GPIO3
- Photo decoding pauses when leaving the Photos page.

Verification:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" reconfigure build
cmake --build build-qdtech
```

Build result:

- `cmake --build build-qdtech` completed successfully.
- `xiaozhi.bin` size: `0x3ae7c0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x251840` bytes, about 39%.

Hardware verification:

- Flashed to COM13 at 921600 baud successfully.
- Runtime boot verified after flashing: PSRAM, display, touch init, WiFi, MQTT, wake-word pipeline, `PhotoService: photo service started`, and `Application: STATE: idle`.
- Photos page SD mount and JPEG playback still need an inserted MicroSD card and an on-device tap into the Photos page.
- Prepare a FAT-formatted MicroSD card with JPG files in `/photos`.

## 2026-06-04: Improve Photo SD Filename Diagnostics

Scope:

- Enabled FATFS long filename support in the QDTech board defaults for the MicroSD photo slideshow.
- Added `errno` / `strerror` details to photo `stat()` and `fopen()` failure logs.

Reason:

- Hardware test reached SD card directory scanning, but photo files failed at open/stat with 8.3-style names such as `/sdcard/photos/WEIQU.JPG`.
- The active build config had `CONFIG_FATFS_LFN_NONE=y`, which is risky for copied photo files and long filenames.

Verification:

- Rebuilt successfully with `cmake --build build-qdtech`.
- Flashed successfully to COM13 at 921600 baud.

## 2026-06-04: Restore XiaoZhi AI Stability After Photo/Radio Work

Problem observed:

- Entering XiaoZhi AI could reboot the board.
- Backtrace resolved to `MqttProtocol::OpenAudioChannel()` -> `EspUdp::Connect()` -> `std::thread`, where UDP receive thread creation failed and C++ terminated the process.
- MCP `tools/list` replies were large enough to cause repeated MQTT publish failures and `esp-aes: Failed to allocate memory`.
- Each MCP publish failure triggered the generic network error alert, causing repeated speaker pops.

Fix:

- Reverted the risky radio streaming optimization commit.
- Made `EspUdp::Connect()` catch `std::system_error` when creating the UDP receive thread and return `false` instead of aborting.
- Reduced MCP `tools/list` page size and shortened common/QDTech MCP tool descriptions.
- Treated MCP publish failures as warnings instead of user-facing audio alerts.
- Made the photo slideshow task lazy-start only when the Photos page is opened.
- Reduced radio task stack from 8192 to 6144 bytes.

Verification:

- Built successfully with `cmake --build build-qdtech`.
- Flashed successfully to COM13 at 921600 baud.
- XiaoZhi AI reached `STATE: listening` and `STATE: speaking`.
- Serial log showed XiaoZhi replies including `你好，小志。` and no reboot.

## 2026-06-04: Add Local Calendar Month View

Scope:

- Added a dedicated Calendar page in the QDTech desktop UI.
- Calendar app tile now opens the local Calendar page instead of starting a XiaoZhi chat prompt.
- Calendar page shows a 7x6 month grid, previous/next month controls, a Today control, weekend coloring, and today highlight.
- Time/weather service now passes the synced year into `DesktopUI::SetTime()` so the calendar can track the current date.
- Status bar time tracking was expanded to cover the added page.

Verification:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build
cmake --build build-qdtech
```

Build result:

- `cmake --build build-qdtech` completed successfully.
- `xiaozhi.bin` size: `0x39e7f0`.
- Smallest app partition: `0x600000`.
- Free app partition space: `0x261810` bytes, about 40%.

Hardware verification:

- Flashed successfully to `COM13`.
- Boot log confirmed `SKU=qdtech-s3-touch-lcd-3.5`.
- Desktop UI was created.
- Touch reported `Touch max points: 5`.
- WiFi connected with IP `192.168.4.92`.
- MQTT connected.
- AFE/wake word started.
- Application reached `STATE: idle`.
- SNTP time synchronized.
- Touch path verified:
  - left swipe from main page opened Apps.
  - tapping Calendar tile opened the Calendar page.
  - tapping Next changed the Calendar month to `2026/07`.
- Weather API returned `502` during this run, but the firmware stayed running.

## 2026-06-04: Stabilize QDTech Desktop Base

Commit:

- `f4451c1 stabilize QDTech desktop base`

Scope:

- Added/updated QDTech board README and root `PATCHES.md`.
- Added lightweight device-state callback support in `Application`.
- Improved radio service audio focus, reconnect, fallback URL memory, and idempotent controls.
- Improved time/weather service retry and cached-weather behavior.
- Clarified touch polling and display flush performance logging in board code.

Verified build:

```powershell
. 'C:\Users\Administrator\esp-idf\export.ps1'
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build
```

Verified flash:

```powershell
idf.py -B build-qdtech -p COM13 -b 921600 flash
```

Observed runtime:

- Board booted with `SKU=qdtech-s3-touch-lcd-3.5`.
- Desktop UI was created.
- Touch reported `Touch max points: 5`.
- WiFi connected and MQTT connected.
- AFE/wake word started.
- Application reached `STATE: idle`.
- Weather API returned transient failures during testing, but firmware stayed running.

## Earlier QDTech Milestones

Known recent commits from this fork:

- `d6f1f04 feat: WiFi管理优化、电台更新、小智AI面部动画改进`
- `83b1983 docs add qdtech reproducible build notes`
- `ac1e8a2 feat add network radio playback`
- `8d79b78 fix desktop button tap handling`
- `18ffaf7 fix app card tap coordinates`
- `1677e5f fix app cards open xiaozhi`
- `a4e98e0 feat configurable weather location`

When continuing from these commits, verify behavior on hardware rather than assuming the commit message proves runtime success.
