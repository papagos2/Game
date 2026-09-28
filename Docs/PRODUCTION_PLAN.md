# Ashenveil production plan — 50-hour commercial target

This is a target and acceptance plan, not a claim that the current game is complete. Ashenveil remains an original single-player fantasy RPG. It should use no World of Warcraft names, art, maps, dialogue, music, or other protected material.

## Product decision

Keep the current playable TypeScript/Three.js/Capacitor prototype while measuring it on real Android hardware. A grounded stylized art direction is the practical mobile target: believable characters, materials, lighting and environments with clear silhouettes and readable combat. Photorealism across a 50-hour world would require a different asset and performance budget. Do not add procedural filler simply to inflate hours.

## Acceptance gates

1. **Reliable vertical slice:** one complete zone and boss, all three classes, touch controls, save/recovery and Android APK verified on at least a low-end and a mid-range device. Record frame-time percentiles, memory, battery and heat after a sustained play session. Verify app background/resume and offline operation.
2. **Art benchmark:** one production-quality player character for each class, one enemy family, one village and wilderness biome, combat animations, lighting, materials, sound and UI at the intended quality. Check readability and performance on the same devices before producing the rest of the world.
3. **Content pipeline:** reusable quest, encounter, dialogue and reward data with validation. Every new region needs authored traversal, story consequences, distinct enemies and mechanics; content is reviewed by playing it, not just by counting records.
4. **Campaign expansion:** create multiple acts with a beginning, escalation, ending and optional activities. Measure blind human completion time per act, route variety and abandonment points. The 50-hour claim is allowed only if representative players actually spend about that long on meaningful content without forced idle time or repetitive kill counts.
5. **Release candidate:** signed Android AAB and iOS TestFlight build, store assets and privacy documentation, accessibility and localization review, broad device matrix, crash-free endurance testing, save migration testing and external playtest feedback. Fix release blockers before store submission.

## Content budget to validate, not a promise

The existing 7-quest, 6-zone prototype is the prologue. A credible 50-hour solo RPG will likely need several full acts, dozens of distinct locations, many authored quest chains, varied encounter families, character progression beyond level 10, and substantial optional exploration. The exact counts should follow timed playtests: quest count alone is a poor proxy for enjoyable playtime. Build and test one additional 2–3 hour chapter before multiplying its design across the campaign.

## Current blockers

- No measured human playtime or Android/iOS device performance.
- No production art assets or animation workflow; current characters and scenery are geometric placeholders.
- No local Android SDK/ADB and no Mac/Xcode in the verified environment.
- One local save slot; a long campaign needs character management and export/recovery options before release.
- Native release signing, store metadata, privacy material and external QA have not been completed.

## Next implementation milestone

Obtain a real Android frame-time and touch baseline from an APK of the current branch. In parallel, define a visual reference sheet for one hero, Hearthmoor, Duskwood Glade and a Dusk Wolf, then integrate that complete set and compare it on device. Continue chapter production only after the art/performance target is proven.
