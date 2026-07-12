# Codex Safety Rules

These instructions apply to every Codex session working in this repository.
Stability and preservation of known-good hardware behavior take priority over
cleanup, refactoring, visual experimentation, and feature growth.

## 1. Read The Project Handoff First

Before proposing or making any change, read and understand:

- `docs/HANDOFF.md`
- `docs/PROJECT_STATUS.md`
- `docs/NEXT_TASKS.md`
- `docs/CODEX_RULES.md`
- `docs/CHANGELOG_QDTECH.md`
- `PATCHES.md`
- `main/boards/qdtech-s3-touch-lcd-3.5/README.md`

Then inspect the current branch and working tree. Report conflicts, stale
documentation, and uncommitted content; do not silently resolve them.

## 2. Plan Before Runtime Changes

- Default to analysis only. Do not modify files merely because a possible fix
  has been identified.
- Before changing firmware runtime code, present a scoped plan that names the
  task, files, affected subsystem, risks, verification, and rollback path.
- Implementation may begin only after the user explicitly replies:
  `PLAN APPROVED: <task name>`.
- Approval for one named task does not authorize another task or an expansion
  into adjacent subsystems.
- Documentation-only authorization applies only to the exact documents named
  by the user.

## 3. Preserve The Stable Product

Do not change the following without explicit, specific user authorization:

- Current UI appearance or page structure.
- Weather artwork or weather animation.
- LVGL `FULL` rendering mode.
- ST77922 initialization, rotation, or screen orientation.
- CST9217 touch parsing, mapping, gestures, or recovery behavior.
- BMI270 polling, thresholds, orientation, or gesture behavior.
- The official XiaoZhi voice flow.
- Music, lyrics, radio, podcast, or audio-focus behavior.
- MCP tool names, parameters, or return semantics.
- Wi-Fi provisioning, connection, persistence, or recovery behavior.
- Hardware pins.
- Flash layout or OTA partitions.
- AFE/WakeNet configuration.

Do not delete working features to simplify an implementation. Keep
QDTech-specific behavior board-local unless there is a documented,
board-independent reason to change common code.

## 4. High-Risk Changes

Display, touch, audio, media scheduling, OTA, partitions, and AFE/WakeNet are
high-risk areas.

Any new high-risk implementation must:

- Be guarded by a `CONFIG_QDTECH_EXPERIMENT_*` option.
- Be disabled by default.
- Preserve the current stable path.
- Have a one-step rollback path.
- Remain experimental until verified on physical hardware.
- Never replace the stable path based on compilation alone.

One task must not modify two high-risk subsystems at the same time. Split such
work into separately planned, approved, implemented, and verified tasks.

## 5. Protect The Working Tree

- Never overwrite user changes or uncommitted content.
- Never delete, move, reformat, stage, ignore, or otherwise manage untracked
  user files unless the user explicitly names and authorizes that operation.
- Never run `git clean`, `git reset`, or `git stash` automatically.
- `docs/ui-concepts/` is current untracked user content and must remain exactly
  as found.
- When the working tree is dirty, report it before changing overlapping files.
  Do not clean or normalize the tree yourself.
- Do not modify `.gitignore` to hide user content or generated changes without
  explicit authorization.

## 6. Git And Device Boundaries

Without explicit user authorization for the exact operation, do not:

- Commit.
- Push or force-push.
- Merge or rebase.
- Create, move, or delete a Tag.
- Publish, edit, or delete a Release.
- Modify the `main` branch history.
- Flash a device.
- Start an OTA update.
- Erase or rewrite NVS.

Force-push is prohibited by repository policy. Stage only task-related files
when staging has been explicitly authorized. Never commit generated `sdkconfig`,
build output, credentials, activation data, secrets, or NVS dumps.

## 7. Verification And Reporting

A successful build is not hardware verification. After every authorized
runtime-code change, report each of the following separately:

- Build result.
- Firmware size.
- Size change relative to the selected baseline.
- Modified files.
- New tasks and their task stacks.
- Expected internal-SRAM and PSRAM impact.
- Hardware checks still required.
- Exact rollback method.

When physical-device evidence has not been collected, write exactly:

`BUILD VERIFIED; HARDWARE VERIFICATION PENDING`

Touch, display, weather, voice, media, audio focus, networking, OTA, and sensor
behavior must not be declared fixed without relevant device logs and, where
visual or audible behavior is involved, a physical user check.
