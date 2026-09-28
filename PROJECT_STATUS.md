# PROJECT_STATUS - Beaconhold (UE5 mobile RTS)

_Last updated: 2026-09-28 - stage: gameplay polish before the first build in Unreal Engine_

## Completed
- **Design** (`Docs/GAME_DESIGN.md`): original world (Wardens vs the Gloam), 5 Warden and 4 Gloam
  units, 9 + 4 building types, Sunstone/Timber/supply, 3-mission campaign with stars, Renown and
  Boons, 3 difficulties, touch-first controls, landscape.
- **Simulation core** (`Source/Beaconhold/Sim`, plain C++): map, pathfinding, units, economy,
  construction, training, research, abilities, combat, adaptive enemy AI, missions, tutorial,
  camera, gestures, HUD model, save/resume, campaign progress.
- **Procedural content**: models, terrain, palette, icons, UI textures, sounds and music, all
  generated at runtime (no binary assets).
- **Unreal layer** (`Game/`, `UI/`): director, runtime meshes/textures, animation, effects,
  audio mixer, Slate menus and HUD, input layer, app lifecycle, saves; build configuration for
  Windows/Mac/Android/iOS; `README.md` with exact steps.
- **This stage (gameplay polish, driven by the playtest matrix)**:
  - Stuck units fixed: ranged units stopping out of range on a target's diagonal; walkers
    shoving each other to a standstill head-on.
  - Controls: tapping the same enemy again no longer cancels the swing (6 s of taps used to deal
    no damage); soldiers kept from their target strike an enemy in reach.
  - Soft-lock fixed: the Long Dusk could become unwinnable once the map's Sunstone ran out
    (Beacons now cost Timber only; the Keep yields a slow Sunstone tithe as a safety net).
  - Economy: workers at a full outcrop move to one with room nearby (queue time -55%).
  - AI: the Gloam counter the player's army composition, and expensive units are no longer
    starved by cheap ones; picks are saved.
  - Balance: Sunburst cast one Sage per tap; shooters prioritise healers; Sage heal 12 -> 10.
  - Stars: data-driven second star (Long Dusk: lose no buildings; Heart par 25:00 -> 18:00),
    shown in the briefing.

## Verified (in this environment, without Unreal)
- **53 native tests** pass with clang (C++17) and g++ (C++20), strict warnings as errors.
  Each fix above has a regression test that fails without it.
- **Playtest matrix** (`Tools/SimTests/build/bhplaytest --seeds 5`): bots play every mission and
  difficulty with 7 strategies, 5 seeds each; 0 invariant violations, 0-4 stuck incidents per
  25+ games, peak ~70 units on the map. Latest results:

  | Mission / difficulty | Result (5 seeds) | Notes |
  |---|---|---|
  | Kindling (tutorial) | 5/5 won, ~3:15 | no losses |
  | Long Dusk Easy / Normal | 5/5 / 5/5 | 0 / 5 units lost on average |
  | Long Dusk Hard | naive bot 0/5, stronger bot 5/5 | hard but fair |
  | Heart Easy / Normal / Hard | 5/5 / 5/5 / 4/5 | 11 / 39 / 72 units lost |
  | Heart Normal by army | all 7 strategies 5/5 | Shieldbearer + Sage fastest (7:35, 11 lost); the others 9-11 min |

- **Unreal layer compile check** (`python3 Tools/UEStubs/check.py`, also `CXX=g++`): every file
  and header compiles against declarations of the UE5 API (types, includes, format strings,
  shadowing, file-name case clashes).
- WebGL preview of real frames (same meshes, palette, lighting) for the look.

## Assumptions (not verifiable here)
- The UE 5.8 API matches the declarations in `Tools/UEStubs/UEStub.h` (signatures were checked
  against documentation where reachable, not against the engine).
- Runtime `UStaticMesh::BuildFromMeshDescriptions`, transient textures and the procedural sound
  wave work in cooked mobile builds as they do in the editor.
- The engine's widget pass-through materials exist, are cooked, and take the parameters used.
- Mobile LDR output shows the palette colours closely enough; the default Slate font is staged.
- Bot playtests stand in for human playtests: they measure pacing, balance and robustness, not
  fun, readability or touch feel.

## Unreal-only checks remaining (first run)
1. Compile the module in UE 5.8 (expect a short fix round).
2. World visible and facing outward (else flip `bReverseTriangleWinding`); colours as in preview.
3. Menus, HUD and command card lay out correctly at phone and tablet aspect ratios and safe areas.
4. Touch: tap, drag-pan, pinch, long-press box, minimap; mouse and keyboard on desktop.
5. Audio plays (music cross-fade, effects), and stops cleanly on exit.
6. Background/resume on a phone keeps the mission (suspend save) and pauses it.
7. Packaged Android build: materials cooked (no grey world), text visible, 60 fps target,
   memory and draw calls on a mid-range device.

## Known risks
- First compile in Unreal will surface API mismatches the stubs cannot catch.
- Performance on low-end Android is unmeasured (unit/mesh counts are modest: ~70 units peak).
- Cooking all of `/Engine/EngineMaterials` adds download size until narrowed.
- Package/bundle ids are placeholders; no app icon or launch screen yet.
- Desktop Shipping builds keep the engine tone mapper, so colours differ slightly there.

## Decisions
- Unreal Engine 5.8, C++ only; all content generated at runtime.
- All rules in the plain C++ core, tested natively; Unreal is a thin presentation shell.
- One unlit palette material with baked lighting; mobile renders in LDR.
- Slate UI in C++; one gesture recognizer for touch and mouse. Landscape only.
- Balance and AI numbers live in data tables (`BhData`, mission definitions) and are tuned
  against the playtest matrix.

## Next highest-value task
Touch UX and control logic review (tap targets, selection, placement flow, camera, tutorial
clarity) with tests, then save/resume robustness (fuzzed and truncated saves) and mobile
performance budgets. In parallel, whenever possible: the first build in Unreal Engine 5.8.
