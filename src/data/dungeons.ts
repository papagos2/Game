// Instanced group dungeons: one per act, fought with AI companions who fill the missing roles.
import type { DungeonId, MapDef, MapId, WorldId } from './world';
import { MAPS } from './world';

const canyonRoad: [number, number][] = [[0, 150], [0, 95], [-18, 35], [10, -35], [0, -100], [30, -172]];

function dungeon(
  id: DungeonId, act: MapId, name: string, minLevel: number, blurb: string,
  mobs: { a: string; b: string; mini: string; boss: string },
  look: { ground: [string, string]; fog: string; sky: string; glow: string; rock: string; flora: MapDef['theme']['flora'] },
): MapDef {
  const k = mobs as Record<string, MapDef['camps'][number]['kind']>;
  return {
    id, act, name, subtitle: 'Dungeon', levels: [minLevel, minLevel + 3], spawn: { x: 0, z: 139 }, finalQuest: '', next: null,
    dungeon: { minLevel, finalBoss: k.boss, blurb },
    zones: [
      { id: `${id}_gate`, name: `${name}: Entrance`, x: 0, z: 150, radius: 30, ground: look.ground[0], groundAlt: look.ground[1] },
      { id: `${id}_depths`, name: `${name}: Depths`, x: 0, z: 30, radius: 80, ground: look.ground[0], groundAlt: look.ground[1] },
      { id: `${id}_sanctum`, name: `${name}: Sanctum`, x: 30, z: -172, radius: 30, ground: look.ground[1], groundAlt: look.ground[0] },
    ],
    camps: [
      { kind: k.a, x: 0, z: 112, count: 3, spread: 6 },
      { kind: k.b, x: 0, z: 78, count: 3, spread: 6 },
      { kind: k.a, x: -18, z: 40, count: 4, spread: 7 },
      { kind: k.b, x: -8, z: 5, count: 3, spread: 6 },
      { kind: k.a, x: 6, z: -25, count: 3, spread: 6 },
      { kind: k.b, x: 10, z: -55, count: 4, spread: 7 },
      { kind: k.mini, x: 0, z: -100, count: 1, spread: 0 },
      { kind: k.boss, x: 30, z: -172, count: 1, spread: 0 },
    ],
    npcs: [{ id: `${id}_exit`, name: 'Waystone Keeper', title: 'Wayfinder', x: 8, z: 146, color: '#4a6a8a', travel: true }],
    roads: [canyonRoad],
    lake: null,
    theme: {
      ...MAPS[act].theme,
      meadow: look.ground, road: look.rock, rock: look.rock, fog: look.fog, skyTop: look.sky, glow: look.glow,
      hazard: undefined, heightScale: 0.6, canyon: true, flora: look.flora,
    },
  };
}

export const DUNGEONS: Record<DungeonId, MapDef> = {
  warrens: dungeon('warrens', 'vale', 'The Smoldering Warrens', 8,
    'Beneath the Scar, the Cindermaw\'s brood still burns. Clear the warrens and face Ignara, Mother of Cinders.',
    { a: 'magmaHound', b: 'emberfiend', mini: 'forgemaster', boss: 'ignara' },
    { ground: ['#3a2a26', '#4a322a'], fog: '#3a2420', sky: '#1a1010', glow: '#ff6a1a', rock: '#2e2624',
      flora: [{ kind: 'spire', count: 220, colors: ['#2b2523', '#3a302b'] }, { kind: 'ember', count: 120 }, { kind: 'rock', count: 160, colors: ['#4a3a34'] }] }),
  drownedHalls: dungeon('drownedHalls', 'frostmarch', 'Halls of the Drowned King', 18,
    'Under the frozen Mere lies a sunken court. Its king never stopped ruling. Break his guard and end his reign.',
    { a: 'drownedGuard', b: 'iceWraith', mini: 'captainVeyl', boss: 'drownedKing' },
    { ground: ['#6a8a98', '#7a9aa8'], fog: '#5a7a8a', sky: '#1a2a3a', glow: '#6affd0', rock: '#4a5a66',
      flora: [{ kind: 'iceshard', count: 220 }, { kind: 'dead', count: 80 }, { kind: 'rock', count: 160, colors: ['#5a6a76'] }] }),
  sunTomb: dungeon('sunTomb', 'sunscar', 'Tomb of the First Sun', 28,
    'The zealots opened a tomb older than Mirel. Something in it is waking. Descend and put it back to sleep.',
    { a: 'tombScarab', b: 'sunPriest', mini: 'colossus', boss: 'nehkt' },
    { ground: ['#b89868', '#c8a878'], fog: '#c8a060', sky: '#6a4a2a', glow: '#ffd84a', rock: '#8a6a4a',
      flora: [{ kind: 'obelisk', count: 120 }, { kind: 'rock', count: 200, colors: ['#a08060', '#8a6a4a'] }, { kind: 'dune', count: 60 }] }),
};

export const DUNGEON_ORDER: DungeonId[] = ['warrens', 'drownedHalls', 'sunTomb'];

export function worldDef(id: WorldId): MapDef {
  return (MAPS as Record<string, MapDef>)[id] ?? DUNGEONS[id as DungeonId];
}

export function isDungeon(id: WorldId): id is DungeonId {
  return id in DUNGEONS;
}

/** Companion roles picked to complete the party around the player's own role. */
export type Role = 'tank' | 'healer' | 'dps';

export const COMPANIONS: Record<Role, { name: string; title: string; color: string }> = {
  tank: { name: 'Brannoc Stonewall', title: 'Shieldbearer', color: '#5a6a8a' },
  healer: { name: 'Sister Yevra', title: 'Lightmender', color: '#e8e0c8' },
  dps: { name: 'Kestrel Vane', title: 'Ranger', color: '#4a6a3a' },
};
