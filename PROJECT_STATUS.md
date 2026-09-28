# Ashenveil — project status

Updated 2026-09-28. Active branch: `claude/zealous-cori-dkwd60` in `papagos2/Game`.

## Verified on this Windows computer

- The previous Beaconhold RTS is preserved in Git history and a local stash. Its old runtime files and save games are preserved outside this checkout in `../BeaconholdRuntimeArchive/`.
- Ashenveil is the current original, single-player fantasy action RPG. Its shipped code currently defines 3 classes, 6 zones, 7 quests, levels 1–10, and one final boss.
- `npm run build` succeeds with TypeScript checking and Vite production output.
- `npm test` passes 14 tests, including recovery from a damaged active save.
- `npm run smoke` passes in phone-sized Chromium on Windows: title, character creation, quest, touch movement, combat, menus, boss, death, reload and save. The updated Cindermaw boss was visually inspected in `tests/output/07-boss.png`. Software-rendered Chromium produced 7–9 FPS in samples; this is not device performance evidence.
- `npm run playthrough` passes for all three classes with deterministic seed 12345 after the boss model update. The scripted bot finishes in 9–13 simulated minutes. Human playtime has not been measured.
- `npx cap sync android` succeeds. [GitHub Actions run 36413686360](https://github.com/papagos2/Game/actions/runs/36413686360) built and uploaded the Android **debug APK** from commit `22f30da`. A local copy is at `../AshenveilAndroidBuild-22f30da/app-debug.apk` (6,985,765 bytes; SHA-256 `413C958024DBF12A441DE6D0C41B7B499A3679657EA1692BD5418369DF0BDBD3`). This verifies CI packaging, not launch on a phone.
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

- This computer has no verified Android SDK, ADB, or connected device. No Android or iOS device playtest, thermal test, frame-time profile, memory profile, touch ergonomics study, or native audio/lifecycle validation.
- No signed Android release bundle or iOS TestFlight build. iOS requires a Mac with Xcode and Apple signing.
- The current art is intentionally low-poly procedural geometry, as confirmed by screenshots from the smoke test. It does not meet the requested realistic visual target. Full character/environment assets, rigging, animations, materials, and a mobile LOD pipeline are absent.
- The current story is far short of a measured 50-hour game. The target cannot be met by renaming zones or repeating grind quests; it requires substantial authored content and blind human playtests.
- Store compliance, privacy materials, accessibility, localization, device matrix QA, and release signing remain unfinished.

## Exact next action

Complete the [production plan](Docs/PRODUCTION_PLAN.md) in staged, playable increments: establish a measured Android device baseline and visual target, replace one full hero/zone/enemy set with production-quality assets, then expand authored quests and systems chapter by chapter with save migration and playtests. Gate any market release on a signed device-tested build and measured human playtime.
