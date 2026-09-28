# Ashenveil — project status

Updated 2026-09-28 (expansion merged). Active branch: `claude/zealous-cori-dkwd60` in `papagos2/Game`.

## MMO systems round (verified in the cloud session)

- Added: auto-travel to quest objectives with mounts (level 5), gathering and crafting (draughts, elixirs,
  reforging), faction reputation with discounts and a Revered epic, daily bounties, 19 achievements with
  titles, three instanced dungeons with AI companions filling tank/healer/damage roles, and an optional
  online layer (`server/`: presence per map and chat with filter, rate limits, mute and report).
- `npm test`: 34 tests (including 5 server tests over real WebSockets). `npm run smoke`: passes, now also
  covering auto-travel on a mount, gathering, crafting, the Journal tabs and a dungeon with its party.
  `npm run online-e2e`: two browsers see each other and chat. `npm run playthrough all 12345`: all 9 paths
  finish all three acts and all three dungeons (0-3 deaths).
- Honest limits: combat is not shared between real players yet; no accounts, moderation process or
  server hosting exist; companions and dungeons are balanced by bot, not by humans. See `docs/RESEARCH.md`
  and `server/README.md`.

## Expansion: specializations, talents, three acts (verified in the cloud session)

- **Content now in code:** 3 classes x 3 specializations (chosen at level 10, two extra abilities each at
  levels 10 and 16), a core talent tree per class plus one per specialization (29 points by level 30, respec
  at vendors), levels 1-30 over three maps (Vale of Ashenveil 1-10, Frostmarch 10-20, Sunscar Dunes 20-30)
  with Wayfinder travel, 23 quests (kill, collect, explore), 21 enemy types, 3 elites and 3 bosses, 8 gear
  slots with a crit stat, 5 rarities and 9 named boss legendaries, vendor gear.
- Abilities are data run by one engine (`src/game/abilities.ts`); map looks are data (`MAPS[id].theme`).
- Save format v2 migrates v1 saves (3 gear slots, one map); the backup-checkpoint recovery above is kept.
- `npm test`: 24 tests pass. `npm run smoke`: passes, now also covering path choice, talents and travel to all
  three maps. `npm run playthrough all 12345`: all 9 paths finish all three acts (level 30) with 0-4 deaths;
  bot time 26-54 simulated minutes without walking. The bot found and drove fixes for: impossible bosses
  for casters (mana did not scale with level), high-level power creep, and a negative first-frame delta.
- Acts II-III are probably too easy for a player who dodges perfectly; human playtests must decide.
- `docs/VISUALS.md` tells an art contributor where every visual lives and what must not change.

## Earlier verification on the Windows computer

## Verified on this Windows computer

- The previous Beaconhold RTS is preserved in Git history and a local stash. Its old runtime files and save games are preserved outside this checkout in `../BeaconholdRuntimeArchive/`.
- Ashenveil is the current original, single-player fantasy action RPG. At that commit its code defined 3 classes, 6 zones, 7 quests, levels 1–10, and one final boss (superseded by the expansion above).
- `npm run build` succeeds with TypeScript checking and Vite production output.
- `npm test` passes 14 tests, including recovery from a damaged active save.
- `npm run smoke` passes in phone-sized Chromium on Windows: title, character creation, quest, touch movement, combat, menus, boss, death, reload and save. The updated Cindermaw boss was visually inspected in `tests/output/07-boss.png`. Software-rendered Chromium produced 7–9 FPS in samples; this is not device performance evidence.
- `npm run playthrough` passes for all three classes with deterministic seed 12345 after the boss model update. The scripted bot finishes in 9–13 simulated minutes. Human playtime has not been measured.
- `npx cap sync android` succeeds. [GitHub Actions run 36415812283](https://github.com/papagos2/Game/actions/runs/36415812283) built and uploaded the Android **debug APK** from commit `a90a92a`. A local copy is at `../AshenveilAndroidBuild-a90a92a/app-debug.apk` (6,986,381 bytes; SHA-256 `A51A9172560DB660A3F9DFA23F7DA430C9B75A1720CDC9ACA8AEA686469DE4F3`). This verifies CI packaging, not launch on a phone.
- `npm audit --omit=dev` reported zero production dependency advisories on this date.

## Bugs found and fixed

- The Windows smoke test constructed screenshot paths from URL `.pathname`, producing `C:\\C:\\...` and aborting. It now uses `fileURLToPath` and `path.join`.
- Ordinary enemies could postpone respawn forever while a player remained within 30 units of a cleared camp. This could leave quests or level progression without targets. Nearby respawn deferral is now capped.
- A single damaged localStorage entry could erase access to a character. Saves now retain a previous valid checkpoint and load it if the active entry is damaged. The existing v1 save format remains readable.
- Bot balance runs used wall-clock randomness. Development builds now accept a deterministic `?seed=` for reproducible simulation.
- Model height was scaled twice for differently sized humanoids and quadrupeds, misplacing picking, projectiles and combat text. The rig now reports local height, as callers expect.
- The final boss used an oversized generic humanoid mesh. Cindermaw now has a distinct quadruped silhouette, wings, horns, tail and ember eyes. It is still procedural low-poly art, not the requested production-quality realistic asset.
- The all-class playthrough could leave previous browser contexts alive and time out opening the third class. Each class run now closes its context before the next starts.

## Not verified or not yet built

- `adb` is not installed on PATH and no connected Android device appeared in the available Windows device list. No Android or iOS device playtest, thermal test, frame-time profile, memory profile, touch ergonomics study, or native audio/lifecycle validation.
- No signed Android release bundle or iOS TestFlight build. iOS requires a Mac with Xcode and Apple signing.
- The current art is intentionally low-poly procedural geometry, as confirmed by screenshots from the smoke test. It does not meet the requested realistic visual target. Full character/environment assets, rigging, animations, materials, and a mobile LOD pipeline are absent.
- The current story (three acts) is still far short of a measured 50-hour game. The target cannot be met by renaming zones or repeating grind quests; it requires substantial authored content and blind human playtests.
- Store compliance, privacy materials, accessibility, localization, device matrix QA, and release signing remain unfinished.

## Exact next action

Complete the [production plan](Docs/PRODUCTION_PLAN.md) in staged, playable increments: establish a measured Android device baseline and visual target, replace one full hero/zone/enemy set with production-quality assets, then expand authored quests and systems chapter by chapter with save migration and playtests. Gate any market release on a signed device-tested build and measured human playtime.
