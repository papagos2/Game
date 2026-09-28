// Pure logic for professions, reputation, daily bounties, elixirs and achievements.
import {
  ACHIEVEMENTS, BOUNTY_POOL, ELIXIR_MINUTES, MATERIALS, MAX_GATHER_SKILL, MAX_REFORGE, RECIPES, REP_RANKS,
  type AchievementDef, type MaterialId, type RecipeId, type StatKey,
} from '../data/economy';
import { MOBS, type MapId, type MobKind } from '../data/world';
import type { StatMod } from '../data/talents';
import { hashString, makeRng, type Item } from './rules';
import type { Progress } from './progress';
import { RARITY_MUL, TYPE_WEIGHT } from '../data/items';

export interface Bounty { id: string; map: MapId; kind: 'kill' | 'gather'; mob: MobKind | null; count: number; progress: number; claimed: boolean }
export interface Buff { id: 'might' | 'stone'; until: number }

export interface Extras {
  materials: Partial<Record<MaterialId, number>>;
  gatherSkill: number;
  buffs: Buff[];
  rep: Partial<Record<MapId, number>>;
  achievements: string[];
  title: string | null;
  stats: Record<StatKey, number>;
  bounties: { day: string; list: Bounty[] };
  dungeons: string[];
}

export const STAT_KEYS: StatKey[] = ['kills', 'eliteKills', 'bosses', 'quests', 'gathered', 'crafted', 'bounties', 'dungeons', 'deaths', 'legendaries'];

export function defaultExtras(): Extras {
  return {
    materials: {}, gatherSkill: 1, buffs: [], rep: {}, achievements: [], title: null,
    stats: Object.fromEntries(STAT_KEYS.map((k) => [k, 0])) as Record<StatKey, number>,
    bounties: { day: '', list: [] }, dungeons: [],
  };
}

export function bump(p: Progress, key: StatKey, n = 1) {
  p.stats[key] = (p.stats[key] ?? 0) + n;
}

// ---------- Gathering ----------

export function canGather(p: Progress, mat: MaterialId): boolean {
  return p.gatherSkill >= MATERIALS[mat].skill;
}

/** Gathers 1-2 of a material; returns the amount, or 0 if the skill is too low. */
export function gather(p: Progress, mat: MaterialId, roll: number): number {
  if (!canGather(p, mat)) return 0;
  const n = roll < 0.3 ? 2 : 1;
  p.materials[mat] = (p.materials[mat] ?? 0) + n;
  // Skill rises quickly on materials near your skill, slower on easy ones.
  const gap = p.gatherSkill - MATERIALS[mat].skill;
  if (p.gatherSkill < MAX_GATHER_SKILL && (gap < 60 || roll < 0.25)) p.gatherSkill += gap < 60 ? 2 : 1;
  p.gatherSkill = Math.min(MAX_GATHER_SKILL, p.gatherSkill);
  bump(p, 'gathered', n);
  return n;
}

export function countKind(p: Progress, kind: 'herb' | 'ore'): number {
  let n = 0;
  for (const [id, c] of Object.entries(p.materials)) if (MATERIALS[id as MaterialId]?.kind === kind) n += c ?? 0;
  return n;
}

function spend(p: Progress, kind: 'herb' | 'ore', n: number) {
  // Use the most plentiful materials first.
  const ids = (Object.keys(p.materials) as MaterialId[]).filter((id) => MATERIALS[id].kind === kind).sort((a, b) => (p.materials[b] ?? 0) - (p.materials[a] ?? 0));
  for (const id of ids) {
    const take = Math.min(n, p.materials[id] ?? 0);
    p.materials[id] = (p.materials[id] ?? 0) - take;
    if (!p.materials[id]) delete p.materials[id];
    n -= take;
    if (n <= 0) break;
  }
}

// ---------- Crafting ----------

export function craftBlocker(p: Progress, id: RecipeId): string | null {
  const r = RECIPES.find((x) => x.id === id)!;
  if (countKind(p, 'herb') < r.herbs) return `Needs ${r.herbs} herbs`;
  if (countKind(p, 'ore') < r.ores) return `Needs ${r.ores} ore`;
  if (p.gold < r.gold) return `Needs ${r.gold} gold`;
  return null;
}

/** Crafts a recipe (reforge needs the item). Returns a short result text, or null if not possible. */
export function craft(p: Progress, id: RecipeId, item?: Item): string | null {
  if (craftBlocker(p, id)) return null;
  const r = RECIPES.find((x) => x.id === id)!;
  if (id === 'reforge') {
    if (!item || (item.upg ?? 0) >= MAX_REFORGE) return null;
    reforge(item);
  }
  spend(p, 'herb', r.herbs);
  spend(p, 'ore', r.ores);
  p.gold -= r.gold;
  bump(p, 'crafted');
  if (id === 'draught') {
    p.potions += 2;
    return '+2 Healing Draughts';
  }
  if (id === 'mightElixir' || id === 'stoneElixir') {
    const buffId = id === 'mightElixir' ? 'might' : 'stone';
    p.buffs = p.buffs.filter((b) => b.id !== buffId);
    p.buffs.push({ id: buffId, until: p.playSeconds + ELIXIR_MINUTES * 60 });
    return `${r.name}: active for ${ELIXIR_MINUTES} minutes`;
  }
  return `${item!.name} is now item level ${item!.ilvl}`;
}

/** Raises an item by 2 item levels, scaling its stats like a freshly made item. */
export function reforge(it: Item) {
  const before = (4 + it.ilvl * 1.4) * RARITY_MUL[it.rarity] * TYPE_WEIGHT[it.type];
  it.ilvl += 2;
  const after = (4 + it.ilvl * 1.4) * RARITY_MUL[it.rarity] * TYPE_WEIGHT[it.type];
  const k = after / before;
  it.power = Math.round(it.power * k);
  it.stamina = Math.round(it.stamina * k);
  it.armor = Math.round(it.armor * k);
  it.crit = Math.round(it.crit * k);
  it.value = Math.round(it.value * k);
  it.upg = (it.upg ?? 0) + 1;
}

export function activeBuffs(p: Progress): Buff[] {
  return p.buffs.filter((b) => b.until > p.playSeconds);
}

export function buffMods(p: Progress): Partial<Record<StatMod, number>> {
  const out: Partial<Record<StatMod, number>> = {};
  for (const b of activeBuffs(p)) {
    if (b.id === 'might') out.powerPct = (out.powerPct ?? 0) + 12;
    else {
      out.armorPct = (out.armorPct ?? 0) + 15;
      out.hpPct = (out.hpPct ?? 0) + 8;
    }
  }
  return out;
}

// ---------- Reputation ----------

export function repRank(value: number) {
  let i = 0;
  for (let k = 0; k < REP_RANKS.length; k++) if (value >= REP_RANKS[k].at) i = k;
  const next = REP_RANKS[i + 1];
  return { index: i, name: REP_RANKS[i].name, discount: REP_RANKS[i].discount, into: value - REP_RANKS[i].at, span: next ? next.at - REP_RANKS[i].at : 0 };
}

/** Adds reputation; returns the new rank name if the rank went up. */
export function addRep(p: Progress, map: MapId, amount: number): string | null {
  const before = repRank(p.rep[map] ?? 0).index;
  p.rep[map] = Math.min(REP_RANKS[REP_RANKS.length - 1].at + 2000, (p.rep[map] ?? 0) + amount);
  const after = repRank(p.rep[map]!);
  return after.index > before ? after.name : null;
}

// ---------- Daily bounties ----------

export function dayKey(d: Date = new Date()): string {
  return `${d.getFullYear()}-${d.getMonth() + 1}-${d.getDate()}`;
}

/** Today's bounties for every map (rolled once a day). */
export function refreshBounties(p: Progress, today = dayKey()) {
  if (p.bounties.day === today) return;
  const list: Bounty[] = [];
  for (const map of Object.keys(BOUNTY_POOL) as MapId[]) {
    const rng = makeRng(hashString(`${today}:${map}:${p.name}`));
    const pool = [...BOUNTY_POOL[map]];
    for (let i = 0; i < 3 && pool.length; i++) {
      const t = pool.splice(Math.floor(rng() * pool.length), 1)[0];
      list.push({ id: `${map}:${i}`, map, kind: t.kind, mob: t.mob ?? null, count: t.count, progress: 0, claimed: false });
    }
  }
  p.bounties = { day: today, list };
}

export function bountyLabel(b: Bounty) {
  return b.kind === 'gather' ? `Gather ${b.count} materials` : `Defeat ${b.count} ${MOBS[b.mob!].name}s`;
}

export function onBountyKill(p: Progress, map: MapId, kind: MobKind) {
  for (const b of p.bounties.list) if (!b.claimed && b.map === map && b.kind === 'kill' && b.mob === kind && b.progress < b.count) b.progress++;
}

export function onBountyGather(p: Progress, map: MapId, n: number) {
  for (const b of p.bounties.list) if (!b.claimed && b.map === map && b.kind === 'gather') b.progress = Math.min(b.count, b.progress + n);
}

export function claimableBounties(p: Progress, map: MapId) {
  return p.bounties.list.filter((b) => b.map === map && !b.claimed && b.progress >= b.count);
}

// ---------- Achievements ----------

function achieved(p: Progress, a: AchievementDef): boolean {
  const c = a.check;
  if ('level' in c) return p.level >= c.level;
  if ('quest' in c) return p.completed.includes(c.quest);
  if ('exalted' in c) return Object.values(p.rep).filter((v) => (v ?? 0) >= REP_RANKS[4].at).length >= c.exalted;
  if ('spec' in c) return !!p.spec;
  return (p.stats[c.stat] ?? 0) >= c.atLeast;
}

/** Awards any newly met achievements and returns them. */
export function checkAchievements(p: Progress): AchievementDef[] {
  const out: AchievementDef[] = [];
  for (const a of ACHIEVEMENTS) {
    if (p.achievements.includes(a.id) || !achieved(p, a)) continue;
    p.achievements.push(a.id);
    out.push(a);
  }
  return out;
}

export function achievementPoints(p: Progress) {
  return ACHIEVEMENTS.filter((a) => p.achievements.includes(a.id)).reduce((s, a) => s + a.points, 0);
}

export function titles(p: Progress): string[] {
  return ACHIEVEMENTS.filter((a) => a.title && p.achievements.includes(a.id)).map((a) => a.title!);
}
