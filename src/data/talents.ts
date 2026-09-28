// Talent trees. Each class has a core tree (from level 2) and each specialization its own tree
// (after choosing it at level 10). One talent point per level from level 2.
import type { AbilityId, ClassId, SpecId } from './classes';

export type StatMod =
  | 'powerPct' | 'hpPct' | 'armorPct' | 'critPct' | 'regenPct' | 'healPct' | 'dotPct' | 'petPct' | 'drPct' | 'speedPct';

export type Mod =
  | { k: StatMod; v: number }
  | { k: 'ability'; id: AbilityId; dmgPct?: number; cdPct?: number; costPct?: number };

export interface TalentNode {
  id: string;
  name: string;
  desc: string;
  maxRank: number;
  /** 1: always, 2: needs 5 points in this tree, 3: needs 10 points in this tree. */
  tier: 1 | 2 | 3;
  perRank: Mod[];
}

export interface TalentTree {
  id: ClassId | SpecId;
  name: string;
  nodes: TalentNode[];
}

export const TIER_POINTS = { 1: 0, 2: 5, 3: 10 } as const;

const ab = (id: AbilityId, dmgPct: number, cdPct = 0, costPct = 0): Mod => ({ k: 'ability', id, dmgPct, cdPct, costPct });
const node = (id: string, name: string, desc: string, maxRank: number, tier: 1 | 2 | 3, perRank: Mod[]): TalentNode => ({ id, name, desc, maxRank, tier, perRank });

export const TREES: Record<ClassId | SpecId, TalentTree> = {
  // ---------------- Core trees ----------------
  stormblade: {
    id: 'stormblade', name: 'Stormblade', nodes: [
      node('sb_might', 'Storm Might', '+3% power per rank.', 5, 1, [{ k: 'powerPct', v: 3 }]),
      node('sb_iron', 'Iron Hide', '+4% health per rank.', 5, 1, [{ k: 'hpPct', v: 4 }]),
      node('sb_cleave', 'Cleave Mastery', 'Thunder Cleave: +12% damage and 8% shorter cooldown per rank.', 3, 1, [ab('thunderCleave', 12, 8)]),
      node('sb_leap', 'High Leap', 'Skyward Leap: +15% damage and 10% shorter cooldown per rank.', 3, 2, [ab('skywardLeap', 15, 10)]),
      node('sb_tempest', 'Eye of the Storm', 'Tempest: +15% damage and 8% shorter cooldown per rank.', 3, 2, [ab('tempest', 15, 8)]),
      node('sb_guard', 'Deep Charge', 'Static Guard absorbs 20% more per rank.', 3, 3, [ab('staticGuard', 20)]),
    ],
  },
  emberseer: {
    id: 'emberseer', name: 'Emberseer', nodes: [
      node('es_focus', 'Kindled Focus', '+3% power per rank.', 5, 1, [{ k: 'powerPct', v: 3 }]),
      node('es_lance', 'Lance Mastery', 'Cinder Lance: +8% damage per rank.', 5, 1, [ab('cinderLance', 8)]),
      node('es_ward', 'Ember Ward', '+5% health and +8% armor per rank.', 3, 1, [{ k: 'hpPct', v: 5 }, { k: 'armorPct', v: 8 }]),
      node('es_clarity', 'Clarity', '+12% mana regeneration per rank.', 3, 2, [{ k: 'regenPct', v: 12 }]),
      node('es_star', 'Starfall', 'Falling Star: +15% damage and 8% shorter cooldown per rank.', 3, 2, [ab('meteor', 15, 8)]),
      node('es_veil', 'Phoenix Rising', 'Phoenix Veil heals 25% more and recharges 10% faster per rank.', 3, 3, [ab('phoenixVeil', 25, 10)]),
    ],
  },
  thornkeeper: {
    id: 'thornkeeper', name: 'Thornkeeper', nodes: [
      node('tk_wild', 'Wild Heart', '+3% power per rank.', 5, 1, [{ k: 'powerPct', v: 3 }]),
      node('tk_bark', 'Thick Bark', '+4% health per rank.', 5, 1, [{ k: 'hpPct', v: 4 }]),
      node('tk_snare', 'Bramble Mastery', 'Bramble Snare: +12% damage per rank.', 3, 1, [ab('brambleSnare', 12)]),
      node('tk_growth', 'Verdant Growth', 'All healing +10% per rank.', 3, 2, [{ k: 'healPct', v: 10 }]),
      node('tk_pack', 'Spirit Bond', 'Companions deal and take 15% more / less per rank.', 3, 2, [{ k: 'petPct', v: 15 }]),
      node('tk_bloom', 'Deep Roots', 'Wild Bloom: +20% damage and 10% shorter cooldown per rank.', 3, 3, [ab('wildBloom', 20, 10)]),
    ],
  },
  // ---------------- Stormblade specs ----------------
  tempestKnight: {
    id: 'tempestKnight', name: 'Tempest Knight', nodes: [
      node('tk2_chain', 'Forked Lightning', 'Chain Lightning: +10% damage per rank.', 5, 1, [ab('chainLightning', 10)]),
      node('tk2_conduct', 'Conductor', '+2% critical chance per rank.', 3, 1, [{ k: 'critPct', v: 2 }]),
      node('tk2_heart', 'Storm Heart', '+4% power per rank.', 3, 1, [{ k: 'powerPct', v: 4 }]),
      node('tk2_strike', 'Skyfire', 'Thunderstrike: +12% damage and 8% shorter cooldown per rank.', 3, 2, [ab('thunderstrike', 12, 8)]),
      node('tk2_tempest', 'Overcharge', 'Tempest: +20% damage per rank.', 3, 2, [ab('tempest', 20)]),
      node('tk2_cap', 'Living Storm', 'All lightning abilities cost 25% less and +10% power.', 1, 3, [ab('chainLightning', 0, 0, 25), ab('thunderstrike', 0, 0, 25), ab('tempest', 0, 0, 25), { k: 'powerPct', v: 10 }]),
    ],
  },
  bulwark: {
    id: 'bulwark', name: 'Bulwark', nodes: [
      node('bw_wall', 'Unyielding', 'Shield Wall lasts longer: 10% shorter cooldown per rank.', 3, 1, [ab('shieldWall', 0, 10)]),
      node('bw_hide', 'Mountain Hide', '+6% health per rank.', 5, 1, [{ k: 'hpPct', v: 6 }]),
      node('bw_plate', 'Reinforced Plate', '+10% armor per rank.', 3, 1, [{ k: 'armorPct', v: 10 }]),
      node('bw_shatter', 'Fault Line', 'Earthshatter: +15% damage and 8% shorter cooldown per rank.', 3, 2, [ab('earthshatter', 15, 8)]),
      node('bw_resolve', 'Resolve', 'Take 3% less damage per rank.', 3, 2, [{ k: 'drPct', v: 3 }]),
      node('bw_cap', 'Bastion', 'Static Guard absorbs 60% more and Thunder Cleave deals 30% more.', 1, 3, [ab('staticGuard', 60), ab('thunderCleave', 30)]),
    ],
  },
  spellblade: {
    id: 'spellblade', name: 'Spellblade', nodes: [
      node('sp_rune', 'Deep Runes', 'Rune Strike: +10% damage per rank.', 5, 1, [ab('runeStrike', 10)]),
      node('sp_edge', 'Keen Edge', '+2% critical chance per rank.', 3, 1, [{ k: 'critPct', v: 2 }]),
      node('sp_burn', 'Searing Steel', 'Burns and other damage over time +10% per rank.', 3, 1, [{ k: 'dotPct', v: 10 }]),
      node('sp_arc', 'Flame Dancer', 'Blazing Arc: +15% damage and 10% shorter cooldown per rank.', 3, 2, [ab('blazingArc', 15, 10)]),
      node('sp_might', 'Twin Paths', '+4% power and +3% health per rank.', 3, 2, [{ k: 'powerPct', v: 4 }, { k: 'hpPct', v: 3 }]),
      node('sp_cap', 'Runic Storm', 'Thunder Cleave and Tempest deal 25% more damage.', 1, 3, [ab('thunderCleave', 25), ab('tempest', 25)]),
    ],
  },
  // ---------------- Emberseer specs ----------------
  pyromancer: {
    id: 'pyromancer', name: 'Pyromancer', nodes: [
      node('py_inferno', 'Wildfire', 'Inferno: +10% damage per rank.', 5, 1, [ab('inferno', 10)]),
      node('py_burn', 'Everburn', 'Damage over time +8% per rank.', 3, 1, [{ k: 'dotPct', v: 8 }]),
      node('py_heat', 'Heat Wave', '+4% power per rank.', 3, 1, [{ k: 'powerPct', v: 4 }]),
      node('py_combust', 'Chain Reaction', 'Combust: +15% damage and 8% shorter cooldown per rank.', 3, 2, [ab('combust', 15, 8)]),
      node('py_lance', 'Blue Flame', 'Cinder Lance: +15% damage per rank.', 3, 2, [ab('cinderLance', 15)]),
      node('py_cap', 'Sunheart', 'Falling Star recharges 40% faster and deals 30% more.', 1, 3, [ab('meteor', 30, 40)]),
    ],
  },
  frostweaver: {
    id: 'frostweaver', name: 'Frostweaver', nodes: [
      node('fw_spike', 'Razor Ice', 'Glacial Spike: +10% damage per rank.', 5, 1, [ab('glacialSpike', 10)]),
      node('fw_shell', 'Frost Shell', '+6% health and +10% armor per rank.', 3, 1, [{ k: 'hpPct', v: 6 }, { k: 'armorPct', v: 10 }]),
      node('fw_crit', 'Shatter', '+2% critical chance per rank.', 3, 1, [{ k: 'critPct', v: 2 }]),
      node('fw_blizzard', 'Whiteout', 'Blizzard: +15% damage and 8% shorter cooldown per rank.', 3, 2, [ab('blizzard', 15, 8)]),
      node('fw_power', 'Cold Mind', '+4% power per rank.', 3, 2, [{ k: 'powerPct', v: 4 }]),
      node('fw_cap', 'Winter Heart', 'Glacial Spike recharges 30% faster and costs 30% less.', 1, 3, [ab('glacialSpike', 0, 30, 30)]),
    ],
  },
  thermancer: {
    id: 'thermancer', name: 'Thermancer', nodes: [
      node('th_bolt', 'Twin Flames', 'Frostfire Bolt: +10% damage per rank.', 5, 1, [ab('frostfireBolt', 10)]),
      node('th_power', 'Balance', '+4% power per rank.', 3, 1, [{ k: 'powerPct', v: 4 }]),
      node('th_dot', 'Lingering Heat', 'Damage over time +8% per rank.', 3, 1, [{ k: 'dotPct', v: 8 }]),
      node('th_shock', 'Fracture', 'Thermal Shock: +15% damage and 8% shorter cooldown per rank.', 3, 2, [ab('thermalShock', 15, 8)]),
      node('th_crit', 'Brittle', '+2% critical chance per rank.', 3, 2, [{ k: 'critPct', v: 2 }]),
      node('th_cap', 'Equilibrium', 'Ash Ring and Falling Star deal 40% more damage.', 1, 3, [ab('ashRing', 40), ab('meteor', 40)]),
    ],
  },
  // ---------------- Thornkeeper specs ----------------
  grovewarden: {
    id: 'grovewarden', name: 'Grovewarden', nodes: [
      node('gw_bloom', 'Full Bloom', 'Lifebloom heals 10% more per rank.', 5, 1, [ab('lifebloom', 10)]),
      node('gw_heal', 'Sap of Life', 'All healing +8% per rank.', 3, 1, [{ k: 'healPct', v: 8 }]),
      node('gw_hide', 'Heartwood', '+5% health per rank.', 3, 1, [{ k: 'hpPct', v: 5 }]),
      node('gw_bark', 'Ironbark', 'Barkskin: 10% shorter cooldown per rank.', 3, 2, [ab('barkskin', 0, 10)]),
      node('gw_thorn', 'Thorn Lash', 'Bramble Snare and Wild Bloom +15% damage per rank.', 3, 2, [ab('brambleSnare', 15), ab('wildBloom', 15)]),
      node('gw_cap', 'Evergreen', 'Take 10% less damage and Renewal heals 50% more.', 1, 3, [{ k: 'drPct', v: 10 }, ab('renewal', 50)]),
    ],
  },
  beastcaller: {
    id: 'beastcaller', name: 'Beastcaller', nodes: [
      node('bc_fang', 'Sharp Fangs', 'Companions deal 10% more damage per rank.', 5, 1, [{ k: 'petPct', v: 10 }]),
      node('bc_alpha', 'Alpha Bond', 'Pack Alpha: 10% shorter cooldown per rank.', 3, 1, [ab('packAlpha', 0, 10)]),
      node('bc_wolf', 'Wolf Brother', 'Spirit Wolf: 15% shorter cooldown per rank.', 3, 1, [ab('spiritWolf', 0, 15)]),
      node('bc_call', 'Wild Howl', 'Feral Call: 12% shorter cooldown per rank.', 3, 2, [ab('feralCall', 0, 12)]),
      node('bc_hide', 'Pack Hide', '+5% health per rank.', 3, 2, [{ k: 'hpPct', v: 5 }]),
      node('bc_cap', 'Lord of Beasts', 'Companions deal 30% more damage and you gain +10% power.', 1, 3, [{ k: 'petPct', v: 30 }, { k: 'powerPct', v: 10 }]),
    ],
  },
  rotbloom: {
    id: 'rotbloom', name: 'Rotbloom', nodes: [
      node('rb_plague', 'Virulence', 'Rot Plague: +10% damage per rank.', 5, 1, [ab('plague', 10)]),
      node('rb_dot', 'Deep Rot', 'Damage over time +8% per rank.', 3, 1, [{ k: 'dotPct', v: 8 }]),
      node('rb_power', 'Blight', '+4% power per rank.', 3, 1, [{ k: 'powerPct', v: 4 }]),
      node('rb_burst', 'Overripe', 'Wither Burst: +15% damage and 8% shorter cooldown per rank.', 3, 2, [ab('witherBurst', 15, 8)]),
      node('rb_snare', 'Venom Thorns', 'Bramble Snare: +20% damage per rank.', 3, 2, [ab('brambleSnare', 20)]),
      node('rb_cap', 'Pestilence', 'Wild Bloom recharges 40% faster and deals 30% more.', 1, 3, [ab('wildBloom', 30, 40)]),
    ],
  },
};

export const NODE_BY_ID: Record<string, { node: TalentNode; tree: TalentTree }> = {};
for (const tree of Object.values(TREES)) for (const n of tree.nodes) NODE_BY_ID[n.id] = { node: n, tree };
