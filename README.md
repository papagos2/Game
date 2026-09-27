# Beaconhold

An original fantasy real-time strategy game for phones and tablets (iOS and Android), built in
Unreal Engine 5 with C++. The last Wardens relight the Beacons of the Vale of Hollowmere and hold
back the Gloam: gather Sunstone and Timber, build a base, train an army, and win a three-mission
campaign with a guided tutorial, stars, permanent upgrades and three difficulty levels.

World, story, names, models, icons, UI and sounds are all original. Nothing is copied from other
games, and the project contains no third-party art or audio.

## Status: read this first

| Part | State |
|---|---|
| Game rules, enemy AI, missions, tutorial, saving (`Source/Beaconhold/Sim`) | Done. 46 automated tests pass with two compilers; a scripted player wins every mission on Normal difficulty. |
| Unreal layer: rendering, UI, touch input, audio, app lifecycle (`Game/`, `UI/`) | Written, and compile-checked against declarations of the Unreal API (`Tools/UEStubs`). **Not yet compiled or run inside Unreal Engine.** |
| Look and feel | Checked in a WebGL preview that uses the same meshes, colours and lighting as the game. |
| Packaging for Android and iOS | Configured, not yet tried on a device. |

This project was written without access to Unreal Engine. Expect the first build in Unreal to
need a short round of fixes; see [If something goes wrong](#if-something-goes-wrong).

## What you need

- **Unreal Engine 5.8** from the Epic Games Launcher. Another 5.x version may work: right-click
  `Beaconhold.uproject` and choose *Switch Unreal Engine version*.
- **Windows:** Visual Studio 2022 or newer with the *Game development with C++* workload
  (Epic's "Setting up Visual Studio" page lists the exact components for your engine version).
  **Mac:** Xcode.
- **Android builds:** Android Studio, SDK and NDK installed as described in Epic's
  "Set up Android SDK, NDK and Android Studio" page for your engine version.
- **iOS builds:** a Mac with Xcode and an Apple Developer account.

## First run on a computer

1. Right-click `Beaconhold.uproject` and choose *Generate Visual Studio project files*
   (on Mac: *Generate Xcode project*).
2. Double-click `Beaconhold.uproject`. When asked to rebuild the missing `Beaconhold` module,
   choose **Yes**. (Or open `Beaconhold.sln`, pick *Development Editor*, and build.)
3. The editor opens an empty map. That is expected: the game builds its world, models, textures,
   sounds and UI in code when it starts. Press **Play**.

The game opens on its main menu. Choose *Campaign*, then *Kindling* (the tutorial mission).

### Controls

| Touch | Mouse and keyboard | Action |
|---|---|---|
| Tap | Left click | Select a unit or building |
| Tap ground, enemy or resource | Right click | Move, attack or gather with the selected units |
| Double-tap a unit | Double click | Select all visible units of that type |
| Long-press, then drag | Left drag | Selection box |
| Drag with one finger | Right drag, WASD or arrow keys | Move the camera |
| Pinch | Mouse wheel | Zoom |
| Tap the minimap | Click the minimap | Jump the camera |
| Command card buttons | Letter shown on each button | Train, build, research, abilities |
| Pause button | Space or Esc | Pause menu |
| Android back button | Esc | Back |

## Build for Android

1. In the editor: *Edit > Project Settings > Platforms > Android*. Click *Accept SDK License*.
2. Replace the package name `com.example.beaconhold` with your own (for example
   `com.yourname.beaconhold`). Stores refuse `com.example`.
3. *Platforms > Android > Package Project* (pick the ASTC texture format if asked) and choose
   an output folder.
4. With USB debugging enabled on the phone, run the `Install_Beaconhold...` script in the output
   folder, or install the `.apk` directly.

The game is landscape-only and uses the whole screen, including the camera cutout area; the
HUD keeps clear of cutouts and rounded corners.

Publishing on Google Play additionally needs an App Bundle (`.aab`) and your own signing key
(*Project Settings > Platforms > Android > Distribution Signing*).

## Build for iOS

1. On a Mac: *Edit > Project Settings > Platforms > iOS*. Replace the bundle identifier
   `com.example.beaconhold` with the one from your Apple Developer account and select your
   signing team.
2. *Platforms > iOS > Package Project*.

## If something goes wrong

| What you see | What to do |
|---|---|
| The build fails with compile errors | Expected on the very first build. Copy the **first** error from Visual Studio's Output window or the editor's Output Log (file, line and message) and send it; errors are usually a line or two to fix. |
| The world is invisible, or you see the inside of objects | In `Config/DefaultGame.ini`, under `[/Script/Beaconhold.BhSettings]`, change `bReverseTriangleWinding=True` to `False`. |
| Everything is grey or checkered in a packaged build | The engine materials the game uses were not packaged. The log says `engine pass-through materials not found`. Check that `Config/DefaultGame.ini` still contains the `DirectoriesToAlwaysCook` line for `/Engine/EngineMaterials`. |
| No sound | Check the device volume and the in-game Settings. The log should show `audio synthesized in ... ms`. |
| Colours look a little darker or flatter on a computer | Expected in a Shipping build on desktop: the engine's tone mapping cannot be switched off there. Phones, and the editor, show the intended colours. |

Useful log lines all start with `Beaconhold:` (*Window > Output Log* in the editor; `Saved/Logs`
in packaged desktop builds; `adb logcat` on Android).

## Project layout

```
Beaconhold.uproject      the Unreal project (code only; no .uasset content is needed)
Config/                  engine, game, input and mobile platform settings
Source/Beaconhold/
  Sim/                   all game rules in plain C++ (no Unreal headers): world, units,
                         combat, AI, missions, tutorial, camera, gestures, HUD model, saving,
                         procedural models, textures, icons and sounds
  Game/                  Unreal actors and objects: director (runs a session), content
                         factory, world view, effects, audio mixer, game instance and saves
  UI/                    Slate user interface: menus, HUD, command card, minimap, input layer
Docs/GAME_DESIGN.md      game design
PROJECT_STATUS.md        what is finished, what remains, decisions and limitations
Tools/                   development tools (not part of the game)
```

## Development tools

All optional; none is needed to build or play the game.

- **Simulation tests** (CMake and a C++17 compiler):
  ```
  cmake -S Tools/SimTests -B Tools/SimTests/build -DCMAKE_BUILD_TYPE=Release
  cmake --build Tools/SimTests/build
  Tools/SimTests/build/bhtests
  ```
- **Unreal layer compile check** without the engine (Python 3 and clang, or `CXX=g++`):
  `python3 Tools/UEStubs/check.py`. It catches type errors, missing includes, file name clashes
  and wrong format strings; it cannot prove the Unreal API matches, only a real build can.
- **Visual preview** (Node.js, three.js and Playwright): `Tools/SimTests/build/bhrender <mission>
  <seconds> <dir>` exports a frame of the game, and `node Tools/Preview/render.mjs <dir> out.png`
  draws it.
- **Map painter:** `python3 Tools/MapGen/mapgen.py` regenerates the campaign maps.

## Licensing

The game code, models, textures, icons, sounds and music are original to this project and are
generated by its own code. It uses Unreal Engine (under Epic's EULA), including the engine's
built-in widget materials and default font. The preview tool uses three.js (MIT licence); it
is not part of the game.
