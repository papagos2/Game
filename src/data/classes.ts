// Playable classes and their abilities. All names and designs are original to Ashenveil.

export type ClassId = 'stormblade' | 'emberseer' | 'thornkeeper';
export type ResourceKind = 'fury' | 'mana';

export type AbilityId =
  | 'thunderCleave' | 'skywardLeap' | 'staticGuard' | 'tempest'
  | 'cinderLance' | 'ashRing' | 'phoenixVeil' | 'meteor'
  | 'brambleSnare' | 'renewal' | 'spiritWolf' | 'wildBloom';

export type Glyph =
  | 'sword' | 'bolt' | 'shield' | 'storm' | 'flame' | 'ring' | 'wing' | 'meteor'
  | 'thorn' | 'leaf' | 'paw' | 'bloom' | 'potion' | 'leap';

export interface AbilityDef {
  id: AbilityId;
  name: string;
  glyph: Glyph;
  color: [string, string];
  unlockLevel: number;
  cooldown: number;
  cost: number;
  /** Max distance to the target; 0 = no target needed. */
  range: number;
  needsTarget: boolean;
  /** Damage or healing coefficient, multiplied by the caster's power scale. */
  amount: number;
  description: string;
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
  colors: { body: string; trim: string; glow: string };
  abilities: AbilityId[];
}

export const ABILITIES: Record<AbilityId, AbilityDef> = {
  // Stormblade
  thunderCleave: {
    id: 'thunderCleave', name: 'Thunder Cleave', glyph: 'sword', color: ['#7fb6ff', '#1d3c7a'],
    unlockLevel: 1, cooldown: 5, cost: 0, range: 0, needsTarget: false, amount: 20,
    description: 'Sweep the blade in front of you, hitting every enemy within 5 m. Builds 20 Fury.',
  },
  skywardLeap: {
    id: 'skywardLeap', name: 'Skyward Leap', glyph: 'leap', color: ['#b7e0ff', '#2b4f8a'],
    unlockLevel: 2, cooldown: 12, cost: 15, range: 22, needsTarget: true, amount: 14,
    description: 'Leap to your target, strike it and stun it for 2 seconds.',
  },
  staticGuard: {
    id: 'staticGuard', name: 'Static Guard', glyph: 'shield', color: ['#c6f0ff', '#285e7a'],
    unlockLevel: 4, cooldown: 20, cost: 25, range: 0, needsTarget: false, amount: 60,
    description: 'A crackling barrier absorbs damage for 8 seconds and shocks attackers.',
  },
  tempest: {
    id: 'tempest', name: 'Tempest', glyph: 'storm', color: ['#e2d5ff', '#3d2a86'],
    unlockLevel: 6, cooldown: 18, cost: 50, range: 0, needsTarget: false, amount: 55,
    description: 'Call down a storm around you: heavy damage to all enemies within 8 m.',
  },
  // Emberseer
  cinderLance: {
    id: 'cinderLance', name: 'Cinder Lance', glyph: 'flame', color: ['#ffc36b', '#8a2a0b'],
    unlockLevel: 1, cooldown: 3, cost: 18, range: 26, needsTarget: true, amount: 26,
    description: 'Hurl a spear of fire that burns the target.',
  },
  ashRing: {
    id: 'ashRing', name: 'Ash Ring', glyph: 'ring', color: ['#ffb08a', '#5c1e14'],
    unlockLevel: 2, cooldown: 14, cost: 25, range: 0, needsTarget: false, amount: 14,
    description: 'A ring of ash erupts around you, damaging and rooting enemies within 7 m for 3 seconds.',
  },
  phoenixVeil: {
    id: 'phoenixVeil', name: 'Phoenix Veil', glyph: 'wing', color: ['#fff0a8', '#b3521a'],
    unlockLevel: 4, cooldown: 16, cost: 20, range: 0, needsTarget: false, amount: 30,
    description: 'Burst forward 12 m in a flash of flame and restore some health.',
  },
  meteor: {
    id: 'meteor', name: 'Falling Star', glyph: 'meteor', color: ['#ffdf8a', '#7a1b0b'],
    unlockLevel: 6, cooldown: 20, cost: 45, range: 26, needsTarget: true, amount: 70,
    description: 'After a short delay a burning star strikes the target area (6 m).',
  },
  // Thornkeeper
  brambleSnare: {
    id: 'brambleSnare', name: 'Bramble Snare', glyph: 'thorn', color: ['#b6e27a', '#2e4d12'],
    unlockLevel: 1, cooldown: 6, cost: 16, range: 22, needsTarget: true, amount: 30,
    description: 'Thorns root the target for 3 seconds and wound it over 9 seconds.',
  },
  renewal: {
    id: 'renewal', name: 'Renewal', glyph: 'leaf', color: ['#a8ffb9', '#1d6b3a'],
    unlockLevel: 2, cooldown: 10, cost: 22, range: 0, needsTarget: false, amount: 45,
    description: 'Heal yourself (and your companion) over 8 seconds.',
  },
  spiritWolf: {
    id: 'spiritWolf', name: 'Spirit Wolf', glyph: 'paw', color: ['#d6f5ff', '#2a5566'],
    unlockLevel: 4, cooldown: 30, cost: 35, range: 0, needsTarget: false, amount: 1,
    description: 'Summon a spirit wolf that fights at your side for 25 seconds.',
  },
  wildBloom: {
    id: 'wildBloom', name: 'Wild Bloom', glyph: 'bloom', color: ['#ffd1f1', '#6b1d56'],
    unlockLevel: 6, cooldown: 16, cost: 40, range: 22, needsTarget: true, amount: 60,
    description: 'Poison flowers burst around the target, wounding all enemies within 7 m over 6 seconds.',
  },
};

export const CLASSES: Record<ClassId, ClassDef> = {
  stormblade: {
    id: 'stormblade', name: 'Stormblade', role: 'Melee - tough',
    blurb: 'A warrior who channels the storm through steel. Leaps into battle and outlasts anything.',
    resource: 'fury', resourceName: 'Fury', resourceMax: 100, resourceRegen: -2,
    baseHp: 150, baseArmor: 30, attackRange: 3.2, attackSpeed: 1.8, attackDamage: 11, ranged: false,
    colors: { body: '#3a5f9e', trim: '#c9d6ea', glow: '#8fd0ff' },
    abilities: ['thunderCleave', 'skywardLeap', 'staticGuard', 'tempest'],
  },
  emberseer: {
    id: 'emberseer', name: 'Emberseer', role: 'Ranged - high damage',
    blurb: 'A scholar of living flame. Burns enemies from afar but must keep them at a distance.',
    resource: 'mana', resourceName: 'Mana', resourceMax: 120, resourceRegen: 5,
    baseHp: 105, baseArmor: 8, attackRange: 22, attackSpeed: 2.0, attackDamage: 10, ranged: true,
    colors: { body: '#8a2f1f', trim: '#f0c27a', glow: '#ffae42' },
    abilities: ['cinderLance', 'ashRing', 'phoenixVeil', 'meteor'],
  },
  thornkeeper: {
    id: 'thornkeeper', name: 'Thornkeeper', role: 'Ranged - heals and summons',
    blurb: 'A warden of the wild groves. Snares foes, heals wounds and calls a spirit wolf.',
    resource: 'mana', resourceName: 'Verdance', resourceMax: 110, resourceRegen: 5,
    baseHp: 125, baseArmor: 15, attackRange: 18, attackSpeed: 1.7, attackDamage: 8, ranged: true,
    colors: { body: '#355e2b', trim: '#d8c38f', glow: '#9cf07a' },
    abilities: ['brambleSnare', 'renewal', 'spiritWolf', 'wildBloom'],
  },
};

export const POTION = {
  name: 'Healing Draught', glyph: 'potion' as Glyph, color: ['#ff9aa8', '#7a1024'] as [string, string],
  cooldown: 20, healFraction: 0.4, price: 10,
};
