// Enemies and the three maps (stages) of Ashenveil: the Vale, Frostmarch and the Sunscar Dunes.
// Map themes hold every colour and scenery rule, so visuals can be restyled without touching gameplay.

export type MobKind =
  | 'wolf' | 'spider' | 'raider' | 'brute' | 'chieftain' | 'imp' | 'boss'
  | 'rimewolf' | 'frostling' | 'yeti' | 'drowned' | 'thane' | 'shard' | 'ysolde'
  | 'scorpion' | 'nomad' | 'golem' | 'cultist' | 'warlord' | 'wisp' | 'azhkar';
export type ModelKind = 'wolf' | 'spider' | 'humanoid' | 'brute' | 'boss' | 'imp';
export type MapId = 'vale' | 'frostmarch' | 'sunscar';

export interface BossDef {
  /** Telegraphed circle dropped on the player's position. */
  ring: { radius: number; every: number; delay: number; frac: number; color: string };
  adds: MobKind;
  addsCount: number;
  /** Health fractions at which adds are summoned. */
  addsAt: number[];
  enrageAt: number;
  title: string;
}

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
  elite?: boolean;
  boss?: BossDef;
  /** Summoned by a boss; never respawns and drops nothing. */
  summon?: boolean;
  onHit?: { effect: 'poison' | 'slow' | 'burn'; chance: number };
  slam?: { radius: number; every: number; mul: number };
  /** Heals once at half health. */
  rally?: boolean;
  questDrop?: { item: string; chance: number };
  respawn?: number;
}

export const MOBS: Record<MobKind, MobDef> = {
  // ---------------- The Vale (1-10) ----------------
  wolf: { kind: 'wolf', name: 'Dusk Wolf', model: 'wolf', levels: [1, 2], hpMul: 1, dmgMul: 1, speed: 6.5, scale: 1, color: '#5a5550', accent: '#2d2a28', questDrop: { item: 'pelt', chance: 0.6 } },
  spider: { kind: 'spider', name: 'Glade Stalker', model: 'spider', levels: [3, 5], hpMul: 0.95, dmgMul: 1.05, speed: 6, scale: 1, color: '#3b2f4a', accent: '#9be26a', onHit: { effect: 'poison', chance: 0.3 } },
  raider: { kind: 'raider', name: 'Hollowkin Raider', model: 'humanoid', levels: [5, 7], hpMul: 1, dmgMul: 1, speed: 5.6, scale: 1, color: '#6b4b2a', accent: '#a33a2a' },
  chieftain: { kind: 'chieftain', name: 'Chieftain Gorran', model: 'humanoid', levels: [8, 8], hpMul: 3.2, dmgMul: 1.3, speed: 5.6, scale: 1.35, color: '#4d2f1c', accent: '#e0b040', elite: true, rally: true, slam: { radius: 6.5, every: 11, mul: 1.8 }, questDrop: { item: 'relic', chance: 1 }, respawn: 60 },
  brute: { kind: 'brute', name: 'Ashbound Brute', model: 'brute', levels: [7, 9], hpMul: 1.25, dmgMul: 1.05, speed: 5, scale: 1, color: '#3a3533', accent: '#ff6a2a', slam: { radius: 5.5, every: 11, mul: 1.8 } },
  imp: { kind: 'imp', name: 'Cinder Imp', model: 'imp', levels: [8, 8], hpMul: 0.5, dmgMul: 0.4, speed: 7, scale: 1, color: '#c2410c', accent: '#ffd166', summon: true },
  boss: {
    kind: 'boss', name: 'Varkul the Cindermaw', model: 'boss', levels: [10, 10], hpMul: 8, dmgMul: 1.1, speed: 5.2, scale: 1, color: '#2a1d1a', accent: '#ff5a1f', elite: true, respawn: 150,
    boss: { ring: { radius: 5.5, every: 8.5, delay: 1.9, frac: 0.25, color: '#ff3b2a' }, adds: 'imp', addsCount: 2, addsAt: [0.65, 0.35], enrageAt: 0.2, title: 'Varkul' },
  },
  // ---------------- Frostmarch (10-20) ----------------
  rimewolf: { kind: 'rimewolf', name: 'Rimefang Wolf', model: 'wolf', levels: [10, 12], hpMul: 1, dmgMul: 1, speed: 6.8, scale: 1.15, color: '#d8e4ee', accent: '#8aa6c0', onHit: { effect: 'slow', chance: 0.2 } },
  frostling: { kind: 'frostling', name: 'Frostling Skulker', model: 'imp', levels: [11, 13], hpMul: 0.9, dmgMul: 1.05, speed: 6.5, scale: 1.2, color: '#5a8ac0', accent: '#dff7ff', questDrop: { item: 'frostcore', chance: 0.55 } },
  yeti: { kind: 'yeti', name: 'Hoarfrost Behemoth', model: 'brute', levels: [13, 15], hpMul: 1.35, dmgMul: 1.05, speed: 5, scale: 1.1, color: '#e8eef4', accent: '#7fc8ff', slam: { radius: 6, every: 12, mul: 1.7 } },
  drowned: { kind: 'drowned', name: 'Drowned Warden', model: 'humanoid', levels: [15, 17], hpMul: 1.05, dmgMul: 1.05, speed: 5.4, scale: 1.05, color: '#2a5a5a', accent: '#8affe0', onHit: { effect: 'slow', chance: 0.25 } },
  thane: { kind: 'thane', name: 'Thane Hrodric the Frostbound', model: 'humanoid', levels: [18, 18], hpMul: 3.2, dmgMul: 1.25, speed: 5.6, scale: 1.45, color: '#3a4a6a', accent: '#bfefff', elite: true, rally: true, slam: { radius: 7, every: 11, mul: 1.8 }, questDrop: { item: 'thanecrown', chance: 1 }, respawn: 60 },
  shard: { kind: 'shard', name: 'Rime Shard', model: 'imp', levels: [18, 18], hpMul: 0.45, dmgMul: 0.35, speed: 7, scale: 0.9, color: '#9fdfff', accent: '#ffffff', summon: true },
  ysolde: {
    kind: 'ysolde', name: 'Ysolde, the Rime Queen', model: 'boss', levels: [20, 20], hpMul: 8, dmgMul: 1, speed: 5.2, scale: 0.95, color: '#c8e0f4', accent: '#6ad0ff', elite: true, respawn: 150,
    boss: { ring: { radius: 6, every: 8, delay: 1.9, frac: 0.25, color: '#6ad0ff' }, adds: 'shard', addsCount: 2, addsAt: [0.7, 0.4], enrageAt: 0.2, title: 'Ysolde' },
  },
  // ---------------- Sunscar Dunes (20-30) ----------------
  scorpion: { kind: 'scorpion', name: 'Dune Stinger', model: 'spider', levels: [20, 22], hpMul: 1, dmgMul: 1, speed: 6.4, scale: 1.2, color: '#b8864a', accent: '#ff5a3a', onHit: { effect: 'poison', chance: 0.35 }, questDrop: { item: 'venomsac', chance: 0.55 } },
  nomad: { kind: 'nomad', name: 'Sandveil Raider', model: 'humanoid', levels: [22, 24], hpMul: 1, dmgMul: 1.05, speed: 5.8, scale: 1, color: '#c8a870', accent: '#6a2a5a' },
  golem: { kind: 'golem', name: 'Glassforged Golem', model: 'brute', levels: [24, 26], hpMul: 1.4, dmgMul: 1.05, speed: 4.8, scale: 1.15, color: '#b8a080', accent: '#8affff', slam: { radius: 6, every: 11, mul: 1.8 } },
  cultist: { kind: 'cultist', name: 'Sunflayer Zealot', model: 'humanoid', levels: [25, 27], hpMul: 1, dmgMul: 1.1, speed: 5.6, scale: 1, color: '#6a2a5a', accent: '#ffd84a', onHit: { effect: 'burn', chance: 0.35 } },
  warlord: { kind: 'warlord', name: 'Warlord Szaran', model: 'humanoid', levels: [28, 28], hpMul: 3.2, dmgMul: 1.25, speed: 5.6, scale: 1.45, color: '#8a5a2a', accent: '#ffd84a', elite: true, rally: true, slam: { radius: 7, every: 10, mul: 1.8 }, questDrop: { item: 'sunsigil', chance: 1 }, respawn: 60 },
  wisp: { kind: 'wisp', name: 'Sun Wisp', model: 'imp', levels: [28, 28], hpMul: 0.45, dmgMul: 0.35, speed: 7.5, scale: 0.8, color: '#ffd84a', accent: '#ffffff', summon: true },
  azhkar: {
    kind: 'azhkar', name: 'Azhkar the Sunflayer', model: 'boss', levels: [30, 30], hpMul: 8.5, dmgMul: 1, speed: 5.4, scale: 1.05, color: '#6a4a1a', accent: '#ffd84a', elite: true, respawn: 150,
    boss: { ring: { radius: 6.5, every: 7.5, delay: 1.8, frac: 0.25, color: '#ffb020' }, adds: 'wisp', addsCount: 2, addsAt: [0.75, 0.5, 0.25], enrageAt: 0.2, title: 'Azhkar' },
  },
};

export interface Zone { id: string; name: string; x: number; z: number; radius: number; ground: string; groundAlt: string }
export interface Camp { kind: MobKind; x: number; z: number; count: number; spread: number }

export interface NpcDef {
  id: string;
  name: string;
  title: string;
  x: number;
  z: number;
  color: string;
  vendor?: boolean;
  travel?: boolean;
}

export type FloraKind = 'pine' | 'oak' | 'dead' | 'rock' | 'spire' | 'ember' | 'web' | 'snowpine' | 'iceshard' | 'cactus' | 'palm' | 'obelisk' | 'dune';

export interface FloraRule {
  kind: FloraKind;
  count: number;
  /** Only in these zones (ids); '' means outside every zone. */
  zones?: string[];
  notZones?: string[];
  /** 0..1 noise threshold that makes clusters (higher = sparser). */
  noise?: number;
  colors?: string[];
}

export interface MapTheme {
  meadow: [string, string];
  road: string;
  rock: string;
  shore: string;
  water: string;
  fog: string;
  skyTop: string;
  hemiSky: string;
  hemiGround: string;
  sun: string;
  /** Emissive colour for braziers, crystals and lava. */
  glow: string;
  house: { wall: string; roofs: string[] };
  tent: [string, string];
  /** A region that tints fog and sky when you are inside it. */
  hazard?: { x: number; z: number; r: number; fog: string; skyTop: string };
  flora: FloraRule[];
  heightScale: number;
}

export interface MapDef {
  id: MapId;
  name: string;
  subtitle: string;
  levels: [number, number];
  zones: Zone[];
  camps: Camp[];
  npcs: NpcDef[];
  roads: [number, number][][];
  lake: { x: number; z: number; r: number } | null;
  spawn: { x: number; z: number };
  /** Final quest of this map; completing it unlocks the next map. */
  finalQuest: string;
  next: MapId | null;
  theme: MapTheme;
}

const HUB = { x: 0, z: 150 };
const hubNpcs = (prefix: string, a: [string, string, string], b: [string, string, string], c: [string, string, string], v: [string, string, string], w: [string, string]): NpcDef[] => [
  { id: `${prefix}_a`, name: a[0], title: a[1], x: 4, z: 128, color: a[2] },
  { id: `${prefix}_b`, name: b[0], title: b[1], x: -14, z: 142, color: b[2] },
  { id: `${prefix}_c`, name: c[0], title: c[1], x: 16, z: 146, color: c[2] },
  { id: `${prefix}_v`, name: v[0], title: v[1], x: -6, z: 160, color: v[2], vendor: true },
  { id: `${prefix}_w`, name: w[0], title: 'Wayfinder', x: 19, z: 154, color: w[1], travel: true },
];

export const MAPS: Record<MapId, MapDef> = {
  vale: {
    id: 'vale', name: 'Vale of Ashenveil', subtitle: 'Act I', levels: [1, 10], spawn: { x: 0, z: 139 }, finalQuest: 'cindermaw', next: 'frostmarch',
    zones: [
      { id: 'hearthmoor', name: 'Hearthmoor', x: HUB.x, z: HUB.z, radius: 38, ground: '#6f8f45', groundAlt: '#86a255' },
      { id: 'duskglade', name: 'Duskwood Glade', x: -115, z: 85, radius: 60, ground: '#3f6a34', groundAlt: '#4f7a3a' },
      { id: 'webhollow', name: 'Webhollow', x: -125, z: -55, radius: 55, ground: '#4d4a55', groundAlt: '#5d5566' },
      { id: 'raidercamp', name: 'Hollowkin Camp', x: 120, z: 35, radius: 60, ground: '#7a6440', groundAlt: '#8c7550' },
      { id: 'ashenscar', name: 'The Ashen Scar', x: 30, z: -115, radius: 65, ground: '#34302e', groundAlt: '#443a35' },
      { id: 'cindermaw', name: "Cindermaw's Hollow", x: 30, z: -172, radius: 28, ground: '#2a2220', groundAlt: '#3b2520' },
    ],
    camps: [
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
    ],
    npcs: hubNpcs('vale',
      ['Warden Elra', 'Captain of Hearthmoor', '#3c5c8a'], ['Bram Hollis', 'Tanner', '#7a5230'], ['Mira Sedge', 'Herbalist', '#3f7a4c'],
      ['Odo Brask', 'Quartermaster', '#8a6d2a'], ['Pell Wayward', '#5a4a7a']),
    roads: [
      [[0, 150], [-40, 120], [-90, 100], [-120, 60], [-125, -30]],
      [[0, 150], [45, 110], [90, 70], [120, 40]],
      [[0, 150], [5, 90], [15, 20], [20, -60], [25, -110], [30, -150]],
    ],
    lake: { x: 70, z: 175, r: 32 },
    theme: {
      meadow: ['#5f8a3e', '#6f9a48'], road: '#9c8662', rock: '#77736c', shore: '#b8a47a', water: '#3b7ea6',
      fog: '#a9bfd0', skyTop: '#5f8fc9', hemiSky: '#dbe8ff', hemiGround: '#5a4a36', sun: '#fff1d6', glow: '#ff7a1a',
      house: { wall: '#d8ccb0', roofs: ['#8a3b2e', '#6b4a8a', '#3b5f8a', '#8a6a2e'] }, tent: ['#8a6a4a', '#6a3a2a'],
      hazard: { x: 30, z: -130, r: 110, fog: '#5a3a30', skyTop: '#2a1a18' },
      heightScale: 1,
      flora: [
        { kind: 'pine', count: 900, notZones: ['ashenscar', 'cindermaw', 'webhollow'], noise: 0.52, colors: ['#2f5a2c', '#3a6b32', '#264d2a'] },
        { kind: 'pine', count: 250, zones: ['duskglade'], colors: ['#2f5a2c', '#3a6b32', '#264d2a'] },
        { kind: 'oak', count: 260, zones: [''], noise: 0.45, colors: ['#5d8f3a', '#77a043', '#4f7f33'] },
        { kind: 'dead', count: 160, zones: ['webhollow'] },
        { kind: 'web', count: 40, zones: ['webhollow'] },
        { kind: 'rock', count: 380, noise: 0.5, colors: ['#7d786f', '#8b857a', '#6a655e'] },
        { kind: 'spire', count: 110, zones: ['ashenscar', 'cindermaw'], colors: ['#2b2523', '#3a302b', '#1f1a19'] },
        { kind: 'ember', count: 90, zones: ['ashenscar'] },
      ],
    },
  },
  frostmarch: {
    id: 'frostmarch', name: 'Frostmarch', subtitle: 'Act II', levels: [10, 20], spawn: { x: 0, z: 139 }, finalQuest: 'fm_queen', next: 'sunscar',
    zones: [
      { id: 'emberhold', name: 'Emberhold', x: HUB.x, z: HUB.z, radius: 38, ground: '#c8d0d8', groundAlt: '#d8dee4' },
      { id: 'rimewood', name: 'Rimewood', x: -110, z: 85, radius: 60, ground: '#b8c4cc', groundAlt: '#c8d4dc' },
      { id: 'frostdeep', name: 'Frostling Deep', x: -125, z: -55, radius: 55, ground: '#9ab0c4', groundAlt: '#aac0d4' },
      { id: 'whitefells', name: 'The White Fells', x: 115, z: 55, radius: 60, ground: '#e0e6ec', groundAlt: '#eef2f6' },
      { id: 'drownedmere', name: 'Drowned Mere', x: 120, z: -75, radius: 55, ground: '#7a8e98', groundAlt: '#8a9ea8' },
      { id: 'thanehold', name: 'Thanehold', x: -25, z: -105, radius: 40, ground: '#8894a0', groundAlt: '#98a4b0' },
      { id: 'rimethrone', name: 'The Rime Throne', x: 30, z: -172, radius: 28, ground: '#a8c8e0', groundAlt: '#b8d8f0' },
    ],
    camps: [
      { kind: 'rimewolf', x: -85, z: 110, count: 6, spread: 22 },
      { kind: 'rimewolf', x: -135, z: 65, count: 6, spread: 22 },
      { kind: 'frostling', x: -115, z: -30, count: 6, spread: 22 },
      { kind: 'frostling', x: -145, z: -80, count: 6, spread: 20 },
      { kind: 'yeti', x: 95, z: 75, count: 5, spread: 22 },
      { kind: 'yeti', x: 140, z: 35, count: 5, spread: 20 },
      { kind: 'drowned', x: 100, z: -60, count: 6, spread: 20 },
      { kind: 'drowned', x: 140, z: -100, count: 5, spread: 18 },
      { kind: 'thane', x: -25, z: -105, count: 1, spread: 0 },
      { kind: 'ysolde', x: 30, z: -172, count: 1, spread: 0 },
    ],
    npcs: hubNpcs('fm',
      ['Captain Rhosk', 'Warden of Emberhold', '#4a5a7a'], ['Seer Ilva', 'Frost Seer', '#6a7aa0'], ['Tamsin Vell', 'Smith', '#6a4a3a'],
      ['Hale Corbin', 'Provisioner', '#7a6a3a'], ['Oren Driftwalker', '#4a6a8a']),
    roads: [
      [[0, 150], [-40, 120], [-85, 100], [-115, 60], [-120, -30]],
      [[0, 150], [45, 115], [95, 75], [115, 20], [110, -60]],
      [[0, 150], [5, 90], [0, 20], [-15, -60], [-20, -90]],
      [[-20, -90], [10, -130], [30, -150]],
    ],
    lake: { x: 150, z: -40, r: 26 },
    theme: {
      meadow: ['#d4dce2', '#e4eaee'], road: '#8a8a90', rock: '#6a7480', shore: '#c0ccd4', water: '#6aa8d0',
      fog: '#c8d8e6', skyTop: '#7a9ec8', hemiSky: '#e8f2ff', hemiGround: '#6a7480', sun: '#f4f8ff', glow: '#6ad0ff',
      house: { wall: '#8a7a6a', roofs: ['#e8eef4', '#d8e0e8'] }, tent: ['#5a6a7a', '#3a4a5a'],
      hazard: { x: 30, z: -160, r: 70, fog: '#9ec0dc', skyTop: '#3a5a8a' },
      heightScale: 1.25,
      flora: [
        { kind: 'snowpine', count: 900, notZones: ['rimethrone', 'drownedmere', 'thanehold'], noise: 0.48, colors: ['#2a4a3a', '#34543e', '#243e34'] },
        { kind: 'snowpine', count: 260, zones: ['rimewood'], colors: ['#2a4a3a', '#34543e'] },
        { kind: 'dead', count: 120, zones: ['drownedmere', 'frostdeep'] },
        { kind: 'rock', count: 420, noise: 0.45, colors: ['#8a949e', '#9aa4ae', '#7a848e'] },
        { kind: 'iceshard', count: 160, zones: ['frostdeep', 'rimethrone', 'thanehold'] },
      ],
    },
  },
  sunscar: {
    id: 'sunscar', name: 'Sunscar Dunes', subtitle: 'Act III', levels: [20, 30], spawn: { x: 0, z: 139 }, finalQuest: 'ss_azhkar', next: null,
    zones: [
      { id: 'mirel', name: 'Oasis of Mirel', x: HUB.x, z: HUB.z, radius: 38, ground: '#9aa860', groundAlt: '#aab870' },
      { id: 'stingfields', name: 'The Sting Fields', x: -110, z: 85, radius: 60, ground: '#d8b878', groundAlt: '#e4c488' },
      { id: 'dunesea', name: 'Sea of Dunes', x: -125, z: -55, radius: 60, ground: '#e0c080', groundAlt: '#ecd090' },
      { id: 'glassflats', name: 'Glass Flats', x: 115, z: 55, radius: 60, ground: '#c8c0a8', groundAlt: '#d8d0b8' },
      { id: 'suntemple', name: 'Sunken Temple', x: 120, z: -75, radius: 55, ground: '#b89868', groundAlt: '#c8a878' },
      { id: 'warcamp', name: 'Szaran\'s Warcamp', x: -25, z: -105, radius: 40, ground: '#a88858', groundAlt: '#b89868' },
      { id: 'sunthrone', name: 'Throne of the Sun', x: 30, z: -172, radius: 28, ground: '#c8a040', groundAlt: '#d8b050' },
    ],
    camps: [
      { kind: 'scorpion', x: -85, z: 110, count: 6, spread: 22 },
      { kind: 'scorpion', x: -135, z: 65, count: 6, spread: 22 },
      { kind: 'nomad', x: -115, z: -30, count: 6, spread: 22 },
      { kind: 'nomad', x: -145, z: -80, count: 6, spread: 20 },
      { kind: 'golem', x: 95, z: 75, count: 5, spread: 22 },
      { kind: 'golem', x: 140, z: 35, count: 5, spread: 20 },
      { kind: 'cultist', x: 100, z: -60, count: 6, spread: 20 },
      { kind: 'cultist', x: 140, z: -100, count: 5, spread: 18 },
      { kind: 'warlord', x: -25, z: -105, count: 1, spread: 0 },
      { kind: 'azhkar', x: 30, z: -172, count: 1, spread: 0 },
    ],
    npcs: hubNpcs('ss',
      ['Captain Aziel', 'Shield of Mirel', '#8a5a2a'], ['Scholar Nesrin', 'Keeper of Records', '#5a3a6a'], ['Old Tahir', 'Water-finder', '#6a7a3a'],
      ['Qadir Salt', 'Trader', '#a0702a'], ['Sahra Farstep', '#3a6a7a']),
    roads: [
      [[0, 150], [-40, 120], [-85, 100], [-115, 60], [-120, -30]],
      [[0, 150], [45, 115], [95, 75], [115, 20], [110, -60]],
      [[0, 150], [5, 90], [0, 20], [-15, -60], [-20, -90]],
      [[-20, -90], [10, -130], [30, -150]],
    ],
    lake: { x: 45, z: 175, r: 22 },
    theme: {
      meadow: ['#d8b878', '#e4c488'], road: '#a88858', rock: '#a08060', shore: '#8aa050', water: '#3ab0b0',
      fog: '#f0dcb0', skyTop: '#6ab0e8', hemiSky: '#fff4dc', hemiGround: '#a08050', sun: '#fff0c8', glow: '#ffb020',
      house: { wall: '#e8d8b0', roofs: ['#c8a060', '#b8804a', '#e0c890'] }, tent: ['#c86a4a', '#e0c890'],
      hazard: { x: 30, z: -165, r: 70, fog: '#f0b870', skyTop: '#c86a2a' },
      heightScale: 0.9,
      flora: [
        { kind: 'palm', count: 70, zones: ['mirel'] },
        { kind: 'cactus', count: 380, notZones: ['mirel', 'sunthrone', 'glassflats'], noise: 0.5 },
        { kind: 'dune', count: 220, zones: ['dunesea', 'stingfields', ''], noise: 0.4 },
        { kind: 'rock', count: 320, noise: 0.5, colors: ['#b89060', '#a88050', '#c8a070'] },
        { kind: 'obelisk', count: 50, zones: ['suntemple', 'sunthrone', 'warcamp'] },
        { kind: 'iceshard', count: 90, zones: ['glassflats'], colors: ['#bff8ff'] },
      ],
    },
  },
};

export const MAP_ORDER: MapId[] = ['vale', 'frostmarch', 'sunscar'];

export const ALL_NPCS: NpcDef[] = MAP_ORDER.flatMap((m) => MAPS[m].npcs);

/** Hard edge of the walkable world. */
export const WORLD_LIMIT = 226;

export function mapOfMob(kind: MobKind): MapId | null {
  for (const id of MAP_ORDER) if (MAPS[id].camps.some((c) => c.kind === kind)) return id;
  return null;
}
