// Balance check: a scripted player plays the whole campaign with each class, in simulated time,
// and reports play time, deaths and level. Fails if a class cannot finish.
// Usage: npm run playthrough
import { chromium } from 'playwright';
import { createServer } from 'vite';
import { existsSync } from 'node:fs';

const server = await createServer({ server: { port: 5179, host: '127.0.0.1' }, logLevel: 'error' });
await server.listen();
const exe = existsSync('/opt/pw-browsers/chromium') ? '/opt/pw-browsers/chromium' : undefined;
const browser = await chromium.launch({ executablePath: exe, args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });

const classes = process.argv[2] ? [process.argv[2]] : ['stormblade', 'emberseer', 'thornkeeper'];
const seed = Number(process.argv[3] ?? 12345);
let failed = false;

for (const cls of classes) {
  const page = await (await browser.newContext({ viewport: { width: 844, height: 390 } })).newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(`http://127.0.0.1:5179/?seed=${seed}`);
  await page.evaluate(() => localStorage.clear());
  await page.reload();
  await page.getByText('New Hero').click();
  await page.locator(`[data-cls="${cls}"]`).click();
  await page.getByText('Enter the Vale').click();
  await page.waitForFunction(() => window.__game);

  const result = await page.evaluate(async () => {
    const g = window.__game;
    const D = window.__debug;
    g.paused = true; // we drive the simulation ourselves
    const p = g.progress;
    const pl = g.player;
    const log = [];
    let deaths = 0;
    const DT = 1 / 30;
    const MAX_T = 4 * 3600;
    let bossTries = 0;
    let engaged = null;
    let lastProgressAt = 0;
    let lastSig = '';

    const teleport = (x, z) => { pl.pos.set(x, pl.pos.y, z); g.moveUnit(pl, 0, 0); };
    const walkTo = (x, z) => {
      const dx = x - pl.pos.x, dz = z - pl.pos.z; const d = Math.hypot(dx, dz);
      if (d < 0.5) return;
      const s = Math.min(d, 7.5 * DT);
      g.moveUnit(pl, (dx / d) * s, (dz / d) * s);
      pl.facing = Math.atan2(dx, dz);
    };
    const upgradeGear = () => {
      for (const it of [...p.bag]) {
        const cur = p.gear[it.slot];
        const sc = (i) => (i ? i.power * 1.2 + i.stamina + i.armor * 0.2 : 0);
        if (sc(it) > sc(cur)) g.equip(it);
      }
      for (const it of [...p.bag]) g.sell(it);
    };
    const town = () => {
      // Hand in, accept, shop.
      for (const n of g.npcs) {
        for (const e of D.questsForNpc(p, n.npc.id)) {
          if (e.status === 'ready') { g.turnInQuest(e.quest.id); log.push(`${Math.round(g.now / 60)}m L${p.level} done ${e.quest.id}`); }
          else if (e.status === 'available') D.acceptQuest(p, e.quest.id);
        }
      }
      upgradeGear();
      while (p.potions < 6 && g.buyPotion());
    };
    const target = () => {
      for (const a of p.active) {
        if (a.done) continue;
        const q = { wolves: 'wolf', pelts: 'wolf', stalkers: 'spider', raiders: 'raider', relic: 'chieftain', brutes: 'brute', cindermaw: 'boss' }[a.id];
        return q;
      }
      return null;
    };
    const rotation = () => {
      const abs = g.abilityList();
      for (const ab of [...abs].reverse()) {
        if (p.level < ab.unlockLevel || g.cooldownLeft(ab.id) > 0 || g.resource < ab.cost) continue;
        if (ab.id === 'renewal' && pl.hp > pl.maxHp * 0.7) continue;
        if (ab.id === 'staticGuard' && pl.hp > pl.maxHp * 0.8) continue;
        if (ab.id === 'phoenixVeil' && pl.hp > pl.maxHp * 0.5) continue;
        if (ab.id === 'ashRing' && g.enemiesNear(pl.pos.x, pl.pos.z, 7).length === 0) continue;
        if ((ab.id === 'tempest' || ab.id === 'thunderCleave') && g.enemiesNear(pl.pos.x, pl.pos.z, 5).length === 0) continue;
        if (g.useAbility(ab.id)) return;
      }
    };
    const inTelegraph = (pad = 0.5, x = pl.pos.x, z = pl.pos.z) => {
      // Red circles are the mechanic a player must react to: step out and wait for them to go off.
      for (const t of g['telegraphs']) {
        if (t.owner && Math.hypot(x - t.x, z - t.z) < t.r + pad) return t;
      }
      return null;
    };

    // Messages from the game would otherwise pile up; silence the UI hooks.
    let tick = 0;
    let dmgLog = {};
    const orig = g.dealDamage.bind(g);
    g.dealDamage = (src, dst, amt, opts) => { const before = dst.hp; orig(src, dst, amt, opts); if (dst === pl) { const k = src.kind + (opts?.color === '#ff7a1a' ? '-ring' : ''); dmgLog[k] = (dmgLog[k] ?? 0) + Math.round(before - Math.max(0, dst.hp)); } };
    while (g.now < MAX_T) {
      tick++;
      if (pl.dead) {
        deaths++;

        { const b = g.units.find((u) => u.kind === 'boss'); log.push(`${Math.round(g.now / 60)}m L${p.level} died vs ${engaged?.kind}${engaged?.kind === 'boss' || engaged?.kind === 'imp' ? ` boss at ${Math.round(100 * b.hp / b.maxHp)}% imps=${g.units.filter((u) => u.kind === 'imp' && !u.dead).length} dmgTaken=${JSON.stringify(dmgLog)}` : ''}`); }
        dmgLog = {};
        g.respawnPlayer();
        engaged = null;
        if (target() === 'boss') bossTries++;
        if (bossTries > 12) break;
      }
      if (p.completed.includes('cindermaw')) break;
      const questKind = target();
      const hasTownWork = p.active.some((a) => a.done) || g.npcs.some((n) => D.questsForNpc(p, n.npc.id).some((e) => e.status === 'available'));
      if (hasTownWork) {
        teleport(0, 139);
        engaged = null;
        town();
        g.update(DT);
        continue;
      }
      // Face the boss at level 9+; otherwise grind the toughest sensible camp.
      const grind = p.level >= 7 ? 'brute' : p.level >= 5 ? 'raider' : p.level >= 3 ? 'spider' : 'wolf';
      const want = questKind === 'boss' && p.level < 9 ? grind : questKind ?? grind;
      // Rest when hurt and safe.
      const threatened = g.units.some((u) => !u.dead && u.target === pl);
      if (!threatened && pl.hp < pl.maxHp * 0.85) { g.update(DT); continue; }
      if (!engaged || engaged.dead || engaged.state === 'evade') {
        const cands = g.units.filter((u) => u.kind === want && !u.dead && u.state !== 'evade');
        cands.sort((a, b) => a.distTo(pl) - b.distTo(pl));
        engaged = threatened ? g.units.find((u) => !u.dead && u.target === pl) : cands[0] ?? null;
        if (engaged && engaged.distTo(pl) > 60) {
          const dx = 0 - engaged.pos.x, dz = 145 - engaged.pos.z; const d = Math.hypot(dx, dz);
          teleport(engaged.pos.x + (dx / d) * 14, engaged.pos.z + (dz / d) * 14);
        }
      }
      if (engaged) {
        pl.target = engaged;
        g.autoAttack = true;
        const range = pl.attackRange + engaged.radius - 0.5;
        const t = inTelegraph();
        if (t) {
          let dx = pl.pos.x - t.x, dz = pl.pos.z - t.z;
          if (Math.hypot(dx, dz) < 0.3) { dx = -(engaged.pos.z - pl.pos.z); dz = engaged.pos.x - pl.pos.x; } // sidestep
          const d = Math.hypot(dx, dz) || 1;
          walkTo(pl.pos.x + (dx / d) * 3, pl.pos.z + (dz / d) * 3);
        } else if (engaged.distTo(pl) > range) {
          const dx = engaged.pos.x - pl.pos.x, dz = engaged.pos.z - pl.pos.z; const d = Math.hypot(dx, dz) || 1;
          if (!inTelegraph(1.2, pl.pos.x + (dx / d) * 1.5, pl.pos.z + (dz / d) * 1.5)) walkTo(engaged.pos.x, engaged.pos.z);
        }
        if (pl.hp < pl.maxHp * 0.35) g.usePotion();
        rotation();
      }
      g.update(DT);
      const sig = `${p.level}|${p.xp}|${p.completed.length}|${p.active.map((a) => a.progress).join()}`;
      if (sig !== lastSig) { lastSig = sig; lastProgressAt = g.now; }
      if (g.now - lastProgressAt > 900) { log.push(`stuck: no progress for 15 minutes; want=${want} engaged=${engaged?.kind}/${engaged?.state} at ${pl.pos.x.toFixed(0)},${pl.pos.z.toFixed(0)} target=${engaged?.pos.x.toFixed(0)},${engaged?.pos.z.toFixed(0)} distance=${engaged?.distTo(pl).toFixed(1)} hp=${pl.hp.toFixed(0)}/${pl.maxHp.toFixed(0)} enemyHp=${engaged?.hp.toFixed(0)} auto=${g.autoAttack} targetSet=${pl.target === engaged} cooldown=${pl.attackTimer.toFixed(1)}`); break; }
    }
    return { minutes: Math.round(g.now / 60), level: p.level, deaths, bossTries, done: p.completed.includes('cindermaw'), log, gear: Object.values(p.gear).map((i) => i && `${i.name} ilvl${i.ilvl}`) };
  });
  console.log(`\n=== ${cls} ===`);
  console.log(result.log.join('\n'));
  console.log(`finished=${result.done} time=${result.minutes} min level=${result.level} deaths=${result.deaths} bossAttempts=${result.bossTries}`);
  console.log(`gear: ${result.gear.join(' | ')}`);
  if (errors.length) console.log('errors:', errors.slice(0, 5));
  if (!result.done || errors.length) failed = true;
}
await browser.close();
await server.close();
console.log(failed ? '\nPLAYTHROUGH FAILED' : '\nPLAYTHROUGH PASSED');
process.exit(failed ? 1 : 0);
