// End-to-end smoke test: builds nothing, serves dist/, plays the game in a phone-sized
// headless browser and fails on any script error. Screenshots go to tests/output/.
// Usage: npm run build && npm run smoke
import { chromium } from 'playwright';
import { createServer } from 'vite';
import { existsSync, mkdirSync } from 'node:fs';

const out = new URL('./output/', import.meta.url).pathname;
mkdirSync(out, { recursive: true });

const server = await createServer({ server: { port: 5178, host: '127.0.0.1' }, logLevel: 'error' });
await server.listen();
const url = 'http://127.0.0.1:5178/';

const exe = existsSync('/opt/pw-browsers/chromium') ? '/opt/pw-browsers/chromium' : undefined;
const browser = await chromium.launch({ executablePath: exe, args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
const ctx = await browser.newContext({ viewport: { width: 844, height: 390 }, deviceScaleFactor: 2, isMobile: true, hasTouch: true });
const page = await ctx.newPage();
const errors = [];
page.on('pageerror', (e) => errors.push(`pageerror: ${e.message}`));
page.on('console', (m) => { if (m.type() === 'error') errors.push(`console: ${m.text()}`); });

const step = async (name, fn) => {
  process.stdout.write(`- ${name} ... `);
  await fn();
  console.log('ok');
};
const shot = (n) => page.screenshot({ path: `${out}${n}.png` });
const G = (fn, arg) => page.evaluate(fn, arg);

let failed = false;
try {
  await step('title screen', async () => {
    await page.goto(url);
    await page.getByText('New Hero').waitFor();
    await shot('01-title');
  });
  await step('class select', async () => {
    await page.getByText('New Hero').click();
    await page.getByText('Choose your path').waitFor();
    await page.locator('[data-cls="emberseer"]').click();
    await shot('02-classes');
    await page.getByText('Enter the Vale').click();
    await page.waitForFunction(() => window.__game && !window.__game.paused);
    await page.waitForTimeout(1500);
    await shot('03-village');
  });
  await step('talk to Elra and accept quest', async () => {
    await G(() => { const g = window.__game; const e = g.npcs[0]; g.player.pos.set(e.pos.x + 2, e.pos.y, e.pos.z + 2); });
    await page.waitForTimeout(300);
    await page.locator('#talkBtn').click();
    await page.getByText('Wolves at the Gate').click();
    await shot('04-quest');
    await page.getByText('Accept').click();
    const ok = await G(() => window.__game.progress.active.some((a) => a.id === 'wolves'));
    if (!ok) throw new Error('quest not accepted');
  });
  await step('walk with the joystick', async () => {
    const before = await G(() => ({ x: window.__game.player.pos.x, z: window.__game.player.pos.z }));
    const cdp = await ctx.newCDPSession(page);
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x: 150, y: 280, id: 1 }] });
    for (let i = 1; i <= 5; i++) await cdp.send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: [{ x: 150, y: 280 - i * 10, id: 1 }] });
    await page.waitForTimeout(1500);
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
    const after = await G(() => ({ x: window.__game.player.pos.x, z: window.__game.player.pos.z }));
    const moved = Math.hypot(after.x - before.x, after.z - before.z);
    if (moved < 1.5) throw new Error(`player barely moved (${moved.toFixed(2)} m)`);
  });
  await step('fight wolves and gain xp/quest progress', async () => {
    await G(() => {
      const g = window.__game;
      const w = g.units.find((u) => u.kind === 'wolf' && !u.dead);
      g.player.pos.set(w.pos.x + 12, w.pos.y, w.pos.z);
      g.player.target = w;
    });
    await page.waitForTimeout(200);
    await page.locator('.ab-main').dispatchEvent('pointerdown');
    await page.waitForTimeout(400);
    await shot('05-combat');
    // Software rendering is slow, so advance game time directly (60 s of play at 30 fps).
    await G(() => {
      const g = window.__game;
      for (let i = 0; i < 1800 && g.progress.xp === 0 && g.progress.level === 1; i++) {
        if (i % 15 === 0) g.useAbility('cinderLance');
        if (g.player.hp < g.player.maxHp * 0.4) g.usePotion();
        g.update(1 / 30);
      }
    });
    const st = await G(() => ({ xp: window.__game.progress.xp, lvl: window.__game.progress.level, q: window.__game.progress.active[0]?.progress }));
    if (!(st.xp > 0 || st.lvl > 1)) throw new Error('no xp gained: ' + JSON.stringify(st));
    if (!(st.q >= 1)) throw new Error('quest did not progress: ' + JSON.stringify(st));
  });
  await step('panels open (bag, quest log, menu)', async () => {
    await page.locator('#btnBag').click();
    await page.getByText('Equipped').waitFor();
    await shot('06-bag');
    await page.locator('#x').click();
    await page.locator('#btnQuests').click();
    await page.getByText('Quest Log').waitFor();
    await page.locator('#x').click();
    await page.locator('#btnMenu').click();
    await page.getByText('How to play').waitFor();
    await page.locator('#resume').click();
  });
  await step('boss fight at level 10', async () => {
    await G(() => {
      const g = window.__game;
      while (g.progress.level < 10) g.gainXp(500);
      const boss = g.units.find((u) => u.kind === 'boss');
      g.player.pos.set(boss.pos.x, boss.pos.y, boss.pos.z + 14);
      g.cam.yaw = 0;
      g.player.target = boss;
      g.mainAction();
      g.useAbility('meteor');
    });
    await G(() => { const g = window.__game; for (let i = 0; i < 90; i++) g.update(1 / 30); });
    await page.waitForTimeout(500);
    await shot('07-boss');
    const s = await G(() => { const g = window.__game; const b = g.units.find((u) => u.kind === 'boss'); return { state: b.state, hp: b.hp, max: b.maxHp }; });
    if (s.state !== 'chase' || s.hp >= s.max) throw new Error('boss did not engage: ' + JSON.stringify(s));
  });
  await step('death and respawn', async () => {
    await G(() => { const g = window.__game; g.player.hp = 1; const b = g.units.find((u) => u.kind === 'boss'); g.dealDamage(b, g.player, 9999); });
    await page.getByText('Return to Hearthmoor').waitFor({ timeout: 4000 });
    await shot('08-death');
    await page.getByText('Return to Hearthmoor').click();
    const alive = await G(() => !window.__game.player.dead && !window.__game.paused);
    if (!alive) throw new Error('did not respawn');
  });
  await step('choose a path (Frostweaver) and spend talents', async () => {
    const badge = await page.locator('#skillBadge').textContent();
    if (badge !== '!') throw new Error('skills badge should ask to choose a path, got ' + badge);
    await page.locator('#btnSkills').click();
    await page.getByText('Choose your path').waitFor();
    await shot('10-paths');
    await page.locator('[data-spec="frostweaver"]').click();
    await page.locator('#yes').click();
    await page.getByText('Razor Ice').waitFor();
    await page.locator('[data-node="fw_spike"]').click();
    await page.locator('[data-tab="core"]').click();
    await page.locator('[data-node="es_focus"]').click();
    await shot('11-talents');
    const st = await G(() => ({ spec: window.__game.progress.spec, t: window.__game.progress.talents }));
    if (st.spec !== 'frostweaver' || st.t.fw_spike !== 1 || st.t.es_focus !== 1) throw new Error('talents not applied ' + JSON.stringify(st));
    await page.locator('#x').click();
    const cast = await G(() => {
      const g = window.__game;
      const w = g.units.find((u) => u.kind === 'brute' && !u.dead);
      g.player.pos.set(w.pos.x + 15, w.pos.y, w.pos.z);
      g.player.target = w;
      return g.useAbility('glacialSpike');
    });
    if (!cast) throw new Error('could not cast the new spec ability');
    const slots = await page.locator('#actionBar .ab-spec').count();
    if (slots !== 2) throw new Error('expected 2 spec buttons, got ' + slots);
  });
  await step('travel to Frostmarch and the Sunscar Dunes', async () => {
    for (const [map, shotName] of [['frostmarch', '12-frostmarch'], ['sunscar', '13-sunscar']]) {
      await G((m) => {
        const g = window.__game;
        if (!g.progress.unlocked.includes(m)) g.progress.unlocked.push(m);
        const w = g.npcs.find((n) => n.npc.travel);
        g.player.pos.set(w.pos.x - 2, w.pos.y, w.pos.z - 2);
        g.tryInteract();
      }, map);
      await page.locator(`[data-map="${map}"]`).click();
      await page.waitForFunction((m) => window.__game && window.__game.map.id === m && !window.__game.paused, map);
      await page.waitForTimeout(1500);
      await shot(shotName);
    }
  });
  await step('save and continue', async () => {
    await G(() => window.__game.save());
    await page.reload();
    await page.getByText('Continue').waitFor();
    await page.getByText('Continue').click();
    await page.waitForFunction(() => window.__game && !window.__game.paused);
    const st = await G(() => ({ lvl: window.__game.progress.level, map: window.__game.map.id, spec: window.__game.progress.spec }));
    if (st.lvl !== 10 || st.map !== 'sunscar' || st.spec !== 'frostweaver') throw new Error('save not restored ' + JSON.stringify(st));
    await page.waitForTimeout(800);
    await shot('14-continued');
  });
  await step('frame rate sample', async () => {
    const fps = await page.evaluate(() => new Promise((r) => { let n = 0; const t0 = performance.now(); const f = () => { n++; if (performance.now() - t0 < 2000) requestAnimationFrame(f); else r(n / 2); }; requestAnimationFrame(f); }));
    console.log(`(${fps.toFixed(0)} fps in software rendering) `);
  });
} catch (e) {
  failed = true;
  console.log('FAILED');
  console.error(e);
  await shot('failure').catch(() => {});
}
if (errors.length) {
  failed = true;
  console.error('Script errors:\n' + errors.join('\n'));
}
await browser.close();
await server.close();
console.log(failed ? 'SMOKE TEST FAILED' : 'SMOKE TEST PASSED');
process.exit(failed ? 1 : 0);
