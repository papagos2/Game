# PROJECT_STATUS - Beaconhold (UE5 mobile RTS)

_Last updated: 2026-09-28 - stage: polish and hardening before the first build in Unreal Engine_

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
- **Gameplay polish (playtest-driven)**: stuck units, spam-tap cancels, a Long Dusk soft-lock,
  worker queues, AI counter-picks and starvation, Sunburst stacking, data-driven second star.
- **Touch controls (this stage)**:
  - Taps pick what is drawn: a tower by its top, a soldier by its head, with a finger-sized
    tolerance; a unit hidden behind a building yields to the building.
  - A building outline dragged by a corner keeps that grab point (it used to jump its centre
    under the finger, hiding it).
  - With soldiers selected, a tap beside (not on) one of your own units is a move; it used to
    select that unit and drop the army.
  - Tutorial: the Cottage step names every tap, and the Place button lights up while placing.
- **Suspend/resume hardening (this stage)**: saves carry a checksum (format version 3); a save
  cut short by the app being killed mid-write, damaged on disk, or from another version is
  refused; everything a save feeds into indices, loops, allocations and positions is validated
  first; the menu offers "Continue" only for a save that passes the check (a bad one is dropped
  at startup instead of a silent failed tap).
- **Performance (this stage)**: spatial queries filter before sorting (same results, verified
  byte-identical over 51 games; 30-45% cheaper in big battles); path searches per tick capped at
  16 (the spike when a big army is ordered at once is a third lower); `bhplaytest --bench`.
- **Clarity (this stage)**: per-unit "X ready" toasts (four in ten of all messages) replaced by
  "<building> finished training" when a queue runs out; messages drop from 8-10 to 5-6 a
  minute. "Under attack" is announced once per fight every 15 s, a raid elsewhere gets its own
  alarm (it used to wait out the other fight's 15 s), and a fight in plain view raises no
  alarm or banner (the minimap ping stays). Pinch zoom keeps the ground between the fingers
  under them.

## Verified (in this environment, without Unreal)
- **63 native tests** pass with clang (C++17) and g++ (C++20), strict warnings as errors, and
  under AddressSanitizer + UBSan (+ float-cast-overflow). Each fix has a regression test that
  fails without it.
- **Save robustness**: truncated saves at every early byte and hundreds of cut points, and
  damaged saves, are all refused; randomly edited saves (checksum recomputed; 240 per test run,
  3000 in a one-off run) are refused or play on without any sanitizer error; without the checks
  the same test hits undefined behaviour at once. Save and load each take ~0.1 ms (57 KB).
- **Playtest matrix** (`bhplaytest --seeds 5`): 85 games, 0 invariant violations, and 444
  suspend/resume round trips during play, all restoring the identical world. 34 games also run
  clean under the sanitizers. Results:

  | Mission / difficulty | Result | Notes |
  |---|---|---|
  | Kindling (tutorial) | 5/5 won, ~3:15 | no losses |
  | Long Dusk Easy / Normal | 5/5 / 5/5 | 0 / 6 units lost on average |
  | Long Dusk Hard | naive bot 1/15, stronger bot 15/15 | hard but fair |
  | Heart Easy / Normal | 5/5 / 5/5 | 11 / 39 units lost |
  | Heart Hard | naive bot 9/15 | about 15 min, ~76 units lost |
  | Heart Normal by army | all 7 strategies 5/5 | Shieldbearer + Sage fastest (7:39); others 9-11 min |
  | Hard with Boons (9 Renown = three stars on every Normal mission) | naive bot 15/15 on both | see Known risks |

  The playtest also counts the messages a player would see: 5-6 a minute, at most 5 in 10 s,
  almost never pushed off screen early.

- **CPU budget** (`bhplaytest --bench`; one 2.1 GHz Xeon thread; a phone core is roughly 2-4x
  slower):

  | Work | Average | Worst |
  |---|---|---|
  | Simulation tick, real games (20 per second) | 0.02 ms | ~2 ms |
  | Simulation tick, 140 units fighting | 0.08 ms | 3.5-4 ms (first tick of a mass order) |
  | Simulation tick, 280 units (twice the caps) | 0.22 ms | ~4.2 ms |
  | HUD model + all unit poses, per frame | < 0.03 ms | < 1 ms |
  | Minimap overlay (5/s) / base (every 2 s) | 0.007 / 0.05 ms | 0.11 ms |

  Unit counts: peak ~94 in play (58 Wardens + 36 Gloam); the rules cap it near 160 (supply 100,
  Gloam army cap 36-40 plus waves). Simulation memory ~1 MB.
- **Unreal layer compile check** (`python3 Tools/UEStubs/check.py`, also `CXX=g++`): every file
  and header compiles against declarations of the UE5 API (types, includes, format strings,
  shadowing, file-name case clashes).
- **Lifecycle review** of the Unreal layer: runtime materials, meshes and textures are held by
  UPROPERTY arrays (safe from garbage collection); effect and projectile components are pooled
  and reused across missions; chunk meshes are released when a mission ends.
- WebGL preview of real frames (same meshes, palette, lighting) for the look.

## Assumptions (not verifiable here)
- The UE 5.8 API matches the declarations in `Tools/UEStubs/UEStub.h` (signatures were checked
  against documentation where reachable, not against the engine).
- Runtime `UStaticMesh::BuildFromMeshDescriptions`, transient textures and the procedural sound
  wave work in cooked mobile builds as they do in the editor.
- The engine's widget pass-through materials exist, are cooked, and take the parameters used.
- Mobile LDR output shows the palette colours closely enough; the default Slate font is staged.
- `SaveGameToSlot` may leave a partial file if the app is killed mid-write (the checksum covers
  this either way).
- Bot playtests stand in for human playtests: they measure pacing, balance and robustness, not
  fun, readability or touch feel. Phone CPU is estimated from desktop timings.

## Unreal-only checks remaining (first run)
1. Compile the module in UE 5.8 (expect a short fix round).
2. World visible and facing outward (else flip `bReverseTriangleWinding`); colours as in preview.
3. Menus, HUD and command card lay out correctly at phone and tablet aspect ratios and safe areas;
   text is sharp on high-density screens (render scale comes from the engine's device profiles,
   `r.MobileContentScaleFactor`).
4. Touch: tap, drag-pan, pinch, long-press box, minimap; mouse and keyboard on desktop. Check
   that taps on tall buildings and on soldiers' heads select them (screen picking).
5. Audio plays (music cross-fade, effects), and stops cleanly on exit.
6. Background/resume on a phone keeps the mission (suspend save) and pauses it; kill the app
   while backgrounded and check "Continue" restores it.
7. Packaged Android build: materials cooked (no grey world), text visible, 60 fps target,
   memory and draw calls on a mid-range device.
8. Frame cost with ~100 units on a mid-range phone (Unreal Insights / `stat game`, `stat rhi`):
   ~450 component transforms per frame, and whether shared unit meshes batch (auto-instancing).

## Known risks
- First compile in Unreal will surface API mismatches the stubs cannot catch.
- Draw calls and per-frame component updates at ~100 units are unmeasured on devices; if too
  high, the fix is instanced unit bodies (a contained change in `BhWorldView`).
- **Design question (Boons vs Hard)**: Hard sits right at the naive bot's threshold (Long Dusk
  2/15, Heart 9/15), so the Boons a player owns after three-starring Normal (9 Renown) make both
  Hard missions 15/15; even Boons 40% weaker still give 15/15. Weaker Boons would feel pointless
  without restoring Hard's bite, so they are unchanged; players can reset Boons for a pure run.
  Decide after human playtests (options: Hard scales Boons down, or a harder Hard).
- Ordering a very large army across the map costs one 3-4 ms simulation tick on desktop
  (roughly one dropped frame at 60 fps on a mid-range phone); group paths would remove it.
- Cooking all of `/Engine/EngineMaterials` adds download size until narrowed.
- Package/bundle ids are placeholders; no app icon or launch screen yet. Before a store release:
  Google Play wants an app bundle and a current target SDK level (check the UE 5.8 Android
  settings then).
- Desktop Shipping builds keep the engine tone mapper, so colours differ slightly there.

## Decisions
- Unreal Engine 5.8, C++ only; all content generated at runtime.
- All rules in the plain C++ core, tested natively; Unreal is a thin presentation shell.
- One unlit palette material with baked lighting; mobile renders in LDR.
- Slate UI in C++; one gesture recognizer for touch and mouse. Landscape only.
- Balance and AI numbers live in data tables (`BhData`, mission definitions) and are tuned
  against the playtest matrix.
- Suspend saves are refused rather than repaired when damaged or from another version.

## Next highest-value task
The first build in Unreal Engine 5.8 on a real device (checks 1-8 above), which only the owner
can run. Until then, in this environment: HUD and notice clarity review (what the player is told
and when), then camera pinch-towards-fingers and audio/VFX event coverage.
