// Balance check: a scripted player plays the whole campaign (3 maps, levels 1-30) with every
// class and specialization, in simulated time, and reports time, deaths and level per act.
// Usage: npm run playthrough            (all 9 paths)
//        npm run playthrough frostweaver (one path)
//        npm run playthrough all 777      (all paths, another random seed)
import { chromium } from 'playwright';
import { createServer } from 'vite';
import { existsSync } from 'node:fs';

const ALL = [
  ['stormblade', 'tempestKnight'], ['stormblade', 'bulwark'], ['stormblade', 'spellblade'],
  ['emberseer', 'pyromancer'], ['emberseer', 'frostweaver'], ['emberseer', 'thermancer'],
  ['thornkeeper', 'grovewarden'], ['thornkeeper', 'beastcaller'], ['thornkeeper', 'rotbloom'],
];
const only = process.argv[2] && process.argv[2] !== 'all' ? process.argv[2] : null;
const seed = Number(process.argv[3] ?? 12345);
const runs = only ? ALL.filter(([c, s]) => s === only || c === only) : ALL;

const server = await createServer({ server: { port: 5179, host: '127.0.0.1' }, logLevel: 'error' });
await server.listen();
const exe = existsSync('/opt/pw-browsers/chromium') ? '/opt/pw-browsers/chromium' : undefined;
const browser = await chromium.launch({ executablePath: exe, args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });

async function play([cls, spec]) {
  const ctx = await browser.newContext({ viewport: { width: 640, height: 360 } });
  const page = await ctx.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(`http://127.0.0.1:5179/?seed=${seed}`);
  await page.evaluate(() => localStorage.clear());
  await page.reload();
  await page.getByText('New Hero').click();
  await page.locator(`[data-cls="${cls}"]`).click();
  await page.getByText('Enter the Vale').click();
  await page.waitForFunction(() => window.__game);

  if (process.env.DBG) await page.evaluate(() => { window.__dbgbot = true; });
  await page.evaluate((r) => { window.__ranged = r; }, cls !== 'stormblade');
  const result = await page.evaluate((spec) => {
    const D = window.__debug;
    let g = window.__game;
    g.paused = true;
    const log = [];
    let deaths = 0;
    let actDeaths = 0;
    const DT = 1 / 20;
    const MAX_T = 8 * 3600;
    let engaged = null;
    let lastProgressAt = 0;
    let lastSig = '';
    const KIND_OF_QUEST = (q) => {
      const o = q.objective;
      if (o.type === 'kill') return o.mob;
      if (o.type === 'collect') return { pelt: 'wolf', relic: 'chieftain', frostcore: 'frostling', thanecrown: 'thane', venomsac: 'scorpion', sunsigil: 'warlord' }[o.item];
      return null;
    };
    const P = () => g.progress;
    const pl = () => g.player;
    const teleport = (x, z) => { pl().pos.set(x, pl().pos.y, z); g.moveUnit(pl(), 0, 0); };
    const walkTo = (x, z) => {
      const dx = x - pl().pos.x, dz = z - pl().pos.z; const d = Math.hypot(dx, dz);
      if (d < 0.5) return;
      const s = Math.min(d, 7.5 * DT * (pl().slowed(g.now) ? 0.6 : 1));
      g.moveUnit(pl(), (dx / d) * s, (dz / d) * s);
      pl().facing = Math.atan2(dx, dz);
    };
    const spendTalents = () => {
      const p = P();
      const trees = [p.spec, p.cls].filter(Boolean);
      for (let guard = 0; guard < 60; guard++) {
        let spent = false;
        for (const t of trees) {
          const ids = Object.keys(g.tb ? {} : {});
          void ids;
          for (const node of window.__trees[t]) if (D.rankUp(p, node)) { spent = true; break; }
          if (spent) break;
        }
        if (!spent) break;
      }
      g.talentsChanged();
    };
    const upgradeGear = () => {
      const p = P();
      for (let i = 0; i < 30; i++) {
        const best = [...p.bag].map((it) => ({ it, v: window.__upgrade(it, p.gear) })).sort((a, b) => b.v - a.v)[0];
        if (!best || best.v <= 0) break;
        g.equip(best.it);
      }
      for (const it of [...p.bag]) g.sell(it);
      for (const v of g.vendorItems()) if (window.__upgrade(v.item, p.gear) > 4 && p.gold > v.price + 200) { g.buyItem(v.item.id); upgradeGear(); return; }
      while (p.potions < 8 && g.buyPotion());
    };
    const town = () => {
      const p = P();
      for (const n of g.npcs) {
        for (const e of D.questsForNpc(p, n.npc.id)) {
          if (e.status === 'ready') {
            g.turnInQuest(e.quest.id);
            g.paused = true;
            log.push(`  ${Math.round(g.now / 60)}m L${p.level} done ${e.quest.id}`);
          } else if (e.status === 'available') D.acceptQuest(p, e.quest.id);
        }
      }
      upgradeGear();
    };
    const target = () => {
      for (const a of P().active) {
        if (a.done) continue;
        const q = window.__quests[a.id];
        if (q.map !== g.map.id) continue;
        return q;
      }
      return null;
    };
    const rotation = () => {
      const abs = g.abilityList();
      const p = P();
      const hurt = pl().hp / pl().maxHp;
      for (const ab of [...abs].reverse()) {
        if (g.cooldownLeft(ab.id) > 0) continue;
        const fx = ab.effects.map((e) => e.t);
        const heals = fx.includes('heal') || fx.includes('hot');
        if (heals && hurt > 0.7 && !fx.includes('damage')) continue;
        if ((fx.includes('shield') || fx.includes('guard')) && hurt > 0.8) continue;
        if (fx.includes('blink') && hurt > 0.5 && ab.id === 'phoenixVeil') continue;
        if (fx.includes('petFrenzy') && !g.pets.length) continue;
        if (fx.includes('summon') && g.pets.length && ab.id === 'spiritWolf' && g.pets.some((x) => x.name === 'Spirit Wolf')) continue;
        const selfArea = ab.effects.find((e) => e.area === 'aroundSelf' || e.area === 'cone');
        if (selfArea && !ab.needsTarget && g.enemiesNear(pl().pos.x, pl().pos.z, (selfArea.radius ?? 5)).length === 0 && !fx.includes('blink')) continue;
        if (fx.includes('blink') && ab.id === 'blazingArc' && (!engaged || engaged.distTo(pl()) > 8)) continue;
        if (g.useAbility(ab.id)) return;
        void p;
      }
    };
    const danger = (pad = 0.5, x = pl().pos.x, z = pl().pos.z) => g.dangerZones.find((t) => Math.hypot(x - t.x, z - t.z) < t.r + pad) ?? null;
    const grindKind = () => {
      const lvl = P().level;
      let best = null, bd = 99;
      for (const c of g.map.camps) {
        const def = window.__mobs[c.kind];
        if (def.elite || def.summon) continue;
        const mid = (def.levels[0] + def.levels[1]) / 2;
        const d = Math.abs(mid - (lvl + 0.5));
        if (mid <= lvl + 2 && d < bd) { bd = d; best = c.kind; }
      }
      return best ?? g.map.camps[0].kind;
    };

    let tick = 0;
    let actStart = 0;
    let eliteKind = null, eliteFails = 0, eliteLevel = 0;
    let engagedHp = 0, engagedAt = 0;
    const dungeonTried = new Set();
    let dgStart = 0, dgDeaths = 0, dungeonFail = false;
    let dmgLog = {};
    const hook = () => {
      const orig = g.dealDamage.bind(g);
      g.dealDamage = (src, dst, amt, o) => { const b = dst.hp; orig(src, dst, amt, o); if (dst === g.player) { const k = src.kind + (o?.color && o.color !== '#fff' ? ':' + o.color : ''); dmgLog[k] = (dmgLog[k] ?? 0) + Math.round(b - Math.max(0, dst.hp)); } };
    };
    hook();
    const skip = new Set();
    while (g.now < MAX_T) {
      tick++;
      const p = P();
      if (pl().dead) {
        deaths++; actDeaths++;
        if (g.map.dungeon) dgDeaths++;
        if (engaged && window.__mobs[engaged.kind]?.elite) eliteFails++;
        const bossU = g.units.find((u) => window.__mobs[u.kind].boss);
        if (actDeaths < 40) log.push(`  ${Math.round(g.now / 60)}m L${p.level} died vs ${engaged?.kind}${engaged && window.__mobs[engaged.kind].boss ? ` boss@${Math.round(100 * bossU.hp / bossU.maxHp)}% dmg=${JSON.stringify(dmgLog)} hp=${pl().maxHp} armor=${pl().armor} dist=${engaged.distTo(pl()).toFixed(1)}` : ''}`);
        dmgLog = {};
        g.respawnPlayer();
        engaged = null;
      }
      if (p.completed.includes('ss_azhkar') && (p.dungeons.includes('sunTomb') || dungeonTried.has('sunTomb')) && !g.map.dungeon) break;
      if (p.level >= 10 && !p.spec) { g.chooseSpec(spec); g.paused = true; }
      if (tick % 200 === 0 && D && p.level > 1) spendTalents();
      // Clear this act's dungeon once, with the AI party, before moving on.
      const dg = window.__dungeonFor[g.map.act];
      if (!g.map.dungeon && p.completed.includes(g.map.finalQuest) && dg && !p.dungeons.includes(dg) && !dungeonTried.has(dg)) {
        dungeonTried.add(dg);
        const now = g.now;
        D.travel(dg);
        g = window.__game;
        hook();
        g.paused = true;
        g.now = now;
        dgStart = now;
        dgDeaths = 0;
        engaged = null;
        continue;
      }
      if (g.map.dungeon) {
        const bossDead = g.units.some((u) => u.kind === g.map.dungeon.finalBoss && u.dead);
        if (bossDead || g.now - dgStart > 1800 || dgDeaths > 25) {
          log.push(`${g.map.name} ${bossDead ? 'cleared' : 'FAILED'} at L${p.level} in ${Math.round((g.now - dgStart) / 60)}m, deaths ${dgDeaths}, party alive ${g.pets.filter((c) => c.role && !c.dead).length}/2`);
          if (!bossDead) dungeonFail = true;
          const now = g.now;
          D.travel(g.map.act);
          g = window.__game;
          hook();
          g.paused = true;
          g.now = now;
          engaged = null;
          continue;
        }
      }
      // Move on to the next map when this one is done.
      if (!g.map.dungeon && p.completed.includes(g.map.finalQuest) && g.map.next && p.unlocked.includes(g.map.next)) {
        log.push(`${g.map.name} done at ${Math.round(g.now / 60)}m (+${Math.round((g.now - actStart) / 60)}m), level ${p.level}, deaths ${actDeaths}`);
        actDeaths = 0;
        const now = g.now;
        D.travel(g.map.next);
        g = window.__game;
        hook();
        g.paused = true;
        g.now = now;
        actStart = now;
        engaged = null;
        continue;
      }
      const hasTownWork = !g.map.dungeon && p.active.some((a) => a.done && window.__quests[a.id].map === g.map.id)
        || (!g.map.dungeon && g.npcs.some((n) => D.questsForNpc(p, n.npc.id).some((e) => e.status === 'available')));
      if (hasTownWork) {
        teleport(g.map.spawn.x, g.map.spawn.z);
        engaged = null;
        town();
        g.update(DT);
        continue;
      }
      const q = target();
      if (q && q.objective.type === 'explore') {
        const zn = g.map.zones.find((z) => z.id === q.objective.zone);
        teleport(zn.x, zn.z);
        g.update(DT);
        continue;
      }
      const qKind = q ? KIND_OF_QUEST(q) : null;
      const qDef = qKind ? window.__mobs[qKind] : null;
      // Take on elites and bosses only near their level.
      // After two defeats by the same elite, grind a level before trying again (as a player would).
      if (qKind !== eliteKind) { eliteKind = qKind; eliteFails = 0; eliteLevel = p.level; }
      if (p.level > eliteLevel) { eliteFails = 0; eliteLevel = p.level; }
      // Mobs that give no experience any more are no reason to wait.
      const outleveled = !g.map.camps.some((c) => !window.__mobs[c.kind].elite && window.__mobs[c.kind].levels[1] > p.level - 5);
      const ready = !qDef || !qDef.elite || p.level >= 30 || outleveled || (p.level >= qDef.levels[0] - (qDef.boss ? 0 : 2) && eliteFails < 2);
      let want = qKind && ready ? qKind : grindKind();
      // In a dungeon, fight forward along the path (north).
      if (g.map.dungeon) want = g.units.filter((u) => !u.dead && !window.__mobs[u.kind].summon).sort((a, b) => b.home.z - a.home.z)[0]?.kind ?? want;
      const threatened = g.units.some((u) => !u.dead && u.target === pl());
      if (!threatened && pl().hp < pl().maxHp * 0.85) { g.update(DT); continue; }
      // Give up on a target we cannot seem to hurt (e.g. stuck behind scenery).
      if (engaged && (engaged.hp !== engagedHp || engaged.state === 'chase')) { engagedHp = engaged.hp; engagedAt = g.now; }
      if (engaged && g.now - engagedAt > 15) { skip.add(engaged); engaged = null; }
      if (!engaged || engaged.dead || engaged.state === 'evade') {
        if (tick % 2000 === 0) skip.clear();
        const cands = g.units.filter((u) => u.kind === want && !u.dead && u.state !== 'evade' && !skip.has(u)).sort((a, b) => a.distTo(pl()) - b.distTo(pl()));
        engagedAt = g.now;
        engaged = threatened ? g.units.find((u) => !u.dead && u.target === pl()) : cands[0] ?? null;
        // Everything here is dead: walk away so the camp can respawn.
        if (!engaged && !g.map.dungeon) teleport(g.map.spawn.x, g.map.spawn.z);
        // In dungeons, walk there (with the party) instead of teleporting.
        if (engaged && g.map.dungeon) engagedAt = g.now;
        if (engaged && engaged.distTo(pl()) > 60 && !g.map.dungeon) {
          const hub = g.map.zones[0];
          const dx = hub.x - engaged.pos.x, dz = hub.z - engaged.pos.z; const d = Math.hypot(dx, dz);
          teleport(engaged.pos.x + (dx / d) * 14, engaged.pos.z + (dz / d) * 14);
        }
      }
      if (engaged) {
        pl().target = engaged;
        g.autoAttack = true;
        const range = pl().attackRange + engaged.radius - 0.5;
        const t = danger();
        if (t) {
          let dx = pl().pos.x - t.x, dz = pl().pos.z - t.z;
          if (Math.hypot(dx, dz) < 0.3) { dx = -(engaged.pos.z - pl().pos.z); dz = engaged.pos.x - pl().pos.x; }
          const d = Math.hypot(dx, dz) || 1;
          walkTo(pl().pos.x + (dx / d) * 3, pl().pos.z + (dz / d) * 3);
        } else if (window.__ranged && engaged.target === pl() && engaged.distTo(pl()) < 7 && (engaged.rooted(g.now) || engaged.slowed(g.now))) {
          // Ranged heroes step away from a snared enemy, as a player would.
          const dx = pl().pos.x - engaged.pos.x, dz = pl().pos.z - engaged.pos.z; const d = Math.hypot(dx, dz) || 1;
          walkTo(pl().pos.x + (dx / d) * 3, pl().pos.z + (dz / d) * 3);
        } else if (engaged.distTo(pl()) > range) {
          const dx = engaged.pos.x - pl().pos.x, dz = engaged.pos.z - pl().pos.z; const d = Math.hypot(dx, dz) || 1;
          if (!danger(1.2, pl().pos.x + (dx / d) * 1.5, pl().pos.z + (dz / d) * 1.5)) walkTo(engaged.pos.x, engaged.pos.z);
        }
        if (pl().hp < pl().maxHp * 0.35) g.usePotion();
        rotation();
      }
      g.update(DT);
      const sig = `${p.level}|${p.xp}|${p.completed.length}|${p.active.map((a) => a.progress).join()}`;
      if (sig !== lastSig) { lastSig = sig; lastProgressAt = g.now; }
      if (window.__dbgbot && tick % 400 === 0 && tick < 4000) log.push(`dbg t=${g.now.toFixed(0)} want=${want} eng=${engaged?.kind}/${engaged?.state} d=${engaged?.distTo(pl()).toFixed(1)} ehp=${engaged?.hp} php=${pl().hp.toFixed(0)} auto=${g.autoAttack} res=${g.resource.toFixed(0)} pos=${pl().pos.x.toFixed(0)},${pl().pos.z.toFixed(0)} q=${q?.id}`);
      if (g.now - lastProgressAt > 1200) { log.push(`  STUCK: no progress for 20 minutes; want=${want} engaged=${engaged?.kind}/${engaged?.state} map=${g.map.id} quest=${q?.id}`); break; }
    }
    const p = P();
    if (p.completed.includes('ss_azhkar')) log.push(`${g.map.name} done at ${Math.round(g.now / 60)}m (+${Math.round((g.now - actStart) / 60)}m), level ${p.level}, deaths ${actDeaths}`);
    return {
      minutes: Math.round(g.now / 60), level: p.level, deaths, done: p.completed.includes('ss_azhkar') && !dungeonFail && p.dungeons.length === 3, log,
      gear: Object.values(p.gear).filter(Boolean).map((i) => `${i.name}(${i.ilvl})`).slice(0, 4),
      talents: Object.values(p.talents).reduce((a, b) => a + b, 0),
    };
  }, spec);
  await ctx.close();
  return { cls, spec, result, errors };
}

// Expose data the bot needs (module state is not reachable from evaluate otherwise).
await browser.contexts(); // no-op
const PRELOAD = `
  import('/src/data/talents.ts').then((m) => { window.__trees = Object.fromEntries(Object.entries(m.TREES).map(([k, t]) => [k, t.nodes.map((n) => n.id)])); });
  import('/src/data/quests.ts').then((m) => { window.__quests = m.QUEST_BY_ID; });
  import('/src/data/world.ts').then((m) => { window.__mobs = m.MOBS; });
  import('/src/game/rules.ts').then((m) => { window.__upgrade = m.upgradeValue; });
  import('/src/data/dungeons.ts').then((m) => { window.__dungeonFor = Object.fromEntries(Object.entries(m.DUNGEONS).map(([id, d]) => [d.act, id])); });
`;
const origNewContext = browser.newContext.bind(browser);
browser.newContext = async (opts) => {
  const c = await origNewContext(opts);
  await c.addInitScript({ content: `window.addEventListener('DOMContentLoaded', () => { const s = document.createElement('script'); s.type = 'module'; s.textContent = ${JSON.stringify(PRELOAD)}; document.head.appendChild(s); });` });
  return c;
};

let failed = false;
const results = [];
for (let i = 0; i < runs.length; i += 3) {
  results.push(...(await Promise.all(runs.slice(i, i + 3).map(play))));
}
for (const { cls, spec, result, errors } of results) {
  console.log(`\n=== ${cls} / ${spec} ===`);
  const deathsBy = {};
  for (const l of result.log) { const m = l.match(/died vs (\w+)/); if (m) deathsBy[m[1]] = (deathsBy[m[1]] ?? 0) + 1; }
  console.log(result.log.filter((l) => !l.startsWith('  ') || l.includes('STUCK') || (process.env.DBG && l.includes('boss@'))).join('\n'));
  console.log(`deaths by: ${JSON.stringify(deathsBy)}`);
  console.log(`finished=${result.done} time=${result.minutes} min level=${result.level} deaths=${result.deaths} talents=${result.talents}`);
  console.log(`gear: ${result.gear.join(' | ')}`);
  if (errors.length) console.log('errors:', errors.slice(0, 5));
  if (!result.done || errors.length) failed = true;
}
await browser.close();
await server.close();
console.log(failed ? '\nPLAYTHROUGH FAILED' : '\nPLAYTHROUGH PASSED');
process.exit(failed ? 1 : 0);
