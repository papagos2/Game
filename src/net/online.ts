// Client for the optional online server: shares your position, shows other heroes and carries chat.
// Everything else (combat, loot, quests) stays on the device, so the game also works fully offline.
import type { ClassId, SpecId } from '../data/classes';

export const PROTOCOL = 1;

export interface RemotePlayer {
  id: number;
  name: string;
  cls: ClassId;
  spec: SpecId | null;
  level: number;
  title: string | null;
  x: number;
  z: number;
  f: number;
  m: number;
  mt: number;
}

export interface ChatLine { from: number; name: string; text: string; ts: number; system?: boolean }

export type OnlineStatus = 'off' | 'connecting' | 'online' | 'error';

export interface Hello { name: string; cls: ClassId; spec: SpecId | null; level: number; title: string | null; world: string | null }

export class Online {
  status: OnlineStatus = 'off';
  myId = 0;
  population = 0;
  world: string | null = null;
  players = new Map<number, RemotePlayer>();
  chat: ChatLine[] = [];
  muted = new Set<string>();
  onChat: ((line: ChatLine) => void) | null = null;
  private ws: WebSocket | null = null;
  private lastPos = 0;
  private retry = 0;
  private retryTimer = 0;
  private wanted = false;

  constructor(private url: string, private hello: () => Hello) {
    try {
      const m = localStorage.getItem('ashenveil.muted');
      if (m) for (const n of JSON.parse(m) as string[]) this.muted.add(n);
    } catch {
      /* ignore */
    }
  }

  connect() {
    this.wanted = true;
    if (this.ws || !this.url) return;
    this.status = 'connecting';
    let ws: WebSocket;
    try {
      ws = new WebSocket(this.url);
    } catch {
      this.status = 'error';
      return;
    }
    this.ws = ws;
    ws.onopen = () => {
      this.retry = 0;
      ws.send(JSON.stringify({ t: 'hello', v: PROTOCOL, ...this.hello() }));
    };
    ws.onmessage = (e) => this.onMessage(String(e.data));
    ws.onclose = () => {
      this.ws = null;
      this.players.clear();
      if (!this.wanted) {
        this.status = 'off';
        return;
      }
      this.status = 'error';
      // Reconnect with back-off (1 s, 2 s, 4 s ... 30 s).
      this.retry = Math.min(this.retry + 1, 5);
      clearTimeout(this.retryTimer);
      this.retryTimer = window.setTimeout(() => this.connect(), 1000 * 2 ** (this.retry - 1));
    };
    ws.onerror = () => undefined;
  }

  disconnect() {
    this.wanted = false;
    clearTimeout(this.retryTimer);
    this.ws?.close();
    this.ws = null;
    this.players.clear();
    this.status = 'off';
  }

  private onMessage(raw: string) {
    let m: Record<string, unknown>;
    try {
      m = JSON.parse(raw);
    } catch {
      return;
    }
    if (m.t === 'welcome') {
      this.status = 'online';
      this.myId = Number(m.id);
      this.population = Number(m.online) || 0;
    } else if (m.t === 'snap') {
      this.population = Number(m.online) || this.population;
      const seen = new Set<number>();
      for (const p of (m.players as RemotePlayer[]) ?? []) {
        if (!p || typeof p.id !== 'number' || this.muted.has(p.name)) continue;
        seen.add(p.id);
        this.players.set(p.id, p);
      }
      for (const id of [...this.players.keys()]) if (!seen.has(id)) this.players.delete(id);
    } else if (m.t === 'chat') {
      const line: ChatLine = { from: Number(m.from), name: String(m.name ?? '?'), text: String(m.text ?? ''), ts: Number(m.ts) || Date.now() };
      if (this.muted.has(line.name)) return;
      this.push(line);
    } else if (m.t === 'sys') {
      this.push({ from: 0, name: '', text: String(m.text ?? ''), ts: Date.now(), system: true });
    }
  }

  private push(line: ChatLine) {
    this.chat.push(line);
    if (this.chat.length > 60) this.chat.shift();
    this.onChat?.(line);
  }

  /** Sends our position at most 8 times a second. */
  sendPos(now: number, x: number, z: number, f: number, moving: boolean, mounted: boolean, level: number) {
    if (this.status !== 'online' || !this.ws || now - this.lastPos < 0.125) return;
    this.lastPos = now;
    this.ws.send(JSON.stringify({ t: 'pos', x, z, f, m: moving ? 1 : 0, mt: mounted ? 1 : 0, world: this.world, level }));
  }

  say(text: string) {
    const t = text.trim().slice(0, 140);
    if (!t || this.status !== 'online' || !this.ws) return false;
    this.ws.send(JSON.stringify({ t: 'chat', text: t }));
    return true;
  }

  report(id: number, reason: string) {
    this.ws?.send(JSON.stringify({ t: 'report', id, reason }));
  }

  mute(name: string) {
    this.muted.add(name);
    this.chat = this.chat.filter((l) => l.name !== name);
    for (const [id, p] of this.players) if (p.name === name) this.players.delete(id);
    try {
      localStorage.setItem('ashenveil.muted', JSON.stringify([...this.muted]));
    } catch {
      /* ignore */
    }
  }
}

/** Server address: a saved setting, or the one baked in at build time (VITE_SERVER_URL). */
export function serverUrl(): string {
  try {
    const saved = localStorage.getItem('ashenveil.server');
    if (saved !== null) return saved;
  } catch {
    /* ignore */
  }
  return (import.meta.env.VITE_SERVER_URL as string | undefined) ?? '';
}
