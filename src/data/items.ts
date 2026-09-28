// Equipment slots, name tables and the named legendary items dropped by bosses.
import type { ClassId } from './classes';

export type ItemType = 'weapon' | 'head' | 'chest' | 'hands' | 'feet' | 'amulet' | 'ring';
export type GearSlot = 'weapon' | 'head' | 'chest' | 'hands' | 'feet' | 'amulet' | 'ring1' | 'ring2';

export const GEAR_SLOTS: GearSlot[] = ['weapon', 'head', 'chest', 'hands', 'feet', 'amulet', 'ring1', 'ring2'];
export const ITEM_TYPES: ItemType[] = ['weapon', 'head', 'chest', 'hands', 'feet', 'amulet', 'ring'];

export const SLOT_LABEL: Record<GearSlot, string> = {
  weapon: 'Weapon', head: 'Head', chest: 'Chest', hands: 'Hands', feet: 'Feet', amulet: 'Amulet', ring1: 'Ring', ring2: 'Ring',
};

export const TYPE_LABEL: Record<ItemType, string> = {
  weapon: 'Weapon', head: 'Head', chest: 'Chest', hands: 'Hands', feet: 'Feet', amulet: 'Amulet', ring: 'Ring',
};

/** How much of the stat budget each item type gets. */
export const TYPE_WEIGHT: Record<ItemType, number> = {
  weapon: 1, chest: 0.8, head: 0.6, hands: 0.5, feet: 0.5, amulet: 0.6, ring: 0.5,
};

/** Relative chance of each type dropping. */
export const TYPE_DROP_WEIGHT: Record<ItemType, number> = {
  weapon: 1, head: 1, chest: 1, hands: 1, feet: 1, amulet: 0.7, ring: 1.2,
};

export const WEAPON_NAMES: Record<ClassId, string[]> = {
  stormblade: ['Blade', 'Greatsword', 'Cleaver', 'Warblade', 'Longsword'],
  emberseer: ['Staff', 'Rod', 'Sceptre', 'Wand', 'Spire'],
  thornkeeper: ['Staff', 'Crook', 'Branch', 'Totem', 'Warstaff'],
};

/** Armor names by class armor type, tier (by item level band) and slot. */
export const ARMOR_NAMES: Record<'plate' | 'cloth' | 'leather', Record<'head' | 'chest' | 'hands' | 'feet', string[]>> = {
  plate: { head: ['Helm', 'Greathelm'], chest: ['Hauberk', 'Breastplate', 'Cuirass'], hands: ['Gauntlets'], feet: ['Sabatons', 'Greaves'] },
  cloth: { head: ['Hood', 'Circlet', 'Cowl'], chest: ['Robe', 'Vestments', 'Mantle'], hands: ['Gloves', 'Wraps'], feet: ['Slippers', 'Sandals'] },
  leather: { head: ['Cap', 'Mask', 'Headdress'], chest: ['Jerkin', 'Tunic', 'Vest'], hands: ['Grips', 'Mitts'], feet: ['Boots', 'Treads'] },
};

export const MATERIALS: Record<'plate' | 'cloth' | 'leather', [string, string, string]> = {
  plate: ['Iron', 'Frostforged', 'Sunsteel'],
  cloth: ['Linen', 'Rimesilk', 'Starweave'],
  leather: ['Hide', 'Wyrmhide', 'Dunestrider'],
};

export const JEWEL_NAMES: Record<'amulet' | 'ring', string[]> = {
  amulet: ['Amulet', 'Pendant', 'Talisman', 'Choker'],
  ring: ['Ring', 'Band', 'Signet', 'Loop'],
};

export const GEMS = ['Garnet', 'Moonstone', 'Jade', 'Amber', 'Sapphire', 'Opal'];

export const PREFIXES: string[][] = [
  ['Worn', 'Plain', 'Rough', 'Simple'],
  ['Sturdy', 'Keen', "Warden's", "Hunter's"],
  ['Stormbound', 'Emberwrought', 'Thornbound', 'Duskwoven'],
  ['Heartfire', 'Sunward', 'Ashenveil', 'Starlit'],
  [],
];

export const RARITY_NAMES = ['Common', 'Uncommon', 'Rare', 'Epic', 'Legendary'] as const;
export const RARITY_COLORS = ['#e8e4dc', '#4fd46b', '#4a9dff', '#c76bff', '#ff9d2a'] as const;
export const RARITY_MUL = [1, 1.25, 1.55, 1.9, 2.3];

export interface LegendaryDef {
  name: string;
  type: ItemType;
  /** Extra flat stats on top of the normal budget. */
  bonus: { power?: number; stamina?: number; armor?: number; crit?: number };
}

/** Each boss drops one of its legendaries (picked at random). */
export const LEGENDARIES: Record<string, LegendaryDef[]> = {
  boss: [
    { name: "Cindermaw's Heart", type: 'amulet', bonus: { power: 6, crit: 6 } },
    { name: 'Emberfang', type: 'weapon', bonus: { power: 8 } },
    { name: 'Scarwalker Treads', type: 'feet', bonus: { stamina: 8, armor: 10 } },
  ],
  ysolde: [
    { name: 'Crown of the Rime Queen', type: 'head', bonus: { stamina: 14, crit: 6 } },
    { name: 'Winterkiss', type: 'ring', bonus: { power: 10, crit: 8 } },
    { name: 'Glacierheart', type: 'weapon', bonus: { power: 16 } },
  ],
  azhkar: [
    { name: 'The Sunflayer', type: 'weapon', bonus: { power: 26, crit: 8 } },
    { name: 'Mantle of the Dune Tyrant', type: 'chest', bonus: { stamina: 24, armor: 40 } },
    { name: 'Eye of Azhkar', type: 'amulet', bonus: { power: 16, crit: 12 } },
  ],
};
