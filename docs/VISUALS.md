# Visual work guide (for the art/visuals contributor)

This file is for whoever improves the look of Ashenveil (another AI or a human artist). It says
where every visual lives, what you can change freely, and what you must not break.

## Rules

1. **Do not change gameplay files** unless asked: `src/game/rules.ts`, `src/game/progress.ts`,
   `src/game/abilities.ts`, `src/data/classes.ts` (numbers and effects), `src/data/talents.ts`,
   `src/data/quests.ts`, the `MOBS` stats and camp positions in `src/data/world.ts`. Balance was
   tested with a bot that plays the whole game (`npm run playthrough`). Visual changes must not
   change hit radii, speeds, ranges or collider sizes.
2. **Keep it original.** No assets, names, logos or look-alikes from World of Warcraft or any other
   game. Only use art you made or that has a licence allowing commercial use in an app, and
   record the source and licence in `docs/ASSETS.md` (create it).
3. **Mobile budget.** Target: 60 fps on a mid-range phone from ~2021 on "High", 30+ fps on
   "Battery saver". Keep draw calls low (use `InstancedMesh` for anything repeated), avoid
   real-time lights beyond the existing sun and hemisphere light, keep textures at most
   1024x1024 and compressed (KTX2 / Basis) if you add them. App download size matters.
4. **Must still pass:** `npm run build`, `npm test`, `npm run smoke` (and look at the screenshots in
   `tests/output/`).

## Where things are

| What | File | Notes |
|---|---|---|
| Map colours, fog, sky, sun, glow, houses, tents, scenery rules | `src/data/world.ts` -> `MAPS[id].theme` | Pure data. Safest place to start restyling each map. |
| Terrain mesh, scenery, village, camps, boss arena, waystone | `src/game/scene.ts` | Visual only (plus the collider list: keep sizes). `floraGeometry()` builds each scenery kind. |
| Terrain colour by height/slope/zone | `src/game/terrain.ts` -> `colorAt()` | Heights (`heightAt`) affect gameplay; do not change them. |
| Hero, enemy, NPC and pet models + animation | `src/game/models.ts` | Built from primitives. A `Rig` has `root`, `body`, `legs`, `arms`, `head`. You can replace a builder with a loaded glTF model as long as it returns a `Rig` with similar size (`height`, `radius`). |
| Spell effects (projectiles, rings, lightning, telegraphs) | `src/game/game.ts` -> section "Projectiles, telegraphs, effects" | Colours come from ability data. |
| Ability look (icon glyph + colours, effect colours) | `src/data/classes.ts` -> `glyph`, `color`, `fx`, `projectile.color` | Only these fields are visual. |
| Ability icons | `src/ui/icons.ts` | SVG paths. Could be replaced with painted PNG/WebP icons. |
| HUD, panels, fonts, colours | `src/styles.css` (tokens in `:root`), `index.html`, `src/ui/hud.ts`, `src/ui/screens.ts` | Keep touch targets at least 44 px and the safe-area insets. |
| App icon and splash screens | `tools/gen-assets.mjs` | Run `node tools/gen-assets.mjs` after changing it. |
| Sounds | `src/game/audio.ts` | Synthesised. Could be swapped for short OGG/M4A files. |

## Ideas with the biggest visual payoff

1. Real character models (glTF, low-poly, with skeletal animation for idle/run/attack/death).
2. Painted ability icons and a matching UI frame style.
3. Terrain textures blended by zone instead of flat vertex colours.
4. Particle effects for spells (GPU-friendly, pooled).
5. Distinct NPC outfits and a proper waystone/portal effect.

## How to see your change

```bash
npm install
npm run dev          # open the printed address on a phone in the same Wi-Fi
npm run smoke        # headless run; screenshots in tests/output/
```
