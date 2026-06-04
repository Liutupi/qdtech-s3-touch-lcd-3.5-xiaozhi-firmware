# Codex Development Rules For This Repository

These rules are for future Codex sessions and for any new computer taking over this firmware project.

## First-Read Rule

Before changing code, read these files:

- `docs/HANDOFF.md`
- `docs/PROJECT_STATUS.md`
- `docs/NEXT_TASKS.md`
- `docs/CODEX_RULES.md`
- `docs/CHANGELOG_QDTECH.md`
- `PATCHES.md`
- `main/boards/qdtech-s3-touch-lcd-3.5/README.md`

Then inspect the current working tree with:

```powershell
git status --short --branch
git remote -v
```

## Source Change Rules

- Keep QDTech-specific code inside `main/boards/qdtech-s3-touch-lcd-3.5/` whenever possible.
- Avoid changing official XiaoZhi common core unless there is a clear board-independent reason.
- If common core must change, document why in `PATCHES.md` or the QDTech changelog.
- Do not delete working features to simplify a task.
- Do not silently change audio pins, display pins, touch pins, flash size, partition table, or board type.
- Do not replace the XiaoZhi voice core with a custom flow.
- Do not broaden radio work from direct MP3 into HLS/AAC unless the user asks for it.

## Build And Flash Rules

- Prefer the QDTech-specific build directory:

```powershell
idf.py -B build-qdtech -D SDKCONFIG="build-qdtech/sdkconfig" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32s3;main/boards/qdtech-s3-touch-lcd-3.5/sdkconfig.defaults" build
```

- Use the actual detected serial port for flashing.
- A compile result alone does not prove touch, display, voice, or radio behavior.
- If `idf.py` times out on Windows, verify whether build/flash actually completed before retrying.

## Git Rules

- User repository remote is `qdtech-new`.
- Official upstream remote is `origin`.
- Do not force-push.
- Do not overwrite user changes in a dirty worktree.
- Stage only files related to the current request.
- Before public pushes, check that no secrets or generated configs are being committed.
- Treat `sdkconfig` as high risk because it can contain local or secret state.

## Runtime Verification Rules

For source changes, verify the affected behavior with logs:

- XiaoZhi: wake/listen/speak returns to idle.
- Display: landscape screen is visible and not inverted incorrectly.
- Touch: tap and swipe logs match screen coordinates.
- Radio: play/stop/next/previous work.
- Audio focus: XiaoZhi speaking/listening pauses radio; idle restores radio if playback was requested.
- Weather: API/network failure does not crash the firmware.

## Documentation Rules

- Keep this handoff set current after meaningful work.
- Update `docs/CHANGELOG_QDTECH.md` with commit hash, build result, flash result, and hardware observations.
- If a fact is not confirmed by source or hardware, write `待确认` instead of guessing.
