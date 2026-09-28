# Ashenveil — project status

Updated 2026-09-28. Active branch: `claude/zealous-cori-dkwd60` in `papagos2/Game`.

## Verified on this Windows computer

- The previous Beaconhold RTS is preserved in Git history and a local stash. Its old runtime files and save games are preserved outside this checkout in `../BeaconholdRuntimeArchive/`.
- Ashenveil is the current original, single-player fantasy action RPG. Its shipped code currently defines 3 classes, 6 zones, 7 quests, levels 1–10, and one final boss.
- `npm run build` succeeds with TypeScript checking and Vite production output.
- `npm test` passes 14 tests, including recovery from a damaged active save.
- `npm run smoke` passes in phone-sized Chromium on Windows: title, character creation, quest, touch movement, combat, menus, boss, death, reload and save. Software-rendered Chromium produced 9 FPS in one sample; this is not device performance evidence.
- `npm run playthrough` passes for all three classes with deterministic seed 12345. The scripted bot finishes in 8–14 simulated minutes. Human playtime has not been measured.
- `npx cap sync android` succeeds. The Android native project exists, but this computer has no verified Android SDK, ADB, or connected device. An Android APK for these changes has **not** been built or device-tested locally.

## Bugs found and fixed

- The Windows smoke test constructed screenshot paths from URL `.pathname`, producing `C:\\C:\\...` and aborting. It now uses `fileURLToPath` and `path.join`.
- Ordinary enemies could postpone respawn forever while a player remained within 30 units of a cleared camp. This could leave quests or level progression without targets. Nearby respawn deferral is now capped.
- A single damaged localStorage entry could erase access to a character. Saves now retain a previous valid checkpoint and load it if the active entry is damaged. The existing v1 save format remains readable.
- Bot balance runs used wall-clock randomness. Development builds now accept a deterministic `?seed=` for reproducible simulation.

## Not verified or not yet built

- No Android or iOS device playtest, thermal test, frame-time profile, memory profile, touch ergonomics study, or native audio/lifecycle validation.
- No signed Android release bundle or iOS TestFlight build. iOS requires a Mac with Xcode and Apple signing.
- The current art is intentionally low-poly procedural geometry, as confirmed by screenshots from the smoke test. It does not meet the requested realistic visual target. Full character/environment assets, rigging, animations, materials, and a mobile LOD pipeline are absent.
- The current story is far short of a measured 50-hour game. The target cannot be met by renaming zones or repeating grind quests; it requires substantial authored content and blind human playtests.
- Store compliance, privacy materials, accessibility, localization, device matrix QA, and release signing remain unfinished.

## Exact next action

Complete the [production plan](Docs/PRODUCTION_PLAN.md) in staged, playable increments: establish a measured Android device baseline and visual target, replace one full hero/zone/enemy set with production-quality assets, then expand authored quests and systems chapter by chapter with save migration and playtests. Gate any market release on a signed device-tested build and measured human playtime.
