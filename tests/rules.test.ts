import { describe, expect, it } from 'vitest';
import { ABILITIES, CLASSES, SPECS, abilitiesFor, type AbilityId, type ClassId } from '../src/data/classes';
import { GEAR_SLOTS } from '../src/data/items';
import { QUESTS, QUEST_BY_ID, QUEST_ITEMS } from '../src/data/quests';
import { TREES, NODE_BY_ID } from '../src/data/talents';
import { ALL_NPCS, MAPS, MAP_ORDER, MOBS, WORLD_LIMIT } from '../src/data/world';
import {
  SAVE_BACKUP_KEY, SAVE_KEY, abilityLearned, acceptQuest, addXp, canChooseSpec, chooseSpec, completeQuest, deleteSave, loadGame,
  newProgress, npcMarker, onItemLooted, onMobKilled, onZoneEntered, parseSave, pointsAvailable, questStatus, rankUp, rankUpBlocker,
  respec, saveGame,
} from '../src/game/progress';
import {
  MAX_LEVEL, emptyGear, itemScore, levelMod, makeItem, makeLegendary, makeRng, mitigation, mobStats, mobXp, playerStats,
  rollMobLoot, slotFor, talentBonuses, vendorStock, xpToNext,
} from '../src/game/rules';
import { Terrain } from '../src/game/terrain';

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
    expect(Math.abs(mobXp(5, 5, true) - mobXp(5, 5, false) * 3)).toBeLessThanOrEqual(2);
  });

  it('stats scale with level, gear and talents', () => {
    const s1 = playerStats('stormblade', 1, emptyGear());
    const s20 = playerStats('stormblade', 20, emptyGear());
    expect(s20.maxHp).toBeGreaterThan(s1.maxHp);
    expect(s20.damageScale).toBeGreaterThan(s1.damageScale);
    const chest = makeItem(makeRng(1), 'stormblade', 20, 2, 'chest');
    const geared = playerStats('stormblade', 20, { ...emptyGear(), chest });
    expect(geared.armor).toBeGreaterThan(s20.armor);
    expect(geared.maxHp).toBeGreaterThan(s20.maxHp);
    const talented = playerStats('stormblade', 20, emptyGear(), { sb_iron: 5, sb_might: 5 });
    expect(talented.maxHp).toBeGreaterThan(s20.maxHp);
    expect(talented.power).toBeGreaterThan(s20.power);
  });

  it('mitigation and level modifiers stay in sane ranges', () => {
    expect(mitigation(0, 10)).toBe(1);
    expect(mitigation(150, 10)).toBeCloseTo(0.5);
    expect(mitigation(100, 30)).toBeGreaterThan(mitigation(100, 1));
    expect(levelMod(1, 10)).toBeGreaterThanOrEqual(0.6);
    expect(levelMod(10, 1)).toBeLessThanOrEqual(1.2);
  });

  it('mobs get tougher every map', () => {
    expect(mobStats('rimewolf', 11).maxHp).toBeGreaterThan(mobStats('brute', 9).maxHp);
    expect(mobStats('scorpion', 21).maxHp).toBeGreaterThan(mobStats('drowned', 17).maxHp);
  });

  it('items: every type, better rarity scores higher, rings fill both slots', () => {
    const rng = makeRng(3);
    for (const t of ['weapon', 'head', 'chest', 'hands', 'feet', 'amulet', 'ring'] as const) {
      const it = makeItem(rng, 'emberseer', 15, 1, t);
      expect(it.type).toBe(t);
      expect(itemScore(it)).toBeGreaterThan(0);
    }
    expect(itemScore(makeItem(rng, 'emberseer', 5, 3, 'weapon'))).toBeGreaterThan(itemScore(makeItem(rng, 'emberseer', 5, 0, 'weapon')));
    const gear = emptyGear();
    const r1 = makeItem(rng, 'thornkeeper', 5, 1, 'ring');
    expect(slotFor(r1, gear)).toBe('ring1');
    gear.ring1 = r1;
    expect(slotFor(makeItem(rng, 'thornkeeper', 5, 1, 'ring'), gear)).toBe('ring2');
    expect(itemScore(null)).toBe(0);
  });

  it('bosses drop a legendary, elites a rare or better', () => {
    for (const boss of ['boss', 'ysolde', 'azhkar'] as const) {
      const loot = rollMobLoot(makeRng(9), 'thornkeeper', boss, MOBS[boss].levels[0]);
      expect(loot.items.some((i) => i.rarity === 4)).toBe(true);
    }
    expect(rollMobLoot(makeRng(2), 'stormblade', 'thane', 18).items[0].rarity).toBeGreaterThanOrEqual(2);
    expect(rollMobLoot(makeRng(2), 'stormblade', 'imp', 8).items.length).toBe(0);
    expect(makeLegendary(makeRng(4), 'emberseer', 'azhkar', 32).name).toMatch(/Sunflayer|Dune Tyrant|Azhkar/);
  });

  it('vendor stock is stable for a level', () => {
    const a = vendorStock('vale', 5, 'stormblade').map((v) => v.item.name);
    const b = vendorStock('vale', 5, 'stormblade').map((v) => v.item.name);
    expect(a).toEqual(b);
    expect(a.length).toBe(4);
  });
});

describe('classes, specs and talents', () => {
  it('each class has 3 specs, each with 2 abilities of its own', () => {
    for (const c of Object.values(CLASSES)) {
      expect(c.specs.length).toBe(3);
      for (const sp of c.specs) {
        expect(SPECS[sp].cls).toBe(c.id);
        for (const a of SPECS[sp].abilities) expect(ABILITIES[a].spec).toBe(sp);
        expect(abilitiesFor(c.id, sp).length).toBe(6);
      }
    }
  });

  it('every talent tree exists and talent ability mods point at real abilities', () => {
    for (const c of Object.values(CLASSES)) {
      expect(TREES[c.id]).toBeTruthy();
      for (const sp of c.specs) expect(TREES[sp]).toBeTruthy();
    }
    for (const { node } of Object.values(NODE_BY_ID)) {
      for (const m of node.perRank) if (m.k === 'ability') expect(ABILITIES[m.id as AbilityId]).toBeTruthy();
    }
  });

  it('talent points: one per level, tiers, class/spec restrictions', () => {
    const p = newProgress('T', 'emberseer');
    expect(pointsAvailable(p)).toBe(0);
    expect(rankUp(p, 'es_focus')).toBe(false);
    p.level = 12;
    expect(pointsAvailable(p)).toBe(11);
    expect(rankUpBlocker(p, 'sb_might')).toBe('Not your class');
    expect(rankUpBlocker(p, 'es_star')).toMatch(/Needs 5/);
    for (let i = 0; i < 5; i++) expect(rankUp(p, 'es_focus')).toBe(true);
    expect(rankUp(p, 'es_focus')).toBe(false); // maxed
    expect(rankUp(p, 'es_star')).toBe(true); // tier 2 open after 5 points
    expect(rankUpBlocker(p, 'fw_spike')).toMatch(/path/i);
    expect(talentBonuses(p.talents).stat.powerPct).toBe(15);
  });

  it('spec choice at level 10, respec refunds for gold', () => {
    const p = newProgress('T', 'thornkeeper');
    expect(chooseSpec(p, 'rotbloom')).toBe(false);
    p.level = 10;
    expect(canChooseSpec(p)).toBe(true);
    expect(chooseSpec(p, 'pyromancer')).toBe(false); // other class
    expect(chooseSpec(p, 'rotbloom')).toBe(true);
    expect(abilityLearned(p, 'plague')).toBe(true);
    expect(abilityLearned(p, 'witherBurst')).toBe(false); // level 16
    expect(abilityLearned(p, 'lifebloom')).toBe(false); // other path
    rankUp(p, 'rb_plague');
    p.gold = 1000;
    expect(respec(p, true)).toBe(true);
    expect(p.spec).toBeNull();
    expect(pointsAvailable(p)).toBe(9);
  });
});

describe('progress, quests and maps', () => {
  it('levels up and carries over xp, capped at max level', () => {
    const p = newProgress('T', 'stormblade');
    expect(addXp(p, xpToNext(1) + 5)).toBe(1);
    expect(p.level).toBe(2);
    expect(p.xp).toBe(5);
    addXp(p, 1e9);
    expect(p.level).toBe(MAX_LEVEL);
    expect(p.xp).toBe(0);
  });

  it('quest chain: accept, progress, complete', () => {
    const p = newProgress('T', 'emberseer');
    expect(npcMarker(p, 'vale_a')).toBe('!');
    expect(questStatus(p, QUEST_BY_ID.stalkers)).toBe('unavailable');
    expect(acceptQuest(p, 'wolves')).toBe(true);
    expect(acceptQuest(p, 'wolves')).toBe(false);
    for (let i = 0; i < 8; i++) onMobKilled(p, 'wolf');
    expect(p.active[0].done).toBe(true);
    expect(npcMarker(p, 'vale_a')).toBe('?');
    const gold = p.gold;
    expect(completeQuest(p, 'wolves')?.quest.id).toBe('wolves');
    expect(p.gold).toBeGreaterThan(gold);
  });

  it('collect and explore objectives', () => {
    const p = newProgress('T', 'thornkeeper');
    acceptQuest(p, 'pelts');
    for (let i = 0; i < 6; i++) onItemLooted(p, 'pelt');
    expect(p.active[0].done).toBe(true);
    p.unlocked.push('frostmarch');
    p.level = 10;
    expect(acceptQuest(p, 'fm_arrival')).toBe(true);
    onZoneEntered(p, 'rimewood');
    expect(p.active.find((a) => a.id === 'fm_arrival')!.done).toBe(true);
  });

  it('the whole campaign can be completed and unlocks every map in order', () => {
    const p = newProgress('T', 'stormblade');
    p.level = MAX_LEVEL;
    let guard = 0;
    while (p.completed.length < QUESTS.length && guard++ < 100) {
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
    expect(p.unlocked).toEqual(['vale', 'frostmarch', 'sunscar']);
  });

  it('quest data is consistent', () => {
    const npcIds = new Set(ALL_NPCS.map((n) => n.id));
    for (const q of QUESTS) {
      expect(npcIds.has(q.giver)).toBe(true);
      expect(MAPS[q.map].npcs.some((n) => n.id === q.giver)).toBe(true);
      for (const r of q.requires) expect(QUEST_BY_ID[r]).toBeTruthy();
      const o = q.objective;
      if (o.type === 'kill') expect(MAPS[q.map].camps.some((c) => c.kind === o.mob)).toBe(true);
      if (o.type === 'collect') {
        expect(QUEST_ITEMS[o.item]).toBeTruthy();
        expect(Object.values(MOBS).some((m) => m.questDrop?.item === o.item)).toBe(true);
      }
      if (o.type === 'explore') expect(MAPS[q.map].zones.some((z) => z.id === o.zone)).toBe(true);
    }
    for (const id of MAP_ORDER) expect(QUEST_BY_ID[MAPS[id].finalQuest]).toBeTruthy();
  });

  it('save round-trips and rejects damaged data', () => {
    const p = newProgress('Hero', 'stormblade');
    p.gear.weapon = makeItem(makeRng(2), 'stormblade', 3, 1, 'weapon');
    p.level = 11;
    chooseSpec(p, 'bulwark');
    rankUp(p, 'bw_hide');
    acceptQuest(p, 'wolves');
    const back = parseSave(JSON.stringify(p));
    expect(back).toEqual(p);
    expect(parseSave('not json')).toBeNull();
    expect(parseSave(JSON.stringify({ ...p, version: 99 }))).toBeNull();
    expect(parseSave(JSON.stringify({ ...p, level: 50 }))).toBeNull();
    expect(parseSave(JSON.stringify({ ...p, cls: 'hacker' }))).toBeNull();
    expect(parseSave(JSON.stringify({ ...p, spec: 'pyromancer' }))).toBeNull();
    expect(parseSave(JSON.stringify({ ...p, talents: { nope: 1 } }))).toBeNull();
    expect(parseSave(JSON.stringify({ ...p, completed: ['nope'] }))).toBeNull();
    expect(parseSave(null)).toBeNull();
    // Too many talent points for the level are refunded, not trusted.
    const cheat = parseSave(JSON.stringify({ ...p, level: 2, talents: { bw_hide: 5 } }));
    expect(cheat?.talents).toEqual({});
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

  it('migrates a version 1 save (3 gear slots, one map)', () => {
    const v1 = {
      version: 1, name: 'Old', cls: 'emberseer', level: 10, xp: 12, gold: 50, potions: 2,
      gear: {
        weapon: { id: 'a', name: 'Staff', slot: 'weapon', ilvl: 9, rarity: 2, power: 20, stamina: 5, armor: 0, value: 30 },
        armor: { id: 'b', name: 'Vest', slot: 'armor', ilvl: 8, rarity: 1, power: 3, stamina: 12, armor: 30, value: 20 },
        trinket: null,
      },
      bag: [{ id: 'c', name: 'Charm', slot: 'trinket', ilvl: 5, rarity: 0, power: 4, stamina: 3, armor: 0, value: 5 }],
      active: [], completed: ['wolves', 'pelts', 'stalkers', 'raiders', 'relic', 'brutes', 'cindermaw'],
      pos: { x: 1, z: 2 }, bossDefeated: true, playSeconds: 3600,
    };
    const p = parseSave(JSON.stringify(v1))!;
    expect(p).not.toBeNull();
    expect(p.version).toBe(2);
    expect(p.gear.chest?.name).toBe('Vest');
    expect(p.gear.weapon?.type).toBe('weapon');
    expect(p.bag[0].type).toBe('amulet');
    expect(p.unlocked).toContain('frostmarch');
    expect(p.spec).toBeNull();
    expect(Object.keys(p.gear).sort()).toEqual([...GEAR_SLOTS].sort());
  });
});

describe('world', () => {
  it('terrain is finite on every map and camps/NPCs are inside the walkable area', () => {
    for (const id of MAP_ORDER) {
      const t = new Terrain(MAPS[id]);
      for (let x = -WORLD_LIMIT; x <= WORLD_LIMIT; x += 30) {
        for (let z = -WORLD_LIMIT; z <= WORLD_LIMIT; z += 30) expect(Number.isFinite(t.heightAt(x, z))).toBe(true);
      }
      for (const c of [...MAPS[id].camps, ...MAPS[id].npcs]) {
        expect(Math.abs(c.x)).toBeLessThan(WORLD_LIMIT - 30);
        expect(Math.abs(c.z)).toBeLessThan(WORLD_LIMIT - 30);
      }
    }
  });

  it('mob levels rise through the maps', () => {
    for (const id of MAP_ORDER) {
      const m = MAPS[id];
      for (const c of m.camps) {
        const lv = MOBS[c.kind].levels;
        expect(lv[0]).toBeGreaterThanOrEqual(m.levels[0]);
        expect(lv[1]).toBeLessThanOrEqual(m.levels[1]);
      }
    }
  });

  it('class data sanity', () => {
    for (const c of Object.keys(CLASSES) as ClassId[]) expect(abilitiesFor(c, null).length).toBe(4);
  });
});

describe('professions, reputation, bounties, achievements', () => {
  it('gathering needs skill, raises it and counts', async () => {
    const { gather, canGather } = await import('../src/game/economy');
    const p = newProgress('G', 'stormblade');
    expect(canGather(p, 'frostbloom')).toBe(false);
    expect(gather(p, 'frostbloom', 0.5)).toBe(0);
    for (let i = 0; i < 40; i++) gather(p, 'sunleaf', 0.1);
    expect(p.gatherSkill).toBeGreaterThanOrEqual(60);
    expect(canGather(p, 'frostbloom')).toBe(true);
    expect(p.stats.gathered).toBeGreaterThanOrEqual(40);
  });

  it('crafting spends materials; elixirs raise stats; reforge raises item level', async () => {
    const { craft, craftBlocker, buffMods } = await import('../src/game/economy');
    const p = newProgress('C', 'emberseer');
    expect(craftBlocker(p, 'draught')).toMatch(/herbs/);
    p.materials = { sunleaf: 6, copper: 6 };
    const pots = p.potions;
    expect(craft(p, 'draught')).toMatch(/Draught/);
    expect(p.potions).toBe(pots + 2);
    p.gold = 100;
    craft(p, 'mightElixir');
    expect(buffMods(p).powerPct).toBe(12);
    const base = playerStats('emberseer', 5, emptyGear());
    const buffed = playerStats('emberseer', 5, emptyGear(), {}, buffMods(p));
    expect(buffed.power).toBeGreaterThan(base.power);
    const w = makeItem(makeRng(1), 'emberseer', 10, 2, 'weapon');
    const before = w.power;
    p.materials.copper = 10;
    craft(p, 'reforge', w);
    expect(w.ilvl).toBe(12);
    expect(w.power).toBeGreaterThan(before);
    p.playSeconds += 16 * 60;
    expect(buffMods(p).powerPct).toBeUndefined();
  });

  it('reputation ranks and discounts', async () => {
    const { addRep, repRank } = await import('../src/game/economy');
    const p = newProgress('R', 'thornkeeper');
    expect(repRank(0).name).toBe('Neutral');
    expect(addRep(p, 'vale', 1200)).toBe('Friendly');
    expect(repRank(p.rep.vale!).discount).toBeCloseTo(0.05);
    addRep(p, 'vale', 20000);
    expect(repRank(p.rep.vale!).name).toBe('Exalted');
  });

  it('daily bounties roll once a day and progress', async () => {
    const { refreshBounties, onBountyKill, onBountyGather, claimableBounties } = await import('../src/game/economy');
    const p = newProgress('B', 'stormblade');
    refreshBounties(p, '2026-9-28');
    expect(p.bounties.list.length).toBe(9);
    const first = JSON.stringify(p.bounties.list);
    refreshBounties(p, '2026-9-28');
    expect(JSON.stringify(p.bounties.list)).toBe(first);
    for (const b of p.bounties.list.filter((x) => x.map === 'vale')) {
      for (let i = 0; i < b.count; i++) {
        if (b.kind === 'kill') onBountyKill(p, 'vale', b.mob!);
        else onBountyGather(p, 'vale', 1);
      }
    }
    expect(claimableBounties(p, 'vale').length).toBe(3);
    refreshBounties(p, '2026-9-29');
    expect(p.bounties.day).toBe('2026-9-29');
  });

  it('achievements award once, with titles; extras survive a save', async () => {
    const { checkAchievements, titles } = await import('../src/game/economy');
    const p = newProgress('A', 'stormblade');
    p.level = 10;
    p.stats.kills = 120;
    const got = checkAchievements(p).map((a) => a.id);
    expect(got).toContain('lvl10');
    expect(got).toContain('kill100');
    expect(checkAchievements(p).length).toBe(0);
    p.completed.push('cindermaw');
    checkAchievements(p);
    expect(titles(p)).toContain('of the Vale');
    p.title = 'of the Vale';
    p.materials = { copper: 3 };
    p.rep = { vale: 1500 };
    const back = parseSave(JSON.stringify(p))!;
    expect(back.title).toBe('of the Vale');
    expect(back.materials.copper).toBe(3);
    expect(back.rep.vale).toBe(1500);
    expect(back.achievements).toEqual(p.achievements);
    // A title you have not earned is dropped.
    expect(parseSave(JSON.stringify({ ...p, title: 'the Legendary' }))!.title).toBeNull();
  });
});
