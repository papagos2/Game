# Ashenveil online server

Relays positions (so heroes on the same map see each other) and chat. Combat, loot and quests stay
on each device, so the game works fully offline and the server is cheap to run.

## Run locally

```bash
npm install
npm run server            # listens on port 8787
```

In the game: **Menu -> Online**, enter `ws://<your-computer-ip>:8787`, tap **Go online**.

## Configuration (environment variables)

| Variable | Default | Meaning |
|---|---|---|
| `PORT` | 8787 | Port to listen on |
| `MAX_CLIENTS` | 500 | Total connections |
| `MAX_PER_IP` | 4 | Connections from one address |
| `ALLOWED_ORIGINS` | (any) | Comma-separated web origins allowed to connect (set this in production) |
| `BLOCKED_WORDS` | (none) | Extra comma-separated words for the chat filter |

`GET /health` returns `{ ok, online }` for uptime checks.

## Deploy

A `Dockerfile` is in this folder (build from the repository root:
`docker build -f server/Dockerfile -t ashenveil-server .`). Any host that runs a container and
supports WebSockets works. Put it behind HTTPS so the app can use `wss://` (phones and app stores
require secure connections). Then build the app with the address baked in:
`VITE_SERVER_URL=wss://play.example.com npm run build`.

## What it protects against

Message size limit (1 KB), position rate limit (20/s), chat rate limit (1 per 1.5 s), 140-character
chat, control/direction characters stripped, names cleaned, numbers clamped to the map, unknown
classes/maps rejected, connections per IP capped, idle clients dropped, misbehaving clients kicked,
dungeons kept private, protocol version check. Reports are written to the server log.

## Not yet (needed before a public launch)

- Accounts and authentication (today anyone can pick any name).
- A moderation process for reports, terms of use and a privacy policy (chat is user content).
- Shared combat, parties between real players, trading and a server-side save (anti-cheat).
- Horizontal scaling (one process holds everyone; fine for hundreds of players).
