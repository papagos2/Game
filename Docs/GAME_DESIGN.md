# Beaconhold — Game Design (v1)

> *Keep the light. Hold the vale.*

An original, mobile-first fantasy real-time strategy game. Classic base-building RTS feel
(gather, build, train, expand, fight), designed for phones and tablets.

## World

The Vale of Hollowmere has fallen under **the Gloam** — a creeping twilight that wakes thorn,
bog and bone into raiders. The last **Wardens** relight the ancient Beacons one outpost at a
time. Where a Beacon burns, the Gloam cannot rest.

| Faction | Look | Palette |
|---|---|---|
| **Wardens** (player) | Cream stone, round towers, conical roofs, lanterns | Warm cream, royal blue, brass gold |
| **Gloam** (enemy) | Dark bark, thorns, bone, glowing bog-light | Violet, bog green, bone white |

## Resources

| Resource | Source | Notes |
|---|---|---|
| **Sunstone** | Golden crystal outcrops (finite) | Main currency. Outcrops run dry, so you must expand. |
| **Timber** | Trees (each tree is felled after a few trips) | Buildings and advanced units. |
| **Supply** | Beacon Keep (+10), Cottages (+8), max 100 | Army size cap. |

Resources are returned to the **Beacon Keep** or a **Storehouse** (cheap outpost used to expand).

## Units (Wardens)

| Unit | Role | Ability (instant, no targeting — mobile friendly) |
|---|---|---|
| Lamplighter | Worker: gathers, builds, repairs | — |
| Shieldbearer | Melee tank | **Brace**: take 50% less damage for 6 s |
| Ranger | Ranged damage | **Volley**: double attack speed for 5 s |
| Stag Rider | Fast shock cavalry | **Charge**: +60% speed, next hit deals double |
| Lumen Sage | Healer / caster (auto-heals) | **Sunburst**: damages nearby Gloam and heals nearby allies |

## Buildings (Wardens)

| Building | Purpose | Requires |
|---|---|---|
| Beacon Keep | Trains Lamplighters, drop-off, +10 supply | — |
| Cottage | +8 supply | — |
| Storehouse | Extra drop-off point for expansions | — |
| Muster Hall | Trains Shieldbearers, Rangers | — |
| Forge | Researches weapon/armor/range upgrades | Muster Hall |
| Stag Lodge | Trains Stag Riders | Forge |
| Sanctum | Trains Lumen Sages, researches Lantern Wisdom | Muster Hall |
| Watchtower | Defensive tower | Muster Hall |
| Beacon | Built only on ancient Beacon sites. Heals nearby allies, grants Sunstone income | — |

## The Gloam (enemy)

Units: **Gloomling** (fast swarm), **Thornback** (armored brute), **Hexer** (ranged caster),
**Bog Titan** (boss, splash damage).
Buildings: **Gloam Heart** (core), **Burrow** (melee spawner), **Hexroot** (caster spawner),
**Thorn Spire** (tower).

The enemy AI earns "gloom" over time, trains from its buildings, keeps a home guard, sends
escalating attack waves (preferring outlying Beacons), and counter-attacks when its base is hit.
Difficulty (Easy / Normal / Hard) scales income, wave size and timing.

## Campaign

| # | Mission | Goal | Teaches |
|---|---|---|---|
| 1 | **Kindling** | Destroy the Gloam Burrow | Guided tutorial: camera, select, gather, build, train, attack |
| 2 | **The Long Dusk** | Relight both Beacons and survive until dawn | Defense, towers, abilities |
| 3 | **Heart of the Gloam** | Destroy the Gloam Heart | Full RTS vs. adaptive AI, expansion |

Each mission awards up to **3 stars** (victory, par time, Keep kept above half health).
Stars grant **Renown**, spent on permanent **Warden Boons** (+HP, +damage, faster gathering,
faster building, more starting resources). Boons can be reset for free.

## Controls (touch first)

| Gesture | Action |
|---|---|
| Tap own unit/building | Select it |
| Tap ground / enemy / resource with units selected | Smart command: move (soldiers attack on the way), attack, gather |
| Double-tap own unit | Select all visible units of that type |
| Long-press, then drag | Selection box |
| One-finger drag | Pan camera (with inertia) |
| Pinch / two-finger drag | Zoom / pan |
| HUD: Army / Idle worker / Home | One-tap selection shortcuts |

Desktop (editor testing): left click = tap, left drag = box select, right click = command,
middle drag / WASD / arrows = pan, wheel = zoom, Esc = pause.

## Orientation: landscape

An RTS needs horizontal room to see the battlefield around the selection while both thumbs
reach the minimap (left) and the action grid (right). Portrait would halve the visible
battlefield width. Both landscape rotations are supported.
