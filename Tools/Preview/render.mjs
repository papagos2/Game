// Renders preview.html headlessly and saves a PNG.
// Usage: node render.mjs <scene.json> <out.png> [width height] [extra query]
import { chromium } from '/opt/node22/lib/node_modules/playwright/index.mjs';
import fs from 'node:fs';
import path from 'node:path';

const here = path.dirname(new URL(import.meta.url).pathname);
const [scenePath, outPng, w = '1200', h = '540', extra = ''] = process.argv.slice(2);

const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
const page = await browser.newPage({ viewport: { width: +w, height: +h }, deviceScaleFactor: 2 });
page.on('console', m => { if (m.type() === 'error') console.error('page:', m.text()); });
await page.route('http://preview.local/**', async route => {
  const url = new URL(route.request().url());
  let file = decodeURIComponent(url.pathname.slice(1));
  const full = file.startsWith('scene/') ? path.resolve(scenePath) : path.join(here, file);
  if (!fs.existsSync(full)) return route.fulfill({ status: 404, body: 'missing ' + file });
  const type = full.endsWith('.js') ? 'text/javascript' : full.endsWith('.json') ? 'application/json' : 'text/html';
  return route.fulfill({ status: 200, contentType: type, body: fs.readFileSync(full) });
});
await page.goto(`http://preview.local/preview.html?scene=scene/x.json${extra ? '&' + extra : ''}`);
await page.waitForFunction(() => document.title === 'ready' || document.title.startsWith('error'), null, { timeout: 180000 });
const title = await page.title();
if (title !== 'ready') { console.error(title); process.exit(1); }
await page.screenshot({ path: outPng });
await browser.close();
console.log('saved', outPng);
