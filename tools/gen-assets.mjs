// Renders the app icon and splash screens for Android and iOS from inline SVG.
// Usage: node tools/gen-assets.mjs   (run after `npx cap add android/ios`)
import { chromium } from 'playwright';
import { existsSync, readFileSync, readdirSync, statSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

const EMBLEM = `
  <defs><radialGradient id="g" cx="50%" cy="38%" r="70%"><stop offset="0" stop-color="#ffb347"/><stop offset="0.45" stop-color="#b8401a"/><stop offset="1" stop-color="#1a0f14"/></radialGradient></defs>
  <circle cx="256" cy="256" r="200" fill="url(#g)"/>
  <path d="M256 70 C290 150 360 190 350 290 C344 370 300 420 256 420 C200 420 160 372 166 300 C172 240 214 222 214 170 C240 196 246 222 246 250 C274 206 276 140 256 70 Z" fill="#fff3d6"/>
  <path d="M150 430 L256 330 L362 430" fill="none" stroke="#d8b36a" stroke-width="22" stroke-linecap="round" stroke-linejoin="round"/>`;
const BG = '#140f16';

const svgIcon = (round) => `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="100%" height="100%">
  ${round ? `<circle cx="256" cy="256" r="256" fill="${BG}"/>` : `<rect width="512" height="512" fill="${BG}"/>`}${EMBLEM}</svg>`;
// Adaptive foreground: 108dp canvas, keep art inside the central 66%.
const svgForeground = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="-136 -136 784 784" width="100%" height="100%">${EMBLEM}</svg>`;
const splashHtml = (w, h) => `<html><body style="margin:0;width:${w}px;height:${h}px;background:radial-gradient(ellipse at 50% 110%, #5a2210, ${BG} 60%);display:flex;align-items:center;justify-content:center">
  <div style="width:${Math.min(w, h) * 0.34}px;height:${Math.min(w, h) * 0.34}px">${`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="100%" height="100%">${EMBLEM}</svg>`}</div></body></html>`;

function pngSize(p) {
  const b = readFileSync(p);
  return { w: b.readUInt32BE(16), h: b.readUInt32BE(20) };
}
function walk(dir, out = []) {
  for (const f of readdirSync(dir)) {
    const p = join(dir, f);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (p.endsWith('.png')) out.push(p);
  }
  return out;
}

const exe = existsSync('/opt/pw-browsers/chromium') ? '/opt/pw-browsers/chromium' : undefined;
const browser = await chromium.launch({ executablePath: exe });
const page = await browser.newPage();
async function render(html, w, h, file, transparent = false) {
  await page.setViewportSize({ width: w, height: h });
  await page.setContent(`<html><body style="margin:0;background:transparent;width:${w}px;height:${h}px">${html}</body></html>`);
  writeFileSync(file, await page.screenshot({ omitBackground: transparent, clip: { x: 0, y: 0, width: w, height: h } }));
}

const files = [...(existsSync('android') ? walk('android/app/src/main/res') : []), ...(existsSync('ios') ? walk('ios/App/App/Assets.xcassets') : [])];
for (const f of files) {
  const { w, h } = pngSize(f);
  if (f.includes('ic_launcher_foreground')) await render(svgForeground, w, h, f, true);
  else if (f.includes('ic_launcher_round')) await render(svgIcon(true), w, h, f, true);
  else if (f.includes('ic_launcher') || f.includes('AppIcon')) await render(svgIcon(false), w, h, f);
  else if (f.includes('splash')) {
    await page.setViewportSize({ width: w, height: h });
    await page.setContent(splashHtml(w, h));
    writeFileSync(f, await page.screenshot());
  } else continue;
  console.log('wrote', f, `${w}x${h}`);
}
const bgXml = 'android/app/src/main/res/values/ic_launcher_background.xml';
if (existsSync(bgXml)) writeFileSync(bgXml, `<?xml version="1.0" encoding="utf-8"?>\n<resources>\n    <color name="ic_launcher_background">${BG.toUpperCase()}</color>\n</resources>\n`);
await browser.close();
