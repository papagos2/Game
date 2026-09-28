// Player progression: quests, talents, specialization, maps, and saving/loading (with migration of
// older saves). Pure logic apart from the storage helpers.
import { ABILITIES, CLASSES, SPEC_LEVEL, SPECS, type AbilityId, type ClassId, type SpecId } from '../data/classes';
import { GEAR_SLOTS, ITEM_TYPES, type GearSlot, type ItemType } from '../data/items';
import { QUESTS, QUEST_BY_ID, type QuestDef } from '../data/quests';
import { TIER_POINTS, TREES, NODE_BY_ID } from '../data/talents';
import { MAPS, MAP_ORDER, type MapId, type MobKind } from '../data/world';
import { MAX_LEVEL, emptyGear, respecCost, talentPointsTotal, xpToNext, type Gear, type Item, type Rarity } from './rules';

export const SAVE_VERSION = 2;
export const SAVE_KEY = 'ashenveil.save.v1';
export const SAVE_BACKUP_KEY = 'ashenveil.save.v1.backup';
export const BAG_SIZE = 24;

export interface QuestState { id: string; progress: number; done: boolean }

export interface Progress {
  version: number;
  name: string;
  cls: ClassId;
  spec: SpecId | null;
  level: number;
  xp: number;
  gold: number;
  potions: number;
  gear: Gear;
  bag: Item[];
  talents: Record<string, number>;
  active: QuestState[];
  completed: string[];
  mapId: MapId;
  unlocked: MapId[];
  pos: { x: number; z: number };
  vendorBought: string[];
  playSeconds: number;
}

export function newProgress(name: string, cls: ClassId): Progress {
  return {
    version: SAVE_VERSION, name, cls, spec: null, level: 1, xp: 0, gold: 5, potions: 3,
    gear: emptyGear(), bag: [], talents: {}, active: [], completed: [],
    mapId: 'vale', unlocked: ['vale'], pos: { ...MAPS.vale.spawn }, vendorBought: [], playSeconds: 0,
  };
}

/** Adds experience; returns the number of levels gained. */
export function addXp(p: Progress, amount: number): number {
  if (p.level >= MAX_LEVEL || amount <= 0) return 0;
  p.xp += amount;
  let gained = 0;
  while (p.level < MAX_LEVEL && p.xp >= xpToNext(p.level)) {
    p.xp -= xpToNext(p.level);
    p.level += 1;
    gained += 1;
  }
  if (p.level >= MAX_LEVEL) p.xp = 0;
  return gained;
}

// ---------- Quests ----------

export type QuestStatus = 'unavailable' | 'available' | 'active' | 'ready' | 'completed';

export function questStatus(p: Progress, q: QuestDef): QuestStatus {
  if (p.completed.includes(q.id)) return 'completed';
  const st = p.active.find((a) => a.id === q.id);
  if (st) return st.done ? 'ready' : 'active';
  if (!p.unlocked.includes(q.map)) return 'unavailable';
  if (p.level < q.minLevel) return 'unavailable';
  if (!q.requires.every((r) => p.completed.includes(r))) return 'unavailable';
  return 'available';
}

export function questsForNpc(p: Progress, npcId: string): { quest: QuestDef; status: QuestStatus }[] {
  const out: { quest: QuestDef; status: QuestStatus }[] = [];
  for (const q of QUESTS) {
    const status = questStatus(p, q);
    const turnIn = q.turnIn ?? q.giver;
    if ((status === 'available' && q.giver === npcId) || ((status === 'active' || status === 'ready') && turnIn === npcId)) {
      out.push({ quest: q, status });
    }
  }
  return out;
}

/** Marker shown above an NPC: '!' new quest, '?' ready to hand in, '' nothing. */
export function npcMarker(p: Progress, npcId: string): '!' | '?' | '' {
  const list = questsForNpc(p, npcId);
  if (list.some((e) => e.status === 'ready')) return '?';
  if (list.some((e) => e.status === 'available')) return '!';
  return '';
}

/** Quests that become available at a higher level (for "come back at level X" hints). */
export function nextQuestLevel(p: Progress, mapId: MapId): number | null {
  let best: number | null = null;
  for (const q of QUESTS) {
    if (q.map !== mapId || p.completed.includes(q.id) || p.active.some((a) => a.id === q.id)) continue;
    if (!q.requires.every((r) => p.completed.includes(r))) continue;
    if (q.minLevel > p.level && (best === null || q.minLevel < best)) best = q.minLevel;
  }
  return best;
}

export function acceptQuest(p: Progress, id: string): boolean {
  const q = QUEST_BY_ID[id];
  if (!q || questStatus(p, q) !== 'available') return false;
  p.active.push({ id, progress: 0, done: false });
  return true;
}

function bump(p: Progress, match: (q: QuestDef) => boolean): QuestState[] {
  const changed: QuestState[] = [];
  for (const st of p.active) {
    const q = QUEST_BY_ID[st.id];
    if (!q || st.done || !match(q)) continue;
    st.progress = Math.min(q.objective.count, st.progress + 1);
    st.done = st.progress >= q.objective.count;
    changed.push(st);
  }
  return changed;
}

export function onMobKilled(p: Progress, kind: MobKind): QuestState[] {
  return bump(p, (q) => q.objective.type === 'kill' && q.objective.mob === kind);
}

export function onItemLooted(p: Progress, item: string): QuestState[] {
  return bump(p, (q) => q.objective.type === 'collect' && q.objective.item === item);
}

export function onZoneEntered(p: Progress, zone: string): QuestState[] {
  return bump(p, (q) => q.objective.type === 'explore' && q.objective.zone === zone);
}

export function wantsQuestItem(p: Progress, item: string): boolean {
  return p.active.some((st) => {
    const q = QUEST_BY_ID[st.id];
    return !st.done && q?.objective.type === 'collect' && q.objective.item === item;
  });
}

/** Completes a quest; returns it, plus the map it unlocked (if any). */
export function completeQuest(p: Progress, id: string): { quest: QuestDef; unlocked: MapId | null } | null {
  const idx = p.active.findIndex((a) => a.id === id && a.done);
  if (idx < 0) return null;
  p.active.splice(idx, 1);
  p.completed.push(id);
  const q = QUEST_BY_ID[id];
  p.gold += q.gold;
  let unlocked: MapId | null = null;
  const map = MAPS[q.map];
  if (map.finalQuest === id && map.next && !p.unlocked.includes(map.next)) {
    p.unlocked.push(map.next);
    unlocked = map.next;
  }
  return { quest: q, unlocked };
}

// ---------- Specialization and talents ----------

export function canChooseSpec(p: Progress): boolean {
  return p.level >= SPEC_LEVEL && !p.spec;
}

export function chooseSpec(p: Progress, spec: SpecId): boolean {
  if (p.level < SPEC_LEVEL || SPECS[spec]?.cls !== p.cls) return false;
  p.spec = spec;
  return true;
}

export function pointsSpent(p: Progress, treeId?: string): number {
  let n = 0;
  for (const [id, r] of Object.entries(p.talents)) {
    if (!treeId || NODE_BY_ID[id]?.tree.id === treeId) n += r;
  }
  return n;
}

export function pointsAvailable(p: Progress): number {
  return talentPointsTotal(p.level) - pointsSpent(p);
}

/** Why a node cannot take another rank, or null if it can. */
export function rankUpBlocker(p: Progress, nodeId: string): string | null {
  const entry = NODE_BY_ID[nodeId];
  if (!entry) return 'Unknown talent';
  const { node, tree } = entry;
  if (tree.id !== p.cls && tree.id !== p.spec) return tree.id in SPECS ? `Choose the ${tree.name} path first` : 'Not your class';
  if ((p.talents[nodeId] ?? 0) >= node.maxRank) return 'Maxed';
  if (pointsAvailable(p) <= 0) return 'No talent points';
  const need = TIER_POINTS[node.tier];
  if (pointsSpent(p, tree.id) < need) return `Needs ${need} points in ${tree.name}`;
  return null;
}

export function rankUp(p: Progress, nodeId: string): boolean {
  if (rankUpBlocker(p, nodeId)) return false;
  p.talents[nodeId] = (p.talents[nodeId] ?? 0) + 1;
  return true;
}

/** Refunds all talents (and optionally the spec) for gold. */
export function respec(p: Progress, alsoSpec: boolean): boolean {
  const cost = respecCost(p.level);
  if (p.gold < cost) return false;
  p.gold -= cost;
  p.talents = {};
  if (alsoSpec) p.spec = null;
  return true;
}

export function abilityLearned(p: Progress, id: AbilityId): boolean {
  const ab = ABILITIES[id];
  if (p.level < ab.unlockLevel) return false;
  return !ab.spec || ab.spec === p.spec;
}

// ---------- Saving ----------

function isNum(v: unknown): v is number {
  return typeof v === 'number' && Number.isFinite(v);
}

function validItem(it: unknown): it is Item {
  if (!it || typeof it !== 'object') return false;
  const o = it as Record<string, unknown>;
  return typeof o.id === 'string' && typeof o.name === 'string' && ITEM_TYPES.includes(o.type as ItemType)
    && isNum(o.ilvl) && isNum(o.rarity) && o.rarity >= 0 && o.rarity <= 4
    && isNum(o.power) && isNum(o.stamina) && isNum(o.armor) && isNum(o.crit) && isNum(o.value);
}

function cleanItem(o: Item): Item {
  return {
    id: o.id, name: String(o.name).slice(0, 60), type: o.type, ilvl: o.ilvl, rarity: o.rarity as Rarity,
    power: o.power, stamina: o.stamina, armor: o.armor, crit: o.crit, value: o.value,
  };
}

/** Converts a version 1 save (3 gear slots, one map) to the current format. */
function migrateV1(d: Record<string, unknown>): Record<string, unknown> {
  const conv = (it: Record<string, unknown> | null | undefined) => {
    if (!it) return null;
    const type = it.slot === 'armor' ? 'chest' : it.slot === 'trinket' ? 'amulet' : 'weapon';
    return { ...it, type, crit: 0, slot: undefined };
  };
  const g = (d.gear ?? {}) as Record<string, Record<string, unknown> | null>;
  const completed = Array.isArray(d.completed) ? (d.completed as string[]) : [];
  const unlocked: MapId[] = ['vale'];
  if (completed.includes('cindermaw')) unlocked.push('frostmarch');
  return {
    ...d,
    version: 2,
    spec: null,
    talents: {},
    gear: { ...emptyGear(), weapon: conv(g.weapon), chest: conv(g.armor), amulet: conv(g.trinket) },
    bag: Array.isArray(d.bag) ? (d.bag as Record<string, unknown>[]).map(conv) : [],
    mapId: 'vale',
    unlocked,
    vendorBought: [],
  };
}

/** Validates untrusted save data. Returns null when it is damaged or from an unknown version. */
export function parseSave(raw: string | null): Progress | null {
  if (!raw) return null;
  let d: Record<string, unknown>;
  try {
    d = JSON.parse(raw);
  } catch {
    return null;
  }
  if (!d || typeof d !== 'object') return null;
  if (d.version === 1) {
    try {
      d = migrateV1(d);
    } catch {
      return null;
    }
  }
  if (d.version !== SAVE_VERSION) return null;
  if (typeof d.name !== 'string' || !(typeof d.cls === 'string' && d.cls in CLASSES)) return null;
  const cls = d.cls as ClassId;
  if (d.spec !== null && !(typeof d.spec === 'string' && SPECS[d.spec as SpecId]?.cls === cls)) return null;
  if (!isNum(d.level) || d.level < 1 || d.level > MAX_LEVEL) return null;
  if (!isNum(d.xp) || !isNum(d.gold) || !isNum(d.potions) || !isNum(d.playSeconds)) return null;
  const gear = d.gear as Record<string, unknown> | undefined;
  if (!gear || typeof gear !== 'object') return null;
  for (const s of GEAR_SLOTS) if (gear[s] != null && !validItem(gear[s])) return null;
  if (!Array.isArray(d.bag) || !d.bag.every(validItem)) return null;
  if (!Array.isArray(d.completed) || !d.completed.every((c) => typeof c === 'string' && c in QUEST_BY_ID)) return null;
  if (!Array.isArray(d.active)) return null;
  for (const a of d.active as QuestState[]) {
    if (!a || typeof a.id !== 'string' || !(a.id in QUEST_BY_ID) || !isNum(a.progress) || typeof a.done !== 'boolean') return null;
  }
  const talents = d.talents as Record<string, unknown>;
  if (!talents || typeof talents !== 'object') return null;
  for (const [k, v] of Object.entries(talents)) if (!(k in NODE_BY_ID) || !isNum(v) || v < 0 || v > NODE_BY_ID[k].node.maxRank) return null;
  if (!(typeof d.mapId === 'string' && d.mapId in MAPS)) return null;
  if (!Array.isArray(d.unlocked) || !d.unlocked.every((m) => MAP_ORDER.includes(m as MapId))) return null;
  const pos = d.pos as { x: unknown; z: unknown } | undefined;
  if (!pos || !isNum(pos.x) || !isNum(pos.z)) return null;
  const outGear = emptyGear();
  for (const s of GEAR_SLOTS) outGear[s as GearSlot] = gear[s] ? cleanItem(gear[s] as Item) : null;
  const p: Progress = {
    version: SAVE_VERSION,
    name: d.name.slice(0, 16),
    cls,
    spec: (d.spec as SpecId | null) ?? null,
    level: Math.floor(d.level),
    xp: Math.max(0, d.xp),
    gold: Math.max(0, Math.floor(d.gold)),
    potions: Math.max(0, Math.floor(d.potions)),
    gear: outGear,
    bag: (d.bag as Item[]).slice(0, BAG_SIZE).map(cleanItem),
    talents: { ...(talents as Record<string, number>) },
    active: (d.active as QuestState[]).map((a) => ({ id: a.id, progress: a.progress, done: a.done })),
    completed: [...(d.completed as string[])],
    mapId: d.mapId as MapId,
    unlocked: [...new Set(['vale', ...(d.unlocked as MapId[])])] as MapId[],
    pos: { x: pos.x, z: pos.z },
    vendorBought: Array.isArray(d.vendorBought) ? (d.vendorBought as unknown[]).filter((x): x is string => typeof x === 'string').slice(-200) : [],
    playSeconds: d.playSeconds,
  };
  if (!p.unlocked.includes(p.mapId)) p.mapId = 'vale';
  // Talents spent beyond what the level allows (e.g. an edited save) are refunded.
  if (pointsAvailable(p) < 0) p.talents = {};
  return p;
}

export function loadGame(storage: Pick<Storage, 'getItem'> = localStorage): Progress | null {
  try {
    return parseSave(storage.getItem(SAVE_KEY)) ?? parseSave(storage.getItem(SAVE_BACKUP_KEY));
  } catch {
    return null;
  }
}

export function saveGame(p: Progress, storage: Pick<Storage, 'getItem' | 'setItem'> = localStorage): boolean {
  try {
    const next = JSON.stringify(p);
    const current = storage.getItem(SAVE_KEY);
    // Keep the last valid checkpoint before replacing the active save. On a fresh
    // character, store the same checkpoint twice so the first save is recoverable.
    storage.setItem(SAVE_BACKUP_KEY, parseSave(current) ? current! : next);
    storage.setItem(SAVE_KEY, next);
    return true;
  } catch {
    return false;
  }
}

export function deleteSave(storage: Pick<Storage, 'removeItem'> = localStorage): void {
  try {
    storage.removeItem(SAVE_KEY);
    storage.removeItem(SAVE_BACKUP_KEY);
  } catch {
    /* storage unavailable */
  }
}

export { TREES };
