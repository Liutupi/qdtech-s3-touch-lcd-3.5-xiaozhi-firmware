const fs = require('fs');
const path = require('path');
const api = require('NeteaseCloudMusicApi');

const cookieFile = process.env.NETEASE_COOKIE_FILE ||
  path.join(__dirname, 'netease-cookie.txt');
const loginStateFile = process.env.NETEASE_LOGIN_STATE_FILE ||
  path.join(__dirname, 'netease-login-state.json');
const preferredLevels = (process.env.NETEASE_MUSIC_LEVELS ||
  'exhigh,higher,standard')
  .split(',')
  .map((level) => level.trim())
  .filter(Boolean);
const privateFmPreferredLevels = (process.env.NETEASE_PRIVATE_FM_LEVELS ||
  'standard,higher,exhigh')
  .split(',')
  .map((level) => level.trim())
  .filter(Boolean);
let cachedCookie = null;
let cachedCookieMtimeMs = 0;
let activeLoginState = null;
const recentPrivateFmSongIds = [];

function text(value) {
  return value == null ? '' : String(value);
}

function normalize(value) {
  return text(value).toLowerCase().replace(/\s+/g, '');
}

function readNeteaseCookie() {
  const envCookie = text(process.env.NETEASE_COOKIE).trim();
  if (envCookie) {
    return envCookie;
  }

  try {
    const stat = fs.statSync(cookieFile);
    if (cachedCookie !== null && cachedCookieMtimeMs === stat.mtimeMs) {
      return cachedCookie;
    }
    cachedCookie = fs.readFileSync(cookieFile, 'utf8').trim();
    cachedCookieMtimeMs = stat.mtimeMs;
    return cachedCookie;
  } catch {
    cachedCookie = '';
    cachedCookieMtimeMs = 0;
    return '';
  }
}

function hasNeteaseCookie() {
  return Boolean(readNeteaseCookie());
}

function writeNeteaseCookie(cookie) {
  fs.writeFileSync(cookieFile, text(cookie).trim(), { mode: 0o600 });
  cachedCookie = text(cookie).trim();
  try {
    cachedCookieMtimeMs = fs.statSync(cookieFile).mtimeMs;
  } catch {
    cachedCookieMtimeMs = 0;
  }
}

function apiParams(extra = {}) {
  const cookie = readNeteaseCookie();
  return cookie
    ? { cookie, os: 'pc', ...extra }
    : extra;
}

function summarizeUrlData(data, level) {
  return {
    level,
    has_url: Boolean(data?.url),
    type: data?.type || '',
    code: data?.code,
    free_trial_info: Boolean(data?.freeTrialInfo),
    fee: data?.fee,
    br: data?.br,
    time: data?.time,
  };
}

function saveLoginState(state) {
  activeLoginState = state;
  fs.writeFileSync(loginStateFile, JSON.stringify(state, null, 2), { mode: 0o600 });
}

function readLoginState() {
  if (activeLoginState) {
    return activeLoginState;
  }
  try {
    activeLoginState = JSON.parse(fs.readFileSync(loginStateFile, 'utf8'));
    return activeLoginState;
  } catch {
    return null;
  }
}

function withLoginQrContent(result) {
  const content = [{ type: 'text', text: JSON.stringify(result, null, 2) }];
  const image = result.qr_img_base64 || result.login_qr?.qr_img_base64;
  if (image) {
    content.push({ type: 'image', data: image, mimeType: 'image/png' });
  }
  return content;
}

function loginQrResponse(qr, message = '网易云登录已过期，请用网易云音乐 App 扫码登录。') {
  return {
    success: false,
    code: 'netease_login_required',
    status: 'need_netease_login',
    source: 'netease',
    message,
    next_tool: 'music.netease_login_check',
    login_qr: qr,
  };
}

async function createNeteaseLoginQr(options = {}) {
  const now = Date.now();
  const existing = readLoginState();
  if (!options.force && existing?.key && existing?.qr_img_base64 && Number(existing.expires_at_ms || 0) > now + 30000) {
    return {
      success: true,
      status: 'waiting_scan',
      source: 'netease',
      message: '请用网易云音乐 App 扫描二维码，并在手机上确认登录。',
      ...existing,
    };
  }

  const keyResult = await api.login_qr_key({ timestamp: now });
  const key = keyResult.body?.data?.unikey;
  if (!key) {
    return {
      success: false,
      code: 'qr_key_failed',
      source: 'netease',
      message: '网易云没有返回二维码登录 key，请稍后再试。',
    };
  }

  const qrResult = await api.login_qr_create({ key, qrimg: true, timestamp: Date.now() });
  const qrImg = text(qrResult.body?.data?.qrimg);
  const qrBase64 = qrImg.replace(/^data:image\/png;base64,/, '');
  const state = {
    key,
    qr_url: qrResult.body?.data?.qrurl || '',
    qr_img_base64: qrBase64,
    qr_img_data_uri: qrBase64 ? `data:image/png;base64,${qrBase64}` : '',
    expires_at_ms: now + 170000,
    created_at_ms: now,
  };
  saveLoginState(state);
  return {
    success: true,
    status: 'waiting_scan',
    source: 'netease',
    message: '请用网易云音乐 App 扫描二维码，并在手机上确认登录。',
    ...state,
  };
}

async function checkNeteaseLoginQr(key = '') {
  const state = readLoginState();
  const loginKey = text(key || state?.key).trim();
  if (!loginKey) {
    return {
      success: false,
      code: 'no_login_qr',
      source: 'netease',
      message: '当前没有待扫码的网易云登录二维码，请先调用 music.netease_login_qr。',
    };
  }

  const result = await api.login_qr_check({ key: loginKey, timestamp: Date.now() });
  const body = result.body || {};
  if (body.code === 803 && body.cookie) {
    writeNeteaseCookie(body.cookie);
    activeLoginState = null;
    try {
      fs.unlinkSync(loginStateFile);
    } catch {
      // The QR state may already be gone.
    }
    return {
      success: true,
      status: 'logged_in',
      source: 'netease',
      message: '网易云会员登录成功，后续点歌会使用新的登录态。',
    };
  }

  if (body.code === 800) {
    return {
      success: false,
      code: 'qr_expired',
      status: 'expired',
      source: 'netease',
      message: '网易云登录二维码已过期，请重新生成二维码。',
    };
  }

  return {
    success: false,
    code: body.code === 802 ? 'waiting_confirm' : 'waiting_scan',
    status: body.code === 802 ? 'waiting_confirm' : 'waiting_scan',
    source: 'netease',
    message: body.message || (body.code === 802 ? '已扫码，请在手机上确认登录。' : '等待扫码。'),
  };
}

async function getNeteaseLoginStatus() {
  if (!hasNeteaseCookie()) {
    return {
      success: false,
      status: 'missing_cookie',
      source: 'netease',
      message: 'NAS 上没有网易云登录态，需要扫码登录。',
    };
  }

  try {
    const result = await api.login_status(apiParams({ timestamp: Date.now() }));
    const profile = result.body?.data?.profile || result.body?.profile;
    return {
      success: Boolean(profile),
      status: profile ? 'logged_in' : 'cookie_invalid',
      source: 'netease',
      nickname: profile?.nickname || '',
      user_id: profile?.userId || profile?.user_id || '',
      message: profile ? '网易云登录态有效。' : '网易云登录态可能已过期，需要重新扫码。',
    };
  } catch (error) {
    return {
      success: false,
      status: 'check_failed',
      source: 'netease',
      message: `网易云登录态检查失败：${error.message || String(error)}`,
    };
  }
}

function artistsOf(song) {
  return (song.ar || song.artists || []).map((artist) => artist.name).filter(Boolean);
}

async function isPlayableAudioUrl(url) {
  if (!/^https?:\/\//.test(url)) {
    return false;
  }
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 5000);
  try {
    const response = await fetch(url, {
      method: 'GET',
      headers: {
        Range: 'bytes=0-0',
        'User-Agent': 'ESP32-Music-Player/1.0',
      },
      redirect: 'follow',
      signal: controller.signal,
    });
    const contentType = response.headers.get('content-type') || '';
    return response.ok && (contentType.startsWith('audio/') || contentType.includes('octet-stream'));
  } catch {
    return false;
  } finally {
    clearTimeout(timer);
  }
}

function scoreSong(song, songName, artistName) {
  const wantedSong = normalize(songName);
  const wantedArtist = normalize(artistName);
  const title = normalize(song.name);
  const artists = normalize(artistsOf(song).join(','));
  let score = 0;

  if (title === wantedSong) score += 100;
  else if (title.includes(wantedSong) || wantedSong.includes(title)) score += 50;

  if (wantedArtist) {
    if (artists.includes(wantedArtist)) score += 80;
    else if (wantedArtist.includes(artists)) score += 20;
  }

  return score;
}

async function searchSongs(keyword, limit = 8) {
  const search = await api.cloudsearch({
    ...apiParams(),
    keywords: keyword,
    type: 1,
    limit,
  });
  return search.body?.result?.songs || [];
}

function parseLrcTime(value) {
  const match = String(value || '').match(/^(\d+):(\d+(?:\.\d+)?)$/);
  if (!match) {
    return null;
  }
  return Math.round((Number(match[1]) * 60 + Number(match[2])) * 1000);
}

function parseClassicLrc(lyric) {
  const lines = [];
  for (const rawLine of text(lyric).split(/\r?\n/)) {
    const matches = [...rawLine.matchAll(/\[(\d+:\d+(?:\.\d+)?)\]/g)];
    const lyricText = rawLine.replace(/\[[^\]]+\]/g, '').trim();
    if (!lyricText) {
      continue;
    }
    for (const match of matches) {
      const time_ms = parseLrcTime(match[1]);
      if (time_ms != null) {
        lines.push({ time_ms, text: lyricText });
      }
    }
  }
  return lines.sort((a, b) => a.time_ms - b.time_ms);
}

function parseJsonLyricLines(lyric) {
  const lines = [];
  for (const rawLine of text(lyric).split(/\r?\n/)) {
    const trimmed = rawLine.trim();
    if (!trimmed.startsWith('{')) {
      continue;
    }
    try {
      const parsed = JSON.parse(trimmed);
      const lyricText = (parsed.c || [])
        .map((part) => text(part.tx))
        .join('')
        .trim();
      if (Number.isFinite(parsed.t) && lyricText) {
        lines.push({ time_ms: Math.max(0, Number(parsed.t)), text: lyricText });
      }
    } catch {
      // Ignore non-JSON lyric metadata lines.
    }
  }
  return lines.sort((a, b) => a.time_ms - b.time_ms);
}

function simplifyLyricLines(lines) {
  const cleaned = [];
  let previousText = '';
  const creditPattern = /^(作词|作曲|编曲|制作人|监制|录音|混音|母带|和声|吉他|贝斯|鼓|键盘|弦乐|企划|出品|发行|词|曲)\s*[:：]/;
  for (const line of lines) {
    const lyricText = text(line.text)
      .replace(/\s+/g, ' ')
      .trim();
    if (!lyricText || lyricText === previousText || creditPattern.test(lyricText)) {
      continue;
    }
    cleaned.push({ time_ms: Math.max(0, Number(line.time_ms) || 0), text: lyricText });
    previousText = lyricText;
  }
  return cleaned.slice(0, 300);
}

function lyricPayload(lines) {
  return simplifyLyricLines(lines)
    .slice(0, 90)
    .map((line) => ({
      time_ms: Math.max(0, Number(line.time_ms) || 0),
      text: text(line.text).slice(0, 80),
    }));
}

async function getLyrics(songId) {
  if (!songId) {
    return [];
  }

  let bestLines = [];
  try {
    const modern = await api.lyric_new(apiParams({ id: songId }));
    bestLines = simplifyLyricLines(parseJsonLyricLines(modern.body?.lrc?.lyric));
  } catch {
    // Fall back to classic LRC below.
  }

  try {
    const classic = await api.lyric(apiParams({ id: songId }));
    const classicLines = simplifyLyricLines(parseClassicLrc(classic.body?.lrc?.lyric));
    if (classicLines.length > bestLines.length) {
      bestLines = classicLines;
    }
  } catch {
    // Keep any modern lines already parsed.
  }

  return bestLines;
}

async function getLyricPayloadForSong(song) {
  if (!song?.id) {
    return {
      lines: [],
      source_song_id: '',
      source_title: '',
      source_artist: '',
      status: 'missing_song_id',
      reason: '搜索结果没有 song id，无法查歌词',
    };
  }

  const lyrics = await getLyrics(song.id);
  const lines = lyricPayload(lyrics);
  return {
    lines,
    source_song_id: song.id,
    source_title: song.name || '',
    source_artist: artistsOf(song).join(','),
    status: lines.length ? 'found' : 'empty',
    reason: lines.length ? '' : '网易云未返回可用歌词，或歌词解析为空',
  };
}

async function findBestLyricPayload(primarySong, ranked) {
  const tried = new Set();
  const candidates = [primarySong, ...ranked.map((item) => item.song)].filter(Boolean);

  for (const song of candidates) {
    if (!song?.id || tried.has(song.id)) {
      continue;
    }
    tried.add(song.id);
    const payload = await getLyricPayloadForSong(song);
    if (payload.lines.length) {
      return payload;
    }
  }

  if (primarySong?.id) {
    return {
      lines: [],
      source_song_id: primarySong.id,
      source_title: primarySong.name || '',
      source_artist: artistsOf(primarySong).join(','),
      status: 'empty',
      reason: '网易云未返回可用歌词，或歌词解析为空',
    };
  }

  return {
    lines: [],
    source_song_id: '',
    source_title: '',
    source_artist: '',
    status: 'missing_song_id',
    reason: '搜索结果没有 song id，无法查歌词',
  };
}

async function searchPlayable(songName, artistName, options = {}) {
  const keywords = [songName, artistName].filter(Boolean).join(' ');
  const songs = await searchSongs(keywords, 12);
  if (!songs.length) {
    return {
      success: false,
      code: 'not_found',
      message: `网易云没有找到歌曲：${keywords}`,
    };
  }

  const ranked = songs
    .map((song, index) => ({ song, score: scoreSong(song, songName, artistName), index }))
    .sort((a, b) => (b.score - a.score) || (a.index - b.index));

  let sawFreeTrial = false;
  for (const item of ranked) {
    const song = item.song;
    for (const level of preferredLevels) {
      const urlResult = await api.song_url_v1(apiParams({
        id: song.id,
        level,
      }));
      const data = urlResult.body?.data?.[0];
      if (options.debugUrlLookup) {
        console.log('netease url candidate', JSON.stringify(summarizeUrlData(data, level)));
      }
      if (data?.freeTrialInfo) {
        sawFreeTrial = true;
      }
      if (
        data?.url &&
        !data.freeTrialInfo &&
        String(data.type || '').toLowerCase() === 'mp3' &&
        await isPlayableAudioUrl(data.url)
      ) {
        const artist = artistsOf(song).join(',');
        const lyricInfo = options.includeLyrics === false
          ? {
            lines: [],
            source_song_id: song.id,
            source_title: song.name,
            source_artist: artist,
            status: 'disabled',
            reason: '本次调用关闭了歌词获取',
          }
          : await findBestLyricPayload(song, ranked);
        const packedLyrics = lyricInfo.lines;
        const lyricsJson = packedLyrics.length ? JSON.stringify(packedLyrics) : '';
        const lyricsJsonBytes = lyricsJson ? Buffer.byteLength(lyricsJson) : 0;
        const hasLyrics = packedLyrics.length > 0 && lyricsJsonBytes > 0;
        return {
          success: true,
          status: 'pending_device_play_url',
          title: song.name,
          artist,
          source: 'netease',
          song_id: song.id,
          url: data.url,
          audio_url: data.url,
          bitrate: data.br,
          duration_ms: data.time,
          url_level: level,
          authenticated: hasNeteaseCookie(),
          lyric_lines: packedLyrics,
          lyric_count: packedLyrics.length,
          has_lyrics: hasLyrics,
          lyrics_json: lyricsJson,
          lyrics_json_bytes: lyricsJsonBytes,
          lyrics_status: lyricInfo.status,
          lyrics_reason: lyricInfo.reason,
          lyrics_source_song_id: lyricInfo.source_song_id,
          lyrics_source_title: lyricInfo.source_title,
          lyrics_source_artist: lyricInfo.source_artist,
          next_tool: 'self.music.play_url',
          message: hasLyrics
            ? '已经拿到新的直链 URL 和歌词。下一步必须调用设备端工具 self.music.play_url，并把 play_url_arguments 原样作为参数传入。'
            : `已经拿到新的直链 URL，但本次没有可用歌词：${lyricInfo.reason || '网易云无歌词/取歌词失败/解析失败'}。仍然必须调用设备端工具 self.music.play_url 播放歌曲。`,
          play_url_arguments: {
            title: song.name,
            artist,
            url: data.url,
            lyrics_json: lyricsJson,
          },
        };
      }
    }
  }

  if (sawFreeTrial || !hasNeteaseCookie()) {
    const qr = await createNeteaseLoginQr();
    return loginQrResponse(qr, `网易云找到了歌曲，但当前登录态只能拿到试听资源或已过期：${keywords}。请扫码登录会员账号后再播放。`);
  }

  return {
    success: false,
    code: 'no_playable_url',
    message: `网易云找到了歌曲，但没有拿到可播放地址：${keywords}`,
  };
}

function rememberPrivateFmSong(songId) {
  if (!songId) {
    return;
  }
  const id = String(songId);
  const existing = recentPrivateFmSongIds.indexOf(id);
  if (existing !== -1) {
    recentPrivateFmSongIds.splice(existing, 1);
  }
  recentPrivateFmSongIds.unshift(id);
  recentPrivateFmSongIds.splice(12);
}

async function getPrivateFmSongs() {
  const result = await api.personal_fm(apiParams({ timestamp: Date.now() }));
  return (result.body?.data || []).filter((song) => song?.id);
}

async function playPrivateFm(options = {}) {
  if (!hasNeteaseCookie()) {
    const qr = await createNeteaseLoginQr();
    return loginQrResponse(qr, '私人漫游需要网易云账号登录，请先扫码登录。');
  }

  let songs = await getPrivateFmSongs();
  if (!songs.length) {
    return {
      success: false,
      code: 'private_fm_empty',
      source: 'netease',
      message: '网易云私人漫游这次没有返回歌曲，请稍后再试。',
    };
  }

  const freshSongs = songs.filter((song) => !recentPrivateFmSongIds.includes(String(song.id)));
  if (freshSongs.length) {
    songs = freshSongs;
  }

  let sawFreeTrial = false;
  for (const song of songs) {
    for (const level of privateFmPreferredLevels) {
      const urlResult = await api.song_url_v1(apiParams({
        id: song.id,
        level,
      }));
      const data = urlResult.body?.data?.[0];
      if (options.debugUrlLookup) {
        console.log('netease private fm url candidate', JSON.stringify(summarizeUrlData(data, level)));
      }
      if (data?.freeTrialInfo) {
        sawFreeTrial = true;
      }
      if (
        data?.url &&
        !data.freeTrialInfo &&
        String(data.type || '').toLowerCase() === 'mp3' &&
        await isPlayableAudioUrl(data.url)
      ) {
        const artist = artistsOf(song).join(',');
        const lyricInfo = options.includeLyrics === false
          ? {
            lines: [],
            source_song_id: song.id,
            source_title: song.name,
            source_artist: artist,
            status: 'disabled',
            reason: '本次调用关闭了歌词获取',
          }
          : await getLyricPayloadForSong(song);
        const packedLyrics = lyricInfo.lines;
        const lyricsJson = packedLyrics.length ? JSON.stringify(packedLyrics) : '';
        const lyricsJsonBytes = lyricsJson ? Buffer.byteLength(lyricsJson) : 0;
        const hasLyrics = packedLyrics.length > 0 && lyricsJsonBytes > 0;
        rememberPrivateFmSong(song.id);
        return {
          success: true,
          status: 'pending_device_play_url',
          mode: 'private_fm',
          title: song.name,
          artist,
          source: 'netease',
          song_id: song.id,
          url: data.url,
          audio_url: data.url,
          bitrate: data.br,
          duration_ms: data.time,
          url_level: level,
          authenticated: hasNeteaseCookie(),
          lyric_lines: packedLyrics,
          lyric_count: packedLyrics.length,
          has_lyrics: hasLyrics,
          lyrics_json: lyricsJson,
          lyrics_json_bytes: lyricsJsonBytes,
          lyrics_status: lyricInfo.status,
          lyrics_reason: lyricInfo.reason,
          lyrics_source_song_id: lyricInfo.source_song_id,
          lyrics_source_title: lyricInfo.source_title,
          lyrics_source_artist: lyricInfo.source_artist,
          next_tool: 'self.music.play_url',
          message: hasLyrics
            ? '私人漫游已经拿到新的直链 URL 和歌词。下一步必须调用设备端工具 self.music.play_url，并把 play_url_arguments 原样作为参数传入。'
            : `私人漫游已经拿到新的直链 URL，但本次没有可用歌词：${lyricInfo.reason || '网易云无歌词/取歌词失败/解析失败'}。仍然必须调用设备端工具 self.music.play_url 播放歌曲。`,
          play_url_arguments: {
            title: song.name,
            artist,
            url: data.url,
            lyrics_json: lyricsJson,
          },
        };
      }
    }
  }

  if (sawFreeTrial) {
    const qr = await createNeteaseLoginQr();
    return loginQrResponse(qr, '私人漫游返回了歌曲，但当前登录态只能拿到试听资源或已过期，请扫码刷新登录。');
  }

  return {
    success: false,
    code: 'private_fm_no_playable_url',
    source: 'netease',
    message: '网易云私人漫游返回了歌曲，但没有拿到可播放地址。',
  };
}

function configureMcp(server, ResourceTemplate, z) {
  server.tool(
    'music.netease_play',
    '搜索网易云音乐并返回板子可播放的 MP3 URL。success=true 后必须继续调用设备端 self.music.play_url，参数 title、artist、url。',
    {
      song: z.string().describe('歌曲名，例如 晴天'),
      artist: z.string().optional().describe('歌手名，例如 周杰伦；不知道可留空'),
    },
    async ({ song, artist = '' }) => {
      const result = await searchPlayable(song, artist);
      return {
        content: withLoginQrContent(result),
        isError: !result.success,
      };
    }
  );

  server.tool(
    'music.netease_login_qr',
    '生成网易云音乐扫码登录二维码。登录过期、会员歌只能试听时调用；返回二维码图片，用网易云音乐 App 扫码。',
    {
      force: z.boolean().optional().describe('是否强制生成新的二维码'),
    },
    async ({ force = false }) => {
      const result = await createNeteaseLoginQr({ force });
      return {
        content: withLoginQrContent(result),
        isError: !result.success,
      };
    }
  );

  server.tool(
    'music.netease_private_fm',
    '从网易云账号私人漫游/私人 FM 获取一首推荐歌曲并播放。success=true 后必须继续调用设备端 self.music.play_url，参数 title、artist、url、lyrics_json。',
    {},
    async () => {
      const result = await playPrivateFm();
      return {
        content: withLoginQrContent(result),
        isError: !result.success,
      };
    }
  );

  server.tool(
    'music.netease_login_check',
    '检查网易云扫码登录状态；扫码确认成功后会把新的会员 cookie 保存到 NAS。',
    {
      key: z.string().optional().describe('二维码 key；通常留空，使用上一次生成的二维码'),
    },
    async ({ key = '' }) => {
      const result = await checkNeteaseLoginQr(key);
      return {
        content: [{ type: 'text', text: JSON.stringify(result, null, 2) }],
        isError: !result.success,
      };
    }
  );

  server.tool(
    'music.netease_login_status',
    '检查 NAS 上保存的网易云登录态是否仍然有效。',
    {},
    async () => {
      const result = await getNeteaseLoginStatus();
      return {
        content: [{ type: 'text', text: JSON.stringify(result, null, 2) }],
        isError: !result.success,
      };
    }
  );

  server.tool(
    'music.netease_search',
    '只搜索网易云音乐并返回候选歌曲，不播放。需要播放时请调用 music.netease_play。',
    {
      keyword: z.string().describe('搜索关键词，例如 晴天 周杰伦'),
    },
    async ({ keyword }) => {
      const songs = (await searchSongs(keyword, 8)).map((song) => ({
        id: song.id,
        title: song.name,
        artist: artistsOf(song).join(','),
        album: song.al?.name || song.album?.name || '',
      }));
      return {
        content: [
          {
            type: 'text',
            text: JSON.stringify({
              success: songs.length > 0,
              source: 'netease',
              songs,
            }, null, 2),
          },
        ],
      };
    }
  );
}

module.exports = {
  configureMcp,
  getLyrics,
  searchPlayable,
  playPrivateFm,
  searchSongs,
  artistsOf,
  hasNeteaseCookie,
  preferredLevels,
  privateFmPreferredLevels,
  createNeteaseLoginQr,
  checkNeteaseLoginQr,
  getNeteaseLoginStatus,
  withLoginQrContent,
};
