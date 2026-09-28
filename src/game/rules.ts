// Pure game rules: stats, damage, experience and loot. No rendering here, so it is unit-tested.
import { CLASSES, type ClassId } from '../data/classes';
import { MOBS, type MobKind } from '../data/world';

export const MAX_LEVEL = 10;

export type Slot = 'weapon' | 'armor' | 'trinket';
export const SLOTS: Slot[] = ['weapon', 'armor', 'trinket'];

export interface Item {
  id: string;
  name: string;
  slot: Slot;
  ilvl: number;
  rarity: 0 | 1 | 2 | 3;
  power: number;
  stamina: number;
  armor: number;
  value: number;
}

export const RARITY_NAMES = ['Common', 'Uncommon', 'Rare', 'Epic'] as const;
export const RARITY_COLORS = ['#e8e4dc', '#4fd46b', '#4a9dff', '#c76bff'] as const;

export type Rng = () => number;

/** Small deterministic PRNG (mulberry32). */
export function makeRng(seed: number): Rng {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

export function xpToNext(level: number): number {
  return level >= MAX_LEVEL ? 0 : 120 + 100 * level;
}

export function mobXp(mobLevel: number, playerLevel: number, elite: boolean): number {
  if (mobLevel <= playerLevel - 5) return 0;
  const base = 12 + 6 * mobLevel;
  const mod = Math.min(1.3, Math.max(0.1, 1 + 0.1 * (mobLevel - playerLevel)));
  return Math.round(base * mod * (elite ? 3 : 1));
}

export interface Gear { weapon: Item | null; armor: Item | null; trinket: Item | null }

export interface PlayerStats {
  maxHp: number;
  armor: number;
  power: number;
  damageScale: number;
}

export function playerStats(cls: ClassId, level: number, gear: Gear): PlayerStats {
  const def = CLASSES[cls];
  let stamina = 0;
  let power = level;
  let armor = def.baseArmor + level * 2;
  for (const s of SLOTS) {
    const it = gear[s];
    if (!it) continue;
    stamina += it.stamina;
    power += it.power;
    armor += it.armor;
  }
  return {
    maxHp: Math.round(def.baseHp + (level - 1) * 22 + stamina * 8),
    armor,
    power,
    damageScale: (1 + (level - 1) * 0.14) * (1 + power / 60),
  };
}

export function mitigation(armor: number): number {
  return 100 / (100 + Math.max(0, armor));
}

export interface MobStats { maxHp: number; damage: number; attackSpeed: number; xp: number }

export function mobStats(kind: MobKind, level: number): MobStats {
  const def = MOBS[kind];
  return {
    maxHp: Math.round((40 + 30 * level) * def.hpMul),
    damage: (4 + 3 * level) * def.dmgMul,
    attackSpeed: kind === 'imp' ? 1.4 : 2,
    xp: 0,
  };
}

/** Level difference modifier on damage dealt by an attacker of level a to a defender of level d. */
export function levelMod(attacker: number, defender: number): number {
  const diff = attacker - defender;
  if (diff >= 0) return 1 + Math.min(diff, 5) * 0.04;
  return Math.max(0.6, 1 + diff * 0.06);
}

/** Difficulty colour of a mob level relative to the player (like a con system). */
export function levelColor(mobLevel: number, playerLevel: number): string {
  const d = mobLevel - playerLevel;
  if (d >= 3) return '#ff4a3d';
  if (d >= 1) return '#ff9b3d';
  if (d >= -2) return '#ffe066';
  if (d >= -4) return '#56d36b';
  return '#9a9a9a';
}

const WEAPON_NAMES: Record<ClassId, string[]> = {
  stormblade: ['Blade', 'Greatsword', 'Cleaver', 'Warblade'],
  emberseer: ['Staff', 'Rod', 'Sceptre', 'Wand'],
  thornkeeper: ['Staff', 'Crook', 'Branch', 'Totem'],
};
const ARMOR_NAMES = ['Jerkin', 'Hauberk', 'Vest', 'Mantle', 'Cuirass'];
const TRINKET_NAMES = ['Charm', 'Amulet', 'Band', 'Talisman', 'Sigil'];
const PREFIXES = [
  ['Worn', 'Plain', 'Rough', 'Simple'],
  ['Sturdy', 'Keen', 'Warden\'s', 'Hunter\'s'],
  ['Stormforged', 'Emberwrought', 'Thornbound', 'Duskwoven'],
  ['Cindermaw\'s', 'Sunward', 'Ashenveil', 'Heartfire'],
];

export function rollRarity(rng: Rng, bonus = 0): 0 | 1 | 2 | 3 {
  const r = rng() + bonus;
  if (r > 0.985) return 3;
  if (r > 0.9) return 2;
  if (r > 0.6) return 1;
  return 0;
}

let itemCounter = 0;

export function makeItem(rng: Rng, cls: ClassId, ilvl: number, rarity: 0 | 1 | 2 | 3, slot?: Slot): Item {
  const s: Slot = slot ?? SLOTS[Math.floor(rng() * SLOTS.length)];
  const budget = Math.max(1, Math.round(ilvl * 2 * [1, 1.3, 1.7, 2.2][rarity]));
  let power = 0;
  let stamina = 0;
  let armor = 0;
  if (s === 'weapon') {
    power = Math.round(budget * 0.75);
    stamina = budget - power;
  } else if (s === 'armor') {
    stamina = Math.round(budget * 0.6);
    power = Math.round(budget * 0.15);
    armor = budget * 3;
  } else {
    power = Math.round(budget * 0.5);
    stamina = budget - power;
  }
  const pick = (a: string[]) => a[Math.floor(rng() * a.length)];
  const base = s === 'weapon' ? pick(WEAPON_NAMES[cls]) : s === 'armor' ? pick(ARMOR_NAMES) : pick(TRINKET_NAMES);
  itemCounter += 1;
  return {
    id: `i${Date.now().toString(36)}${itemCounter}${Math.floor(rng() * 1e6).toString(36)}`,
    name: `${pick(PREFIXES[rarity])} ${base}`,
    slot: s,
    ilvl,
    rarity,
    power,
    stamina,
    armor,
    value: Math.max(1, Math.round(ilvl * (1 + rarity) * 1.5)),
  };
}

/** Rough single-number score used to compare items of the same slot. */
export function itemScore(it: Item | null): number {
  if (!it) return 0;
  return it.power * 1.2 + it.stamina + it.armor * 0.2;
}

export interface MobLoot { gold: number; item: Item | null }

export function rollMobLoot(rng: Rng, cls: ClassId, kind: MobKind, level: number): MobLoot {
  const def = MOBS[kind];
  const gold = Math.round(level * (1 + rng()) * (def.elite ? 4 : 1));
  let item: Item | null = null;
  if (kind === 'boss') item = makeItem(rng, cls, 12, 3);
  else if (def.elite) item = makeItem(rng, cls, level + 1, Math.max(2, rollRarity(rng)) as 2 | 3);
  else if (rng() < 0.22) item = makeItem(rng, cls, level, rollRarity(rng));
  return { gold, item };
}
