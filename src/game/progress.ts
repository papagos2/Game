// Player progression and quest state, plus saving/loading. Pure logic (no DOM besides storage).
import { CLASSES, type ClassId } from '../data/classes';
import { QUESTS, QUEST_BY_ID, type QuestDef } from '../data/quests';
import { SPAWN, type MobKind } from '../data/world';
import { MAX_LEVEL, SLOTS, xpToNext, type Gear, type Item } from './rules';

export const SAVE_VERSION = 1;
export const SAVE_KEY = 'ashenveil.save.v1';
export const BAG_SIZE = 16;

export interface QuestState { id: string; progress: number; done: boolean }

export interface Progress {
  version: number;
  name: string;
  cls: ClassId;
  level: number;
  xp: number;
  gold: number;
  potions: number;
  gear: Gear;
  bag: Item[];
  active: QuestState[];
  completed: string[];
  pos: { x: number; z: number };
  bossDefeated: boolean;
  playSeconds: number;
}

export function newProgress(name: string, cls: ClassId): Progress {
  return {
    version: SAVE_VERSION, name, cls, level: 1, xp: 0, gold: 5, potions: 3,
    gear: { weapon: null, armor: null, trinket: null }, bag: [],
    active: [], completed: [], pos: { ...SPAWN }, bossDefeated: false, playSeconds: 0,
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

export type QuestStatus = 'unavailable' | 'available' | 'active' | 'ready' | 'completed';

export function questStatus(p: Progress, q: QuestDef): QuestStatus {
  if (p.completed.includes(q.id)) return 'completed';
  const st = p.active.find((a) => a.id === q.id);
  if (st) return st.done ? 'ready' : 'active';
  if (p.level < q.minLevel) return 'unavailable';
  if (!q.requires.every((r) => p.completed.includes(r))) return 'unavailable';
  return 'available';
}

/** Quests that an NPC offers or accepts right now. */
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

/** Marker shown above an NPC: '!' = new quest, '?' = ready to hand in, '' = nothing. */
export function npcMarker(p: Progress, npcId: string): '!' | '?' | '' {
  const list = questsForNpc(p, npcId);
  if (list.some((e) => e.status === 'ready')) return '?';
  if (list.some((e) => e.status === 'available')) return '!';
  return '';
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

/** Whether an active quest currently wants this quest item. */
export function wantsQuestItem(p: Progress, item: string): boolean {
  return p.active.some((st) => {
    const q = QUEST_BY_ID[st.id];
    return !st.done && q?.objective.type === 'collect' && q.objective.item === item;
  });
}

export function completeQuest(p: Progress, id: string): QuestDef | null {
  const idx = p.active.findIndex((a) => a.id === id && a.done);
  if (idx < 0) return null;
  p.active.splice(idx, 1);
  p.completed.push(id);
  const q = QUEST_BY_ID[id];
  p.gold += q.gold;
  return q;
}

// ---------- Saving ----------

function isNum(v: unknown): v is number {
  return typeof v === 'number' && Number.isFinite(v);
}

function validItem(it: unknown): it is Item {
  if (!it || typeof it !== 'object') return false;
  const o = it as Record<string, unknown>;
  return typeof o.id === 'string' && typeof o.name === 'string' && SLOTS.includes(o.slot as never)
    && isNum(o.ilvl) && isNum(o.rarity) && isNum(o.power) && isNum(o.stamina) && isNum(o.armor) && isNum(o.value);
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
  if (!d || typeof d !== 'object' || d.version !== SAVE_VERSION) return null;
  if (typeof d.name !== 'string' || !(typeof d.cls === 'string' && d.cls in CLASSES)) return null;
  if (!isNum(d.level) || d.level < 1 || d.level > MAX_LEVEL) return null;
  if (!isNum(d.xp) || !isNum(d.gold) || !isNum(d.potions) || !isNum(d.playSeconds)) return null;
  const gear = d.gear as Record<string, unknown> | undefined;
  if (!gear || typeof gear !== 'object') return null;
  for (const s of SLOTS) if (gear[s] !== null && !validItem(gear[s])) return null;
  if (!Array.isArray(d.bag) || !d.bag.every(validItem)) return null;
  if (!Array.isArray(d.completed) || !d.completed.every((c) => typeof c === 'string' && c in QUEST_BY_ID)) return null;
  if (!Array.isArray(d.active)) return null;
  for (const a of d.active as QuestState[]) {
    if (!a || typeof a.id !== 'string' || !(a.id in QUEST_BY_ID) || !isNum(a.progress) || typeof a.done !== 'boolean') return null;
  }
  const pos = d.pos as { x: unknown; z: unknown } | undefined;
  if (!pos || !isNum(pos.x) || !isNum(pos.z)) return null;
  return {
    version: SAVE_VERSION,
    name: d.name.slice(0, 16),
    cls: d.cls as ClassId,
    level: Math.floor(d.level),
    xp: Math.max(0, d.xp),
    gold: Math.max(0, Math.floor(d.gold)),
    potions: Math.max(0, Math.floor(d.potions)),
    gear: { weapon: (gear.weapon as Item) ?? null, armor: (gear.armor as Item) ?? null, trinket: (gear.trinket as Item) ?? null },
    bag: (d.bag as Item[]).slice(0, BAG_SIZE),
    active: (d.active as QuestState[]).map((a) => ({ id: a.id, progress: a.progress, done: a.done })),
    completed: [...(d.completed as string[])],
    pos: { x: pos.x, z: pos.z },
    bossDefeated: d.bossDefeated === true,
    playSeconds: d.playSeconds,
  };
}

export function loadGame(storage: Pick<Storage, 'getItem'> = localStorage): Progress | null {
  try {
    return parseSave(storage.getItem(SAVE_KEY));
  } catch {
    return null;
  }
}

export function saveGame(p: Progress, storage: Pick<Storage, 'setItem'> = localStorage): boolean {
  try {
    storage.setItem(SAVE_KEY, JSON.stringify(p));
    return true;
  } catch {
    return false;
  }
}

export function deleteSave(storage: Pick<Storage, 'removeItem'> = localStorage): void {
  try {
    storage.removeItem(SAVE_KEY);
  } catch {
    /* storage unavailable */
  }
}
