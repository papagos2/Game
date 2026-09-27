#!/usr/bin/env python3
"""Syntax and type check of Beaconhold's Unreal layer without the engine.

Copies Source/Beaconhold to Tools/UEStubs/build/src, expands GENERATED_BODY() the way the
generated code does for what the game relies on (Super, ThisClass, StaticClass), splits
UEStub.h into per-section headers, makes a shim for every engine header the code includes
(pulling in only the sections that header provides) and runs `clang++ -fsyntax-only` on
every .cpp file of Game/ and UI/.

Engine includes must be listed in ENGINE_HEADERS: a path that is not listed fails the
check, so a misspelt or invented engine header is caught here rather than in Unreal.

A clean run proves the code is consistent with UEStub.h. It does not prove the engine has
exactly that API; only a build in Unreal does.

Usage: python3 Tools/UEStubs/check.py [file.cpp ...]
"""
import os
import re
import shutil
import subprocess
import sys

# Every engine header the game module may include (UE 5.x paths), with the UEStub.h
# sections it provides. A header exposes only what its real counterpart declares or reliably
# includes, so a file that relies on an include it does not have fails the check.
ENGINE_HEADERS = {
    "Async/Async.h": ["Async"],
    "Brushes/SlateColorBrush.h": ["SlateColorBrush"],
    "Camera/CameraComponent.h": ["CameraComponent"],
    "CanvasItem.h": ["CanvasItem"],
    "Components/AudioComponent.h": ["AudioComponent"],
    "Components/SceneComponent.h": ["SceneComponent"],
    "Components/StaticMeshComponent.h": ["StaticMeshComponent"],
    "CoreMinimal.h": ["Core"],
    "Engine/Canvas.h": ["Canvas"],
    "Engine/DeveloperSettings.h": ["DeveloperSettings"],
    "Engine/Engine.h": ["Engine"],
    "Engine/Font.h": ["Font"],
    "Engine/GameInstance.h": ["GameInstance"],
    "Engine/GameViewportClient.h": ["GameViewportClient"],
    "Engine/StaticMesh.h": ["StaticMesh"],
    "Engine/Texture2D.h": ["Texture"],
    "Engine/World.h": ["World"],
    "Fonts/SlateFontInfo.h": ["SlateFontInfo"],
    "Framework/Application/SlateApplication.h": ["SlateApplication"],
    "GameFramework/Actor.h": ["Actor"],
    "GameFramework/GameModeBase.h": ["GameModeBase"],
    "GameFramework/HUD.h": ["HUD"],
    "GameFramework/PlayerController.h": ["PlayerController"],
    "GameFramework/SaveGame.h": ["SaveGame"],
    "HAL/CriticalSection.h": ["Threads"],
    "HAL/PlatformTime.h": ["PlatformTime"],
    "Input/Reply.h": ["SlateCore"],
    "InputCoreTypes.h": ["InputCore"],
    "Kismet/GameplayStatics.h": ["GameplayStatics"],
    "Kismet/KismetSystemLibrary.h": ["KismetSystemLibrary"],
    "Layout/Margin.h": ["Margin"],
    "MaterialDomain.h": ["MaterialDomain"],
    "Materials/Material.h": ["Material"],
    "Materials/MaterialInstanceDynamic.h": ["MaterialInstanceDynamic"],
    "Materials/MaterialInterface.h": ["MaterialInterface"],
    "MeshDescription.h": ["MeshDescription"],
    "Misc/Attribute.h": ["Attribute"],
    "Misc/CoreDelegates.h": ["CoreDelegates"],
    "Misc/ScopeLock.h": ["Threads"],
    "Modules/ModuleManager.h": ["Modules"],
    "Sound/SoundBase.h": ["SoundBase"],
    "Sound/SoundWaveProcedural.h": ["SoundWaveProcedural"],
    "StaticMeshAttributes.h": ["StaticMeshAttributes"],
    "Styling/CoreStyle.h": ["CoreStyle"],
    "Styling/SlateBrush.h": ["SlateCore"],
    "Styling/SlateColor.h": ["SlateCore"],
    "Styling/SlateTypes.h": ["SlateTypes"],
    "Templates/Function.h": ["Core"],
    "TextureResource.h": ["TextureResource"],
    "UObject/Object.h": ["CoreUObject"],
    "UObject/Package.h": ["Package"],
    "UObject/SoftObjectPtr.h": ["CoreUObject"],
    "Widgets/Images/SImage.h": ["SImage"],
    "Widgets/Input/SButton.h": ["SButton"],
    "Widgets/Layout/SBorder.h": ["SBorder"],
    "Widgets/Layout/SBox.h": ["SBox"],
    "Widgets/Layout/SSafeZone.h": ["SSafeZone"],
    "Widgets/Layout/SSpacer.h": ["SSpacer"],
    "Widgets/SBoxPanel.h": ["SBoxPanel"],
    "Widgets/SCompoundWidget.h": ["SlateCore"],
    "Widgets/SNullWidget.h": ["SNullWidget"],
    "Widgets/SOverlay.h": ["SOverlay"],
    "Widgets/SWidget.h": ["SlateCore"],
    "Widgets/Text/STextBlock.h": ["STextBlock"],
}

SECTION_RE = re.compile(r"^//@section (\w+)(?::\s*(.*))?$")


def write_sections(stub_path, out_dir):
    """Splits UEStub.h into one header per section; returns the section names."""
    chunks = {}
    requires = {}
    current = None
    with open(stub_path) as f:
        for line in f.read().split("\n"):
            match = SECTION_RE.match(line)
            if match:
                current = match.group(1)
                chunks.setdefault(current, [])
                requires.setdefault(current, [])
                for req in (match.group(2) or "").split():
                    if req not in requires[current]:
                        requires[current].append(req)
                continue
            if current is not None:
                chunks[current].append(line)
    os.makedirs(out_dir)
    for name, body in chunks.items():
        for req in requires[name]:
            if req not in chunks:
                sys.exit(f"UEStub.h: section {name} requires unknown section {req}")
        # "Stub" prefix: a shim such as HAL/PlatformTime.h must not find itself when it
        # includes its section (quoted includes search the including file's folder first).
        with open(os.path.join(out_dir, "Stub" + name + ".h"), "w") as f:
            f.write("#pragma once\n")
            for req in requires[name]:
                f.write(f'#include "Stub{req}.h"\n')
            f.write("\n".join(body) + "\n")
    return set(chunks)


INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.M)
CLASS_RE = re.compile(r'\b(class|struct)\s+(?:\w+_API\s+)?(\w+)\s*(?:final\s*)?(?::\s*public\s+(\w+))?\s*\{')


def expand_generated_body(text, path):
    """Replaces each GENERATED_BODY() with what the code needs from the generated macro."""
    out = []
    pos = 0
    for match in re.finditer(r'GENERATED_BODY\(\)', text):
        head = text[: match.start()]
        decls = list(CLASS_RE.finditer(head))
        if not decls:
            sys.exit(f"{path}: GENERATED_BODY() outside a class")
        kind, name, base = decls[-1].groups()
        if kind == "class":
            if not base:
                sys.exit(f"{path}: reflected class {name} has no base class")
            body = (f"public: using Super = {base}; using ThisClass = {name}; "
                    f"static UClass* StaticClass(); private:")
        else:
            body = "static class UScriptStruct* StaticStruct();"
        out.append(text[pos: match.start()])
        out.append(body)
        pos = match.end()
    out.append(text[pos:])
    return "".join(out)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(here, "..", ".."))
    source = os.path.join(root, "Source", "Beaconhold")
    build = os.path.join(here, "build")
    copy = os.path.join(build, "src")
    shims = os.path.join(build, "include")

    shutil.rmtree(build, ignore_errors=True)
    shutil.copytree(source, copy)
    os.makedirs(shims)

    sections = write_sections(os.path.join(here, "UEStub.h"), os.path.join(build, "stub"))
    for header, provided in ENGINE_HEADERS.items():
        path = os.path.join(shims, header)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write("#pragma once\n")
            for section in provided:
                if section not in sections:
                    sys.exit(f"check.py: {header} maps to unknown section {section}")
                f.write(f'#include "Stub{section}.h"\n')

    errors = []
    sources = []
    # Windows and macOS ignore case: two files whose names differ only in case would let a quoted
    # include find the wrong one (it searches the including file's folder first) and would give
    # two object files the same name.
    seen = {}
    for folder, _, files in os.walk(source):
        for name in files:
            if name.endswith((".h", ".cpp")):
                seen.setdefault(name.lower(), []).append(os.path.relpath(os.path.join(folder, name), source))
    for paths in seen.values():
        if len(paths) > 1:
            errors.append("file names differ only in case: " + ", ".join(sorted(paths)))
    for folder in ("Game", "UI"):
        for name in sorted(os.listdir(os.path.join(copy, folder))):
            path = os.path.join(copy, folder, name)
            with open(path) as f:
                text = f.read()
            # Every include must resolve to the module, the simulation or a listed engine header.
            for bracket, include in INCLUDE_RE.findall(text):
                if bracket == "<":
                    continue
                if include.endswith(".generated.h"):
                    stem = include[: -len(".generated.h")]
                    if stem + ".h" != name:
                        errors.append(f"{folder}/{name}: {include} does not match the file name")
                    open(os.path.join(shims, include), "w").close()
                    continue
                local = [os.path.join(copy, include), os.path.join(copy, "Sim", include)]
                if any(os.path.exists(p) for p in local) or include in ENGINE_HEADERS:
                    continue
                errors.append(f"{folder}/{name}: unknown include \"{include}\"")
            if name.endswith(".h") and "GENERATED_BODY()" in text:
                if f'#include "{name[:-2]}.generated.h"' not in text:
                    errors.append(f"{folder}/{name}: missing {name[:-2]}.generated.h include")
                with open(path, "w") as f:
                    f.write(expand_generated_body(text, path))
            label = f"{folder}/{name}"
            if name.endswith(".cpp"):
                sources.append((label, path))
            elif name.endswith(".h"):
                # Every header must compile on its own (include what you use).
                alone = os.path.join(build, "headers", folder + "_" + name[:-2] + ".cpp")
                os.makedirs(os.path.dirname(alone), exist_ok=True)
                with open(alone, "w") as f:
                    f.write(f'#include "{label}"\n')
                sources.append((label, alone))

    if errors:
        print("\n".join(errors))
        return 1

    wanted = [os.path.normpath(a) for a in sys.argv[1:]]
    if wanted:
        sources = [s for s in sources if any(s[0].endswith(w) or w.endswith(s[0]) for w in wanted)]

    # clang by default; CXX=g++ adds GCC's -Wshadow, which (like MSVC, where Unreal makes
    # shadowing an error) also flags parameters named like a member of their class.
    compiler = os.environ.get("CXX", "clang++")
    includes = ["-I", os.path.join(build, "stub"), "-I", shims, "-I", copy, "-I", os.path.join(copy, "Sim")]
    common = ["-fsyntax-only", "-std=c++20", "-fno-exceptions", "-fno-rtti", "-Wall", "-Wextra", "-Wno-unused-parameter"]
    if "clang" in compiler:
        flags = [compiler] + common + ["-Wshadow-all", "-Wno-unused-private-field", "-ferror-limit=50", "-fno-caret-diagnostics"] + includes
    else:
        flags = [compiler] + common + ["-Wshadow", "-fmax-errors=50", "-fno-diagnostics-show-caret"] + includes
    failed = 0
    for label, path in sources:
        result = subprocess.run(flags + [path], capture_output=True, text=True)
        output = (result.stdout + result.stderr).replace(copy + os.sep, "")
        if result.returncode != 0 or output.strip():
            failed += 1
            print(f"---- {label}")
            print(output.rstrip())
        else:
            print(f"ok   {label}")
    print(f"{len(sources) - failed}/{len(sources)} files clean (each .cpp, and each header on its own)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
