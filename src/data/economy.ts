// Professions (gathering and crafting), factions (reputation), daily bounties and achievements.
import type { MapId, MobKind } from './world';

// ---------------- Gathering ----------------

export type MaterialId = 'sunleaf' | 'copper' | 'frostbloom' | 'silver' | 'emberroot' | 'sunsteel';

export interface MaterialDef { id: MaterialId; name: string; kind: 'herb' | 'ore'; tier: 1 | 2 | 3; color: string; skill: number }

export const MATERIALS: Record<MaterialId, MaterialDef> = {
  sunleaf: { id: 'sunleaf', name: 'Sunleaf', kind: 'herb', tier: 1, color: '#b8e05a', skill: 0 },
  copper: { id: 'copper', name: 'Copper Ore', kind: 'ore', tier: 1, color: '#c8743a', skill: 0 },
  frostbloom: { id: 'frostbloom', name: 'Frostbloom', kind: 'herb', tier: 2, color: '#9fe8ff', skill: 60 },
  silver: { id: 'silver', name: 'Silver Ore', kind: 'ore', tier: 2, color: '#d8e0e8', skill: 60 },
  emberroot: { id: 'emberroot', name: 'Emberroot', kind: 'herb', tier: 3, color: '#ff9a3c', skill: 140 },
  sunsteel: { id: 'sunsteel', name: 'Sunsteel Ore', kind: 'ore', tier: 3, color: '#ffd84a', skill: 140 },
};

export const MAP_MATERIALS: Record<MapId, [MaterialId, MaterialId]> = {
  vale: ['sunleaf', 'copper'],
  frostmarch: ['frostbloom', 'silver'],
  sunscar: ['emberroot', 'sunsteel'],
};

export const NODES_PER_MATERIAL = 14;
export const NODE_RESPAWN = 60;
export const MAX_GATHER_SKILL = 225;

// ---------------- Crafting ----------------

export type RecipeId = 'draught' | 'mightElixir' | 'stoneElixir' | 'reforge';

export interface RecipeDef {
  id: RecipeId;
  name: string;
  desc: string;
  herbs: number;
  ores: number;
  gold: number;
}

export const RECIPES: RecipeDef[] = [
  { id: 'draught', name: 'Healing Draught x2', desc: 'Two healing draughts.', herbs: 2, ores: 0, gold: 0 },
  { id: 'mightElixir', name: 'Elixir of Might', desc: '+12% power for 15 minutes.', herbs: 3, ores: 1, gold: 5 },
  { id: 'stoneElixir', name: 'Elixir of Stone', desc: '+15% armor and +8% health for 15 minutes.', herbs: 1, ores: 3, gold: 5 },
  { id: 'reforge', name: 'Reforge an item', desc: 'Raise an equipped item by 2 item levels (up to +10).', herbs: 0, ores: 4, gold: 20 },
];

export const ELIXIR_MINUTES = 15;
export const MAX_REFORGE = 5;

// ---------------- Factions ----------------

export interface FactionDef { id: string; map: MapId; name: string; color: string }

export const FACTIONS: Record<MapId, FactionDef> = {
  vale: { id: 'wardens', map: 'vale', name: 'Wardens of Hearthmoor', color: '#6aa0ff' },
  frostmarch: { id: 'watch', map: 'frostmarch', name: 'Emberhold Watch', color: '#bfefff' },
  sunscar: { id: 'mirel', map: 'sunscar', name: 'Keepers of Mirel', color: '#ffd84a' },
};

export const REP_RANKS = [
  { name: 'Neutral', at: 0, discount: 0 },
  { name: 'Friendly', at: 1000, discount: 0.05 },
  { name: 'Honored', at: 3000, discount: 0.1 },
  { name: 'Revered', at: 6000, discount: 0.15 },
  { name: 'Exalted', at: 10000, discount: 0.2 },
] as const;

export const REP_PER_QUEST = 350;
export const REP_PER_KILL = 6;
export const REP_PER_BOUNTY = 250;
export const REP_PER_DUNGEON = 600;

// ---------------- Daily bounties ----------------

export interface BountyTemplate { kind: 'kill' | 'gather'; mob?: MobKind; count: number }

/** Each map offers three bounties a day from its pool (picked by date). */
export const BOUNTY_POOL: Record<MapId, BountyTemplate[]> = {
  vale: [
    { kind: 'kill', mob: 'wolf', count: 10 }, { kind: 'kill', mob: 'spider', count: 10 }, { kind: 'kill', mob: 'raider', count: 10 },
    { kind: 'kill', mob: 'brute', count: 8 }, { kind: 'gather', count: 8 },
  ],
  frostmarch: [
    { kind: 'kill', mob: 'rimewolf', count: 12 }, { kind: 'kill', mob: 'frostling', count: 12 }, { kind: 'kill', mob: 'yeti', count: 10 },
    { kind: 'kill', mob: 'drowned', count: 10 }, { kind: 'gather', count: 10 },
  ],
  sunscar: [
    { kind: 'kill', mob: 'scorpion', count: 12 }, { kind: 'kill', mob: 'nomad', count: 12 }, { kind: 'kill', mob: 'golem', count: 10 },
    { kind: 'kill', mob: 'cultist', count: 10 }, { kind: 'gather', count: 12 },
  ],
};

// ---------------- Achievements ----------------

export type StatKey = 'kills' | 'eliteKills' | 'bosses' | 'quests' | 'gathered' | 'crafted' | 'bounties' | 'dungeons' | 'deaths' | 'legendaries';

export interface AchievementDef {
  id: string;
  name: string;
  desc: string;
  points: number;
  title?: string;
  /** Checked against the hero's progress. */
  check: { stat: StatKey; atLeast: number } | { level: number } | { quest: string } | { exalted: number } | { spec: true };
}

export const ACHIEVEMENTS: AchievementDef[] = [
  { id: 'lvl10', name: 'Blooded', desc: 'Reach level 10.', points: 10, check: { level: 10 } },
  { id: 'lvl20', name: 'Seasoned', desc: 'Reach level 20.', points: 10, check: { level: 20 } },
  { id: 'lvl30', name: 'Legend of the Vale', desc: 'Reach level 30.', points: 25, title: 'the Legendary', check: { level: 30 } },
  { id: 'path', name: 'A Path Chosen', desc: 'Choose a specialization.', points: 5, check: { spec: true } },
  { id: 'kill100', name: 'Monster Slayer', desc: 'Defeat 100 enemies.', points: 10, check: { stat: 'kills', atLeast: 100 } },
  { id: 'kill1000', name: 'Scourge of the Wilds', desc: 'Defeat 1000 enemies.', points: 25, title: 'the Relentless', check: { stat: 'kills', atLeast: 1000 } },
  { id: 'elite3', name: 'Chieftain Breaker', desc: 'Defeat 3 elite enemies.', points: 10, check: { stat: 'eliteKills', atLeast: 3 } },
  { id: 'act1', name: 'Hero of the Vale', desc: 'Defeat Varkul the Cindermaw.', points: 15, title: 'of the Vale', check: { quest: 'cindermaw' } },
  { id: 'act2', name: 'Winterbreaker', desc: 'Defeat Ysolde, the Rime Queen.', points: 20, title: 'the Winterbreaker', check: { quest: 'fm_queen' } },
  { id: 'act3', name: 'Sunflayer\'s Bane', desc: 'Defeat Azhkar the Sunflayer.', points: 30, title: 'the Sunbane', check: { quest: 'ss_azhkar' } },
  { id: 'quest20', name: 'Helping Hand', desc: 'Complete 20 quests.', points: 10, check: { stat: 'quests', atLeast: 20 } },
  { id: 'gather50', name: 'Green Thumb', desc: 'Gather 50 materials.', points: 10, check: { stat: 'gathered', atLeast: 50 } },
  { id: 'gather250', name: 'Master Gatherer', desc: 'Gather 250 materials.', points: 20, title: 'the Forager', check: { stat: 'gathered', atLeast: 250 } },
  { id: 'craft20', name: 'Artisan', desc: 'Craft 20 times.', points: 10, title: 'the Artisan', check: { stat: 'crafted', atLeast: 20 } },
  { id: 'bounty10', name: 'Bounty Hunter', desc: 'Complete 10 daily bounties.', points: 15, check: { stat: 'bounties', atLeast: 10 } },
  { id: 'dungeon1', name: 'Delver', desc: 'Clear a dungeon.', points: 10, check: { stat: 'dungeons', atLeast: 1 } },
  { id: 'dungeon3', name: 'Into Every Dark', desc: 'Clear 3 dungeons.', points: 20, title: 'the Delver', check: { stat: 'dungeons', atLeast: 3 } },
  { id: 'legend', name: 'Legendary!', desc: 'Obtain a legendary item.', points: 15, check: { stat: 'legendaries', atLeast: 1 } },
  { id: 'exalted', name: 'Beloved', desc: 'Reach Exalted with any faction.', points: 25, title: 'the Beloved', check: { exalted: 1 } },
];
