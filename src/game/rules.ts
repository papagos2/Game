// Pure game rules: stats, damage, experience, talents and loot. No rendering here, so it is unit-tested.
import { CLASSES, type AbilityId, type ClassId, type SpecId } from '../data/classes';
import {
  ARMOR_NAMES, GEAR_SLOTS, GEMS, ITEM_TYPES, JEWEL_NAMES, LEGENDARIES, MATERIALS, PREFIXES, RARITY_MUL, TYPE_DROP_WEIGHT,
  TYPE_WEIGHT, WEAPON_NAMES, type GearSlot, type ItemType,
} from '../data/items';
import { NODE_BY_ID, type StatMod } from '../data/talents';
import { MOBS, type MobKind } from '../data/world';

export { RARITY_COLORS, RARITY_NAMES } from '../data/items';

export const MAX_LEVEL = 30;

export type Rarity = 0 | 1 | 2 | 3 | 4;

export interface Item {
  id: string;
  name: string;
  type: ItemType;
  ilvl: number;
  rarity: Rarity;
  power: number;
  stamina: number;
  armor: number;
  crit: number;
  value: number;
}

export type Gear = Record<GearSlot, Item | null>;

export function emptyGear(): Gear {
  return { weapon: null, head: null, chest: null, hands: null, feet: null, amulet: null, ring1: null, ring2: null };
}

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

export function hashString(s: string): number {
  let h = 2166136261;
  for (let i = 0; i < s.length; i++) h = Math.imul(h ^ s.charCodeAt(i), 16777619);
  return h >>> 0;
}

// ---------- Experience ----------

export function xpToNext(level: number): number {
  return level >= MAX_LEVEL ? 0 : Math.round(100 + 80 * level + 6 * level * level);
}

export function mobXp(mobLevel: number, playerLevel: number, elite: boolean): number {
  if (mobLevel <= playerLevel - 5) return 0;
  const base = 12 + 6 * mobLevel + 0.25 * mobLevel * mobLevel;
  const mod = Math.min(1.3, Math.max(0.1, 1 + 0.1 * (mobLevel - playerLevel)));
  return Math.round(base * mod * (elite ? 3 : 1));
}

// ---------- Talents ----------

export interface TalentBonuses {
  stat: Record<StatMod, number>;
  ability: Partial<Record<AbilityId, { dmg: number; cd: number; cost: number }>>;
}

export function talentBonuses(talents: Record<string, number>): TalentBonuses {
  const stat: Record<StatMod, number> = {
    powerPct: 0, hpPct: 0, armorPct: 0, critPct: 0, regenPct: 0, healPct: 0, dotPct: 0, petPct: 0, drPct: 0, speedPct: 0,
  };
  const ability: TalentBonuses['ability'] = {};
  for (const [id, rank] of Object.entries(talents)) {
    const entry = NODE_BY_ID[id];
    if (!entry || rank <= 0) continue;
    for (const m of entry.node.perRank) {
      if (m.k === 'ability') {
        const a = (ability[m.id] ??= { dmg: 0, cd: 0, cost: 0 });
        a.dmg += (m.dmgPct ?? 0) * rank;
        a.cd += (m.cdPct ?? 0) * rank;
        a.cost += (m.costPct ?? 0) * rank;
      } else {
        stat[m.k] += m.v * rank;
      }
    }
  }
  return { stat, ability };
}

export function talentPointsTotal(level: number) {
  return Math.max(0, level - 1);
}

// ---------- Player stats ----------

export interface PlayerStats {
  maxHp: number;
  armor: number;
  power: number;
  damageScale: number;
  critChance: number;
  /** Fraction of incoming damage removed by talents (0..0.5). */
  damageReduction: number;
}

export function playerStats(cls: ClassId, level: number, gear: Gear, talents: Record<string, number> = {}): PlayerStats {
  const def = CLASSES[cls];
  const tb = talentBonuses(talents).stat;
  let stamina = 0;
  let power = level;
  let armor = def.baseArmor + level * 2;
  let crit = 0;
  for (const s of GEAR_SLOTS) {
    const it = gear[s];
    if (!it) continue;
    stamina += it.stamina;
    power += it.power;
    armor += it.armor;
    crit += it.crit;
  }
  power *= 1 + tb.powerPct / 100;
  armor *= 1 + tb.armorPct / 100;
  const maxHp = (def.baseHp + (level - 1) * 20 + 0.4 * level * level + stamina * 6) * (1 + tb.hpPct / 100);
  return {
    maxHp: Math.round(maxHp),
    armor: Math.round(armor),
    power: Math.round(power),
    damageScale: (1 + (level - 1) * 0.12) * (1 + power / 100),
    critChance: Math.min(0.6, 0.08 + crit * 0.004 + tb.critPct / 100),
    damageReduction: Math.min(0.5, tb.drPct / 100),
  };
}

/** Mana pools and regeneration grow with level; Fury is always 0-100. */
export function resourceMax(cls: ClassId, level: number) {
  const d = CLASSES[cls];
  return d.resource === 'mana' ? Math.round(d.resourceMax + 5 * (level - 1)) : d.resourceMax;
}

export function resourceRegen(cls: ClassId, level: number) {
  const d = CLASSES[cls];
  return d.resource === 'mana' ? d.resourceRegen + 0.25 * (level - 1) : d.resourceRegen;
}

/** Fraction of damage that gets through armor, against an attacker of the given level. */
export function mitigation(armor: number, attackerLevel = 1): number {
  const k = 50 + 10 * attackerLevel;
  return k / (k + Math.max(0, armor));
}

export interface MobStats { maxHp: number; damage: number; attackSpeed: number }

export function mobStats(kind: MobKind, level: number): MobStats {
  const def = MOBS[kind];
  return {
    maxHp: Math.round((30 + 25 * level + 1.2 * level * level + 0.07 * level ** 3) * def.hpMul),
    damage: (3 + 2.6 * level + 0.05 * level * level + 0.0035 * level ** 3) * def.dmgMul,
    attackSpeed: def.summon ? 1.4 : 2,
  };
}

/** Level difference modifier on damage dealt by an attacker of level a to a defender of level d. */
export function levelMod(attacker: number, defender: number): number {
  const diff = attacker - defender;
  if (diff >= 0) return 1 + Math.min(diff, 5) * 0.04;
  return Math.max(0.6, 1 + diff * 0.06);
}

/** Difficulty colour of a mob level relative to the player. */
export function levelColor(mobLevel: number, playerLevel: number): string {
  const d = mobLevel - playerLevel;
  if (d >= 3) return '#ff4a3d';
  if (d >= 1) return '#ff9b3d';
  if (d >= -2) return '#ffe066';
  if (d >= -4) return '#56d36b';
  return '#9a9a9a';
}
export const GREY = '#9a9a9a';

// ---------- Items ----------

export function rollRarity(rng: Rng, bonus = 0): Rarity {
  const r = rng() + bonus;
  if (r > 0.985) return 3;
  if (r > 0.9) return 2;
  if (r > 0.6) return 1;
  return 0;
}

function pickType(rng: Rng): ItemType {
  const total = ITEM_TYPES.reduce((s, t) => s + TYPE_DROP_WEIGHT[t], 0);
  let r = rng() * total;
  for (const t of ITEM_TYPES) {
    r -= TYPE_DROP_WEIGHT[t];
    if (r <= 0) return t;
  }
  return 'ring';
}

let itemCounter = 0;
const pick = <T>(rng: Rng, a: T[]) => a[Math.floor(rng() * a.length)];

export function tierOf(ilvl: number): 0 | 1 | 2 {
  return ilvl <= 10 ? 0 : ilvl <= 20 ? 1 : 2;
}

export function makeItem(rng: Rng, cls: ClassId, ilvl: number, rarity: Rarity, type?: ItemType, name?: string): Item {
  const t: ItemType = type ?? pickType(rng);
  const budget = Math.max(1, (4 + ilvl * 1.4) * RARITY_MUL[rarity] * TYPE_WEIGHT[t]);
  const armorType = CLASSES[cls].armorType;
  let power = 0;
  let stamina = 0;
  let armor = 0;
  let crit = 0;
  if (t === 'weapon') {
    power = budget * 0.75;
    stamina = budget * 0.25;
  } else if (t === 'amulet' || t === 'ring') {
    power = budget * (t === 'ring' ? 0.4 : 0.45);
    stamina = budget * (t === 'ring' ? 0.25 : 0.3);
    crit = budget * (t === 'ring' ? 0.35 : 0.25);
  } else {
    stamina = budget * 0.6;
    power = budget * 0.2;
    armor = budget * 1.5 * (armorType === 'plate' ? 1.2 : armorType === 'leather' ? 0.85 : 0.6);
  }
  const tier = tierOf(ilvl);
  let base: string;
  if (t === 'weapon') base = pick(rng, WEAPON_NAMES[cls]);
  else if (t === 'amulet' || t === 'ring') base = `${pick(rng, GEMS)} ${pick(rng, JEWEL_NAMES[t])}`;
  else base = `${MATERIALS[armorType][tier]} ${pick(rng, ARMOR_NAMES[armorType][t])}`;
  itemCounter += 1;
  return {
    id: `i${Date.now().toString(36)}${itemCounter}${Math.floor(rng() * 1e6).toString(36)}`,
    name: name ?? `${pick(rng, PREFIXES[Math.min(3, rarity)])} ${base}`,
    type: t,
    ilvl,
    rarity,
    power: Math.round(power),
    stamina: Math.round(stamina),
    armor: Math.round(armor),
    crit: Math.round(crit),
    value: Math.max(1, Math.round(ilvl * (1 + rarity) * 2 * TYPE_WEIGHT[t])),
  };
}

export function makeLegendary(rng: Rng, cls: ClassId, bossKind: string, ilvl: number): Item {
  const defs = LEGENDARIES[bossKind] ?? LEGENDARIES.boss;
  const d = pick(rng, defs);
  const it = makeItem(rng, cls, ilvl, 4, d.type, d.name);
  it.power += d.bonus.power ?? 0;
  it.stamina += d.bonus.stamina ?? 0;
  it.armor += d.bonus.armor ?? 0;
  it.crit += d.bonus.crit ?? 0;
  return it;
}

/** Rough single-number score used to compare items. */
export function itemScore(it: Item | null): number {
  if (!it) return 0;
  return it.power * 1.2 + it.stamina + it.armor * 0.15 + it.crit * 1.5;
}

/** The gear slot an item would go into (rings replace the weaker ring). */
export function slotFor(it: Item, gear: Gear): GearSlot {
  if (it.type !== 'ring') return it.type;
  if (!gear.ring1) return 'ring1';
  if (!gear.ring2) return 'ring2';
  return itemScore(gear.ring1) <= itemScore(gear.ring2) ? 'ring1' : 'ring2';
}

/** Score gain from equipping this item (negative = worse). */
export function upgradeValue(it: Item, gear: Gear): number {
  return itemScore(it) - itemScore(gear[slotFor(it, gear)]);
}

export interface MobLoot { gold: number; items: Item[] }

export function rollMobLoot(rng: Rng, cls: ClassId, kind: MobKind, level: number): MobLoot {
  const def = MOBS[kind];
  const gold = Math.round(level * (1 + rng()) * (def.boss ? 10 : def.elite ? 4 : 1));
  const items: Item[] = [];
  if (def.summon) return { gold: 0, items };
  if (def.boss) {
    items.push(makeLegendary(rng, cls, kind, level + 2));
    items.push(makeItem(rng, cls, level + 1, 3));
  } else if (def.elite) {
    items.push(makeItem(rng, cls, level + 1, Math.max(2, rollRarity(rng)) as Rarity));
  } else if (rng() < 0.22) {
    items.push(makeItem(rng, cls, level, rollRarity(rng)));
  }
  return { gold, items };
}

/** Items a vendor offers: stable for a given map, level and class. */
export function vendorStock(mapId: string, level: number, cls: ClassId): { item: Item; price: number }[] {
  const rng = makeRng(hashString(`${mapId}:${level}:${cls}`));
  const out: { item: Item; price: number }[] = [];
  for (let i = 0; i < 4; i++) {
    const it = makeItem(rng, cls, level, i === 3 ? 2 : 1);
    it.id = `v:${mapId}:${level}:${i}`;
    out.push({ item: it, price: it.value * 5 });
  }
  return out;
}

export function respecCost(level: number) {
  return 10 * level;
}

export function isSpecOf(spec: SpecId | null, cls: ClassId) {
  return !!spec && CLASSES[cls].specs.includes(spec);
}
