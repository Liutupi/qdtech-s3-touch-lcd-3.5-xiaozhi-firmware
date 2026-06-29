# QDTech 3.5 XiaoZhi Project Handoff

> Future Codex note: read this file, `docs/PROJECT_STATUS.md`, `docs/NEXT_TASKS.md`, and `docs/CODEX_RULES.md` before changing code.

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
- `吞食天地2 [先锋卡通汉化 (laopix简体中文名字版)].nes` is confirmed by serial log as `crc=3963f12a`, `header_mapper=4`, `corrected_mapper=198`; it still shows black/solid-color output while CPU/audio continue running and frame samples stay effectively constant (`min=max=1084` after startup).

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
  - `weather ok 27 C 中山 阴 00:00 H98% C98%`

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
  - Weather synced: `weather ok 28 C 中山 毛毛雨 17:30 H95% C100% raw=55 refined=55`.
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
weather ok 28 C 中山 阴 00:30 H95% C97% raw=95 refined=3 rain=0.00 cloud=97 humidity=95 updated=00:30
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
- English city verification passed: posting `city=dongguan` resolved to `东莞市 23.0180,113.7487`, `/config` updated, weather fetched `31 C 东莞市 Storm`, and no reboot occurred.
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
- Cat theme visual updates include a pinker background, pink/white cards, pink-orange high-contrast time digits, Chinese brand mark `小苍兰 / 端午`, a small cat on the daily-card panel, and a cat-style XiaoZhi face page.
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
- Battery hardware follow-up on 2026-06-20: local schematic `D:\3.5inch_ESP32-S3\5-_Schematic\ESP32-S3原理图.pdf` shows a USB-C powered battery charge/discharge circuit with `U2 TP4054`, `VBUS/+5V`, `JP1 BAT+`, and `CHRG`. USB-C should charge a connected single-cell Li-ion/LiPo battery automatically in hardware; firmware does not need to control charging.
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

- Workspace: `/Users/tupi/Documents/带小智 3.5 寸/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware`
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

- Pulled `https://github.com/Liutupi/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware.git` into `/Users/tupi/Documents/带小智 3.5 寸/qdtech-s3-touch-lcd-3.5-xiaozhi-firmware`.
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
  - Hardware monitor confirmed `/sdcard/FC` scan and readable Chinese ROM names in logs, for example `10.能源战士2(无限HP+无限生命)` and `100.恶魔城1无敌版`.
  - Entering Nofrendo after the ROM-name pass stayed stable and logged about `27-32` FPS, so this directory/name fix did not regress the smoothness-first `320x240` path.
  - After the user reported that later ROM names still looked garbled on the panel, the FC list/detail labels were switched from Montserrat to the existing `font_puhui_16_4` Chinese font. This addresses the separate display-font layer: decoded UTF-8 names need a font with Chinese glyph coverage.
  - The font-only pass built and flashed successfully to `/dev/cu.usbmodem212401` with the same `xiaozhi.bin` size `0x3c89f0`; boot was verified. The monitor session did not capture a fresh FC page entry after this last flash, so the user still needs to visually confirm the FC list on the panel.
  - User confirmed the FC ROM list became readable after the font pass.
  - Release prep bumped `PROJECT_VER` to `1.7.9` and changed `.gitignore` so `components/nofrendo` is tracked while other generated `components/` content remains ignored. Do not drop the Nofrendo component from future commits; `FcEmulatorService` includes `qdtech_nofrendo.h`.
  - Final `v1.7.9` release build passed from `/private/tmp/qdtech_s3_build_src`: `xiaozhi.bin` `0x3c89f0`, smallest app partition `0x600000`, free `0x237610`.
  - Final `v1.7.9` build was flashed to `/dev/cu.usbmodem212401`; monitor logs confirmed `App version: 1.7.9`, OTA current version `1.7.9`, early SD mount, FC page activation, `/sdcard/FC` scan, and readable Chinese list navigation through names such as `10.能源战士2(无限HP+无限生命)`, `100.恶魔城1无敌版`, and `115.未来战士HACK`.
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
