# PROJECT_STATUS — Beaconhold (UE5 mobile RTS)

_Last updated: 2026-09-27 — stage: architecture & simulation core_

## Finished
- Environment inspection: no Unreal Engine available in the build container (network blocks Epic,
  GitHub releases, Android SDK). User chose to build the UE5 project "blind".
- Game design: `Docs/GAME_DESIGN.md`.

## In progress
- Pure C++ simulation core (all game rules) with a native test harness that runs full missions.

## Remaining
- UE5 layer (rendering from engine primitives, Slate UI, touch input, audio, save/load).
- Syntax check of UE layer against API stubs; visual preview; mobile build config; docs; review.

## Important decisions
- **Engine:** Unreal Engine 5 (target 5.6–5.8), C++ only. No binary assets are required: the level,
  visuals, UI, icons and sounds are generated from code at runtime, because `.uasset` files cannot
  be authored without the editor.
- **Architecture:** all gameplay rules live in a pure C++ core (`Source/Beaconhold/Sim`, no UE
  includes) that is compiled and tested here with UE-like strict flags. The UE layer is a thin
  presentation/input/UI shell over it.
- **Orientation:** landscape.
- **UI:** Slate in C++ (no Widget Blueprints). **Input:** handled in a Slate input layer (touch + mouse),
  independent of the legacy/Enhanced Input systems.

## Known limitations
- The UE layer cannot be compiled or run in this environment; first compile on your machine may
  surface errors that need a fix round.

## Exact next action
- Finish the simulation core and its automated mission playthrough tests.
