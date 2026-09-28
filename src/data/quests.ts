import type { MobKind } from './world';

export interface QuestDef {
  id: string;
  title: string;
  giver: string;
  /** NPC to hand the quest in to (defaults to giver). */
  turnIn?: string;
  requires: string[];
  minLevel: number;
  objective:
    | { type: 'kill'; mob: MobKind; count: number; label: string }
    | { type: 'collect'; item: string; count: number; label: string };
  offer: string;
  progress: string;
  complete: string;
  xp: number;
  gold: number;
  /** Item level of the guaranteed reward item (0 = none). */
  rewardItemLevel: number;
}

export const QUEST_ITEMS: Record<string, string> = {
  pelt: 'Dusk Wolf Pelt',
  relic: 'Sunward Relic',
};

export const QUESTS: QuestDef[] = [
  {
    id: 'wolves', title: 'Wolves at the Gate', giver: 'elra', requires: [], minLevel: 1,
    objective: { type: 'kill', mob: 'wolf', count: 8, label: 'Dusk Wolves slain' },
    offer: 'Stranger, you came just in time. The Dusk Wolves have grown bold since the Scar began to smoulder. They took two of our sheep last night. Thin their pack in the glade west of the village.',
    progress: 'The wolves still prowl the glade. Head west along the road.',
    complete: 'Good work. The night watch will sleep easier. You fight like one of the old Wardens.',
    xp: 160, gold: 8, rewardItemLevel: 2,
  },
  {
    id: 'pelts', title: 'Pelts for the Tanner', giver: 'bram', requires: [], minLevel: 1,
    objective: { type: 'collect', item: 'pelt', count: 6, label: 'Dusk Wolf Pelts' },
    offer: 'Winter comes early this close to the Scar. Bring me six pelts from the Dusk Wolves and I will stitch you something that turns a blade.',
    progress: 'Six pelts, friend. The wolves in Duskwood Glade carry fine coats.',
    complete: 'Fine pelts, these. Here, take this. Wear it well.',
    xp: 140, gold: 6, rewardItemLevel: 3,
  },
  {
    id: 'stalkers', title: 'Silk and Venom', giver: 'mira', requires: ['wolves'], minLevel: 2,
    objective: { type: 'kill', mob: 'spider', count: 8, label: 'Glade Stalkers slain' },
    offer: 'The stalkers of Webhollow have spread their nests to the herb fields. I cannot gather a single root. Cull them, north-west past the glade, and I will brew you something special.',
    progress: 'Webhollow lies north of Duskwood Glade. Watch for the green venom.',
    complete: 'The fields are safe again. You have my thanks, and this.',
    xp: 260, gold: 12, rewardItemLevel: 4,
  },
  {
    id: 'raiders', title: 'The Hollowkin Threat', giver: 'elra', requires: ['wolves'], minLevel: 3,
    objective: { type: 'kill', mob: 'raider', count: 10, label: 'Hollowkin Raiders slain' },
    offer: 'Hollowkin raiders have made camp to the east. They burned the Tallow farm and carried off supplies. Drive them back before they grow any bolder.',
    progress: 'The Hollowkin camp lies east, beyond the river road.',
    complete: 'That will make them think twice. But their chieftain still holds something of ours.',
    xp: 380, gold: 18, rewardItemLevel: 5,
  },
  {
    id: 'relic', title: 'The Stolen Relic', giver: 'elra', requires: ['raiders'], minLevel: 5,
    objective: { type: 'collect', item: 'relic', count: 1, label: 'Sunward Relic recovered' },
    offer: 'Chieftain Gorran took the Sunward Relic from our shrine. Without it the wards around Hearthmoor are failing. He stands at the heart of the camp. Bring it back.',
    progress: 'Gorran is strong. Make sure you are ready before you face him.',
    complete: 'The Relic! The wards are already brightening. There is one more thing I must ask of you.',
    xp: 480, gold: 30, rewardItemLevel: 7,
  },
  {
    id: 'brutes', title: 'Into the Ashen Scar', giver: 'elra', requires: ['relic'], minLevel: 6,
    objective: { type: 'kill', mob: 'brute', count: 8, label: 'Ashbound Brutes slain' },
    offer: 'The Relic showed me the truth: something ancient is waking beneath the Ashen Scar to the north. Its Ashbound servants guard the way. Break their line.',
    progress: 'Head north. The ground itself burns in the Scar.',
    complete: 'You returned alive. Then there is hope. Rest, and prepare for the end of this.',
    xp: 560, gold: 30, rewardItemLevel: 8,
  },
  {
    id: 'cindermaw', title: 'The Cindermaw', giver: 'elra', requires: ['brutes'], minLevel: 7,
    objective: { type: 'kill', mob: 'boss', count: 1, label: 'Varkul the Cindermaw defeated' },
    offer: 'Varkul the Cindermaw sleeps no longer. It lairs in the hollow at the far end of the Scar. When it strikes the ground, move out of the burning circle. Heal when you must. The whole vale is counting on you.',
    progress: "Cindermaw's Hollow is at the northern edge of the Scar. Step out of the fire.",
    complete: 'It is over. The Scar is cooling and the Vale is safe. Ashenveil will remember your name.',
    xp: 900, gold: 100, rewardItemLevel: 10,
  },
];

export const QUEST_BY_ID: Record<string, QuestDef> = Object.fromEntries(QUESTS.map((q) => [q.id, q]));
