#!/usr/bin/env node
'use strict';

const fs = require('fs');
const dgram = require('dgram');
const http = require('http');
const os = require('os');
const WebSocket = require('ws');
const {
  searchPlayable,
  playPrivateFm,
  searchSongs,
  artistsOf,
  createNeteaseLoginQr,
  checkNeteaseLoginQr,
  getNeteaseLoginStatus,
  withLoginQrContent,
} = require('./netease-mcp');

const pendingRequests = new Map();
let nextRequestId = 1;
const musicApiPort = Number(process.env.MUSIC_API_PORT || 3099);
const lyricUdpPort = Number(process.env.LYRIC_UDP_PORT || 45678);
const lyricDeviceHost = process.env.LYRIC_DEVICE_HOST || '';
const deviceMusicHost = process.env.DEVICE_MUSIC_HOST || lyricDeviceHost || '';
const deviceMusicPort = Number(process.env.DEVICE_MUSIC_PORT || lyricUdpPort);
const lyricUdpSocket = dgram.createSocket('udp4');
let lyricTimers = [];
let privateFmAutoplay = false;
let privateFmTimer = null;
let privateFmGeneration = 0;
let privateFmCurrent = null;
let privateFmStartedAtMs = 0;
let activeWebSocket = null;
let connectionStartedAt = 0;
let lastInboundAt = Date.now();
let restartingForWatchdog = false;

const watchdogEnabled = process.env.MCP_WATCHDOG_ENABLED !== '0';
const watchdogCheckMs = Number(process.env.MCP_WATCHDOG_CHECK_MS || 60000);
const watchdogNoInboundMs = Number(process.env.MCP_WATCHDOG_NO_INBOUND_MS || 180000);
const watchdogMaxAgeMs = Number(process.env.MCP_WATCHDOG_MAX_AGE_MS || 21600000);
const watchdogExitOnStale = process.env.MCP_WATCHDOG_EXIT_ON_STALE !== '0';

function lyricsJsonBytes(value) {
  return value ? Buffer.byteLength(String(value)) : 0;
}

function hasUsableLyrics(value, lyricCount = 0) {
  return Number(lyricCount || 0) > 0 && lyricsJsonBytes(value) > 2;
}

lyricUdpSocket.bind(() => {
  lyricUdpSocket.setBroadcast(true);
});

const endpoint =
  process.env.MCP_ENDPOINT ||
  (fs.existsSync('./current-endpoint.txt') ? fs.readFileSync('./current-endpoint.txt', 'utf8').trim() : '');

if (require.main === module && !endpoint.startsWith('wss://api.xiaozhi.me/mcp/?token=')) {
  console.error('MCP_ENDPOINT is missing or invalid');
  process.exit(1);
}

const tools = [
  {
    name: 'music.netease_play',
    description:
      '搜索网易云音乐并返回板子可播放的 MP3 URL。拿到 success=true/status=pending_device_play_url 后，下一步必须调用设备端 self.music.play_url(title, artist, url)。',
    inputSchema: {
      type: 'object',
      properties: {
        song: { type: 'string', description: '歌曲名，例如 晴天' },
        artist: { type: 'string', description: '歌手名，例如 周杰伦；不知道可留空' },
      },
      required: ['song'],
    },
  },
  {
    name: 'music.netease_search',
    description: '只搜索网易云音乐并返回候选歌曲，不播放。需要播放时请调用 music.netease_play。',
    inputSchema: {
      type: 'object',
      properties: {
        keyword: { type: 'string', description: '搜索关键词，例如 晴天 周杰伦' },
      },
      required: ['keyword'],
    },
  },
  {
    name: 'music.netease_private_fm',
    description:
      '从网易云账号的私人漫游/私人 FM 自动推荐歌曲并连续播放。适合用户说“不知道听什么”“随便放点歌”“私人漫游”。成功后必须调用设备端 self.music.play_url(title, artist, url, lyrics_json)，并在歌曲结束后自动播放下一首。',
    inputSchema: {
      type: 'object',
      properties: {
        continuous: {
          type: 'boolean',
          description: '是否连续播放；默认 true。设为 false 时只播放一首私人 FM。',
        },
        restart: {
          type: 'boolean',
          description: '是否强制重新随机一首；默认 false，连续播放中重复调用不会切歌。',
        },
      },
    },
  },
  {
    name: 'music.netease_private_fm_stop',
    description: '停止网易云私人漫游连续播放，取消自动播放下一首。',
    inputSchema: {
      type: 'object',
      properties: {},
    },
  },
  {
    name: 'music.netease_login_qr',
    description: '生成网易云音乐扫码登录二维码。登录过期、会员歌只能试听时调用；返回二维码图片，用网易云音乐 App 扫码。',
    inputSchema: {
      type: 'object',
      properties: {
        force: { type: 'boolean', description: '是否强制生成新的二维码' },
      },
    },
  },
  {
    name: 'music.netease_login_check',
    description: '检查网易云扫码登录状态；扫码确认成功后会把新的会员 cookie 保存到 NAS。',
    inputSchema: {
      type: 'object',
      properties: {
        key: { type: 'string', description: '二维码 key；通常留空，使用上一次生成的二维码' },
      },
    },
  },
  {
    name: 'music.netease_login_status',
    description: '检查 NAS 上保存的网易云登录态是否仍然有效。',
    inputSchema: {
      type: 'object',
      properties: {},
    },
  },
];

function send(ws, id, result, error) {
  const response = error
    ? { jsonrpc: '2.0', id, error }
    : { jsonrpc: '2.0', id, result };
  ws.send(JSON.stringify(response));
}

function getLanAddresses() {
  return Object.values(os.networkInterfaces())
    .flat()
    .filter((info) => info && info.family === 'IPv4' && !info.internal)
    .map((info) => info.address);
}

function getLyricUdpTargets() {
  const targets = new Set(['255.255.255.255']);
  if (lyricDeviceHost) {
    targets.add(lyricDeviceHost);
  }
  for (const address of getLanAddresses()) {
    const parts = address.split('.');
    if (parts.length === 4) {
      parts[3] = '255';
      targets.add(parts.join('.'));
    }
  }
  return [...targets];
}

function sendJson(response, statusCode, body) {
  const text = JSON.stringify(body);
  response.writeHead(statusCode, {
    'Content-Type': 'application/json; charset=utf-8',
    'Content-Length': Buffer.byteLength(text),
    'Access-Control-Allow-Origin': '*',
  });
  response.end(text);
}

function startMusicApiServer() {
  const server = http.createServer(async (request, response) => {
    try {
      const requestUrl = new URL(request.url, `http://${request.headers.host || 'localhost'}`);
      if (request.method === 'GET' && requestUrl.pathname === '/health') {
        sendJson(response, 200, { ok: true, service: 'xiaozhi-netease-music-api' });
        return;
      }
      if (request.method !== 'GET' || requestUrl.pathname !== '/stream_pcm') {
        sendJson(response, 404, { success: false, message: 'not found' });
        return;
      }

      const song = requestUrl.searchParams.get('song') || requestUrl.searchParams.get('title') || '';
      const artist = requestUrl.searchParams.get('artist') || '';
      console.log('http music api request', JSON.stringify({ song, artist }));
      const result = await searchPlayable(song, artist);
      if (!result.success) {
        sendJson(response, 404, result);
        return;
      }
      sendJson(response, 200, {
        success: true,
        title: result.title,
        artist: result.artist,
        source: result.source,
        song_id: result.song_id,
        audio_url: result.url,
        url: result.url,
        bitrate: result.bitrate,
        duration_ms: result.duration_ms,
      });
    } catch (error) {
      console.error('http music api failed:', error.message || String(error));
      sendJson(response, 500, { success: false, message: error.message || String(error) });
    }
  });

  server.on('error', (error) => {
    console.error('music api listen failed:', error.message || String(error));
  });
  server.listen(musicApiPort, '0.0.0.0', () => {
    const addresses = getLanAddresses().map((address) => `http://${address}:${musicApiPort}`);
    console.log('music api listening', JSON.stringify({
      port: musicApiPort,
      base_urls: addresses.length ? addresses : [`http://127.0.0.1:${musicApiPort}`],
      stream_pcm: '/stream_pcm?song=歌曲名&artist=歌手',
    }));
  });
}

function sendRequest(ws, method, params, timeoutMs = 10000) {
  const id = `netease-${Date.now()}-${nextRequestId++}`;
  const request = { jsonrpc: '2.0', id, method, params };

  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      pendingRequests.delete(id);
      reject(new Error(`${method} timed out`));
    }, timeoutMs);

    pendingRequests.set(id, { resolve, reject, timer });
    ws.send(JSON.stringify(request));
  });
}

function sendFireAndForgetRequest(ws, method, params) {
  const id = `netease-device-${Date.now()}-${nextRequestId++}`;
  ws.send(JSON.stringify({ jsonrpc: '2.0', id, method, params }));
}

async function callLocalDevicePlayUrl(args) {
  if (!deviceMusicHost) {
    return false;
  }
  const payload = Buffer.from(JSON.stringify({
    type: 'play_url',
    title: args.title,
    artist: args.artist,
    url: args.url,
    lyrics_json: args.lyrics_json || '',
  }));
  return new Promise((resolve) => {
    lyricUdpSocket.send(payload, 0, payload.length, deviceMusicPort, deviceMusicHost, (error) => {
      if (error) {
        console.error('<< local device play_url udp failed', error.message || String(error));
        resolve(false);
        return;
      }
	      console.log('<< local device play_url udp', JSON.stringify({
	        host: deviceMusicHost,
	        port: deviceMusicPort,
	        bytes: payload.length,
	        has_lyrics: hasUsableLyrics(args.lyrics_json, args.lyric_count),
	        lyrics_json_bytes: lyricsJsonBytes(args.lyrics_json),
	        ok: true,
	      }));
      resolve(true);
    });
  });
}

function toMcpResult(result) {
  return {
    content: result.content,
    isError: result.isError,
  };
}

function completePendingResponse(message) {
  if (!Object.prototype.hasOwnProperty.call(message, 'id')) {
    return false;
  }
  const pending = pendingRequests.get(message.id);
  if (!pending) {
    return false;
  }

  pendingRequests.delete(message.id);
  clearTimeout(pending.timer);
  if (message.error) {
    pending.reject(new Error(message.error.message || JSON.stringify(message.error)));
  } else {
    pending.resolve(message.result);
  }
  return true;
}

function extractPlayUrlArguments(toolResult) {
  const directArgs = toolResult?.playResult?.play_url_arguments;
	  if (directArgs?.url && /^https?:\/\//.test(String(directArgs.url))) {
	    return {
	      title: String(directArgs.title || toolResult.playResult.title || ''),
	      artist: String(directArgs.artist || toolResult.playResult.artist || ''),
	      url: String(directArgs.url),
	      lyrics_json: String(directArgs.lyrics_json || ''),
	      lyric_count: Number(toolResult.playResult.lyric_count || 0),
	    };
  }

  const text = toolResult.content?.find((item) => item?.type === 'text')?.text || '';
  if (!text) {
    return null;
  }

  const parsed = parseLoggedResult(text);
  if (!parsed?.success) {
    return null;
  }

  const url = parsed.play_url_arguments?.url || parsed.url || parsed.audio_url;
  if (!/^https?:\/\//.test(String(url || ''))) {
    return null;
  }

	  return {
	    title: String(parsed.play_url_arguments?.title || parsed.title || ''),
	    artist: String(parsed.play_url_arguments?.artist || parsed.artist || ''),
	    url,
	    lyrics_json: String(parsed.play_url_arguments?.lyrics_json || parsed.lyrics_json || ''),
	    lyric_count: Number(parsed.lyric_count || 0),
	  };
}

function formatToolResult(result) {
  if (!result?.success || !result?.play_url_arguments) {
    return JSON.stringify(result, null, 2);
  }

	  const lyricCount = Number(result.lyric_count || 0);
	  const lyricBytes = lyricsJsonBytes(result.play_url_arguments.lyrics_json);
	  const hasLyrics = hasUsableLyrics(result.play_url_arguments.lyrics_json, lyricCount);
	  return JSON.stringify({
	    success: true,
	    status: 'pending_device_play_url',
    title: result.title,
    artist: result.artist,
    source: result.source,
    mode: result.mode,
    song_id: result.song_id,
    audio_url: result.audio_url || result.url,
    url: result.url,
    bitrate: result.bitrate,
    duration_ms: result.duration_ms,
	    lyric_count: lyricCount,
	    has_lyrics: hasLyrics,
	    lyrics_json_bytes: lyricBytes,
	    lyrics_status: result.lyrics_status,
	    lyrics_reason: result.lyrics_reason,
	    lyrics_source_song_id: result.lyrics_source_song_id,
	    lyrics_source_title: result.lyrics_source_title,
	    lyrics_source_artist: result.lyrics_source_artist,
	    next_tool: 'self.music.play_url',
	    must_call_device_tool: true,
	    message: hasLyrics
	      ? '已经找到歌曲、歌词和新的 MP3 直链。现在必须继续调用设备端工具 self.music.play_url(title, artist, url, lyrics_json)，不要只回复文字。'
	      : `已经找到歌曲和新的 MP3 直链，但本次没有可用歌词：${result.lyrics_reason || '网易云无歌词/取歌词失败/解析失败'}。现在仍然必须调用设备端工具 self.music.play_url(title, artist, url, lyrics_json)。`,
    play_url_arguments: {
      title: result.play_url_arguments.title,
      artist: result.play_url_arguments.artist,
      url: result.play_url_arguments.url,
      lyrics_json: result.play_url_arguments.lyrics_json || '',
    },
  }, null, 2);
}

function parseLoggedResult(text) {
  try {
    return JSON.parse(text);
  } catch {
    const marker = 'FULL_RESULT_JSON\n';
    const markerIndex = text.indexOf(marker);
    if (markerIndex === -1) {
      return null;
    }
    try {
      return JSON.parse(text.slice(markerIndex + marker.length));
    } catch {
      return null;
    }
  }
}

async function callDevicePlayUrl(ws, toolResult) {
  const args = extractPlayUrlArguments(toolResult);
  if (!args) {
    return;
  }

  console.log('>> tools/call self.music.play_url', JSON.stringify({
    title: args.title,
    artist: args.artist,
	    has_url: true,
	    has_lyrics: hasUsableLyrics(args.lyrics_json, args.lyric_count),
	    lyrics_json_bytes: lyricsJsonBytes(args.lyrics_json),
	    url: args.url,
	  }));

	  const deviceArgs = {
	    title: args.title,
	    artist: args.artist,
	    url: args.url,
	    lyrics_json: args.lyrics_json || '',
	  };
	  sendFireAndForgetRequest(ws, 'tools/call', {
	    name: 'self.music.play_url',
	    arguments: deviceArgs,
	  });
	  console.log('<< self.music.play_url sent');
	  await callLocalDevicePlayUrl(args);
}

function extractLoginQrArguments(toolResult) {
  const text = toolResult?.content?.find((item) => item?.type === 'text')?.text || '';
  const parsed = parseLoggedResult(text);
  const qr = parsed?.login_qr || parsed;
  const content = String(qr?.qr_url || parsed?.qr_url || '').trim();
  if (!content) {
    return null;
  }

  return {
    content,
    title: '网易云扫码登录',
    hint: '用网易云音乐 App 扫码；点屏幕关闭',
  };
}

async function callDeviceQrCode(ws, toolResult) {
  const args = extractLoginQrArguments(toolResult);
  if (!args || ws.readyState !== WebSocket.OPEN) {
    return;
  }

  console.log('>> tools/call self.display.qrcode', JSON.stringify({
    title: args.title,
    content_len: args.content.length,
  }));
  sendFireAndForgetRequest(ws, 'tools/call', {
    name: 'self.display.qrcode',
    arguments: args,
  });
  console.log('<< self.display.qrcode sent');
}

function stopLyricSync() {
  for (const timer of lyricTimers) {
    clearTimeout(timer);
  }
  lyricTimers = [];
}

function stopPrivateFmAutoplay(reason = '') {
  privateFmAutoplay = false;
  privateFmGeneration += 1;
  privateFmCurrent = null;
  privateFmStartedAtMs = 0;
  if (privateFmTimer) {
    clearTimeout(privateFmTimer);
    privateFmTimer = null;
  }
  if (reason) {
    console.log('private fm autoplay stopped', JSON.stringify({ reason }));
  }
}

function setPrivateFmCurrent(playResult) {
  if (!playResult?.success) {
    return;
  }
  privateFmCurrent = playResult;
  privateFmStartedAtMs = Date.now();
}

function privateFmAlreadyPlayingResult() {
  return {
    success: true,
    source: 'netease',
    status: 'private_fm_already_playing',
    mode: 'private_fm',
    title: privateFmCurrent?.title || '',
    artist: privateFmCurrent?.artist || '',
    song_id: privateFmCurrent?.song_id || '',
    started_at_ms: privateFmStartedAtMs,
    message: '网易云私人漫游已经在连续播放中，不重复切歌。',
  };
}

function privateFmDelayMs(playResult) {
  const durationMs = Number(playResult?.duration_ms || 0);
  if (durationMs > 30000) {
    return Math.max(30000, durationMs + 2500);
  }
  return 240000;
}

function toolResultFromPlayResult(playResult) {
  return {
    content: [{ type: 'text', text: formatToolResult(playResult) }],
    isError: !playResult?.success,
    playResult,
  };
}

function logPrivateFmResult(result, prefix = 'private fm song selected') {
  if (!result?.success) {
    console.log('private fm unavailable', JSON.stringify({
      code: result?.code || '',
      message: result?.message || '',
    }));
    return;
  }
  const lyricBytes = lyricsJsonBytes(result.play_url_arguments?.lyrics_json);
  const hasLyrics = hasUsableLyrics(result.play_url_arguments?.lyrics_json, result.lyric_count);
  console.log(prefix, JSON.stringify({
    song_id: result.song_id,
    title: result.title,
    artist: result.artist,
    duration_ms: result.duration_ms || 0,
    lyric_count: result.lyric_count || 0,
    has_lyrics: hasLyrics,
    lyrics_json_bytes: lyricBytes,
    lyrics_status: result.lyrics_status,
    lyrics_source_song_id: result.lyrics_source_song_id,
    lyrics_source_title: result.lyrics_source_title,
  }));
  if (!hasLyrics) {
    console.log('lyrics unavailable', JSON.stringify({
      song_id: result.song_id,
      title: result.title,
      reason: result.lyrics_reason || '网易云无歌词/取歌词失败/解析失败',
      lyrics_json_bytes: lyricBytes,
    }));
  }
}

function schedulePrivateFmNext(ws, playResult) {
  if (!privateFmAutoplay || !playResult?.success) {
    return;
  }
  const generation = privateFmGeneration;
  const delayMs = privateFmDelayMs(playResult);
  if (privateFmTimer) {
    clearTimeout(privateFmTimer);
  }
  privateFmTimer = setTimeout(async () => {
    privateFmTimer = null;
    if (!privateFmAutoplay || generation !== privateFmGeneration || ws.readyState !== WebSocket.OPEN) {
      return;
    }
    try {
      console.log('private fm autoplay next fetching');
      stopLyricSync();
      const next = await playPrivateFm();
      logPrivateFmResult(next, 'private fm autoplay next selected');
      if (!next.success) {
        schedulePrivateFmRetry(ws, next);
        return;
      }
      setPrivateFmCurrent(next);
      const toolResult = toolResultFromPlayResult(next);
      await callDevicePlayUrl(ws, toolResult);
      setTimeout(() => startLyricSync(ws, next), 1000);
      schedulePrivateFmNext(ws, next);
    } catch (error) {
      console.error('private fm autoplay next failed', error.message || String(error));
      schedulePrivateFmRetry(ws, { message: error.message || String(error) });
    }
  }, delayMs);
  console.log('private fm autoplay scheduled', JSON.stringify({
    title: playResult.title,
    artist: playResult.artist,
    delay_ms: delayMs,
  }));
}

function schedulePrivateFmRetry(ws, result) {
  if (!privateFmAutoplay) {
    return;
  }
  const generation = privateFmGeneration;
  if (privateFmTimer) {
    clearTimeout(privateFmTimer);
  }
  privateFmTimer = setTimeout(async () => {
    privateFmTimer = null;
    if (!privateFmAutoplay || generation !== privateFmGeneration || ws.readyState !== WebSocket.OPEN) {
      return;
    }
    try {
      const next = await playPrivateFm();
      logPrivateFmResult(next, 'private fm autoplay retry selected');
      if (!next.success) {
        schedulePrivateFmRetry(ws, next);
        return;
      }
      setPrivateFmCurrent(next);
      const toolResult = toolResultFromPlayResult(next);
      await callDevicePlayUrl(ws, toolResult);
      setTimeout(() => startLyricSync(ws, next), 1000);
      schedulePrivateFmNext(ws, next);
    } catch (error) {
      console.error('private fm autoplay retry failed', error.message || String(error));
      schedulePrivateFmRetry(ws, { message: error.message || String(error) });
    }
  }, 30000);
  console.log('private fm autoplay retry scheduled', JSON.stringify({
    reason: result?.message || result?.code || 'unknown',
    delay_ms: 30000,
  }));
}

function safeLyricLine(line) {
  return String(line || '')
    .replace(/\s+/g, ' ')
    .trim()
    .slice(0, 80);
}

function sendLyricLine(ws, title, artist, line) {
  const cleanLine = safeLyricLine(line);
  if (!cleanLine) {
    return;
  }
  const packet = Buffer.from(JSON.stringify({
    title,
    artist,
    line: cleanLine,
  }));
  for (const target of getLyricUdpTargets()) {
    lyricUdpSocket.send(packet, lyricUdpPort, target, (error) => {
      if (error) {
        console.error('lyric udp send failed', target, error.message || String(error));
      }
    });
  }
  if (ws.readyState === WebSocket.OPEN) {
    sendFireAndForgetRequest(ws, 'tools/call', {
      name: 'self.music.show_lyric',
      arguments: {
        title,
        artist,
        line: cleanLine,
      },
    });
  }
  console.log('>> lyric udp/self.music.show_lyric', JSON.stringify({
    title,
    artist,
    line: cleanLine,
    udp_port: lyricUdpPort,
  }));
}

function startLyricSync(ws, playResult) {
  stopLyricSync();
  const lines = Array.isArray(playResult?.lyric_lines) ? playResult.lyric_lines : [];
  if (!lines.length) {
    console.log('lyric sync skipped: no lyrics', JSON.stringify({
      title: playResult?.title,
      artist: playResult?.artist,
    }));
    return;
  }

  const title = String(playResult.title || '');
  const artist = String(playResult.artist || '');
  sendLyricLine(ws, title, artist, `♪ ${title}${artist ? ` - ${artist}` : ''}`);

  const startDelayMs = 800;
  let lastText = '';
  for (const item of lines) {
    const cleanLine = safeLyricLine(item.text);
    const timeMs = Math.max(0, Number(item.time_ms) || 0);
    if (!cleanLine || cleanLine === lastText) {
      continue;
    }
    lastText = cleanLine;
    const timer = setTimeout(() => {
      sendLyricLine(ws, title, artist, cleanLine);
    }, startDelayMs + timeMs);
    lyricTimers.push(timer);
  }

  console.log('lyric sync started', JSON.stringify({
    title,
    artist,
    lines: lyricTimers.length,
  }));
}

async function handleTool(name, args) {
  if (name === 'music.netease_play') {
    stopPrivateFmAutoplay('manual_netease_play');
    stopLyricSync();
    const result = await searchPlayable(String(args.song || args.title || ''), String(args.artist || ''));
    if (result.success) {
      const lyricBytes = lyricsJsonBytes(result.play_url_arguments?.lyrics_json);
      const hasLyrics = hasUsableLyrics(result.play_url_arguments?.lyrics_json, result.lyric_count);
      console.log('lyrics fetched', JSON.stringify({
        song_id: result.song_id,
        title: result.title,
        artist: result.artist,
        lyric_count: result.lyric_count || 0,
        has_lyrics: hasLyrics,
        lyrics_json_bytes: lyricBytes,
        lyrics_status: result.lyrics_status,
        lyrics_source_song_id: result.lyrics_source_song_id,
        lyrics_source_title: result.lyrics_source_title,
      }));
      if (!hasLyrics) {
        console.log('lyrics unavailable', JSON.stringify({
          song_id: result.song_id,
          title: result.title,
          reason: result.lyrics_reason || '网易云无歌词/取歌词失败/解析失败',
          lyrics_json_bytes: lyricBytes,
        }));
      }
    }
    return {
      content: result.login_qr ? withLoginQrContent(result) : [{ type: 'text', text: formatToolResult(result) }],
      isError: !result.success,
      playResult: result,
    };
  }

  if (name === 'music.netease_private_fm') {
    if (privateFmAutoplay && privateFmCurrent?.success && args.restart !== true) {
      const result = privateFmAlreadyPlayingResult();
      console.log('private fm duplicate request ignored', JSON.stringify({
        title: result.title,
        artist: result.artist,
        song_id: result.song_id,
        started_at_ms: result.started_at_ms,
      }));
      return {
        content: [{ type: 'text', text: JSON.stringify(result, null, 2) }],
        isError: false,
      };
    }

    stopLyricSync();
    privateFmAutoplay = args.continuous !== false;
    privateFmGeneration += 1;
    const result = await playPrivateFm();
    logPrivateFmResult(result);
    if (result.success) {
      setPrivateFmCurrent(result);
    }
    return {
      content: result.login_qr ? withLoginQrContent(result) : [{ type: 'text', text: formatToolResult(result) }],
      isError: !result.success,
      playResult: result,
    };
  }

  if (name === 'music.netease_private_fm_stop') {
    stopPrivateFmAutoplay('tool_call');
    stopLyricSync();
    const result = {
      success: true,
      source: 'netease',
      status: 'private_fm_stopped',
      message: '已停止网易云私人漫游连续播放。',
    };
    return {
      content: [{ type: 'text', text: JSON.stringify(result, null, 2) }],
      isError: false,
    };
  }

  if (name === 'music.netease_login_qr') {
    const result = await createNeteaseLoginQr({ force: Boolean(args.force) });
    return {
      content: withLoginQrContent(result),
      isError: !result.success,
    };
  }

  if (name === 'music.netease_login_check') {
    const result = await checkNeteaseLoginQr(String(args.key || ''));
    return {
      content: [{ type: 'text', text: JSON.stringify(result, null, 2) }],
      isError: !result.success,
    };
  }

  if (name === 'music.netease_login_status') {
    const result = await getNeteaseLoginStatus();
    return {
      content: [{ type: 'text', text: JSON.stringify(result, null, 2) }],
      isError: !result.success,
    };
  }

  if (name === 'music.netease_search') {
    const songs = (await searchSongs(String(args.keyword || ''), 8)).map((song) => ({
      id: song.id,
      title: song.name,
      artist: artistsOf(song).join(','),
      album: song.al?.name || song.album?.name || '',
    }));
    const result = {
      success: songs.length > 0,
      source: 'netease',
      songs,
    };
    return {
      content: [{ type: 'text', text: JSON.stringify(result, null, 2) }],
      isError: false,
    };
  }

  return {
    content: [{ type: 'text', text: JSON.stringify({ success: false, message: 'unknown tool' }) }],
    isError: true,
  };
}

function connect() {
  console.log('connecting to xiaozhi mcp...');
  const ws = new WebSocket(endpoint, 'mcp');
  activeWebSocket = ws;
  connectionStartedAt = Date.now();
  lastInboundAt = Date.now();

  ws.on('open', () => {
    console.log('connected to xiaozhi mcp');
  });

  ws.on('message', async (data) => {
    lastInboundAt = Date.now();
    const message = JSON.parse(data.toString('utf8'));
    console.log('<<', message.method || `response:${message.id}`);

    if (message.method === 'initialize') {
      send(ws, message.id, {
        protocolVersion: message.params?.protocolVersion || '2024-11-05',
        capabilities: { tools: {} },
        serverInfo: { name: 'xiaozhi-netease-music', version: '1.0.0' },
      });
      return;
    }

    if (message.method === 'tools/list') {
      send(ws, message.id, { tools });
      return;
    }

    if (message.method === 'ping') {
      send(ws, message.id, {});
      return;
    }

    if (message.method === 'tools/call') {
      try {
        console.log('tool call', message.params?.name, JSON.stringify(message.params?.arguments || {}));
        const result = await handleTool(message.params?.name, message.params?.arguments || {});
        const text = result.content?.[0]?.text || '';
        const parsed = parseLoggedResult(text);
        if (parsed) {
          console.log('tool result', JSON.stringify({
            success: parsed.success,
            status: parsed.status,
            title: parsed.title,
            artist: parsed.artist,
            source: parsed.source,
            has_url: Boolean(parsed.url || parsed.audio_url),
            lyric_count: parsed.lyric_count || 0,
	            song_id: parsed.song_id,
	            has_lyrics: hasUsableLyrics(
	              parsed.play_url_arguments?.lyrics_json || '',
	              parsed.lyric_count || 0
	            ),
	            lyrics_json_bytes: parsed.lyrics_json_bytes || lyricsJsonBytes(parsed.play_url_arguments?.lyrics_json || ''),
	            lyrics_status: parsed.lyrics_status,
	            lyrics_source_song_id: parsed.lyrics_source_song_id,
	            next_tool: parsed.next_tool,
            code: parsed.code,
            message: parsed.message,
          }));
        } else {
          console.log('tool result text', text.slice(0, 200));
        }
        send(ws, message.id, toMcpResult(result));
        if (message.params?.name === 'music.netease_play' || message.params?.name === 'music.netease_private_fm') {
          setTimeout(() => {
            callDevicePlayUrl(ws, result);
            callDeviceQrCode(ws, result);
            if (result.playResult?.success) {
              setTimeout(() => startLyricSync(ws, result.playResult), 1000);
              if (message.params?.name === 'music.netease_private_fm') {
                schedulePrivateFmNext(ws, result.playResult);
              }
            }
          }, 100);
        } else if (message.params?.name === 'music.netease_login_qr') {
          setTimeout(() => {
            callDeviceQrCode(ws, result);
          }, 100);
        }
      } catch (error) {
        send(ws, message.id, null, { code: -32000, message: error.message || String(error) });
      }
      return;
    }

    if (completePendingResponse(message)) {
      return;
    }
  });

  ws.on('close', (code, reason) => {
    if (activeWebSocket === ws) {
      activeWebSocket = null;
    }
    if (restartingForWatchdog) {
      return;
    }
    console.error(`xiaozhi mcp disconnected code=${code} reason=${reason ? reason.toString() : ''}, reconnecting in 3s`);
    setTimeout(connect, 3000);
  });

  ws.on('error', (error) => {
    console.error('xiaozhi mcp websocket error:', error.message);
  });
}

function restartByWatchdog(reason, details) {
  if (restartingForWatchdog) {
    return;
  }
  restartingForWatchdog = true;
  console.error('mcp watchdog restart', JSON.stringify({ reason, ...details }));
  stopPrivateFmAutoplay(`watchdog:${reason}`);
  stopLyricSync();

  if (watchdogExitOnStale) {
    setTimeout(() => process.exit(42), 250);
    if (activeWebSocket?.readyState === WebSocket.OPEN) {
      activeWebSocket.close(4000, `watchdog:${reason}`);
    }
    return;
  }

  const ws = activeWebSocket;
  activeWebSocket = null;
  if (ws?.readyState === WebSocket.OPEN || ws?.readyState === WebSocket.CONNECTING) {
    ws.terminate();
  }
  setTimeout(() => {
    restartingForWatchdog = false;
    connect();
  }, 1000);
}

function startWatchdog() {
  if (!watchdogEnabled) {
    console.log('mcp watchdog disabled');
    return;
  }

  console.log('mcp watchdog enabled', JSON.stringify({
    check_ms: watchdogCheckMs,
    no_inbound_ms: watchdogNoInboundMs,
    max_age_ms: watchdogMaxAgeMs,
    exit_on_stale: watchdogExitOnStale,
  }));

  setInterval(() => {
    const now = Date.now();
    const readyState = activeWebSocket ? activeWebSocket.readyState : -1;
    const noInboundForMs = now - lastInboundAt;
    const ageMs = connectionStartedAt ? now - connectionStartedAt : 0;

    if (!activeWebSocket || readyState === WebSocket.CLOSED || readyState === WebSocket.CLOSING) {
      restartByWatchdog('socket_not_open', { ready_state: readyState });
      return;
    }

    if (noInboundForMs > watchdogNoInboundMs) {
      restartByWatchdog('no_inbound_messages', {
        ready_state: readyState,
        no_inbound_for_ms: noInboundForMs,
      });
      return;
    }

    if (watchdogMaxAgeMs > 0 && ageMs > watchdogMaxAgeMs) {
      restartByWatchdog('scheduled_refresh', {
        ready_state: readyState,
        age_ms: ageMs,
      });
    }
  }, watchdogCheckMs);
}

if (require.main === module) {
  startMusicApiServer();
  connect();
  startWatchdog();

  setInterval(() => {
    console.log('heartbeat');
  }, 30000);
}

module.exports = {
  callDeviceQrCode,
  callDevicePlayUrl,
  completePendingResponse,
  extractLoginQrArguments,
  extractPlayUrlArguments,
  handleTool,
  sendFireAndForgetRequest,
  sendRequest,
};
