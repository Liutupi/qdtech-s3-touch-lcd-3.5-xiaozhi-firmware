# 小智网易云 MCP 服务

这个目录是长期保留目录，不依赖“论文中期考核”文件夹。

## 智能体

- 宇宇：`current-endpoint.txt`，本地音乐 API 端口 `3099`
- 小苍兰：`current-endpoint-xiaocanglan.txt`，本地音乐 API 端口 `3100`

## 开机自启动

macOS 用户级启动项：

- `~/Library/LaunchAgents/com.tupi.xiaozhi.netease.yuyu.plist`
- `~/Library/LaunchAgents/com.tupi.xiaozhi.netease.xiaocanglan.plist`

电脑重启并登录当前用户后，两个网易云 MCP 桥接服务会自动启动。

## 日志

- 宇宇：`netease-mcp-yuyu.log`
- 小苍兰：`netease-mcp-xiaocanglan.log`

正常日志会出现：

- `music api listening`
- `connected to xiaozhi mcp`
- `tools/list`

## 播放链路要求

网易云 MCP 只负责搜索歌曲、解析歌曲和拿 direct HTTP/HTTPS 音频 URL。

当 `music.netease_play` 或其他搜歌逻辑拿到新歌曲 URL 后，必须继续调用对应设备端工具：

```json
self.music.play_url({
  "title": "歌名",
  "artist": "歌手",
  "url": "直链MP3地址",
  "lyrics_json": "[{\"time_ms\":0,\"text\":\"第一句\"}]"
})
```

关键点：

- 每次点歌都要调用 `self.music.play_url`。
- 第二首、第三首也必须调用，不能只播第一首缓存。
- 不能只让服务端内部 `play_music` 返回文字。
- 不要对外暴露 MCP 工具名 `play_music`。小智内部已有同名工具，曾导致板子日志报 `Duplicate tool names: play_music`，点歌链路中断。
- 如果工具名冲突，保留服务端网易云工具名 `music.netease_play`，把设备播放工具明确写成 `self.music.play_url`。
- 日志验证时，连续点两首歌，第二首也必须出现 `tools/call self.music.play_url`，参数里要有新的 `url`。
- 调用 `self.music.play_url` 成功后，不要再朗读“播放”“我先退下了”等收尾句，避免 TTS 抢占音频焦点导致音乐从头重播。
- QDTech 固件 `v1.7.66` 起，设备端对刚启动的 direct MP3 URL 有 4 秒 speaking 保护窗口，可兜底避免短收尾 TTS 打断音乐开头。
- QDTech 固件 `v1.7.67` 已烧录并硬件验证：连续点两首歌时，第二首也能收到 `self.music.play_url` 和新的直链 URL；自动收尾语音会被固件忽略音频焦点并中止，不再导致音乐从头重播。
- QDTech 固件 `v1.7.73` 已烧录并硬件验证：修复 `play_music` 重名后的链路，连续播放《道别是一件难事》和《绿光》均出现新的 `self.music.play_url`，板子 `stream open ... status=200` 并持续播放。
- QDTech 固件 `v1.7.73` 的 `self.music.play_url` 还支持 `lyrics_json`。网易云 MCP 已把 LRC 歌词转成 JSON 字符串数组，例如 `[{"time_ms":17290,"text":"遇见了一个传奇 却如此熟悉"}]`，并随播放 URL 一起发给板子。
- 歌词调试时看两条日志：`lyrics fetched ... lyric_count=...` 和 `>> tools/call self.music.play_url ... has_lyrics:true ... lyrics_json_bytes:...`。
- 电脑桥接脚本已把设备播放工具等待时间放宽到 30 秒。如果日志出现 `self.music.play_url no result yet; device call was sent`，表示调用已经发到小智云端；继续以板子串口 `tools/call self.music.play_url`、`stream open ... status=200` 和连续 `playing ... frames=` 作为播放是否成功的准绳。

## 私人漫游

已新增 MCP 工具 `music.netease_private_fm`，用于网易云账号的私人漫游/私人 FM。

适合用户说：

- “不知道听什么，放私人漫游”
- “随便放点歌”
- “打开网易云私人 FM”

链路要求和普通点歌一样：`music.netease_private_fm` 取到推荐歌曲、直链 URL 和歌词后，桥接服务必须继续调用设备端 `self.music.play_url(title, artist, url, lyrics_json)`，不能只返回文字。默认会连续播放：桥接服务根据当前歌曲 `duration_ms` 定时获取下一首私人 FM，并再次调用 `self.music.play_url`。

停止连续播放时调用 `music.netease_private_fm_stop`。如果用户改为指定歌名点歌，`music.netease_play` 会自动停止私人漫游连续播放，避免后台下一首打断指定歌曲。

连续播放期间，如果模型重复调用 `music.netease_private_fm`，桥接服务会返回 `private_fm_already_playing`，不会再次推送 `self.music.play_url` 抢播当前歌曲；需要强制换一首时才传 `restart:true`。私人漫游默认使用 `NETEASE_PRIVATE_FM_LEVELS=standard,higher,exhigh`，优先拿更适合 ESP32 稳定播放的直链，普通点歌仍使用 `NETEASE_MUSIC_LEVELS`。

正常日志会出现：

- `private fm song selected ...`
- `private fm autoplay scheduled ...`
- `private fm autoplay next selected ...`
- `>> tools/call self.music.play_url ...`

私人漫游依赖网易云登录 cookie。如果登录态过期，会走现有的 `music.netease_login_qr` 扫码刷新流程。

## 手动检查

查看自启动状态：

```bash
launchctl list | grep 'com.tupi.xiaozhi.netease'
```

查看端口：

```bash
lsof -nP -iTCP:3099 -sTCP:LISTEN
lsof -nP -iTCP:3100 -sTCP:LISTEN
```

## 注意

不要把这个目录删掉。里面包含两个智能体的 MCP 接入点 token。
