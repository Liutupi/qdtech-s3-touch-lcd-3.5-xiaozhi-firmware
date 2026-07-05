# XiaoZhi Mac MCP Handoff

> Future Codex note: this file documents the Mac-side MCP services that work with the QDTech XiaoZhi firmware. It is intentionally secret-free. Do not commit endpoint token files, logs containing tokens, or local API keys.

## 2026-07-05 NetEase NAS QR Login Bridge

- Repository copy: `tools/nas/xiaozhi-ws-mcp.js`.
- Local working copy: `/Users/tupi/xiaozhi-mcp-services/netease-music/xiaozhi-ws-mcp.js`.
- The bridge extracts NetEase login QR data from `music.netease_login_qr` and login-required play results.
- When QR data is available, it calls device tool `self.display.qrcode` with title `网易云扫码登录` and a short hint so the board can show the QR code directly.
- QDTech firmware `v1.7.85` adds the matching `self.display.qrcode` display tool and LVGL QR support.

UGREEN NAS warning:

- The Docker container copy `/app/xiaozhi-ws-mcp.js` was observed truncated at `23515` bytes and failed `node -c`.
- The complete local/repository bridge is `24683` bytes.
- Repair the NAS shared mount file before restarting `xiaozhi-netease-xiaocanglan` or `xiaozhi-netease-yuyu`.

## 2026-07-04 Music Playback Stability Notes

- QDTech firmware `v1.7.76` now rejects likely preview-length MP3 URLs before playback. A URL around a few hundred KB is treated as a preview clip, not a real song.
- The firmware fix prevents the board from crashing/restarting on these URLs, but the Mac/remote NetEase MCP still needs to provide a full direct MP3 URL for the song to actually play.
- `self.music.play_url` should be called only with a direct playable MP3 URL plus stable `title` and `artist`; do not send a preview URL and then rely on firmware retries.
- If the device result says the music URL was rejected as short/preview, search another source or return a clear failure instead of starting lyrics or telling the user the song is playing.
- Lyrics are best-effort: under very low internal SRAM, the firmware may skip scheduled lyric display to preserve playback stability.

## 2026-06-30 Working Setup

Long-term local service directory on the user's Mac:

- `/Users/tupi/xiaozhi-mcp-services`

Do not move these services back into temporary school/project folders such as thesis draft directories. They are configured as macOS user LaunchAgents and should survive a computer reboot after the user logs in.

Sensitive files that must stay local only:

- `*/current-endpoint*.txt`
- OpenClaw/XiaoZhi/API credential files
- raw service logs if they contain endpoint URLs or tokens

## NetEase Music MCP

Local directory:

- `/Users/tupi/xiaozhi-mcp-services/netease-music`

Agents:

- `瀹囧畤`: local music API port `3099`, LaunchAgent `com.tupi.xiaozhi.netease.yuyu`
- `灏忚媿鍏癭: local music API port `3100`, LaunchAgent `com.tupi.xiaozhi.netease.xiaocanglan`

Logs:

- `/Users/tupi/xiaozhi-mcp-services/netease-music/netease-mcp-yuyu.log`
- `/Users/tupi/xiaozhi-mcp-services/netease-music/netease-mcp-xiaocanglan.log`

Core playback rule:

- `music.netease_play` searches NetEase and gets a fresh direct HTTP/HTTPS MP3 URL.
- After every successful URL lookup, the Mac bridge must continue to call the device tool:

```json
self.music.play_url({
  "title": "song title",
  "artist": "artist name",
  "url": "direct http/https mp3 url"
})
```

Do not only return text such as "playing". Do not only call an internal `play_music`. Do not depend on first-song cache behavior. The second and third songs must also produce a new `self.music.play_url` call.

Current lyric contract for QDTech firmware `v1.7.75`:

- `self.music.play_url` may include `lyrics_json`; if non-empty and parseable, the firmware schedules lyric lines locally.
- `self.music.show_lyric(line,title,artist)` may still be sent, but the firmware filters stale lines and may ignore external lines while scheduled lyrics for the same current song are active.
- If the Mac MCP returns `lyrics_json length=0`, the firmware cannot invent lyrics; it clears old lyrics and shows `Playing: <title>`.
- For debugging, the board serial should show `self.music.play_url ... lyrics_json length=...`, `ParseLyricsJson ... lines=...`, and `SetMusicLyric overlay line=...`.
- Keep `title` and `artist` stable between `play_url` and `show_lyric`; mismatched fields are treated as stale after a song cutover.

2026-07-01 critical fix:

- Do not expose a NetEase MCP tool named `play_music`.
- XiaoZhi already has an internal `play_music`; exposing a Mac-side tool with the same name caused board alerts like `Duplicate tool names: play_music` and broke the music chain.
- The Mac bridge should expose only `music.netease_play` and `music.netease_search`.
- The bridge result for `music.netease_play` should include JSON fields such as `next_tool: "self.music.play_url"` and `play_url_arguments`, so the model/device chain has the fresh `title`, `artist`, and `url`.
- Both `com.tupi.xiaozhi.netease.yuyu` and `com.tupi.xiaozhi.netease.xiaocanglan` must be restarted after this change so their `tools/list` no longer includes `play_music`.

Expected log sequence when point-singing works:

```text
tool call music.netease_play {"song":"..."}
tool result {"success":true,"status":"pending_device_play_url",...,"next_tool":"self.music.play_url"}
>> tools/call self.music.play_url {"title":"...","artist":"...","has_url":true,"url":"http://...mp3..."}
<< self.music.play_url result ...
```

On 2026-06-30 the bridge was hardened so `self.music.play_url` is no longer fire-and-forget only. It now waits for the device/tool result for up to 8 seconds and logs either:

- `<< self.music.play_url result ...`
- `<< self.music.play_url failed ...`

This makes it easier to tell whether the Mac bridge sent the URL but the board failed to execute it.

Quick status checks:

```bash
launchctl print gui/$(id -u)/com.tupi.xiaozhi.netease.yuyu
launchctl print gui/$(id -u)/com.tupi.xiaozhi.netease.xiaocanglan
lsof -nP -iTCP:3099 -iTCP:3100 -sTCP:LISTEN
tail -n 120 /Users/tupi/xiaozhi-mcp-services/netease-music/netease-mcp-yuyu.log
tail -n 120 /Users/tupi/xiaozhi-mcp-services/netease-music/netease-mcp-xiaocanglan.log
```

Quick local URL lookup tests:

```bash
curl -sS 'http://127.0.0.1:3099/health'
curl -sS 'http://127.0.0.1:3099/stream_pcm?song=%E6%99%B4%E5%A4%A9&artist=%E5%91%A8%E6%9D%B0%E4%BC%A6'
curl -sS 'http://127.0.0.1:3100/health'
curl -sS 'http://127.0.0.1:3100/stream_pcm?song=%E6%99%B4%E5%A4%A9&artist=%E5%91%A8%E6%9D%B0%E4%BC%A6'
```

Restart after code or connection changes:

```bash
launchctl kickstart -k gui/$(id -u)/com.tupi.xiaozhi.netease.yuyu
launchctl kickstart -k gui/$(id -u)/com.tupi.xiaozhi.netease.xiaocanglan
```

If the local health and URL tests pass but the board does not play:

1. Watch the Mac log for `music.netease_play`.
2. Confirm the Mac log has `>> tools/call self.music.play_url` with a new URL.
3. Confirm the new result/failed line after the device call.
4. If the Mac log has no `music.netease_play`, the XiaoZhi agent did not select the NetEase MCP tool.
5. If the Mac log sent `self.music.play_url`, continue debugging from board serial logs and audio output.

## OpenClaw / Dodo MCP

Local directory:

- `/Users/tupi/xiaozhi-mcp-services/openclaw-mcp`

LaunchAgent:

- `com.tupi.xiaozhi.openclaw.yuyu`

Log:

- `/Users/tupi/xiaozhi-mcp-services/openclaw-mcp/openclaw-mcp-yuyu.log`

OpenClaw gateway:

- `http://127.0.0.1:18789/health`

OpenClaw agent:

- default agent `main`
- user-facing name `澶氬`

Important implementation note:

- Normal `openclaw.ask_dodo` voice answers use a lightweight direct OpenAI-compatible call to Xiaomi `mimo-v2.5` so XiaoZhi receives short answers fast enough for speech.
- Full OpenClaw CLI is kept as a fallback, because repeated full CLI calls were too slow and caused XiaoZhi to report that Dodo was offline.
- "鍦ㄧ嚎鍚? style checks can be answered locally and quickly.

Current tools:

- `openclaw.status`
- `openclaw.check_services`
- `openclaw.ask_dodo`
- `openclaw.open_app`
- `openclaw.open_url`
- `openclaw.safe_action`
- `openclaw.create_folder`
- `openclaw.list_files`
- `openclaw.prepare_file_move`
- `openclaw.prepare_organize_folder`
- `openclaw.confirm_file_action`

File management safety model:

- Allowed roots only:
  - `/Users/tupi/Desktop`
  - `/Users/tupi/Documents`
  - `/Users/tupi/Downloads`
  - `/Users/tupi/xiaozhi-mcp-services`
- Creating folders is allowed in those roots.
- Moving files and organizing folders are two-step actions:
  1. Call `openclaw.prepare_file_move` or `openclaw.prepare_organize_folder`.
  2. Read the plan and confirmation code to the user.
  3. Only after the user confirms with the code, call `openclaw.confirm_file_action`.
- No delete operations.
- No overwriting existing files.
- No arbitrary terminal command execution.

Useful voice tests:

- `瀹囧畤锛岄棶澶氬鍦ㄧ嚎鍚楋紵`
- `瀹囧畤锛屾墦寮€璁胯揪銆俙
- `瀹囧畤锛屽垪鍑烘闈㈡枃浠躲€俙
- `瀹囧畤锛屾妸妗岄潰涓婄殑娴嬭瘯鏂囦欢绉诲姩鍒版枃绋裤€俙
- `瀹囧畤锛屾暣鐞嗕笅杞芥枃浠跺す銆俙
- `纭鎵ц锛岀‘璁ょ爜 123456銆俙

Quick status checks:

```bash
launchctl print gui/$(id -u)/com.tupi.xiaozhi.openclaw.yuyu
curl -sS http://127.0.0.1:18789/health
tail -n 160 /Users/tupi/xiaozhi-mcp-services/openclaw-mcp/openclaw-mcp-yuyu.log
```

Restart:

```bash
launchctl kickstart -k gui/$(id -u)/com.tupi.xiaozhi.openclaw.yuyu
launchctl kickstart -k gui/$(id -u)/ai.openclaw.gateway
launchctl kickstart -k gui/$(id -u)/ai.clawket.bridge.cli
```

The OpenClaw LaunchAgent must run with a PATH that includes the user's local Node runtime, otherwise the clean launchd environment can fail with `env: node: No such file or directory`.

## XiaoZhi Backend Role Prompt

For `瀹囧畤`, keep these intent-routing rules in the XiaoZhi console role description:

- For music requests, actually call tools. Do not only reply "playing".
- If an external music tool returns `audio_url` or `url`, call `self.music.play_url`.
- For Mac/OpenClaw/service status questions, call `openclaw.status` or `openclaw.check_services`.
- For "ask Dodo" style requests, call `openclaw.ask_dodo` and read the returned answer.
- For opening Mac apps, call `openclaw.open_app`. Only open the OpenClaw gateway page when the user explicitly asks for the OpenClaw console.
- For creating folders, call `openclaw.create_folder`.
- For moving or organizing files, call the prepare tool first, read the plan and confirmation code, and only then call `openclaw.confirm_file_action` after user confirmation.

After saving the XiaoZhi backend role/MCP configuration, restart or reconnect the physical board. The console itself warns that new configuration only takes effect after device restart/reconnect.

## 2026-06-30 Verified Experiences

- Both NetEase services can be run at the same time, one for `瀹囧畤` and one for `灏忚媿鍏癭; separate boards/agents do not conflict when each has its own MCP endpoint and local music API port.
- The Mac-side NetEase services can be left as LaunchAgents for reboot persistence.
- The direct local NetEase API can return a playable MP3 URL even when the board is not playing; this means playback failures must be separated into "Mac did not fetch", "Mac did not call device", and "device did not play".
- A successful point-song chain was observed for `鍐夋槬`: `music.netease_play` returned a URL, then the bridge logged `>> tools/call self.music.play_url` with the direct MP3 URL.
- For OpenClaw, direct lightweight Dodo answers are more reliable for voice than repeated full CLI calls.
- File management should be expanded conservatively with prepare/confirm tools rather than broad computer-control access.
