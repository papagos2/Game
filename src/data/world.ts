// Enemy types, zones, spawn camps and NPCs of the Vale of Ashenveil.

export type MobKind = 'wolf' | 'spider' | 'raider' | 'brute' | 'chieftain' | 'imp' | 'boss';
export type ModelKind = 'wolf' | 'spider' | 'humanoid' | 'brute' | 'boss' | 'imp';

export interface MobDef {
  kind: MobKind;
  name: string;
  model: ModelKind;
  levels: [number, number];
  hpMul: number;
  dmgMul: number;
  speed: number;
  scale: number;
  color: string;
  accent: string;
  ranged?: boolean;
  elite?: boolean;
  /** Quest item this mob can drop, with chance 0..1. */
  questDrop?: { item: string; chance: number };
}

export const MOBS: Record<MobKind, MobDef> = {
  wolf: {
    kind: 'wolf', name: 'Dusk Wolf', model: 'wolf', levels: [1, 2], hpMul: 1, dmgMul: 1,
    speed: 6.5, scale: 1, color: '#5a5550', accent: '#2d2a28',
    questDrop: { item: 'pelt', chance: 0.6 },
  },
  spider: {
    kind: 'spider', name: 'Glade Stalker', model: 'spider', levels: [3, 5], hpMul: 0.95, dmgMul: 1.05,
    speed: 6, scale: 1, color: '#3b2f4a', accent: '#9be26a',
  },
  raider: {
    kind: 'raider', name: 'Hollowkin Raider', model: 'humanoid', levels: [5, 7], hpMul: 1, dmgMul: 1,
    speed: 5.6, scale: 1, color: '#6b4b2a', accent: '#a33a2a',
  },
  chieftain: {
    kind: 'chieftain', name: 'Chieftain Gorran', model: 'humanoid', levels: [8, 8], hpMul: 3.2, dmgMul: 1.3,
    speed: 5.6, scale: 1.35, color: '#4d2f1c', accent: '#e0b040', elite: true,
    questDrop: { item: 'relic', chance: 1 },
  },
  brute: {
    kind: 'brute', name: 'Ashbound Brute', model: 'brute', levels: [7, 9], hpMul: 1.25, dmgMul: 1.05,
    speed: 5, scale: 1, color: '#3a3533', accent: '#ff6a2a',
  },
  imp: {
    kind: 'imp', name: 'Cinder Imp', model: 'imp', levels: [8, 8], hpMul: 0.5, dmgMul: 0.4,
    speed: 7, scale: 1, color: '#c2410c', accent: '#ffd166',
  },
  boss: {
    kind: 'boss', name: 'Varkul the Cindermaw', model: 'boss', levels: [10, 10], hpMul: 8, dmgMul: 1.1,
    speed: 5.2, scale: 1, color: '#2a1d1a', accent: '#ff5a1f', elite: true,
  },
};

export interface Zone {
  id: string;
  name: string;
  x: number;
  z: number;
  radius: number;
  ground: string;
  groundAlt: string;
}

// World spans -240..240 on x and z. North is -z.
export const ZONES: Zone[] = [
  { id: 'hearthmoor', name: 'Hearthmoor', x: 0, z: 150, radius: 38, ground: '#6f8f45', groundAlt: '#86a255' },
  { id: 'duskglade', name: 'Duskwood Glade', x: -115, z: 85, radius: 60, ground: '#3f6a34', groundAlt: '#4f7a3a' },
  { id: 'webhollow', name: 'Webhollow', x: -125, z: -55, radius: 55, ground: '#4d4a55', groundAlt: '#5d5566' },
  { id: 'raidercamp', name: 'Hollowkin Camp', x: 120, z: 35, radius: 60, ground: '#7a6440', groundAlt: '#8c7550' },
  { id: 'ashenscar', name: 'The Ashen Scar', x: 30, z: -115, radius: 65, ground: '#34302e', groundAlt: '#443a35' },
  { id: 'cindermaw', name: "Cindermaw's Hollow", x: 30, z: -172, radius: 28, ground: '#2a2220', groundAlt: '#3b2520' },
];

export const DEFAULT_ZONE = { name: 'Vale of Ashenveil' };

export interface Camp {
  kind: MobKind;
  x: number;
  z: number;
  count: number;
  spread: number;
}

export const CAMPS: Camp[] = [
  { kind: 'wolf', x: -90, z: 110, count: 6, spread: 22 },
  { kind: 'wolf', x: -135, z: 70, count: 6, spread: 24 },
  { kind: 'wolf', x: -60, z: 70, count: 4, spread: 16 },
  { kind: 'spider', x: -125, z: -30, count: 6, spread: 22 },
  { kind: 'spider', x: -140, z: -80, count: 6, spread: 22 },
  { kind: 'raider', x: 105, z: 55, count: 6, spread: 22 },
  { kind: 'raider', x: 140, z: 15, count: 6, spread: 20 },
  { kind: 'chieftain', x: 128, z: 36, count: 1, spread: 0 },
  { kind: 'brute', x: 0, z: -105, count: 5, spread: 20 },
  { kind: 'brute', x: 60, z: -125, count: 5, spread: 20 },
  { kind: 'boss', x: 30, z: -172, count: 1, spread: 0 },
];

export interface NpcDef {
  id: string;
  name: string;
  title: string;
  x: number;
  z: number;
  color: string;
  vendor?: boolean;
}

export const NPCS: NpcDef[] = [
  { id: 'elra', name: 'Warden Elra', title: 'Captain of Hearthmoor', x: 4, z: 128, color: '#3c5c8a' },
  { id: 'bram', name: 'Bram Hollis', title: 'Tanner', x: -14, z: 142, color: '#7a5230' },
  { id: 'mira', name: 'Mira Sedge', title: 'Herbalist', x: 16, z: 146, color: '#3f7a4c' },
  { id: 'odo', name: 'Odo Brask', title: 'Quartermaster', x: -6, z: 160, color: '#8a6d2a', vendor: true },
];

/** Where the player starts and returns to after death. */
export const SPAWN = { x: 0, z: 139 };

/** Hard edge of the walkable world. */
export const WORLD_LIMIT = 226;
