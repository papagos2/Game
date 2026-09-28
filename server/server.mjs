// Ashenveil online server: shared presence (see other heroes on the same map) and chat.
// Combat and progression stay on each device; the server only relays validated positions and chat.
// Start with `npm run server` (see server/README.md for configuration and deployment).
import { createServer } from 'node:http';
import { WebSocketServer } from 'ws';

const PROTOCOL = 1;
const CLASSES = new Set(['stormblade', 'emberseer', 'thornkeeper']);
const SPECS = new Set(['tempestKnight', 'bulwark', 'spellblade', 'pyromancer', 'frostweaver', 'thermancer', 'grovewarden', 'beastcaller', 'rotbloom']);
const WORLDS = new Set(['vale', 'frostmarch', 'sunscar']);
const LIMIT = 230;
const SNAPSHOT_HZ = 10;
const MAX_MESSAGE_BYTES = 1024;
const CHAT_MAX = 140;
const CHAT_INTERVAL_MS = 1500;
const POS_PER_SECOND = 20;
const IDLE_TIMEOUT_MS = 45000;

/** Small default word list; extend it with the BLOCKED_WORDS environment variable. */
const DEFAULT_BLOCKED = ['fuck', 'shit', 'cunt', 'nigger', 'faggot', 'retard', 'whore', 'slut'];

function cleanText(s, max) {
  return String(s ?? '')
    .replace(/[\u0000-\u001f\u007f​-‏‪-‮]/g, '')
    .replace(/\s+/g, ' ')
    .trim()
    .slice(0, max);
}

function cleanName(s) {
  const n = cleanText(s, 16).replace(/[^\p{L}\p{N} '-]/gu, '').trim();
  return n || 'Wanderer';
}

function num(v, lo, hi) {
  return typeof v === 'number' && Number.isFinite(v) ? Math.max(lo, Math.min(hi, v)) : null;
}

export function makeFilter(words) {
  const list = words.map((w) => w.toLowerCase()).filter(Boolean);
  return (text) => {
    let out = text;
    for (const w of list) {
      const re = new RegExp(w.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'gi');
      out = out.replace(re, '*'.repeat(w.length));
    }
    return out;
  };
}

/**
 * Starts the server.
 * @param {{ port?: number, maxClients?: number, perIp?: number, allowedOrigins?: string[], blockedWords?: string[], log?: (m: string) => void }} opts
 */
export function startServer(opts = {}) {
  const log = opts.log ?? ((m) => console.log(new Date().toISOString(), m));
  const maxClients = opts.maxClients ?? 500;
  const perIp = opts.perIp ?? 4;
  const allowed = opts.allowedOrigins ?? [];
  const filter = makeFilter([...DEFAULT_BLOCKED, ...(opts.blockedWords ?? [])]);
  const clients = new Map();
  const ipCount = new Map();
  let nextId = 1;

  const http = createServer((req, res) => {
    if (req.url === '/health') {
      res.writeHead(200, { 'content-type': 'application/json' });
      res.end(JSON.stringify({ ok: true, online: clients.size }));
      return;
    }
    res.writeHead(404);
    res.end();
  });

  const wss = new WebSocketServer({
    server: http,
    maxPayload: MAX_MESSAGE_BYTES,
    verifyClient: (info, done) => {
      const origin = info.origin ?? '';
      if (allowed.length && !allowed.includes(origin)) return done(false, 403, 'Origin not allowed');
      const ip = info.req.socket.remoteAddress ?? '?';
      if (clients.size >= maxClients) return done(false, 503, 'Server full');
      if ((ipCount.get(ip) ?? 0) >= perIp) return done(false, 429, 'Too many connections');
      done(true);
    },
  });

  const send = (c, msg) => {
    if (c.ws.readyState === 1) c.ws.send(JSON.stringify(msg));
  };

  wss.on('connection', (ws, req) => {
    const ip = req.socket.remoteAddress ?? '?';
    ipCount.set(ip, (ipCount.get(ip) ?? 0) + 1);
    const c = {
      id: nextId++, ws, ip, joined: false, name: 'Wanderer', cls: 'stormblade', spec: null, level: 1, title: null,
      world: null, x: 0, z: 0, f: 0, m: 0, mt: 0, lastChat: 0, posTokens: POS_PER_SECOND, lastSeen: Date.now(), strikes: 0,
    };
    clients.set(c.id, c);

    ws.on('message', (data, isBinary) => {
      c.lastSeen = Date.now();
      if (isBinary) return;
      let msg;
      try {
        msg = JSON.parse(String(data));
      } catch {
        return strike(c, 'bad json');
      }
      if (!msg || typeof msg !== 'object') return strike(c, 'bad message');
      switch (msg.t) {
        case 'hello': {
          if (msg.v !== PROTOCOL) {
            send(c, { t: 'sys', text: 'Please update the game to play online.' });
            return ws.close(4000, 'protocol');
          }
          c.name = cleanName(msg.name);
          c.cls = CLASSES.has(msg.cls) ? msg.cls : 'stormblade';
          c.spec = SPECS.has(msg.spec) ? msg.spec : null;
          c.level = Math.round(num(msg.level, 1, 30) ?? 1);
          c.title = msg.title ? cleanText(msg.title, 24) : null;
          c.world = WORLDS.has(msg.world) ? msg.world : null;
          c.joined = true;
          send(c, { t: 'welcome', id: c.id, online: clients.size });
          log(`join #${c.id} ${c.name} (${c.cls}) world=${c.world}`);
          return;
        }
        case 'pos': {
          if (!c.joined) return;
          if (c.posTokens <= 0) return;
          c.posTokens--;
          const x = num(msg.x, -LIMIT, LIMIT);
          const z = num(msg.z, -LIMIT, LIMIT);
          if (x === null || z === null) return strike(c, 'bad position');
          c.x = x;
          c.z = z;
          c.f = num(msg.f, -100, 100) ?? 0;
          c.m = msg.m ? 1 : 0;
          c.mt = msg.mt ? 1 : 0;
          c.world = WORLDS.has(msg.world) ? msg.world : null;
          if (typeof msg.level === 'number') c.level = Math.round(num(msg.level, 1, 30) ?? c.level);
          return;
        }
        case 'chat': {
          if (!c.joined || !c.world) return;
          const now = Date.now();
          if (now - c.lastChat < CHAT_INTERVAL_MS) return send(c, { t: 'sys', text: 'You are sending messages too quickly.' });
          const text = filter(cleanText(msg.text, CHAT_MAX));
          if (!text) return;
          c.lastChat = now;
          const out = { t: 'chat', from: c.id, name: c.name, text, ts: now };
          for (const o of clients.values()) if (o.joined && o.world === c.world) send(o, out);
          return;
        }
        case 'report': {
          const target = clients.get(Number(msg.id));
          if (target) log(`REPORT by #${c.id} ${c.name} against #${target.id} ${target.name}: ${cleanText(msg.reason, 100)}`);
          send(c, { t: 'sys', text: 'Thank you. The report was recorded.' });
          return;
        }
        default:
          return strike(c, 'unknown type');
      }
    });

    ws.on('close', () => {
      clients.delete(c.id);
      ipCount.set(ip, Math.max(0, (ipCount.get(ip) ?? 1) - 1));
      if (c.joined) log(`leave #${c.id} ${c.name}`);
    });
    ws.on('error', () => undefined);
  });

  function strike(c, why) {
    c.strikes++;
    if (c.strikes > 20) {
      log(`kick #${c.id}: ${why}`);
      c.ws.close(4001, 'misbehaving');
    }
  }

  // Snapshots of everyone else on the same map, 10 times a second.
  const tick = setInterval(() => {
    const byWorld = new Map();
    const now = Date.now();
    for (const c of clients.values()) {
      c.posTokens = Math.min(POS_PER_SECOND, c.posTokens + POS_PER_SECOND / SNAPSHOT_HZ);
      if (now - c.lastSeen > IDLE_TIMEOUT_MS) {
        c.ws.close(4002, 'idle');
        continue;
      }
      if (!c.joined || !c.world) continue;
      let arr = byWorld.get(c.world);
      if (!arr) byWorld.set(c.world, (arr = []));
      arr.push(c);
    }
    for (const [world, arr] of byWorld) {
      const players = arr.map((c) => ({ id: c.id, name: c.name, cls: c.cls, spec: c.spec, level: c.level, title: c.title, x: Math.round(c.x * 10) / 10, z: Math.round(c.z * 10) / 10, f: Math.round(c.f * 100) / 100, m: c.m, mt: c.mt }));
      for (const c of arr) send(c, { t: 'snap', world, online: clients.size, players: players.filter((p) => p.id !== c.id).slice(0, 60) });
    }
  }, 1000 / SNAPSHOT_HZ);

  return new Promise((resolve) => {
    http.listen(opts.port ?? 8787, () => {
      const port = http.address().port;
      log(`Ashenveil server listening on ${port}`);
      resolve({
        port,
        close: () => new Promise((r) => {
          clearInterval(tick);
          for (const c of clients.values()) c.ws.terminate();
          wss.close(() => http.close(() => r()));
        }),
      });
    });
  });
}
