// Hand-drawn SVG glyphs for ability and UI icons (no image files, crisp at any size).
import type { Glyph } from '../data/classes';

const PATHS: Record<Glyph, string> = {
  sword: 'M50 10 L58 18 L30 62 L22 70 L18 66 L26 58 Z M20 58 L30 68 M14 72 L24 82 L30 76 L20 66 Z',
  bolt: 'M52 8 L22 50 L42 50 L32 88 L70 40 L48 40 L60 8 Z',
  shield: 'M50 10 L80 22 C80 55 70 75 50 88 C30 75 20 55 20 22 Z M50 24 L50 76',
  storm: 'M26 40 C18 40 16 26 28 24 C30 12 50 10 54 22 C64 16 78 24 72 38 Z M44 46 L34 66 L46 66 L38 86 L60 58 L48 58 L56 46 Z',
  flame: 'M50 8 C58 28 76 36 72 60 C70 78 58 88 50 88 C38 88 26 78 28 60 C30 46 40 42 40 30 C46 36 48 42 48 48 C54 38 54 22 50 8 Z',
  ring: 'M50 18 A32 32 0 1 0 50.1 18 Z M50 32 A18 18 0 1 1 49.9 32 Z',
  wing: 'M50 80 C30 60 12 50 10 22 C24 34 34 34 42 30 C38 44 44 52 50 56 C56 52 62 44 58 30 C66 34 76 34 90 22 C88 50 70 60 50 80 Z',
  meteor: 'M62 38 A18 18 0 1 1 61.9 38 Z M16 16 L50 44 M24 10 L54 38 M10 26 L44 50',
  thorn: 'M50 90 L50 20 M50 70 L30 58 M50 54 L72 42 M50 38 L34 28 M30 58 L22 50 M72 42 L80 36 M34 28 L28 18 M50 20 L44 10 M50 20 L58 12',
  leaf: 'M20 82 C20 40 40 18 84 16 C82 58 62 80 20 82 Z M20 82 L66 34',
  paw: 'M50 50 C64 50 72 64 70 74 C68 84 58 82 50 80 C42 82 32 84 30 74 C28 64 36 50 50 50 Z M28 44 A8 10 0 1 0 28.1 44 Z M44 30 A8 10 0 1 0 44.1 30 Z M60 30 A8 10 0 1 0 60.1 30 Z M76 44 A8 10 0 1 0 76.1 44 Z',
  bloom: 'M50 50 m-8 0 a8 8 0 1 0 16 0 a8 8 0 1 0 -16 0 M50 42 C40 20 60 20 50 42 M58 50 C80 40 80 60 58 50 M50 58 C60 80 40 80 50 58 M42 50 C20 60 20 40 42 50',
  potion: 'M40 10 L60 10 L60 18 L56 18 L56 34 C72 40 80 52 78 66 C76 82 64 90 50 90 C36 90 24 82 22 66 C20 52 28 40 44 34 L44 18 L40 18 Z M28 62 L72 62',
  leap: 'M16 80 C30 40 60 24 84 20 M84 20 L66 18 M84 20 L78 36 M20 86 L40 86',
  snow: 'M50 10 L50 90 M15 30 L85 70 M15 70 L85 30 M40 18 L50 28 L60 18 M40 82 L50 72 L60 82',
  skull: 'M50 14 C28 14 18 30 20 48 C22 58 28 62 30 66 L30 80 L70 80 L70 66 C72 62 78 58 80 48 C82 30 72 14 50 14 Z M38 44 A7 7 0 1 0 38.1 44 Z M62 44 A7 7 0 1 0 62.1 44 Z',
  rune: 'M50 8 L80 30 L80 70 L50 92 L20 70 L20 30 Z M50 25 L50 75 M35 40 L65 60 M65 40 L35 60',
  claw: 'M25 20 C45 40 45 65 30 85 M45 15 C65 40 65 65 50 88 M65 20 C85 40 85 65 70 85',
};

export function glyphSvg(g: Glyph, stroke = '#fff'): string {
  const filled = ['bolt', 'flame', 'wing', 'shield', 'storm', 'leaf', 'paw', 'potion', 'skull'].includes(g);
  return `<svg viewBox="0 0 100 100" aria-hidden="true"><path d="${PATHS[g]}" fill="${filled ? 'rgba(255,255,255,0.92)' : 'none'}" stroke="${stroke}" stroke-width="${filled ? 3 : 7}" stroke-linecap="round" stroke-linejoin="round"/></svg>`;
}

export function iconHtml(g: Glyph, colors: [string, string]): string {
  return `<div class="icon" style="background:radial-gradient(circle at 35% 30%, ${colors[0]}, ${colors[1]} 75%)">${glyphSvg(g)}</div>`;
}
