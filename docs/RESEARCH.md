# Design research: what makes a WoW-like game work on phones

Short notes behind the features added in this round, with sources. Findings are summaries of the
sources; "decision" lines are our own design choices.

## 1. Mobile MMORPGs lean on convenience

- Successful mobile MMORPGs pair deep progression with conveniences such as auto-play/auto-pathing
  and touch-first controls, so players can progress in short sessions
  ([MMORPG.com, best mobile MMOs 2025](https://www.mmorpg.com/columns/best-mobile-mmos-to-play-in-2025-2000134219);
  [ExitLag, mobile MMORPG guide](https://www.exitlag.com/blog/best-mobile-mmorpgs-list/)).
- Players are split on full auto-battle: many enjoy auto-pathing (walking to the objective) but feel
  auto-combat removes the game ([MMO-Champion discussion](https://www.mmo-champion.com/threads/2665020-Why-everyone-complains-so-much-about-autopathing-autobattling-They-are-excellent);
  [AdventureQuest 3D community thread](https://steamcommunity.com/app/429790/discussions/3/3049485912466742694/)).
- Monetisation in some Korean mobile MMOs pushes auto-play and pay-to-progress, with retention
  risks ([arXiv 2504.10714](https://arxiv.org/pdf/2504.10714)).

**Decision:** auto-travel (tap a quest -> the hero mounts and walks there) and mounts, but combat
stays in the player's hands. No pay-to-win systems.

## 2. The WoW gameplay core

- Three roles: the tank takes the hits and holds enemies' attention, the healer keeps the group
  alive, damage dealers kill; the specialization decides the role
  ([Warcraft Wiki: Class role](https://warcraft.wiki.gg/wiki/Class_role);
  [Wowpedia: Class role](https://wowpedia.fandom.com/wiki/Class_role)).
- Group dungeons are central, with a finder that assembles a party
  ([Addictivepoints: role distinctions](https://addictivepoints.com/wow-role-distinctions/)).
- Gathering professions feed crafting professions (herbs -> potions, ore -> smithing).

**Decision:** 9 specializations already map to roles (Bulwark = tank, Grovewarden = healer, the
rest damage). Dungeons use AI companions that fill the roles you do not play, so a solo phone
player still gets the group experience. Gathering (herbs, ore) feeds crafting (draughts, elixirs,
reforging). Reputation, daily bounties and achievements/titles give long-term goals.

## 3. Online foundation

- Authoritative Node.js servers with WebSocket rooms are the common approach for web/mobile
  multiplayer (e.g. [Colyseus](https://docs.colyseus.io/)).

**Decision:** a small dependency-light server (`server/`, using `ws`) that relays validated
positions and chat per map. Combat stays on each device for now. Full shared combat (server-side
simulation, anti-cheat, accounts) is a separate, larger project that should follow real player
numbers. See `server/README.md`.

## 4. Store rules for chat (user-generated content)

App stores require apps with user-generated content to filter objectionable material, let users
report and block others, and act on reports. We provide a word filter, rate limits, mute and report;
a real moderation process (who reads reports, response time, terms of use, age rating) is still
needed before launch.
