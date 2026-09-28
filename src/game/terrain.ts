// Deterministic terrain for a map: height, colour and zone of any point. Pure functions.
import { MOBS, WORLD_LIMIT, type MapDef, type Zone } from '../data/world';

function hash(ix: number, iz: number, seed: number): number {
  let h = (ix * 374761393 + iz * 668265263 + seed * 2246822519) | 0;
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
}

function smooth(t: number) {
  return t * t * (3 - 2 * t);
}

export function valueNoise(x: number, z: number, seed = 0): number {
  const ix = Math.floor(x);
  const iz = Math.floor(z);
  const fx = smooth(x - ix);
  const fz = smooth(z - iz);
  const a = hash(ix, iz, seed);
  const b = hash(ix + 1, iz, seed);
  const c = hash(ix, iz + 1, seed);
  const d = hash(ix + 1, iz + 1, seed);
  return (a + (b - a) * fx) * (1 - fz) + (c + (d - c) * fx) * fz;
}

export function fbm(x: number, z: number, octaves = 4, seed = 0): number {
  let sum = 0;
  let amp = 0.5;
  let f = 1;
  for (let i = 0; i < octaves; i++) {
    sum += amp * (valueNoise(x * f, z * f, seed) * 2 - 1);
    f *= 2.03;
    amp *= 0.5;
  }
  return sum;
}

export function smoothstep(e0: number, e1: number, x: number): number {
  const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0)));
  return t * t * (3 - 2 * t);
}

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

export interface Rgb { r: number; g: number; b: number }

export function hexToRgb(hex: string): Rgb {
  const n = parseInt(hex.slice(1), 16);
  return { r: ((n >> 16) & 255) / 255, g: ((n >> 8) & 255) / 255, b: (n & 255) / 255 };
}

function mix(a: Rgb, b: Rgb, t: number): Rgb {
  return { r: a.r + (b.r - a.r) * t, g: a.g + (b.g - a.g) * t, b: a.b + (b.b - a.b) * t };
}

export const WATER_LEVEL = -1.2;

export class Terrain {
  readonly seed: number;
  private flats: { x: number; z: number; r: number; h: number }[];
  private zoneRgb: { zone: Zone; a: Rgb; b: Rgb }[];
  private meadow: [Rgb, Rgb];
  private road: Rgb;
  private rock: Rgb;
  private shore: Rgb;
  private rough: { x: number; z: number } | null;

  constructor(readonly map: MapDef) {
    this.seed = map.id === 'vale' ? 0 : map.id === 'frostmarch' ? 17 : 31;
    const hub = map.zones[0];
    this.flats = [
      { x: hub.x, z: hub.z, r: 34, h: 0.6 },
      // Elite and boss camps get a flattened arena.
      ...map.camps
        .filter((c) => MOBS[c.kind].elite)
        .map((c) => ({ x: c.x, z: c.z, r: 20, h: MOBS[c.kind].boss ? -1 : 0.8 })),
    ];
    this.zoneRgb = map.zones.map((z) => ({ zone: z, a: hexToRgb(z.ground), b: hexToRgb(z.groundAlt) }));
    const t = map.theme;
    this.meadow = [hexToRgb(t.meadow[0]), hexToRgb(t.meadow[1])];
    this.road = hexToRgb(t.road);
    this.rock = hexToRgb(t.rock);
    this.shore = hexToRgb(t.shore);
    this.rough = t.hazard ? { x: t.hazard.x, z: t.hazard.z } : null;
  }

  roadDistance(x: number, z: number): number {
    let best = Infinity;
    for (const r of this.map.roads) {
      for (let i = 0; i < r.length - 1; i++) {
        best = Math.min(best, distToSegment(x, z, r[i][0], r[i][1], r[i + 1][0], r[i + 1][1]));
      }
    }
    return best;
  }

  heightAt(x: number, z: number): number {
    const s = this.seed;
    let h = (fbm(x * 0.011, z * 0.011, 4, s) * 10 + fbm(x * 0.05 + 7, z * 0.05 - 3, 2, s) * 1.2) * this.map.theme.heightScale;
    if (this.rough) {
      const k = 1 - smoothstep(40, 90, Math.hypot(x - this.rough.x, z - this.rough.z));
      h += k * fbm(x * 0.04, z * 0.04, 3, s) * 4;
    }
    const edge = Math.max(Math.abs(x), Math.abs(z));
    if (edge > WORLD_LIMIT - 30) h += Math.pow(edge - (WORLD_LIMIT - 30), 1.5) * 0.45;
    const lake = this.map.lake;
    if (lake) {
      const ld = Math.hypot(x - lake.x, z - lake.z);
      h -= (1 - smoothstep(lake.r * 0.3, lake.r, ld)) * 5;
    }
    const roadW = 1 - smoothstep(3, 9, this.roadDistance(x, z));
    h = h * (1 - roadW * 0.75);
    for (const f of this.flats) {
      const w = 1 - smoothstep(f.r * 0.6, f.r, Math.hypot(x - f.x, z - f.z));
      h = h + (f.h - h) * w;
    }
    return h;
  }

  colorAt(x: number, z: number, h: number, slope: number): Rgb {
    const n = valueNoise(x * 0.09, z * 0.09, this.seed);
    let c = mix(this.meadow[0], this.meadow[1], n);
    for (const zr of this.zoneRgb) {
      const d = Math.hypot(x - zr.zone.x, z - zr.zone.z);
      const w = 1 - smoothstep(zr.zone.radius * 0.6, zr.zone.radius * 1.15, d);
      if (w > 0) c = mix(c, mix(zr.a, zr.b, n), w);
    }
    const rd = this.roadDistance(x, z);
    c = mix(c, this.road, (1 - smoothstep(2.2, 4.5, rd)) * 0.9);
    if (h < WATER_LEVEL + 0.8) c = mix(c, this.shore, 1 - smoothstep(WATER_LEVEL, WATER_LEVEL + 0.8, h));
    c = mix(c, this.rock, smoothstep(0.55, 0.9, slope));
    return c;
  }

  zoneAt(x: number, z: number): Zone | null {
    let best: Zone | null = null;
    let bestD = Infinity;
    for (const zn of this.map.zones) {
      const d = Math.hypot(x - zn.x, z - zn.z) / zn.radius;
      if (d < 1 && d < bestD) {
        bestD = d;
        best = zn;
      }
    }
    return best;
  }

  /** Zone id used by scenery rules (within 1.1 radius), '' outside every zone. */
  sceneryZone(x: number, z: number): string {
    let best = '';
    let bd = Infinity;
    for (const zn of this.map.zones) {
      const d = Math.hypot(x - zn.x, z - zn.z) / zn.radius;
      if (d < 1.1 && d < bd) {
        bd = d;
        best = zn.id;
      }
    }
    return best;
  }
}
