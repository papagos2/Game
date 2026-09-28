// Playable classes, their specializations and abilities. All names and designs are original.
// Abilities are data: a list of effects run by the ability engine in src/game/abilities.ts.

export type ClassId = 'stormblade' | 'emberseer' | 'thornkeeper';
export type SpecId =
  | 'tempestKnight' | 'bulwark' | 'spellblade'
  | 'pyromancer' | 'frostweaver' | 'thermancer'
  | 'grovewarden' | 'beastcaller' | 'rotbloom';
export type ResourceKind = 'fury' | 'mana';
export type PetKind = 'spiritWolf' | 'bear';

export type AbilityId =
  // Stormblade
  | 'thunderCleave' | 'skywardLeap' | 'staticGuard' | 'tempest'
  | 'chainLightning' | 'thunderstrike' | 'shieldWall' | 'earthshatter' | 'runeStrike' | 'blazingArc'
  // Emberseer
  | 'cinderLance' | 'ashRing' | 'phoenixVeil' | 'meteor'
  | 'inferno' | 'combust' | 'glacialSpike' | 'blizzard' | 'frostfireBolt' | 'thermalShock'
  // Thornkeeper
  | 'brambleSnare' | 'renewal' | 'spiritWolf' | 'wildBloom'
  | 'lifebloom' | 'barkskin' | 'packAlpha' | 'feralCall' | 'plague' | 'witherBurst';

export type Glyph =
  | 'sword' | 'bolt' | 'shield' | 'storm' | 'flame' | 'ring' | 'wing' | 'meteor'
  | 'thorn' | 'leaf' | 'paw' | 'bloom' | 'potion' | 'leap' | 'snow' | 'skull' | 'rune' | 'claw' | 'horse';

/** Who an effect hits. */
export type Area = 'self' | 'target' | 'aroundSelf' | 'aroundTarget' | 'cone';
export type Status = 'root' | 'stun' | 'slow';

export type Effect =
  | { t: 'damage'; area: Area; coef: number; radius?: number; bonusVs?: 'slowed' | 'dotted' | 'rooted'; bonusMul?: number }
  | { t: 'dot'; area: Area; coef: number; ticks: number; interval: number; radius?: number; color: string }
  | { t: 'status'; area: Area; status: Status; dur: number; radius?: number }
  | { t: 'heal'; coef: number }
  | { t: 'hot'; coef: number; ticks: number; interval: number }
  | { t: 'shield'; coef: number; dur: number }
  | { t: 'guard'; reduce: number; dur: number; reflect?: number }
  | { t: 'resource'; amount: number }
  | { t: 'blink'; dist: number }
  | { t: 'summon'; pet: PetKind; dur: number }
  | { t: 'petFrenzy'; dur: number }
  | { t: 'chain'; coef: number; jumps: number; radius: number }
  /** Consumes the caster's damage-over-time effects on enemies and deals the remaining damage at once. */
  | { t: 'detonate'; area: Area; radius: number; mult: number; coef: number }
  /** Marks the ground at the target and resolves the inner effects after a delay. */
  | { t: 'delayed'; delay: number; radius: number; effects: Effect[] };

export interface Fx { kind: 'ring' | 'arc' | 'lightning' | 'star' | 'sparkle' | 'nova'; color: string; radius?: number; at?: 'self' | 'target' }

export interface AbilityDef {
  id: AbilityId;
  name: string;
  glyph: Glyph;
  color: [string, string];
  /** Level it is learned at; spec abilities also need the spec. */
  unlockLevel: number;
  spec?: SpecId;
  cooldown: number;
  cost: number;
  /** Max distance to the target; 0 = no target needed. */
  range: number;
  needsTarget: boolean;
  delivery?: 'instant' | 'projectile' | 'leap';
  projectile?: { color: string; speed: number; size?: number };
  effects: Effect[];
  fx?: Fx[];
  sound?: 'hit' | 'cast' | 'fire' | 'shock' | 'heal' | 'boom' | 'swing' | 'frost';
  description: string;
}

export interface SpecDef {
  id: SpecId;
  cls: ClassId;
  name: string;
  role: string;
  blurb: string;
  color: string;
  glyph: Glyph;
  abilities: [AbilityId, AbilityId];
}

export interface ClassDef {
  id: ClassId;
  name: string;
  role: string;
  blurb: string;
  resource: ResourceKind;
  resourceName: string;
  resourceMax: number;
  resourceRegen: number;
  baseHp: number;
  baseArmor: number;
  attackRange: number;
  attackSpeed: number;
  attackDamage: number;
  ranged: boolean;
  /** Materials used for generated gear names. */
  armorType: 'plate' | 'cloth' | 'leather';
  colors: { body: string; trim: string; glow: string };
  abilities: [AbilityId, AbilityId, AbilityId, AbilityId];
  specs: [SpecId, SpecId, SpecId];
}

export const SPEC_LEVEL = 10;
/** Level at which heroes get their mount. */
export const MOUNT_LEVEL = 5;
export const SPEC_SECOND_ABILITY_LEVEL = 16;

const burn = '#ff9a3c';
const poison = '#9be26a';

export const ABILITIES: Record<AbilityId, AbilityDef> = {
  // ---------------- Stormblade ----------------
  thunderCleave: {
    id: 'thunderCleave', name: 'Thunder Cleave', glyph: 'sword', color: ['#7fb6ff', '#1d3c7a'],
    unlockLevel: 1, cooldown: 5, cost: 0, range: 0, needsTarget: false,
    effects: [{ t: 'damage', area: 'cone', radius: 5.5, coef: 20 }, { t: 'resource', amount: 20 }],
    fx: [{ kind: 'arc', color: '#8fd0ff' }], sound: 'shock',
    description: 'Sweep the blade in front of you, hitting every enemy within 5 m. Builds 20 Fury.',
  },
  skywardLeap: {
    id: 'skywardLeap', name: 'Skyward Leap', glyph: 'leap', color: ['#b7e0ff', '#2b4f8a'],
    unlockLevel: 2, cooldown: 12, cost: 15, range: 22, needsTarget: true, delivery: 'leap',
    effects: [{ t: 'damage', area: 'target', coef: 16 }, { t: 'status', area: 'target', status: 'stun', dur: 2 }],
    fx: [{ kind: 'ring', color: '#bfe3ff', radius: 4, at: 'self' }], sound: 'boom',
    description: 'Leap to your target, strike it and stun it for 2 seconds.',
  },
  staticGuard: {
    id: 'staticGuard', name: 'Static Guard', glyph: 'shield', color: ['#c6f0ff', '#285e7a'],
    unlockLevel: 4, cooldown: 20, cost: 25, range: 0, needsTarget: false,
    effects: [{ t: 'shield', coef: 60, dur: 8 }],
    fx: [{ kind: 'ring', color: '#c6f0ff', radius: 2.5 }], sound: 'shock',
    description: 'A crackling barrier absorbs damage for 8 seconds and shocks attackers.',
  },
  tempest: {
    id: 'tempest', name: 'Tempest', glyph: 'storm', color: ['#e2d5ff', '#3d2a86'],
    unlockLevel: 6, cooldown: 18, cost: 50, range: 0, needsTarget: false,
    effects: [{ t: 'damage', area: 'aroundSelf', radius: 8, coef: 55 }],
    fx: [{ kind: 'lightning', color: '#e8ddff' }, { kind: 'ring', color: '#b9a2ff', radius: 8 }], sound: 'boom',
    description: 'Call down a storm around you: heavy damage to all enemies within 8 m.',
  },
  chainLightning: {
    id: 'chainLightning', name: 'Chain Lightning', glyph: 'bolt', color: ['#d8f0ff', '#2a4fa0'], spec: 'tempestKnight',
    unlockLevel: SPEC_LEVEL, cooldown: 7, cost: 20, range: 20, needsTarget: true,
    effects: [{ t: 'chain', coef: 42, jumps: 4, radius: 9 }],
    sound: 'shock',
    description: 'Lightning leaps from your target to up to 4 more enemies nearby.',
  },
  thunderstrike: {
    id: 'thunderstrike', name: 'Thunderstrike', glyph: 'storm', color: ['#fff3a8', '#4a3aa0'], spec: 'tempestKnight',
    unlockLevel: SPEC_SECOND_ABILITY_LEVEL, cooldown: 14, cost: 35, range: 20, needsTarget: true,
    effects: [{ t: 'damage', area: 'target', coef: 95 }, { t: 'damage', area: 'aroundTarget', radius: 4, coef: 25 }],
    fx: [{ kind: 'lightning', color: '#fff3a8', at: 'target' }, { kind: 'ring', color: '#fff3a8', radius: 4, at: 'target' }], sound: 'boom',
    description: 'A bolt from the sky for massive damage to your target, and some to enemies beside it.',
  },
  shieldWall: {
    id: 'shieldWall', name: 'Shield Wall', glyph: 'shield', color: ['#e6d7b0', '#5a4a2a'], spec: 'bulwark',
    unlockLevel: SPEC_LEVEL, cooldown: 24, cost: 20, range: 0, needsTarget: false,
    effects: [{ t: 'guard', reduce: 0.6, dur: 6, reflect: 6 }],
    fx: [{ kind: 'nova', color: '#e6d7b0', radius: 3 }], sound: 'shock',
    description: 'Brace behind your guard: take 60% less damage for 6 seconds and punish attackers.',
  },
  earthshatter: {
    id: 'earthshatter', name: 'Earthshatter', glyph: 'claw', color: ['#d9b27a', '#5a3a1a'], spec: 'bulwark',
    unlockLevel: SPEC_SECOND_ABILITY_LEVEL, cooldown: 15, cost: 30, range: 0, needsTarget: false,
    effects: [{ t: 'damage', area: 'cone', radius: 8, coef: 50 }, { t: 'status', area: 'cone', radius: 8, status: 'stun', dur: 2 }],
    fx: [{ kind: 'arc', color: '#d9b27a' }], sound: 'boom',
    description: 'Split the ground in front of you, damaging and stunning enemies within 8 m.',
  },
  runeStrike: {
    id: 'runeStrike', name: 'Rune Strike', glyph: 'rune', color: ['#ffc36b', '#3a2a86'], spec: 'spellblade',
    unlockLevel: SPEC_LEVEL, cooldown: 5, cost: 0, range: 4, needsTarget: true,
    effects: [
      { t: 'damage', area: 'target', coef: 34 },
      { t: 'dot', area: 'target', coef: 30, ticks: 6, interval: 1, color: burn },
      { t: 'resource', amount: 15 },
    ],
    fx: [{ kind: 'ring', color: '#ffc36b', radius: 1.8, at: 'target' }], sound: 'fire',
    description: 'A blade strike charged with a burning rune. Builds 15 Fury.',
  },
  blazingArc: {
    id: 'blazingArc', name: 'Blazing Arc', glyph: 'wing', color: ['#ffd27a', '#5a1a6a'], spec: 'spellblade',
    unlockLevel: SPEC_SECOND_ABILITY_LEVEL, cooldown: 14, cost: 30, range: 0, needsTarget: false,
    effects: [
      { t: 'blink', dist: 10 },
      { t: 'damage', area: 'aroundSelf', radius: 6, coef: 45 },
      { t: 'dot', area: 'aroundSelf', radius: 6, coef: 30, ticks: 3, interval: 1, color: burn },
    ],
    fx: [{ kind: 'ring', color: '#ffb347', radius: 6 }], sound: 'fire',
    description: 'Dash 10 m forward in a sweep of flame that burns everything around you.',
  },
  // ---------------- Emberseer ----------------
  cinderLance: {
    id: 'cinderLance', name: 'Cinder Lance', glyph: 'flame', color: ['#ffc36b', '#8a2a0b'],
    unlockLevel: 1, cooldown: 3, cost: 18, range: 26, needsTarget: true, delivery: 'projectile',
    projectile: { color: '#ffb347', speed: 34, size: 0.45 },
    effects: [{ t: 'damage', area: 'target', coef: 26 }, { t: 'dot', area: 'target', coef: 20, ticks: 3, interval: 1, color: burn }],
    sound: 'fire',
    description: 'Hurl a spear of fire that burns the target.',
  },
  ashRing: {
    id: 'ashRing', name: 'Ash Ring', glyph: 'ring', color: ['#ffb08a', '#5c1e14'],
    unlockLevel: 2, cooldown: 14, cost: 25, range: 0, needsTarget: false,
    effects: [{ t: 'damage', area: 'aroundSelf', radius: 7, coef: 14 }, { t: 'status', area: 'aroundSelf', radius: 7, status: 'root', dur: 3 }],
    fx: [{ kind: 'ring', color: '#ff8a4a', radius: 7 }], sound: 'fire',
    description: 'A ring of ash erupts around you, damaging and rooting enemies within 7 m for 3 seconds.',
  },
  phoenixVeil: {
    id: 'phoenixVeil', name: 'Phoenix Veil', glyph: 'wing', color: ['#fff0a8', '#b3521a'],
    unlockLevel: 4, cooldown: 16, cost: 20, range: 0, needsTarget: false,
    effects: [{ t: 'blink', dist: 12 }, { t: 'heal', coef: 45 }],
    fx: [{ kind: 'ring', color: '#ffd27a', radius: 2.5 }], sound: 'fire',
    description: 'Burst forward 12 m in a flash of flame and restore some health.',
  },
  meteor: {
    id: 'meteor', name: 'Falling Star', glyph: 'meteor', color: ['#ffdf8a', '#7a1b0b'],
    unlockLevel: 6, cooldown: 20, cost: 45, range: 26, needsTarget: true,
    effects: [{ t: 'delayed', delay: 1.2, radius: 6, effects: [{ t: 'damage', area: 'aroundTarget', radius: 6, coef: 70 }] }],
    fx: [{ kind: 'star', color: '#ffdf8a', at: 'target' }], sound: 'cast',
    description: 'After a short delay a burning star strikes the target area (6 m).',
  },
  inferno: {
    id: 'inferno', name: 'Inferno', glyph: 'flame', color: ['#ff8a4a', '#6a0b0b'], spec: 'pyromancer',
    unlockLevel: SPEC_LEVEL, cooldown: 12, cost: 35, range: 26, needsTarget: true,
    effects: [{ t: 'dot', area: 'aroundTarget', radius: 7, coef: 95, ticks: 8, interval: 1, color: burn }],
    fx: [{ kind: 'ring', color: '#ff6a2a', radius: 7, at: 'target' }], sound: 'fire',
    description: 'Set the ground ablaze: all enemies within 7 m of the target burn for 8 seconds.',
  },
  combust: {
    id: 'combust', name: 'Combust', glyph: 'meteor', color: ['#ffe08a', '#a01a0b'], spec: 'pyromancer',
    unlockLevel: SPEC_SECOND_ABILITY_LEVEL, cooldown: 10, cost: 25, range: 26, needsTarget: true,
    effects: [{ t: 'detonate', area: 'aroundTarget', radius: 6, mult: 1.6, coef: 30 }],
    fx: [{ kind: 'nova', color: '#ffb347', radius: 6, at: 'target' }], sound: 'boom',
    description: 'Detonate every burn you have placed near the target, dealing the rest of their damage at once.',
  },
  glacialSpike: {
    id: 'glacialSpike', name: 'Glacial Spike', glyph: 'snow', color: ['#c8f0ff', '#1a4a8a'], spec: 'frostweaver',
    unlockLevel: SPEC_LEVEL, cooldown: 8, cost: 25, range: 26, needsTarget: true, delivery: 'projectile',
    projectile: { color: '#bfefff', speed: 30, size: 0.55 },
    effects: [{ t: 'damage', area: 'target', coef: 58 }, { t: 'status', area: 'target', status: 'stun', dur: 2 }],
    sound: 'frost',
    description: 'A spike of ice that deals heavy damage and freezes the target for 2 seconds.',
  },
  blizzard: {
    id: 'blizzard', name: 'Blizzard', glyph: 'snow', color: ['#e8fbff', '#2a5a9a'], spec: 'frostweaver',
    unlockLevel: SPEC_SECOND_ABILITY_LEVEL, cooldown: 16, cost: 40, range: 26, needsTarget: true,
    effects: [
      { t: 'status', area: 'aroundTarget', radius: 8, status: 'slow', dur: 6 },
      { t: 'dot', area: 'aroundTarget', radius: 8, coef: 75, ticks: 6, interval: 1, color: '#bfefff' },
    ],
    fx: [{ kind: 'nova', color: '#dff7ff', radius: 8, at: 'target' }], sound: 'frost',
    description: 'A storm of ice slows and wounds all enemies within 8 m of the target.',
  },
  frostfireBolt: {
    id: 'frostfireBolt', name: 'Frostfire Bolt', glyph: 'flame', color: ['#c8a8ff', '#3a1a6a'], spec: 'thermancer',
    unlockLevel: SPEC_LEVEL, cooldown: 4, cost: 20, range: 26, needsTarget: true, delivery: 'projectile',
    projectile: { color: '#c8a8ff', speed: 32, size: 0.45 },
    effects: [
      { t: 'damage', area: 'target', coef: 36 },
      { t: 'status', area: 'target', status: 'slow', dur: 3 },
      { t: 'dot', area: 'target', coef: 24, ticks: 3, interval: 1, color: burn },
    ],
    sound: 'frost',
    description: 'Fire and ice together: damages, burns and slows the target.',
  },
  thermalShock: {
    id: 'thermalShock', name: 'Thermal Shock', glyph: 'ring', color: ['#ffd1f1', '#2a1a6a'], spec: 'thermancer',
    unlockLevel: SPEC_SECOND_ABILITY_LEVEL, cooldown: 12, cost: 30, range: 26, needsTarget: true,
    effects: [{ t: 'damage', area: 'aroundTarget', radius: 6, coef: 45, bonusVs: 'slowed', bonusMul: 2 }],
    fx: [{ kind: 'nova', color: '#c8a8ff', radius: 6, at: 'target' }], sound: 'boom',
    description: 'Shatter heat against cold: damage around the target, doubled on slowed enemies.',
  },
  // ---------------- Thornkeeper ----------------
  brambleSnare: {
    id: 'brambleSnare', name: 'Bramble Snare', glyph: 'thorn', color: ['#b6e27a', '#2e4d12'],
    unlockLevel: 1, cooldown: 6, cost: 16, range: 22, needsTarget: true, delivery: 'projectile',
    projectile: { color: '#b6e27a', speed: 30 },
    effects: [{ t: 'status', area: 'target', status: 'root', dur: 3 }, { t: 'dot', area: 'target', coef: 30, ticks: 6, interval: 1.5, color: poison }],
    fx: [{ kind: 'ring', color: '#7ac24a', radius: 1.8, at: 'target' }], sound: 'cast',
    description: 'Thorns root the target for 3 seconds and wound it over 9 seconds.',
  },
  renewal: {
    id: 'renewal', name: 'Renewal', glyph: 'leaf', color: ['#a8ffb9', '#1d6b3a'],
    unlockLevel: 2, cooldown: 10, cost: 22, range: 0, needsTarget: false,
    effects: [{ t: 'hot', coef: 45, ticks: 8, interval: 1 }],
    fx: [{ kind: 'sparkle', color: '#7dff8a' }], sound: 'heal',
    description: 'Heal yourself and your companions over 8 seconds.',
  },
  spiritWolf: {
    id: 'spiritWolf', name: 'Spirit Wolf', glyph: 'paw', color: ['#d6f5ff', '#2a5566'],
    unlockLevel: 4, cooldown: 30, cost: 35, range: 0, needsTarget: false,
    effects: [{ t: 'summon', pet: 'spiritWolf', dur: 25 }],
    fx: [{ kind: 'ring', color: '#9fe8ff', radius: 2.5 }], sound: 'cast',
    description: 'Summon a spirit wolf that fights at your side for 25 seconds.',
  },
  wildBloom: {
    id: 'wildBloom', name: 'Wild Bloom', glyph: 'bloom', color: ['#ffd1f1', '#6b1d56'],
    unlockLevel: 6, cooldown: 16, cost: 40, range: 22, needsTarget: true,
    effects: [{ t: 'dot', area: 'aroundTarget', radius: 7, coef: 60, ticks: 4, interval: 1.5, color: '#ff8ae0' }],
    fx: [{ kind: 'ring', color: '#ff8ae0', radius: 7, at: 'target' }], sound: 'cast',
    description: 'Poison flowers burst around the target, wounding all enemies within 7 m over 6 seconds.',
  },
  lifebloom: {
    id: 'lifebloom', name: 'Lifebloom', glyph: 'leaf', color: ['#d8ffb0', '#2a7a2a'], spec: 'grovewarden',
    unlockLevel: SPEC_LEVEL, cooldown: 12, cost: 30, range: 0, needsTarget: false,
    effects: [{ t: 'heal', coef: 70 }, { t: 'hot', coef: 40, ticks: 5, interval: 1 }],
    fx: [{ kind: 'sparkle', color: '#d8ffb0' }, { kind: 'ring', color: '#9cf07a', radius: 3 }], sound: 'heal',
    description: 'A burst of life heals you and your companions at once, then again over 5 seconds.',
  },
  barkskin: {
    id: 'barkskin', name: 'Barkskin', glyph: 'shield', color: ['#c8a878', '#3a2a12'], spec: 'grovewarden',
    unlockLevel: SPEC_SECOND_ABILITY_LEVEL, cooldown: 28, cost: 15, range: 0, needsTarget: false,
    effects: [{ t: 'guard', reduce: 0.45, dur: 10, reflect: 8 }],
    fx: [{ kind: 'nova', color: '#c8a878', radius: 3 }], sound: 'heal',
    description: 'Your skin turns to thorned bark: 45% less damage for 10 seconds, attackers are pricked.',
  },
  packAlpha: {
    id: 'packAlpha', name: 'Pack Alpha', glyph: 'claw', color: ['#e0c8a0', '#4a2a12'], spec: 'beastcaller',
    unlockLevel: SPEC_LEVEL, cooldown: 45, cost: 30, range: 0, needsTarget: false,
    effects: [{ t: 'summon', pet: 'bear', dur: 45 }],
    fx: [{ kind: 'ring', color: '#e0c8a0', radius: 3 }], sound: 'cast',
    description: 'Call a great bear that fights beside you for 45 seconds and draws enemies to it.',
  },
  feralCall: {
    id: 'feralCall', name: 'Feral Call', glyph: 'paw', color: ['#ffd08a', '#6a2a0a'], spec: 'beastcaller',
    unlockLevel: SPEC_SECOND_ABILITY_LEVEL, cooldown: 24, cost: 20, range: 0, needsTarget: false,
    effects: [{ t: 'petFrenzy', dur: 10 }],
    fx: [{ kind: 'nova', color: '#ffd08a', radius: 4 }], sound: 'cast',
    description: 'Your companions heal 30% and attack 60% faster for 10 seconds.',
  },
  plague: {
    id: 'plague', name: 'Rot Plague', glyph: 'skull', color: ['#c8f08a', '#3a4a0a'], spec: 'rotbloom',
    unlockLevel: SPEC_LEVEL, cooldown: 8, cost: 25, range: 22, needsTarget: true,
    effects: [{ t: 'dot', area: 'aroundTarget', radius: 5, coef: 85, ticks: 8, interval: 1.5, color: poison }],
    fx: [{ kind: 'ring', color: '#9be26a', radius: 5, at: 'target' }], sound: 'cast',
    description: 'A creeping rot infects the target and every enemy within 5 m of it for 12 seconds.',
  },
  witherBurst: {
    id: 'witherBurst', name: 'Wither Burst', glyph: 'bloom', color: ['#e8ff8a', '#4a1a3a'], spec: 'rotbloom',
    unlockLevel: SPEC_SECOND_ABILITY_LEVEL, cooldown: 12, cost: 30, range: 22, needsTarget: true,
    effects: [{ t: 'detonate', area: 'aroundTarget', radius: 8, mult: 1.4, coef: 35 }],
    fx: [{ kind: 'nova', color: '#c8f08a', radius: 8, at: 'target' }], sound: 'boom',
    description: 'Ripen every poison you have placed near the target, dealing the rest of their damage at once.',
  },
};

export const SPECS: Record<SpecId, SpecDef> = {
  tempestKnight: { id: 'tempestKnight', cls: 'stormblade', name: 'Tempest Knight', role: 'Lightning damage', color: '#9fc8ff', glyph: 'bolt',
    blurb: 'Becomes the storm itself: chain lightning and bolts from the sky.', abilities: ['chainLightning', 'thunderstrike'] },
  bulwark: { id: 'bulwark', cls: 'stormblade', name: 'Bulwark', role: 'Unbreakable defender', color: '#e6d7b0', glyph: 'shield',
    blurb: 'An immovable wall of steel that shrugs off blows and shatters the earth.', abilities: ['shieldWall', 'earthshatter'] },
  spellblade: { id: 'spellblade', cls: 'stormblade', name: 'Spellblade', role: 'Hybrid: steel and flame', color: '#ffc36b', glyph: 'rune',
    blurb: 'Carves burning runes into steel. Storm and fire in one blade.', abilities: ['runeStrike', 'blazingArc'] },
  pyromancer: { id: 'pyromancer', cls: 'emberseer', name: 'Pyromancer', role: 'Fire: burns and explosions', color: '#ff8a4a', glyph: 'flame',
    blurb: 'Pure fire. Sets everything ablaze, then makes it explode.', abilities: ['inferno', 'combust'] },
  frostweaver: { id: 'frostweaver', cls: 'emberseer', name: 'Frostweaver', role: 'Ice: control and big hits', color: '#bfefff', glyph: 'snow',
    blurb: 'Turned from flame to ice. Freezes and slows enemies before they can strike.', abilities: ['glacialSpike', 'blizzard'] },
  thermancer: { id: 'thermancer', cls: 'emberseer', name: 'Thermancer', role: 'Hybrid: fire and ice', color: '#c8a8ff', glyph: 'ring',
    blurb: 'Masters both extremes. Slows with frost, then shatters with heat.', abilities: ['frostfireBolt', 'thermalShock'] },
  grovewarden: { id: 'grovewarden', cls: 'thornkeeper', name: 'Grovewarden', role: 'Healer and survivor', color: '#9cf07a', glyph: 'leaf',
    blurb: 'The grove keeps its warden alive. Strong heals and bark-hard skin.', abilities: ['lifebloom', 'barkskin'] },
  beastcaller: { id: 'beastcaller', cls: 'thornkeeper', name: 'Beastcaller', role: 'Companions', color: '#e0c8a0', glyph: 'paw',
    blurb: 'Never fights alone. A bear and a wolf answer the call.', abilities: ['packAlpha', 'feralCall'] },
  rotbloom: { id: 'rotbloom', cls: 'thornkeeper', name: 'Rotbloom', role: 'Poison damage', color: '#c8f08a', glyph: 'skull',
    blurb: 'Nature at its cruelest. Spreads rot, then makes it burst.', abilities: ['plague', 'witherBurst'] },
};

export const CLASSES: Record<ClassId, ClassDef> = {
  stormblade: {
    id: 'stormblade', name: 'Stormblade', role: 'Melee - tough',
    blurb: 'A warrior who channels the storm through steel. Leaps into battle and outlasts anything.',
    resource: 'fury', resourceName: 'Fury', resourceMax: 100, resourceRegen: -2,
    baseHp: 150, baseArmor: 30, attackRange: 3.2, attackSpeed: 1.8, attackDamage: 11, ranged: false, armorType: 'plate',
    colors: { body: '#3a5f9e', trim: '#c9d6ea', glow: '#8fd0ff' },
    abilities: ['thunderCleave', 'skywardLeap', 'staticGuard', 'tempest'],
    specs: ['tempestKnight', 'bulwark', 'spellblade'],
  },
  emberseer: {
    id: 'emberseer', name: 'Emberseer', role: 'Ranged - high damage',
    blurb: 'A scholar of living flame. Burns enemies from afar but must keep them at a distance.',
    resource: 'mana', resourceName: 'Mana', resourceMax: 120, resourceRegen: 5,
    baseHp: 125, baseArmor: 14, attackRange: 22, attackSpeed: 2.0, attackDamage: 12, ranged: true, armorType: 'cloth',
    colors: { body: '#8a2f1f', trim: '#f0c27a', glow: '#ffae42' },
    abilities: ['cinderLance', 'ashRing', 'phoenixVeil', 'meteor'],
    specs: ['pyromancer', 'frostweaver', 'thermancer'],
  },
  thornkeeper: {
    id: 'thornkeeper', name: 'Thornkeeper', role: 'Ranged - heals and summons',
    blurb: 'A warden of the wild groves. Snares foes, heals wounds and calls a spirit wolf.',
    resource: 'mana', resourceName: 'Verdance', resourceMax: 110, resourceRegen: 5,
    baseHp: 125, baseArmor: 15, attackRange: 18, attackSpeed: 1.7, attackDamage: 8, ranged: true, armorType: 'leather',
    colors: { body: '#355e2b', trim: '#d8c38f', glow: '#9cf07a' },
    abilities: ['brambleSnare', 'renewal', 'spiritWolf', 'wildBloom'],
    specs: ['grovewarden', 'beastcaller', 'rotbloom'],
  },
};

export const POTION = {
  name: 'Healing Draught', glyph: 'potion' as Glyph, color: ['#ff9aa8', '#7a1024'] as [string, string],
  cooldown: 20, healFraction: 0.4,
};

/** Potion price grows with level so gold stays meaningful. */
export function potionPrice(level: number) {
  return 8 + level * 3;
}

/** The abilities a hero has access to (learned or not): four class abilities plus the two of the chosen spec. */
export function abilitiesFor(cls: ClassId, spec: SpecId | null): AbilityId[] {
  const base: AbilityId[] = [...CLASSES[cls].abilities];
  return spec ? [...base, ...SPECS[spec].abilities] : base;
}
