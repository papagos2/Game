import type { MapId, MobKind } from './world';

export type Objective =
  | { type: 'kill'; mob: MobKind; count: number; label: string }
  | { type: 'collect'; item: string; count: number; label: string }
  | { type: 'explore'; zone: string; count: 1; label: string };

export interface QuestDef {
  id: string;
  map: MapId;
  title: string;
  giver: string;
  /** NPC to hand the quest in to (defaults to giver). */
  turnIn?: string;
  requires: string[];
  minLevel: number;
  objective: Objective;
  offer: string;
  progress: string;
  complete: string;
  xp: number;
  gold: number;
  /** Item level of the guaranteed reward item (0 = none). */
  rewardItemLevel: number;
  rewardRarity?: 1 | 2 | 3;
}

export const QUEST_ITEMS: Record<string, string> = {
  pelt: 'Dusk Wolf Pelt',
  relic: 'Sunward Relic',
  frostcore: 'Frost Core',
  thanecrown: 'Crown of Hrodric',
  venomsac: 'Stinger Venom Sac',
  sunsigil: 'Sigil of the Sun',
};

export const QUESTS: QuestDef[] = [
  // ======================= Act I: Vale of Ashenveil =======================
  {
    id: 'wolves', map: 'vale', title: 'Wolves at the Gate', giver: 'vale_a', requires: [], minLevel: 1,
    objective: { type: 'kill', mob: 'wolf', count: 8, label: 'Dusk Wolves slain' },
    offer: 'Stranger, you came just in time. The Dusk Wolves have grown bold since the Scar began to smoulder. They took two of our sheep last night. Thin their pack in the glade west of the village.',
    progress: 'The wolves still prowl the glade. Head west along the road.',
    complete: 'Good work. The night watch will sleep easier. You fight like one of the old Wardens.',
    xp: 170, gold: 8, rewardItemLevel: 2,
  },
  {
    id: 'pelts', map: 'vale', title: 'Pelts for the Tanner', giver: 'vale_b', requires: [], minLevel: 1,
    objective: { type: 'collect', item: 'pelt', count: 6, label: 'Dusk Wolf Pelts' },
    offer: 'Winter comes early this close to the Scar. Bring me six pelts from the Dusk Wolves and I will stitch you something that turns a blade.',
    progress: 'Six pelts, friend. The wolves in Duskwood Glade carry fine coats.',
    complete: 'Fine pelts, these. Here, take this. Wear it well.',
    xp: 150, gold: 6, rewardItemLevel: 3,
  },
  {
    id: 'stalkers', map: 'vale', title: 'Silk and Venom', giver: 'vale_c', requires: ['wolves'], minLevel: 2,
    objective: { type: 'kill', mob: 'spider', count: 8, label: 'Glade Stalkers slain' },
    offer: 'The stalkers of Webhollow have spread their nests to the herb fields. I cannot gather a single root. Cull them, north-west past the glade, and I will brew you something special.',
    progress: 'Webhollow lies north of Duskwood Glade. Watch for the green venom.',
    complete: 'The fields are safe again. You have my thanks, and this.',
    xp: 300, gold: 12, rewardItemLevel: 4,
  },
  {
    id: 'raiders', map: 'vale', title: 'The Hollowkin Threat', giver: 'vale_a', requires: ['wolves'], minLevel: 3,
    objective: { type: 'kill', mob: 'raider', count: 10, label: 'Hollowkin Raiders slain' },
    offer: 'Hollowkin raiders have made camp to the east. They burned the Tallow farm and carried off supplies. Drive them back before they grow any bolder.',
    progress: 'The Hollowkin camp lies east, beyond the river road.',
    complete: 'That will make them think twice. But their chieftain still holds something of ours.',
    xp: 480, gold: 18, rewardItemLevel: 5,
  },
  {
    id: 'relic', map: 'vale', title: 'The Stolen Relic', giver: 'vale_a', requires: ['raiders'], minLevel: 5,
    objective: { type: 'collect', item: 'relic', count: 1, label: 'Sunward Relic recovered' },
    offer: 'Chieftain Gorran took the Sunward Relic from our shrine. Without it the wards around Hearthmoor are failing. He stands at the heart of the camp. Bring it back.',
    progress: 'Gorran is strong. Make sure you are ready before you face him.',
    complete: 'The Relic! The wards are already brightening. There is one more thing I must ask of you.',
    xp: 620, gold: 30, rewardItemLevel: 7, rewardRarity: 2,
  },
  {
    id: 'brutes', map: 'vale', title: 'Into the Ashen Scar', giver: 'vale_a', requires: ['relic'], minLevel: 6,
    objective: { type: 'kill', mob: 'brute', count: 8, label: 'Ashbound Brutes slain' },
    offer: 'The Relic showed me the truth: something ancient is waking beneath the Ashen Scar to the north. Its Ashbound servants guard the way. Break their line.',
    progress: 'Head north. The ground itself burns in the Scar.',
    complete: 'You returned alive. Then there is hope. Rest, and prepare for the end of this.',
    xp: 760, gold: 30, rewardItemLevel: 8, rewardRarity: 2,
  },
  {
    id: 'cindermaw', map: 'vale', title: 'The Cindermaw', giver: 'vale_a', requires: ['brutes'], minLevel: 7,
    objective: { type: 'kill', mob: 'boss', count: 1, label: 'Varkul the Cindermaw defeated' },
    offer: 'Varkul the Cindermaw sleeps no longer. It lairs in the hollow at the far end of the Scar. When it strikes the ground, move out of the burning circle. Heal when you must. The whole vale is counting on you.',
    progress: "Cindermaw's Hollow is at the northern edge of the Scar. Step out of the fire.",
    complete: 'It is over. The Scar is cooling and the Vale is safe. But the Relic whispers of a cold that is coming from the north. Speak with Pell the Wayfinder when you are ready: the road to Frostmarch is open.',
    xp: 1200, gold: 100, rewardItemLevel: 10, rewardRarity: 3,
  },
  // ======================= Act II: Frostmarch =======================
  {
    id: 'fm_arrival', map: 'frostmarch', title: 'The Frozen Road', giver: 'fm_a', requires: [], minLevel: 10,
    objective: { type: 'explore', zone: 'rimewood', count: 1, label: 'Scout the Rimewood' },
    offer: 'So you are the one who felled the Cindermaw. Good. We need someone who does not freeze. Scout the Rimewood to the west and tell me what hunts there.',
    progress: 'The Rimewood is west of Emberhold, along the old road.',
    complete: 'Rimefang packs, then. Worse than I feared. Now that you have chosen your path, the frost will test it.',
    xp: 900, gold: 20, rewardItemLevel: 0,
  },
  {
    id: 'fm_wolves', map: 'frostmarch', title: 'Rimefang Culling', giver: 'fm_a', requires: ['fm_arrival'], minLevel: 10,
    objective: { type: 'kill', mob: 'rimewolf', count: 10, label: 'Rimefang Wolves slain' },
    offer: 'The Rimefangs run our supply sleds down. Their bite freezes the blood. Thin the packs in the Rimewood.',
    progress: 'Mind their bite. It slows you like ice water in the veins.',
    complete: 'The sleds are moving again. Emberhold eats tonight thanks to you.',
    xp: 1500, gold: 40, rewardItemLevel: 12,
  },
  {
    id: 'fm_frostlings', map: 'frostmarch', title: 'Things in the Deep', giver: 'fm_b', requires: ['fm_arrival'], minLevel: 11,
    objective: { type: 'kill', mob: 'frostling', count: 10, label: 'Frostling Skulkers slain' },
    offer: 'I have seen them in the ice: small, many, hungry. The frostlings crawl out of the Deep to the north-west. Stop them before they reach the town.',
    progress: 'Frostling Deep lies north of the Rimewood.',
    complete: 'The visions are quieter. Something still stirs beneath, but it is no longer close.',
    xp: 1700, gold: 45, rewardItemLevel: 13,
  },
  {
    id: 'fm_cores', map: 'frostmarch', title: 'Cores for the Forge', giver: 'fm_c', requires: ['fm_arrival'], minLevel: 11,
    objective: { type: 'collect', item: 'frostcore', count: 6, label: 'Frost Cores' },
    offer: 'Frost cores burn colder than any coal burns hot. With six of them I can temper steel that will not shatter. The frostlings carry them in their chests.',
    progress: 'Six cores. The frostlings of the Deep carry them.',
    complete: 'Beautiful. Listen to them hum. Here, this was tempered in the first one.',
    xp: 1600, gold: 40, rewardItemLevel: 14, rewardRarity: 2,
  },
  {
    id: 'fm_behemoths', map: 'frostmarch', title: 'The White Fells', giver: 'fm_a', requires: ['fm_wolves'], minLevel: 13,
    objective: { type: 'kill', mob: 'yeti', count: 8, label: 'Hoarfrost Behemoths slain' },
    offer: 'The behemoths of the White Fells have come down from the peaks and block the eastern pass. They hit like an avalanche. Watch the ground when they raise their fists.',
    progress: 'The Fells are east. Step away when they slam the ground.',
    complete: 'The pass is open. The Queen has noticed you now, I think.',
    xp: 2300, gold: 55, rewardItemLevel: 15,
  },
  {
    id: 'fm_drowned', map: 'frostmarch', title: 'The Drowned Wardens', giver: 'fm_b', requires: ['fm_frostlings'], minLevel: 15,
    objective: { type: 'kill', mob: 'drowned', count: 10, label: 'Drowned Wardens laid to rest' },
    offer: 'The old Wardens who drowned in the Mere walk again, bound by the Rime Queen. Give them rest. They were our people once.',
    progress: 'The Drowned Mere lies south-east of the Fells.',
    complete: 'They are at peace. And you are ready for what must come next.',
    xp: 2800, gold: 60, rewardItemLevel: 16, rewardRarity: 2,
  },
  {
    id: 'fm_thane', map: 'frostmarch', title: 'The Frostbound Thane', giver: 'fm_a', requires: ['fm_behemoths', 'fm_drowned'], minLevel: 16,
    objective: { type: 'collect', item: 'thanecrown', count: 1, label: 'Crown of Hrodric' },
    offer: 'Thane Hrodric was our lord before the Queen took him. He holds Thanehold in her name. His crown binds her power to this land. Take it.',
    progress: 'Thanehold lies north, on the road to the Rime Throne.',
    complete: 'The crown. Her hold on the land weakens. Now there is only her.',
    xp: 3300, gold: 80, rewardItemLevel: 18, rewardRarity: 2,
  },
  {
    id: 'fm_queen', map: 'frostmarch', title: 'The Rime Queen', giver: 'fm_a', requires: ['fm_thane'], minLevel: 17,
    objective: { type: 'kill', mob: 'ysolde', count: 1, label: 'Ysolde, the Rime Queen defeated' },
    offer: 'Ysolde waits on her throne of ice at the end of the northern road. Her frost falls in circles. Do not stand in them. End this winter.',
    progress: 'The Rime Throne is north of Thanehold.',
    complete: 'The ice is breaking on the Mere. Spring, after thirty years. Oren says the southern deserts burn with a strange sun. Your road goes on.',
    xp: 5000, gold: 200, rewardItemLevel: 20, rewardRarity: 3,
  },
  // ======================= Act III: Sunscar Dunes =======================
  {
    id: 'ss_arrival', map: 'sunscar', title: 'Into the Glare', giver: 'ss_a', requires: [], minLevel: 20,
    objective: { type: 'explore', zone: 'glassflats', count: 1, label: 'Reach the Glass Flats' },
    offer: 'Welcome to Mirel, Warden. The sun here is not the sun you know. It burns wrong since the zealots woke Azhkar. Go east to the Glass Flats and see what the heat has made.',
    progress: 'The Glass Flats are east. Carry water.',
    complete: 'Golems of melted sand. The sun is shaping servants now.',
    xp: 3000, gold: 60, rewardItemLevel: 0,
  },
  {
    id: 'ss_stingers', map: 'sunscar', title: 'Stingers in the Grass', giver: 'ss_a', requires: ['ss_arrival'], minLevel: 20,
    objective: { type: 'kill', mob: 'scorpion', count: 10, label: 'Dune Stingers slain' },
    offer: 'The stingers nest in the fields west of the oasis. Their poison has killed three of our water-bearers. Clear them out.',
    progress: 'The Sting Fields are west. Their venom lingers.',
    complete: 'The water-bearers can travel again. Mirel lives on its water.',
    xp: 4200, gold: 90, rewardItemLevel: 22,
  },
  {
    id: 'ss_venom', map: 'sunscar', title: 'The Antidote', giver: 'ss_c', requires: ['ss_arrival'], minLevel: 20,
    objective: { type: 'collect', item: 'venomsac', count: 6, label: 'Stinger Venom Sacs' },
    offer: 'Poison is also its own cure, if you know how. Bring me six venom sacs from the stingers and I will save the ones already bitten.',
    progress: 'Six sacs. Cut carefully.',
    complete: 'Three lives saved tonight. Take this. I found it in the sand years ago, waiting for someone like you.',
    xp: 4000, gold: 80, rewardItemLevel: 23, rewardRarity: 2,
  },
  {
    id: 'ss_nomads', map: 'sunscar', title: 'The Sandveil', giver: 'ss_b', requires: ['ss_stingers'], minLevel: 22,
    objective: { type: 'kill', mob: 'nomad', count: 12, label: 'Sandveil Raiders defeated' },
    offer: 'The Sandveil raiders sell our water to the zealots. The records name every well they have poisoned. It is time they answered for it.',
    progress: 'They camp in the Sea of Dunes, north-west.',
    complete: 'The wells are ours again. The records will remember this.',
    xp: 5200, gold: 110, rewardItemLevel: 24,
  },
  {
    id: 'ss_golems', map: 'sunscar', title: 'Glass and Fury', giver: 'ss_a', requires: ['ss_stingers'], minLevel: 23,
    objective: { type: 'kill', mob: 'golem', count: 8, label: 'Glassforged Golems shattered' },
    offer: 'The golems of the Flats march on the eastern road. Stone does not tire. You must break them.',
    progress: 'The Glass Flats, east. Step out when they pound the ground.',
    complete: 'Shattered. The road east is ours.',
    xp: 5600, gold: 120, rewardItemLevel: 25, rewardRarity: 2,
  },
  {
    id: 'ss_zealots', map: 'sunscar', title: 'The Sunflayer Cult', giver: 'ss_b', requires: ['ss_nomads', 'ss_golems'], minLevel: 25,
    objective: { type: 'kill', mob: 'cultist', count: 12, label: 'Sunflayer Zealots defeated' },
    offer: 'The zealots chant in the Sunken Temple, feeding Azhkar with their fire. Silence them.',
    progress: 'The Sunken Temple is south-east. Their fire clings.',
    complete: 'The chanting has stopped. Azhkar is weaker, but his warlord still stands.',
    xp: 6600, gold: 140, rewardItemLevel: 26,
  },
  {
    id: 'ss_warlord', map: 'sunscar', title: 'The Sigil of the Sun', giver: 'ss_a', requires: ['ss_zealots'], minLevel: 26,
    objective: { type: 'collect', item: 'sunsigil', count: 1, label: 'Sigil of the Sun' },
    offer: 'Warlord Szaran carries the Sigil that opens the Throne of the Sun. Take it from him. He does not fall easily.',
    progress: "Szaran's Warcamp lies north, on the road to the Throne.",
    complete: 'The Sigil. With this, the Throne is open. There is nothing left between you and Azhkar.',
    xp: 7400, gold: 180, rewardItemLevel: 28, rewardRarity: 2,
  },
  {
    id: 'ss_azhkar', map: 'sunscar', title: 'The Sunflayer', giver: 'ss_a', requires: ['ss_warlord'], minLevel: 27,
    objective: { type: 'kill', mob: 'azhkar', count: 1, label: 'Azhkar the Sunflayer defeated' },
    offer: 'Azhkar sits on the Throne of the Sun at the end of the northern road. Where the ground glows gold, his sun will fall. Move, heal, and strike. End the burning.',
    progress: 'The Throne of the Sun is north of the Warcamp.',
    complete: 'The sun is only the sun again. From the Vale to the dunes, the Wardens have returned. Your legend is written.',
    xp: 0, gold: 500, rewardItemLevel: 30, rewardRarity: 3,
  },
];

export const QUEST_BY_ID: Record<string, QuestDef> = Object.fromEntries(QUESTS.map((q) => [q.id, q]));

/** The last quest of the whole game. */
export const FINAL_QUEST = 'ss_azhkar';
