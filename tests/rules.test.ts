import { describe, expect, it } from 'vitest';
import { QUESTS } from '../src/data/quests';
import {
  SAVE_BACKUP_KEY, SAVE_KEY, acceptQuest, addXp, completeQuest, deleteSave, loadGame, newProgress,
  npcMarker, onItemLooted, onMobKilled, parseSave, questStatus, saveGame,
} from '../src/game/progress';
import {
  MAX_LEVEL, itemScore, levelMod, makeItem, makeRng, mitigation, mobStats, mobXp, playerStats, rollMobLoot, xpToNext,
} from '../src/game/rules';
import { heightAt } from '../src/game/terrain';
import { CAMPS, NPCS, WORLD_LIMIT } from '../src/data/world';

describe('rules', () => {
  it('rng is deterministic', () => {
    const a = makeRng(5);
    const b = makeRng(5);
    for (let i = 0; i < 10; i++) expect(a()).toBe(b());
  });

  it('xp curve grows and stops at max level', () => {
    for (let l = 1; l < MAX_LEVEL - 1; l++) expect(xpToNext(l + 1)).toBeGreaterThan(xpToNext(l));
    expect(xpToNext(MAX_LEVEL)).toBe(0);
  });

  it('grey mobs give no xp, higher mobs give more', () => {
    expect(mobXp(1, 6, false)).toBe(0);
    expect(mobXp(5, 4, false)).toBeGreaterThan(mobXp(4, 4, false));
    expect(mobXp(5, 5, true)).toBe(mobXp(5, 5, false) * 3);
  });

  it('stats scale with level and gear', () => {
    const empty = { weapon: null, armor: null, trinket: null };
    const s1 = playerStats('stormblade', 1, empty);
    const s5 = playerStats('stormblade', 5, empty);
    expect(s5.maxHp).toBeGreaterThan(s1.maxHp);
    expect(s5.damageScale).toBeGreaterThan(s1.damageScale);
    const rng = makeRng(1);
    const armor = makeItem(rng, 'stormblade', 5, 2, 'armor');
    const geared = playerStats('stormblade', 5, { ...empty, armor });
    expect(geared.armor).toBeGreaterThan(s5.armor);
    expect(geared.maxHp).toBeGreaterThan(s5.maxHp);
  });

  it('mitigation and level modifiers stay in sane ranges', () => {
    expect(mitigation(0)).toBe(1);
    expect(mitigation(100)).toBeCloseTo(0.5);
    expect(levelMod(1, 10)).toBeGreaterThanOrEqual(0.6);
    expect(levelMod(10, 1)).toBeLessThanOrEqual(1.2);
  });

  it('items: better rarity means better score', () => {
    const rng = makeRng(3);
    const common = makeItem(rng, 'emberseer', 5, 0, 'weapon');
    const epic = makeItem(rng, 'emberseer', 5, 3, 'weapon');
    expect(itemScore(epic)).toBeGreaterThan(itemScore(common));
    expect(itemScore(null)).toBe(0);
  });

  it('boss always drops an epic', () => {
    const loot = rollMobLoot(makeRng(9), 'thornkeeper', 'boss', 10);
    expect(loot.item?.rarity).toBe(3);
    expect(mobStats('boss', 10).maxHp).toBeGreaterThan(mobStats('brute', 9).maxHp * 5);
  });
});

describe('progress and quests', () => {
  it('levels up and carries over xp', () => {
    const p = newProgress('T', 'stormblade');
    expect(addXp(p, xpToNext(1) + 5)).toBe(1);
    expect(p.level).toBe(2);
    expect(p.xp).toBe(5);
    addXp(p, 1e9);
    expect(p.level).toBe(MAX_LEVEL);
    expect(p.xp).toBe(0);
  });

  it('quest chain: accept, progress, complete, unlock next', () => {
    const p = newProgress('T', 'emberseer');
    expect(npcMarker(p, 'elra')).toBe('!');
    expect(questStatus(p, QUESTS[2])).toBe('unavailable');
    expect(acceptQuest(p, 'wolves')).toBe(true);
    expect(acceptQuest(p, 'wolves')).toBe(false);
    for (let i = 0; i < 8; i++) onMobKilled(p, 'wolf');
    expect(p.active[0].done).toBe(true);
    expect(npcMarker(p, 'elra')).toBe('?');
    const gold = p.gold;
    expect(completeQuest(p, 'wolves')?.id).toBe('wolves');
    expect(p.gold).toBeGreaterThan(gold);
    p.level = 3;
    expect(questStatus(p, QUESTS.find((q) => q.id === 'stalkers')!)).toBe('available');
  });

  it('collect quests count loot', () => {
    const p = newProgress('T', 'thornkeeper');
    acceptQuest(p, 'pelts');
    for (let i = 0; i < 6; i++) onItemLooted(p, 'pelt');
    expect(p.active[0].done).toBe(true);
  });

  it('every quest can be reached', () => {
    const p = newProgress('T', 'stormblade');
    p.level = MAX_LEVEL;
    let guard = 0;
    while (p.completed.length < QUESTS.length && guard++ < 50) {
      for (const q of QUESTS) {
        if (questStatus(p, q) === 'available') {
          acceptQuest(p, q.id);
          const st = p.active.find((a) => a.id === q.id)!;
          st.progress = q.objective.count;
          st.done = true;
          completeQuest(p, q.id);
        }
      }
    }
    expect(p.completed.length).toBe(QUESTS.length);
  });

  it('save round-trips and rejects damaged data', () => {
    const p = newProgress('Hero', 'stormblade');
    p.gear.weapon = makeItem(makeRng(2), 'stormblade', 3, 1, 'weapon');
    acceptQuest(p, 'wolves');
    const back = parseSave(JSON.stringify(p));
    expect(back).toEqual(p);
    expect(parseSave('not json')).toBeNull();
    expect(parseSave(JSON.stringify({ ...p, version: 99 }))).toBeNull();
    expect(parseSave(JSON.stringify({ ...p, level: 50 }))).toBeNull();
    expect(parseSave(JSON.stringify({ ...p, cls: 'hacker' }))).toBeNull();
    expect(parseSave(JSON.stringify({ ...p, completed: ['nope'] }))).toBeNull();
    expect(parseSave(null)).toBeNull();
  });

  it('recovers the previous valid checkpoint if the active save is damaged', () => {
    const values = new Map<string, string>();
    const storage = {
      getItem: (key: string) => values.get(key) ?? null,
      setItem: (key: string, value: string) => { values.set(key, value); },
      removeItem: (key: string) => { values.delete(key); },
    };
    const p = newProgress('Hero', 'stormblade');
    expect(saveGame(p, storage)).toBe(true);
    p.level = 2;
    expect(saveGame(p, storage)).toBe(true);
    values.set(SAVE_KEY, '{broken');
    expect(loadGame(storage)?.level).toBe(1);
    p.level = 3;
    expect(saveGame(p, storage)).toBe(true);
    expect(loadGame(storage)?.level).toBe(3);
    deleteSave(storage);
    expect(values.has(SAVE_KEY)).toBe(false);
    expect(values.has(SAVE_BACKUP_KEY)).toBe(false);
  });
});

describe('world', () => {
  it('terrain is finite everywhere and camps/NPCs are inside the walkable area', () => {
    for (let x = -WORLD_LIMIT; x <= WORLD_LIMIT; x += 20) {
      for (let z = -WORLD_LIMIT; z <= WORLD_LIMIT; z += 20) expect(Number.isFinite(heightAt(x, z))).toBe(true);
    }
    for (const c of [...CAMPS, ...NPCS]) {
      expect(Math.abs(c.x)).toBeLessThan(WORLD_LIMIT - 30);
      expect(Math.abs(c.z)).toBeLessThan(WORLD_LIMIT - 30);
    }
  });
});
