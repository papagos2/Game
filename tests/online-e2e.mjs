// End-to-end check of the online layer: two heroes in two browsers join a local server,
// see each other on the map and exchange a chat message. Usage: npm run online-e2e
import { chromium } from 'playwright';
import { createServer } from 'vite';
import { existsSync, mkdirSync } from 'node:fs';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { startServer } from '../server/server.mjs';

const out = fileURLToPath(new URL('./output/', import.meta.url));
mkdirSync(out, { recursive: true });
const game = await startServer({ port: 0, log: () => undefined });
const vite = await createServer({ server: { port: 5183, host: '127.0.0.1' }, logLevel: 'error' });
await vite.listen();
const exe = existsSync('/opt/pw-browsers/chromium') ? '/opt/pw-browsers/chromium' : undefined;
const browser = await chromium.launch({ executablePath: exe, args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
const errors = [];
let failed = false;

async function hero(name, cls) {
  const page = await (await browser.newContext({ viewport: { width: 844, height: 390 } })).newPage();
  page.on('pageerror', (e) => errors.push(`${name}: ${e.message}`));
  await page.goto('http://127.0.0.1:5183/');
  await page.evaluate(() => localStorage.clear());
  await page.reload();
  await page.getByText('New Hero').click();
  await page.locator(`[data-cls="${cls}"]`).click();
  await page.locator('#heroName').fill(name);
  await page.getByText('Enter the Vale').click();
  await page.waitForFunction(() => window.__game && !window.__game.paused);
  await page.locator('#btnMenu').click();
  await page.locator('#srv').fill(`ws://127.0.0.1:${game.port}`);
  await page.locator('#onl').click();
  await page.waitForTimeout(700);
  await page.locator('#resume').click();
  return page;
}

try {
  const a = await hero('Aren', 'stormblade');
  const b = await hero('Bryn', 'emberseer');
  // Stand close to each other.
  await a.evaluate(() => { const g = window.__game; g.player.pos.set(4, 0, 136); g.moveUnit(g.player, 0, 0); });
  await b.evaluate(() => { const g = window.__game; g.player.pos.set(-2, 0, 134); g.moveUnit(g.player, 0, 0); g.cam.yaw = 0.6; });
  await b.waitForFunction(() => [...window.__game.others.values()].some((o) => o.unit.name.startsWith('Aren')), null, { timeout: 8000 });
  await a.waitForFunction(() => [...window.__game.others.values()].some((o) => o.unit.name.startsWith('Bryn')), null, { timeout: 8000 });
  console.log('- both heroes see each other ... ok');
  await a.locator('#btnChat').click();
  await a.locator('#msg').fill('Well met, Bryn!');
  await a.locator('#send').click();
  await b.waitForFunction(() => document.querySelector('#chatFeed')?.textContent?.includes('Well met, Bryn!'), null, { timeout: 5000 });
  console.log('- chat delivered ... ok');
  await a.locator('#x').click();
  await b.waitForTimeout(800);
  await b.screenshot({ path: join(out, '15-online.png') });
  const status = await b.evaluate(() => document.querySelector('#onlineDot')?.textContent);
  if (!status?.includes('2 heroes')) throw new Error('population not shown: ' + status);
  console.log('- online status shown ... ok');
} catch (e) {
  failed = true;
  console.error(e);
}
if (errors.length) {
  failed = true;
  console.error(errors.join('\n'));
}
await browser.close();
await vite.close();
await game.close();
console.log(failed ? 'ONLINE E2E FAILED' : 'ONLINE E2E PASSED');
process.exit(failed ? 1 : 0);
