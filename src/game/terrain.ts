// Deterministic terrain: height and colour of any point in the world. Pure functions.
import { ZONES, CAMPS, WORLD_LIMIT } from '../data/world';

function hash(ix: number, iz: number): number {
  let h = (ix * 374761393 + iz * 668265263) | 0;
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
}

function smooth(t: number) {
  return t * t * (3 - 2 * t);
}

export function valueNoise(x: number, z: number): number {
  const ix = Math.floor(x);
  const iz = Math.floor(z);
  const fx = smooth(x - ix);
  const fz = smooth(z - iz);
  const a = hash(ix, iz);
  const b = hash(ix + 1, iz);
  const c = hash(ix, iz + 1);
  const d = hash(ix + 1, iz + 1);
  return (a + (b - a) * fx) * (1 - fz) + (c + (d - c) * fx) * fz;
}

export function fbm(x: number, z: number, octaves = 4): number {
  let sum = 0;
  let amp = 0.5;
  let f = 1;
  for (let i = 0; i < octaves; i++) {
    sum += amp * (valueNoise(x * f, z * f) * 2 - 1);
    f *= 2.03;
    amp *= 0.5;
  }
  return sum;
}

export function smoothstep(e0: number, e1: number, x: number): number {
  const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0)));
  return t * t * (3 - 2 * t);
}

// Roads as polylines between the village and each area.
export const ROADS: [number, number][][] = [
  [[0, 150], [-40, 120], [-90, 100], [-120, 60], [-125, -30]],
  [[0, 150], [45, 110], [90, 70], [120, 40]],
  [[0, 150], [5, 90], [15, 20], [20, -60], [25, -110], [30, -150]],
];

export const LAKE = { x: 70, z: 175, r: 32 };
export const WATER_LEVEL = -1.2;

function distToSegment(px: number, pz: number, ax: number, az: number, bx: number, bz: number): number {
  const dx = bx - ax;
  const dz = bz - az;
  const l2 = dx * dx + dz * dz;
  let t = l2 > 0 ? ((px - ax) * dx + (pz - az) * dz) / l2 : 0;
  t = Math.max(0, Math.min(1, t));
  const cx = ax + dx * t - px;
  const cz = az + dz * t - pz;
  return Math.sqrt(cx * cx + cz * cz);
}

export function roadDistance(x: number, z: number): number {
  let best = Infinity;
  for (const r of ROADS) {
    for (let i = 0; i < r.length - 1; i++) {
      best = Math.min(best, distToSegment(x, z, r[i][0], r[i][1], r[i + 1][0], r[i + 1][1]));
    }
  }
  return best;
}

/** Flat areas: the village, camps and the boss arena. */
const FLATS: { x: number; z: number; r: number; h: number }[] = [
  { x: 0, z: 150, r: 34, h: 0.6 },
  ...CAMPS.filter((c) => c.kind === 'chieftain' || c.kind === 'boss').map((c) => ({ x: c.x, z: c.z, r: 20, h: c.kind === 'boss' ? -1 : 0.8 })),
];

export function heightAt(x: number, z: number): number {
  let h = fbm(x * 0.011, z * 0.011) * 10 + fbm(x * 0.05 + 7, z * 0.05 - 3, 2) * 1.2;
  // The Scar is rougher.
  const scar = 1 - smoothstep(40, 90, Math.hypot(x - 30, z + 125));
  h += scar * fbm(x * 0.04, z * 0.04, 3) * 4;
  // Mountains ring the vale.
  const edge = Math.max(Math.abs(x), Math.abs(z));
  if (edge > WORLD_LIMIT - 30) h += Math.pow(edge - (WORLD_LIMIT - 30), 1.5) * 0.45;
  // Lake bowl.
  const ld = Math.hypot(x - LAKE.x, z - LAKE.z);
  h -= (1 - smoothstep(LAKE.r * 0.3, LAKE.r, ld)) * 5;
  // Flatten roads and settlements.
  const rd = roadDistance(x, z);
  const roadW = 1 - smoothstep(3, 9, rd);
  h = h * (1 - roadW * 0.75);
  for (const f of FLATS) {
    const w = 1 - smoothstep(f.r * 0.6, f.r, Math.hypot(x - f.x, z - f.z));
    h = h + (f.h - h) * w;
  }
  return h;
}

export interface Rgb { r: number; g: number; b: number }

export function hexToRgb(hex: string): Rgb {
  const n = parseInt(hex.slice(1), 16);
  return { r: ((n >> 16) & 255) / 255, g: ((n >> 8) & 255) / 255, b: (n & 255) / 255 };
}

function mix(a: Rgb, b: Rgb, t: number): Rgb {
  return { r: a.r + (b.r - a.r) * t, g: a.g + (b.g - a.g) * t, b: a.b + (b.b - a.b) * t };
}

const MEADOW = hexToRgb('#5f8a3e');
const MEADOW_ALT = hexToRgb('#6f9a48');
const ROAD = hexToRgb('#9c8662');
const ROCK = hexToRgb('#77736c');
const SAND = hexToRgb('#b8a47a');
const ZONE_RGB = ZONES.map((z) => ({ zone: z, a: hexToRgb(z.ground), b: hexToRgb(z.groundAlt) }));

export function colorAt(x: number, z: number, h: number, slope: number): Rgb {
  const n = valueNoise(x * 0.09, z * 0.09);
  let c = mix(MEADOW, MEADOW_ALT, n);
  for (const zr of ZONE_RGB) {
    const d = Math.hypot(x - zr.zone.x, z - zr.zone.z);
    const w = 1 - smoothstep(zr.zone.radius * 0.6, zr.zone.radius * 1.15, d);
    if (w > 0) c = mix(c, mix(zr.a, zr.b, n), w);
  }
  const rd = roadDistance(x, z);
  c = mix(c, ROAD, (1 - smoothstep(2.2, 4.5, rd)) * 0.9);
  if (h < WATER_LEVEL + 0.8) c = mix(c, SAND, 1 - smoothstep(WATER_LEVEL, WATER_LEVEL + 0.8, h));
  c = mix(c, ROCK, smoothstep(0.55, 0.9, slope));
  return c;
}

export function zoneAt(x: number, z: number) {
  let best = null as (typeof ZONES)[number] | null;
  let bestD = Infinity;
  for (const zn of ZONES) {
    const d = Math.hypot(x - zn.x, z - zn.z) / zn.radius;
    if (d < 1 && d < bestD) {
      bestD = d;
      best = zn;
    }
  }
  return best;
}
