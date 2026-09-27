# PROJECT_STATUS - Beaconhold (UE5 mobile RTS)

_Last updated: 2026-09-27 - stage: code complete, awaiting the first build in Unreal Engine_

## Finished
- **Game design** (`Docs/GAME_DESIGN.md`): original world (Wardens vs the Gloam), 5 Warden units,
  4 Gloam units, 9 + 4 building types, 2 resources + supply, 3-mission campaign, stars, Renown
  and permanent Boons, 3 difficulties, touch-first controls, landscape.
- **Simulation core** (`Source/Beaconhold/Sim`, plain C++, no Unreal headers): map, pathfinding,
  units, buildings, gathering, construction, training, research, abilities, combat, projectiles,
  enemy AI (economy, waves, counter-attacks), missions and objectives, guided tutorial, camera,
  touch/mouse gesture recognizer, HUD model and command card, notices, save/resume, campaign
  progress.
- **Procedural content**: low-poly models of every unit and building, terrain, trees and props;
  a baked-light colour palette; UI textures, icons and decals; synthesized sound effects and two
  music loops. No binary assets are needed.
- **Unreal layer** (`Game/`, `UI/`): director actor running a session, runtime meshes and
  textures, animated units, effects, audio mixer on a procedural sound wave, Slate menus, HUD,
  minimap and command card, full-screen input layer, suspend/resume on app background, saves.
- **Verification available without the engine**
  - 46 native tests (clang C++17 and g++ C++20, `-Wall -Wextra -Wshadow -Werror`, no exceptions,
    no RTTI), including a bot that wins every mission on Normal, and save/load determinism.
  - `Tools/UEStubs/check.py`: compiles every Unreal-layer file, and every header on its own,
    against declarations of the UE5 API (clang and g++): types, includes, format strings, name
    shadowing (an error in Unreal's MSVC builds) and case-only file name clashes.
  - WebGL preview of real game frames (same meshes, palette and lighting).
- **Final review fixes** (this stage): runtime textures now get unique object names (a new
  minimap per mission would otherwise have replaced a live texture in place); the audio thread
  can no longer see the mixer released under it at shutdown; the director initializes itself
  whichever of it and the controller starts first; the last minimap textures stay alive while
  the HUD may still show them; touch positions map to viewport pixels by fraction (correct with
  mobile resolution scaling); `BhHUD.h` renamed to `BhOverlayHUD.h` (it clashed with `BhHud.h`
  on Windows and macOS); all source files are pure ASCII; the Stop command moved from S (camera
  key) to T, with a test that keeps hotkeys off W/A/S/D and unique per card.
- **Build configuration**: `.uproject`, targets, module rules, mobile renderer settings (LDR,
  MSAA, no dynamic lighting features), Android (arm64, Vulkan + GLES, landscape, cutout-aware)
  and iOS (landscape, 60 fps) settings, input settings, `README.md` with exact steps.

## Remaining
**MUST (before calling the game done)**
1. First build in Unreal Engine 5.8 on Windows or Mac, and fix what it reports.
2. Play the campaign in the editor: check colours, facing of models (winding switch), UI layout,
   touch/mouse input, audio, pause/resume.
3. Package for Android and play on a real phone (performance, touch feel, back button,
   background/resume).

**NEXT**
- iOS device build and test.
- App icon and launch screen (the engine defaults are shown now).
- Replace `com.example.beaconhold` with the real package/bundle id; Play Store App Bundle and
  signing.
- Narrow the cooking of `/Engine/EngineMaterials` to the two materials the game uses, once a
  packaged build confirms them (smaller download).
- Device performance pass (draw calls, tree chunk rebuilds, audio buffer) on a mid-range phone.

**LATER**
- More missions, a skirmish mode, localization.

## Important decisions
- **Engine:** Unreal Engine 5 (5.8), C++ only. Everything visual and audible is generated at
  runtime, because `.uasset` files cannot be authored without the editor; the only engine assets
  used are the built-in widget pass-through materials and the default font.
- **Architecture:** all rules in the plain C++ core, tested natively; Unreal is a thin shell for
  presentation, input, UI, audio and platform lifecycle.
- **Rendering:** one unlit palette material for every mesh, lighting baked into the palette
  (flat-shaded low-poly look), blob shadows as decals. Mobile renders in LDR.
- **UI and input:** Slate in C++; one gesture recognizer for touch and mouse.
- **Orientation:** landscape only.

## Known limitations
- The Unreal layer has never been compiled or run in Unreal: the stub check proves consistency
  with the declared API, not with the engine. A first-build fix round is expected.
- Desktop Shipping builds keep the engine's tone mapper (it cannot be disabled there), so colours
  differ slightly from phones and the editor.
- Package and bundle ids are placeholders.
- Cooking the whole `/Engine/EngineMaterials` folder adds package size (see NEXT).

## Exact next action
Open `Beaconhold.uproject` in Unreal Engine 5.8, let it build the `Beaconhold` module, and send
the first compile error (if any) with file, line and message.
