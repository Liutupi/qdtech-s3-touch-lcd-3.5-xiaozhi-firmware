# XiaoZhi Mac MCP Handoff

> Future Codex note: this file documents the Mac-side MCP services that work with the QDTech XiaoZhi firmware. It is intentionally secret-free. Do not commit endpoint token files, logs containing tokens, or local API keys.

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

- `宇宇`: local music API port `3099`, LaunchAgent `com.tupi.xiaozhi.netease.yuyu`
- `小苍兰`: local music API port `3100`, LaunchAgent `com.tupi.xiaozhi.netease.xiaocanglan`

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
- user-facing name `多多`

Important implementation note:

- Normal `openclaw.ask_dodo` voice answers use a lightweight direct OpenAI-compatible call to Xiaomi `mimo-v2.5` so XiaoZhi receives short answers fast enough for speech.
- Full OpenClaw CLI is kept as a fallback, because repeated full CLI calls were too slow and caused XiaoZhi to report that Dodo was offline.
- "在线吗" style checks can be answered locally and quickly.

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

- `宇宇，问多多在线吗？`
- `宇宇，打开访达。`
- `宇宇，列出桌面文件。`
- `宇宇，把桌面上的测试文件移动到文稿。`
- `宇宇，整理下载文件夹。`
- `确认执行，确认码 123456。`

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

For `宇宇`, keep these intent-routing rules in the XiaoZhi console role description:

- For music requests, actually call tools. Do not only reply "playing".
- If an external music tool returns `audio_url` or `url`, call `self.music.play_url`.
- For Mac/OpenClaw/service status questions, call `openclaw.status` or `openclaw.check_services`.
- For "ask Dodo" style requests, call `openclaw.ask_dodo` and read the returned answer.
- For opening Mac apps, call `openclaw.open_app`. Only open the OpenClaw gateway page when the user explicitly asks for the OpenClaw console.
- For creating folders, call `openclaw.create_folder`.
- For moving or organizing files, call the prepare tool first, read the plan and confirmation code, and only then call `openclaw.confirm_file_action` after user confirmation.

After saving the XiaoZhi backend role/MCP configuration, restart or reconnect the physical board. The console itself warns that new configuration only takes effect after device restart/reconnect.

## 2026-06-30 Verified Experiences

- Both NetEase services can be run at the same time, one for `宇宇` and one for `小苍兰`; separate boards/agents do not conflict when each has its own MCP endpoint and local music API port.
- The Mac-side NetEase services can be left as LaunchAgents for reboot persistence.
- The direct local NetEase API can return a playable MP3 URL even when the board is not playing; this means playback failures must be separated into "Mac did not fetch", "Mac did not call device", and "device did not play".
- A successful point-song chain was observed for `冉春`: `music.netease_play` returned a URL, then the bridge logged `>> tools/call self.music.play_url` with the direct MP3 URL.
- For OpenClaw, direct lightweight Dodo answers are more reliable for voice than repeated full CLI calls.
- File management should be expanded conservatively with prepare/confirm tools rather than broad computer-control access.
