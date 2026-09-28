import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import WebSocket from 'ws';
// @ts-expect-error plain JavaScript module
import { makeFilter, startServer } from '../server/server.mjs';

let server: { port: number; close: () => Promise<void> };
const logs: string[] = [];

beforeAll(async () => {
  server = await startServer({ port: 0, perIp: 10, log: (m: string) => logs.push(m) });
});
afterAll(async () => {
  await server.close();
});

interface Client { ws: WebSocket; msgs: Record<string, unknown>[]; wait: (pred: (m: Record<string, unknown>) => boolean, ms?: number) => Promise<Record<string, unknown>> }

function connect(): Promise<Client> {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(`ws://127.0.0.1:${server.port}`);
    const msgs: Record<string, unknown>[] = [];
    const waiters: { pred: (m: Record<string, unknown>) => boolean; res: (m: Record<string, unknown>) => void }[] = [];
    ws.on('message', (d) => {
      const m = JSON.parse(String(d));
      msgs.push(m);
      for (const w of [...waiters]) if (w.pred(m)) {
        waiters.splice(waiters.indexOf(w), 1);
        w.res(m);
      }
    });
    ws.on('open', () => resolve({
      ws, msgs,
      wait: (pred, ms = 2000) => new Promise((res, rej) => {
        const found = msgs.find(pred);
        if (found) return res(found);
        waiters.push({ pred, res });
        setTimeout(() => rej(new Error('timeout')), ms);
      }),
    }));
    ws.on('error', reject);
  });
}

const send = (c: Client, m: unknown) => c.ws.send(JSON.stringify(m));

describe('online server', () => {
  it('shares positions only with heroes on the same map', async () => {
    const a = await connect();
    const b = await connect();
    const c = await connect();
    send(a, { t: 'hello', v: 1, name: 'Aren', cls: 'stormblade', level: 5, world: 'vale' });
    send(b, { t: 'hello', v: 1, name: 'Bryn', cls: 'emberseer', spec: 'frostweaver', level: 12, world: 'vale' });
    send(c, { t: 'hello', v: 1, name: 'Cass', cls: 'thornkeeper', level: 22, world: 'sunscar' });
    await a.wait((m) => m.t === 'welcome');
    send(b, { t: 'pos', x: 10, z: 20, f: 1, m: 1, world: 'vale' });
    const snap = await a.wait((m) => m.t === 'snap' && (m.players as { name: string; spec: string }[]).some((p) => p.name === 'Bryn' && p.spec === 'frostweaver'));
    const players = snap.players as { name: string; x: number }[];
    expect(players.find((p) => p.name === 'Bryn')!.x).toBe(10);
    expect(players.some((p) => p.name === 'Cass')).toBe(false);
    // A dungeon (world null) is private.
    send(b, { t: 'pos', x: 10, z: 20, world: null });
    await new Promise((r) => setTimeout(r, 250));
    const last = [...a.msgs].reverse().find((m) => m.t === 'snap')!;
    expect((last.players as unknown[]).length).toBe(0);
    for (const x of [a, b, c]) x.ws.close();
  });

  it('chat: same map only, filtered, rate limited', async () => {
    const a = await connect();
    const b = await connect();
    const c = await connect();
    send(a, { t: 'hello', v: 1, name: 'Aren', cls: 'stormblade', world: 'vale' });
    send(b, { t: 'hello', v: 1, name: 'Bryn', cls: 'emberseer', world: 'vale' });
    send(c, { t: 'hello', v: 1, name: 'Cass', cls: 'emberseer', world: 'frostmarch' });
    await b.wait((m) => m.t === 'welcome');
    await c.wait((m) => m.t === 'welcome');
    send(a, { t: 'chat', text: 'hello   shit\u0000 world' });
    const got = await b.wait((m) => m.t === 'chat');
    expect(got.text).toBe('hello **** world');
    expect(got.name).toBe('Aren');
    send(a, { t: 'chat', text: 'again' });
    await a.wait((m) => m.t === 'sys' && String(m.text).includes('too quickly'));
    await new Promise((r) => setTimeout(r, 200));
    expect(c.msgs.some((m) => m.t === 'chat')).toBe(false);
    for (const x of [a, b, c]) x.ws.close();
  });

  it('cleans names and rejects bad data', async () => {
    const a = await connect();
    const b = await connect();
    send(a, { t: 'hello', v: 1, name: '<script>x</script>Evil‮Name that is far too long', cls: 'hacker', level: 999, world: 'moon' });
    send(b, { t: 'hello', v: 1, name: 'Bryn', cls: 'emberseer', world: 'vale' });
    await a.wait((m) => m.t === 'welcome');
    send(a, { t: 'pos', x: 1e9, z: NaN, world: 'vale' });
    send(a, { t: 'pos', x: 5000, z: -5000, world: 'vale' });
    const snap = await b.wait((m) => m.t === 'snap' && (m.players as unknown[]).length > 0);
    const p = (snap.players as { name: string; cls: string; level: number; x: number; z: number }[])[0];
    expect(p.name).not.toMatch(/[<>]/);
    expect(p.name.length).toBeLessThanOrEqual(16);
    expect(p.cls).toBe('stormblade');
    expect(p.level).toBe(30);
    expect(Math.abs(p.x)).toBeLessThanOrEqual(230);
    expect(Math.abs(p.z)).toBeLessThanOrEqual(230);
    a.ws.close();
    b.ws.close();
  });

  it('refuses old clients and records reports', async () => {
    const a = await connect();
    send(a, { t: 'hello', v: 99, name: 'Old' });
    await a.wait((m) => m.t === 'sys');
    const b = await connect();
    send(b, { t: 'hello', v: 1, name: 'Bryn', world: 'vale' });
    const w = await b.wait((m) => m.t === 'welcome');
    send(b, { t: 'report', id: w.id, reason: 'testing' });
    await b.wait((m) => m.t === 'sys');
    expect(logs.some((l) => l.includes('REPORT'))).toBe(true);
    b.ws.close();
  });

  it('word filter masks whole words case-insensitively', () => {
    const f = makeFilter(['bad']);
    expect(f('BAD day, bad')).toBe('*** day, ***');
  });
});
