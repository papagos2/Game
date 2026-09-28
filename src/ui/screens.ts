// Full-screen and panel UI: title, character creation, NPC dialogs, bag, quest log, menu, death, victory.
import { ABILITIES, CLASSES, POTION, SPEC_LEVEL, SPECS, type AbilityDef, type ClassId, type SpecId } from '../data/classes';
import { GEAR_SLOTS, SLOT_LABEL, TYPE_LABEL, type GearSlot } from '../data/items';
import { QUESTS, QUEST_BY_ID, type QuestDef } from '../data/quests';
import { TIER_POINTS, TREES, type TalentTree } from '../data/talents';
import { DUNGEONS, DUNGEON_ORDER } from '../data/dungeons';
import { MAPS, MAP_ORDER, type MapDef, type MapId, type NpcDef, type WorldId } from '../data/world';
import { abilityCooldown, abilityCost } from '../game/abilities';
import type { Game } from '../game/game';
import {
  acceptQuest, canChooseSpec, pointsAvailable, pointsSpent, questsForNpc, rankUp, rankUpBlocker, type Progress,
} from '../game/progress';
import { RARITY_COLORS, RARITY_NAMES, itemScore, respecCost, slotFor, upgradeValue, type Item } from '../game/rules';
import { isSoundEnabled, play, setSoundEnabled } from '../game/audio';
import { ACHIEVEMENTS, FACTIONS, MATERIALS, MAX_GATHER_SKILL, MAX_REFORGE, RECIPES, type MaterialId, type RecipeId } from '../data/economy';
import { achievementPoints, bountyLabel, claimableBounties, craft, craftBlocker, repRank, titles } from '../game/economy';
import { escapeHtml, npcName } from './hud';
import { glyphSvg, iconHtml } from './icons';

const root = () => document.getElementById('screens')!;

export function closeScreens() {
  root().innerHTML = '';
}

function show(html: string, cls = 'dim'): HTMLElement {
  const r = root();
  r.innerHTML = `<div class="screen ${cls}">${html}</div>`;
  return r.firstElementChild as HTMLElement;
}

function on(el: HTMLElement, sel: string, fn: (e: Event, target: HTMLElement) => void) {
  el.querySelectorAll<HTMLElement>(sel).forEach((b) =>
    b.addEventListener('click', (e) => {
      play('click');
      fn(e, b);
    }),
  );
}

// ---------- Title and character creation ----------

export function titleScreen(hasSave: Progress | null, onContinue: () => void, onNew: () => void) {
  const el = show(
    `<div class="logo">ASHENVEIL</div>
     <div class="tagline">THE VALE AWAITS ITS WARDEN</div>
     <div class="title-buttons">
       ${hasSave ? `<button class="btn" id="cont">Continue<br><small style="font-weight:600">${escapeHtml(hasSave.name)} - Level ${hasSave.level} ${hasSave.spec ? SPECS[hasSave.spec].name : CLASSES[hasSave.cls].name} - ${MAPS[hasSave.mapId].name}</small></button>` : ''}
       <button class="btn ${hasSave ? 'secondary' : ''}" id="new">New Hero</button>
     </div>
     <div class="fineprint">Best played in landscape. Left thumb moves, right thumb turns the camera.</div>`,
    'title-screen',
  );
  if (hasSave) on(el, '#cont', onContinue);
  on(el, '#new', () => {
    if (hasSave) confirmBox('Start a new hero? Your current hero will be lost.', 'Start new', onNew, () => titleScreen(hasSave, onContinue, onNew));
    else onNew();
  });
}

function confirmBox(text: string, ok: string, onOk: () => void, onCancel: () => void) {
  const el = show(
    `<div class="panel center-col" style="max-width:420px"><p>${escapeHtml(text)}</p>
      <div class="row"><button class="btn secondary" id="no">Cancel</button><button class="btn danger" id="yes">${escapeHtml(ok)}</button></div></div>`,
  );
  on(el, '#yes', onOk);
  on(el, '#no', onCancel);
}

export function classSelect(onDone: (name: string, cls: ClassId) => void, onBack: () => void) {
  let chosen: ClassId = 'stormblade';
  const cards = (Object.keys(CLASSES) as ClassId[])
    .map((id) => {
      const c = CLASSES[id];
      const main = ABILITIES[c.abilities[0]];
      return `<button class="classCard ${id === chosen ? 'sel' : ''}" data-cls="${id}">
        <div class="ch"><div class="icon" style="background:radial-gradient(circle at 35% 30%, ${c.colors.glow}, ${c.colors.body} 75%)">${glyphSvg(main.glyph)}</div>
        <div><b>${c.name}</b><div class="role">${c.role}</div></div></div>
        
        <div class="miniIcons">${c.abilities.map((a) => iconHtml(ABILITIES[a].glyph, ABILITIES[a].color)).join('')}</div>
      </button>`;
    })
    .join('');
  const el = show(
    `<div class="panel" style="width:min(820px,100%)">
      <button class="close" id="back" aria-label="Back">&times;</button>
      <h2>Choose your path</h2>
      <p class="sub">Each class has its own abilities, learned as you level up.</p>
      <div class="classes">${cards}</div>
      <p class="clsInfo" id="clsInfo">${clsInfo(chosen)}</p>
      <div class="row end">
        <input class="name" id="heroName" maxlength="14" placeholder="Hero name" value="${randomName()}" aria-label="Hero name" />
        <button class="btn" id="go">Enter the Vale</button>
      </div>
    </div>`,
  );
  on(el, '.classCard', (_e, b) => {
    chosen = b.dataset.cls as ClassId;
    el.querySelectorAll('.classCard').forEach((c) => c.classList.toggle('sel', c === b));
    el.querySelector('#clsInfo')!.textContent = clsInfo(chosen);
  });
  on(el, '#back', onBack);
  on(el, '#go', () => {
    const input = el.querySelector<HTMLInputElement>('#heroName')!;
    const name = input.value.replace(/[^\p{L}\p{N} '-]/gu, '').trim().slice(0, 14) || randomName();
    onDone(name, chosen);
  });
}

function clsInfo(id: ClassId) {
  const c = CLASSES[id];
  return `${c.blurb} At level ${SPEC_LEVEL} choose a path: ${c.specs.map((sp) => SPECS[sp].name).join(', ')}.`;
}

function randomName() {
  const a = ['Aren', 'Kaela', 'Doran', 'Lysa', 'Torvin', 'Mirel', 'Cass', 'Edrin', 'Sera', 'Bryn'];
  return a[Math.floor(Math.random() * a.length)];
}

// ---------- In-game panels ----------

function statLine(it: Item) {
  return [it.power ? `+${it.power} Power` : '', it.stamina ? `+${it.stamina} Stamina` : '', it.armor ? `+${it.armor} Armor` : '', it.crit ? `+${it.crit} Crit` : '']
    .filter(Boolean).join(', ');
}

function abilityText(g: Game, ab: AbilityDef) {
  const cost = abilityCost(g, ab.id);
  const cd = abilityCooldown(g, ab.id);
  return `${cost ? `${cost} ${CLASSES[g.progress.cls].resourceName}, ` : ''}${cd.toFixed(cd < 10 ? 1 : 0)}s cooldown${ab.range ? `, ${ab.range} m` : ''}`;
}

export class Panels {
  private tab: 'core' | 'spec' = 'core';

  constructor(
    private game: Game,
    private onClose: () => void,
    private onQuit: () => void,
    private onTravel: (map: WorldId) => void,
    private hooks: { serverUrl: () => string; setOnline: (on: boolean, url: string) => void } = { serverUrl: () => '', setOnline: () => undefined },
  ) {}

  private close = () => {
    closeScreens();
    this.onClose();
  };

  // ----- NPCs -----

  dialog(npc: NpcDef) {
    if (npc.travel) return this.wayfinder(npc);
    const g = this.game;
    const list = questsForNpc(g.progress, npc.id);
    const items = list
      .map(({ quest, status }) => `<button class="questItem ${status === 'active' ? 'dim' : ''}" data-q="${quest.id}">
         <span class="mk">${status === 'available' ? '!' : '?'}</span>
         <span><b>${escapeHtml(quest.title)}</b><br><small style="color:var(--ink-dim)">${status === 'available' ? 'New quest' : status === 'ready' ? 'Ready to complete' : 'In progress'}</small></span></button>`)
      .join('');
    const finished = g.progress.completed.includes(g.map.finalQuest);
    const greet = npc.vendor
      ? 'Supplies for the road? Draughts, and gear for someone of your standing. I also buy what you do not need.'
      : list.length
        ? 'Well met, Warden.'
        : finished
          ? 'This land owes you everything.'
          : 'Nothing for you right now. Grow stronger and come back to me.';
    const el = show(
      `<div class="panel dialog ${npc.vendor ? 'wide' : ''}">
        <button class="close" id="x" aria-label="Close">&times;</button>
        <div class="npcHead"><div><h2>${escapeHtml(npc.name)}</h2><p class="sub">${escapeHtml(npc.title)}</p></div></div>
        <p>${greet}</p>
        <div class="questList">${items}</div>
        ${npc.vendor ? this.vendorHtml() : ''}
      </div>`,
    );
    on(el, '#x', this.close);
    on(el, '.questItem', (_e, b) => this.questDetail(npc, QUEST_BY_ID[b.dataset.q!]));
    if (npc.vendor) this.bindVendor(el, npc);
  }

  private vendorHtml() {
    const g = this.game;
    const p = g.progress;
    const junk = p.bag.filter((i) => this.isJunk(i));
    const price = g.potionPrice();
    const stock = g.vendorItems()
      .map(({ item, price: pr }) => {
        const up = Math.round(upgradeValue(item, p.gear));
        return `<button class="itemCard" data-buy="${item.id}" ${p.gold < pr ? 'disabled' : ''}>
          <span class="in" style="color:${RARITY_COLORS[item.rarity]}">${escapeHtml(item.name)}</span>
          <span class="is">${TYPE_LABEL[item.type]} - ${statLine(item)}</span>
          <span class="${up > 0 ? 'up' : 'down'}">${up > 0 ? `+${up} upgrade` : 'not an upgrade'}</span>
          <span class="gold">${pr} gold</span></button>`;
      })
      .join('');
    const cost = respecCost(p.level);
    return `<h3>Trade</h3>
      <div class="row">
        <button class="btn" id="buy" ${p.gold < price ? 'disabled' : ''}>Buy ${POTION.name} (${price}g)</button>
        <button class="btn secondary" id="sellJunk" ${junk.length ? '' : 'disabled'}>Sell ${junk.length} unneeded item${junk.length === 1 ? '' : 's'}</button>
      </div>
      <p class="sub" style="margin-top:8px">You have <span class="gold">${p.gold} gold</span> and ${p.potions} draught${p.potions === 1 ? '' : 's'}.</p>
      ${stock ? `<h3>Gear for your level</h3><div class="bag">${stock}</div>` : ''}
      <h3>Relearn talents</h3>
      <p class="sub">Refund all talent points${p.spec ? ', or also change your path,' : ''} for <span class="gold">${cost} gold</span>.</p>
      <div class="row"><button class="btn secondary" id="respec" ${p.gold < cost || pointsSpent(p) === 0 ? 'disabled' : ''}>Reset talents</button>
      ${p.spec ? `<button class="btn secondary" id="respecSpec" ${p.gold < cost ? 'disabled' : ''}>Reset talents and path</button>` : ''}</div>`;
  }

  private isJunk(i: Item) {
    return upgradeValue(i, this.game.progress.gear) <= 0;
  }

  private bindVendor(el: HTMLElement, npc: NpcDef) {
    const g = this.game;
    on(el, '#buy', () => {
      g.buyPotion();
      this.dialog(npc);
    });
    on(el, '#sellJunk', () => {
      for (const i of g.progress.bag.filter((it) => this.isJunk(it))) g.sell(i);
      this.dialog(npc);
    });
    on(el, '[data-buy]', (_e, b) => {
      if (!g.buyItem(b.dataset.buy!)) g.fail(g.progress.bag.length >= 24 ? 'Your bag is full' : 'Not enough gold');
      this.dialog(npc);
    });
    on(el, '#respec', () => confirmBox(`Reset all talents for ${respecCost(g.progress.level)} gold?`, 'Reset', () => {
      g.respec(false);
      this.dialog(npc);
    }, () => this.dialog(npc)));
    on(el, '#respecSpec', () => confirmBox(`Reset talents and your path for ${respecCost(g.progress.level)} gold? You can choose a new path right away.`, 'Reset', () => {
      g.respec(true);
      this.skills();
    }, () => this.dialog(npc)));
  }

  private wayfinder(npc: NpcDef) {
    const g = this.game;
    const p = g.progress;
    const rows = MAP_ORDER.map((id) => {
      const m = MAPS[id];
      const open = p.unlocked.includes(id);
      const here = id === g.map.id;
      return `<button class="questItem ${open && !here ? '' : 'dim'}" data-map="${id}" ${open && !here ? '' : 'disabled'}>
        <span class="mk">${here ? '&bull;' : open ? '&rarr;' : '&times;'}</span>
        <span><b>${escapeHtml(m.subtitle)}: ${escapeHtml(m.name)}</b><br><small style="color:var(--ink-dim)">Levels ${m.levels[0]}-${m.levels[1]}${here ? ' - you are here' : open ? '' : ' - sealed'}</small></span></button>`;
    }).join('');
    const el = show(
      `<div class="panel dialog">
        <button class="close" id="x" aria-label="Close">&times;</button>
        <h2>${escapeHtml(npc.name)}</h2><p class="sub">Wayfinder</p>
        <p>The waystones remember every road a Warden has opened. Where will you go?</p>
        <div class="questList">${rows}</div>
        <p class="sub" style="margin-top:10px">New lands open when you defeat the master of the last one.</p>
        ${g.map.dungeon ? '' : `<h3>Dungeons</h3>
        <p class="sub">Group adventures. AI companions join you to fill the roles you do not play (tank, healer or damage).</p>
        <div class="questList">${DUNGEON_ORDER.map((id) => {
          const d = DUNGEONS[id];
          const open = p.unlocked.includes(d.act) && p.level >= d.dungeon!.minLevel;
          const cleared = p.dungeons.includes(id);
          return `<button class="questItem ${open ? '' : 'dim'}" data-map="${id}" ${open ? '' : 'disabled'}>
            <span class="mk">${open ? '&#9876;' : '&times;'}</span>
            <span><b>${escapeHtml(d.name)}</b>${cleared ? ' <small style="color:#6ee07a">(cleared)</small>' : ''}<br><small style="color:var(--ink-dim)">Level ${d.dungeon!.minLevel}+ - ${escapeHtml(MAPS[d.act].name)}. ${escapeHtml(d.dungeon!.blurb)}</small></span></button>`;
        }).join('')}</div>`}
      </div>`,
    );
    on(el, '#x', this.close);
    on(el, '[data-map]', (_e, b) => {
      closeScreens();
      this.onTravel(b.dataset.map as MapId);
    });
  }

  private questDetail(npc: NpcDef, q: QuestDef) {
    const g = this.game;
    const st = g.progress.active.find((a) => a.id === q.id);
    const status = st ? (st.done ? 'ready' : 'active') : 'available';
    const text = status === 'available' ? q.offer : status === 'ready' ? q.complete : q.progress;
    const obj = `${q.objective.label}: ${st ? st.progress : 0}/${q.objective.count}`;
    const el = show(
      `<div class="panel dialog">
        <button class="close" id="x" aria-label="Close">&times;</button>
        <h2>${escapeHtml(q.title)}</h2>
        <p class="sub">${escapeHtml(npc.name)}</p>
        <p>${escapeHtml(text)}</p>
        <h3>Objective</h3><p>${escapeHtml(obj)}</p>
        <div class="reward"><span>Reward:</span>${q.xp ? `<b>${q.xp} XP</b>` : ''}<span class="gold">${q.gold} gold</span>${q.rewardItemLevel ? `<b style="color:${RARITY_COLORS[q.rewardRarity ?? 1]}">+ ${RARITY_NAMES[q.rewardRarity ?? 1]} item</b>` : ''}</div>
        <div class="row end">
          <button class="btn secondary" id="back">Back</button>
          ${status === 'available' ? '<button class="btn" id="accept">Accept</button>' : ''}
          ${status === 'ready' ? '<button class="btn" id="complete">Complete quest</button>' : ''}
        </div>
      </div>`,
    );
    on(el, '#x', this.close);
    on(el, '#back', () => this.dialog(npc));
    on(el, '#accept', () => {
      if (acceptQuest(g.progress, q.id)) {
        play('quest');
        g.save();
      }
      this.close();
      document.dispatchEvent(new CustomEvent('quests-changed'));
    });
    on(el, '#complete', () => {
      this.close();
      g.turnInQuest(q.id);
    });
  }

  questLog(tab: 'quests' | 'bounties' | 'factions' | 'achievements' = 'quests') {
    const g = this.game;
    const p = g.progress;
    const tabs = `<div class="tabs">${(['quests', 'bounties', 'factions', 'achievements'] as const)
      .map((t) => `<button class="tab ${t === tab ? 'on' : ''}" data-tab="${t}">${cap(t)}${t === 'bounties' && claimableBounties(p, g.map.act).length ? ' !' : ''}</button>`).join('')}</div>`;
    let body = '';
    if (tab === 'quests') {
      const active = p.active
        .map((a) => {
          const q = QUEST_BY_ID[a.id];
          const where = q.map !== g.map.id ? ` <small style="color:var(--ink-dim)">(${escapeHtml(MAPS[q.map].name)})</small>` : '';
          return `<div class="questItem" style="display:block"><b>${escapeHtml(q.title)}</b>${where}
            <p style="margin:4px 0">${a.done ? `Complete. Return to ${escapeHtml(npcName(q.turnIn ?? q.giver))}.` : escapeHtml(q.progress)}</p>
            <small style="color:var(--ink-dim)">${escapeHtml(q.objective.label)}: ${a.progress}/${q.objective.count}</small>
            ${q.map === g.map.id ? `<div class="row" style="margin-top:6px"><button class="btn secondary small" data-go="${q.id}">Go there</button></div>` : ''}</div>`;
        })
        .join('');
      body = `<p class="sub">${p.completed.length} of ${QUESTS.length} quests completed. Tap <b>Go there</b> (or a quest on the screen) to travel automatically.</p>
        <div class="questList">${active || '<p>No active quests. Look for villagers with a <b style="color:#ffd84a">!</b> above their heads.</p>'}</div>`;
    } else if (tab === 'bounties') {
      const list = p.bounties.list
        .filter((b) => p.unlocked.includes(b.map))
        .map((b) => `<div class="questItem" style="display:block"><b>${escapeHtml(bountyLabel(b))}</b> <small style="color:var(--ink-dim)">(${escapeHtml(MAPS[b.map].name)})</small>
          <p style="margin:4px 0"><small>${b.claimed ? 'Claimed' : `${b.progress}/${b.count}`} - reward: experience, gold and ${escapeHtml(FACTIONS[b.map].name)} reputation</small></p>
          ${!b.claimed && b.progress >= b.count ? `<button class="btn small" data-claim="${b.id}">Claim reward</button>` : ''}</div>`)
        .join('');
      body = `<p class="sub">New bounties every day. Finish them anywhere and claim the reward here.</p><div class="questList">${list}</div>`;
    } else if (tab === 'factions') {
      body = `<p class="sub">Gain reputation from quests, bounties, dungeons and defeating enemies in each land. Higher ranks lower vendor prices; <b>Revered</b> unlocks an epic item at that land's vendor.</p>
        <div class="questList">${MAP_ORDER.map((m) => {
          const f = FACTIONS[m];
          const r = repRank(p.rep[m] ?? 0);
          const pct = r.span ? Math.round((r.into / r.span) * 100) : 100;
          return `<div class="questItem" style="display:block"><b style="color:${f.color}">${escapeHtml(f.name)}</b> <small style="color:var(--ink-dim)">(${escapeHtml(MAPS[m].name)})</small>
            <div class="bar rep"><div class="fill" style="transform:scaleX(${pct / 100});background:${f.color}"></div><span>${r.name}${r.span ? ` ${r.into}/${r.span}` : ''}</span></div>
            <small style="color:var(--ink-dim)">${r.discount ? `${Math.round(r.discount * 100)}% vendor discount` : 'No discount yet'}</small></div>`;
        }).join('')}</div>`;
    } else {
      const list = ACHIEVEMENTS.map((a) => {
        const has = p.achievements.includes(a.id);
        return `<div class="ach ${has ? 'has' : ''}"><b>${escapeHtml(a.name)}</b> <small>${a.points} pts</small><br><span>${escapeHtml(a.desc)}</span>${a.title ? `<br><small>Title: "${escapeHtml(a.title)}"</small>` : ''}</div>`;
      }).join('');
      const tl = titles(p);
      body = `<p class="sub">${p.achievements.length}/${ACHIEVEMENTS.length} achievements - ${achievementPoints(p)} points.</p>
        ${tl.length ? `<div class="row" style="margin:6px 0 10px"><span class="sub">Title:</span>${['', ...tl].map((t) => `<button class="tab ${p.title === (t || null) ? 'on' : ''}" data-title="${escapeHtml(t)}">${t ? escapeHtml(t) : 'None'}</button>`).join('')}</div>` : ''}
        <div class="achGrid">${list}</div>`;
    }
    const el = show(
      `<div class="panel dialog wide">
        <button class="close" id="x" aria-label="Close">&times;</button>
        <h2>Journal</h2>
        ${tabs}
        ${body}
      </div>`,
    );
    on(el, '#x', this.close);
    on(el, '[data-tab]', (_e, b) => this.questLog(b.dataset.tab as typeof tab));
    on(el, '[data-go]', (_e, b) => {
      this.close();
      g.travelTo(b.dataset.go!);
    });
    on(el, '[data-claim]', (_e, b) => {
      g.claimBounty(b.dataset.claim!);
      this.questLog('bounties');
    });
    on(el, '[data-title]', (_e, b) => {
      p.title = b.dataset.title || null;
      g.save();
      this.questLog('achievements');
    });
  }

  // ----- Skills: path choice and talent trees -----

  skills() {
    const g = this.game;
    const p = g.progress;
    if (canChooseSpec(p)) return this.specChoice();
    const tree: TalentTree = this.tab === 'spec' && p.spec ? TREES[p.spec] : TREES[p.cls];
    const spent = pointsSpent(p, tree.id);
    const nodes = tree.nodes
      .map((n) => {
        const rank = p.talents[n.id] ?? 0;
        const block = rankUpBlocker(p, n.id);
        const tierLocked = spent < TIER_POINTS[n.tier];
        return `<button class="talent ${rank ? 'has' : ''} ${rank >= n.maxRank ? 'max' : ''} ${tierLocked ? 'tlock' : ''}" data-node="${n.id}" ${block ? 'aria-disabled="true"' : ''} title="${escapeHtml(block ?? 'Add a point')}">
          <span class="tr">${rank}/${n.maxRank}</span>
          <b>${escapeHtml(n.name)}</b>
          <span class="td">${escapeHtml(n.desc)}</span>
          ${tierLocked ? `<span class="tl">Needs ${TIER_POINTS[n.tier]} points here</span>` : ''}
        </button>`;
      })
      .join('');
    const abilities = g.abilityList()
      .map((ab) => {
        const learned = ab.unlockLevel <= p.level && (!ab.spec || ab.spec === p.spec);
        return `<div class="abRow ${learned ? '' : 'dim'}">${iconHtml(ab.glyph, ab.color)}<div><b>${escapeHtml(ab.name)}</b> <small>${learned ? escapeHtml(abilityText(g, ab)) : `Level ${ab.unlockLevel}`}</small><br><span>${escapeHtml(ab.description)}</span></div></div>`;
      })
      .join('');
    const specName = p.spec ? SPECS[p.spec].name : null;
    const el = show(
      `<div class="panel skillsPanel">
        <button class="close" id="x" aria-label="Close">&times;</button>
        <h2>Skills</h2>
        <p class="sub">${pointsAvailable(p)} talent point${pointsAvailable(p) === 1 ? '' : 's'} to spend. You gain one each level.${p.spec ? '' : ` At level ${SPEC_LEVEL} you choose your path.`}</p>
        <div class="tabs">
          <button class="tab ${this.tab === 'core' || !p.spec ? 'on' : ''}" data-tab="core">${escapeHtml(CLASSES[p.cls].name)} (${pointsSpent(p, p.cls)})</button>
          <button class="tab ${this.tab === 'spec' && p.spec ? 'on' : ''}" data-tab="spec" ${p.spec ? '' : 'disabled'}>${specName ? `${escapeHtml(specName)} (${pointsSpent(p, p.spec!)})` : `Path - level ${SPEC_LEVEL}`}</button>
        </div>
        <div class="skillCols">
          <div class="talents">${nodes}</div>
          <div class="abilityList"><h3>Abilities</h3>${abilities}</div>
        </div>
      </div>`,
    );
    on(el, '#x', this.close);
    on(el, '[data-tab]', (_e, b) => {
      this.tab = b.dataset.tab as 'core' | 'spec';
      this.skills();
    });
    on(el, '[data-node]', (_e, b) => {
      const id = b.dataset.node!;
      const why = rankUpBlocker(p, id);
      if (why) {
        g.fail(why);
        return;
      }
      rankUp(p, id);
      g.talentsChanged();
      play('levelup');
      this.skills();
    });
  }

  specChoice() {
    const g = this.game;
    const p = g.progress;
    const cls = CLASSES[p.cls];
    const cards = cls.specs
      .map((id) => {
        const sp = SPECS[id];
        return `<button class="classCard specCard" data-spec="${id}">
          <div class="ch"><div class="icon" style="background:radial-gradient(circle at 35% 30%, ${sp.color}, ${cls.colors.body} 75%)">${glyphSvg(sp.glyph)}</div>
          <div><b>${escapeHtml(sp.name)}</b><div class="role">${escapeHtml(sp.role)}</div></div></div>
          <p>${escapeHtml(sp.blurb)}</p>
          <div class="specAbs">${sp.abilities.map((a) => `<div>${iconHtml(ABILITIES[a].glyph, ABILITIES[a].color)}<span><b>${escapeHtml(ABILITIES[a].name)}</b> <small>(level ${ABILITIES[a].unlockLevel})</small><br>${escapeHtml(ABILITIES[a].description)}</span></div>`).join('')}</div>
        </button>`;
      })
      .join('');
    const el = show(
      `<div class="panel" style="width:min(900px,100%)">
        <button class="close" id="x" aria-label="Close">&times;</button>
        <h2>Choose your path</h2>
        <p class="sub">Your ${escapeHtml(cls.name)} can walk one of three paths. Each brings two new abilities and its own talent tree. You can change it later at a vendor, for gold.</p>
        <div class="classes">${cards}</div>
      </div>`,
    );
    on(el, '#x', this.close);
    on(el, '[data-spec]', (_e, b) => {
      const id = b.dataset.spec as SpecId;
      confirmBox(`Walk the path of the ${SPECS[id].name}?`, 'Choose', () => {
        g.chooseSpec(id);
        this.tab = 'spec';
        this.skills();
      }, () => this.specChoice());
    });
  }

  // ----- Bag -----

  crafting(reforgeSlot?: GearSlot) {
    const g = this.game;
    const p = g.progress;
    const mats = (Object.keys(MATERIALS) as MaterialId[])
      .filter((m) => (p.materials[m] ?? 0) > 0)
      .map((m) => `<span class="mat" style="border-color:${MATERIALS[m].color}">${escapeHtml(MATERIALS[m].name)} x${p.materials[m]}</span>`).join('') || '<span class="sub">No materials yet. Look for glowing herbs and ore veins in the wilds.</span>';
    const recipes = RECIPES.map((r) => {
      const block = craftBlocker(p, r.id);
      return `<div class="questItem" style="display:block"><b>${escapeHtml(r.name)}</b><br><small style="color:var(--ink-dim)">${escapeHtml(r.desc)} Cost: ${r.herbs ? `${r.herbs} herbs ` : ''}${r.ores ? `${r.ores} ore ` : ''}${r.gold ? `${r.gold} gold` : ''}</small>
        <div class="row" style="margin-top:6px">${r.id === 'reforge'
          ? GEAR_SLOTS.filter((sl) => p.gear[sl]).map((sl) => `<button class="btn secondary small" data-reforge="${sl}" ${block || (p.gear[sl]!.upg ?? 0) >= MAX_REFORGE ? 'disabled' : ''}>${escapeHtml(SLOT_LABEL[sl])} (+${(p.gear[sl]!.upg ?? 0) * 2})</button>`).join('') || '<span class="sub">Equip an item first.</span>'
          : `<button class="btn small" data-craft="${r.id}" ${block ? 'disabled' : ''}>${block ? escapeHtml(block) : 'Craft'}</button>`}</div></div>`;
    }).join('');
    const el = show(
      `<div class="panel bagPanel">
        <button class="close" id="x" aria-label="Close">&times;</button>
        <h2>Crafting</h2>
        <div class="tabs"><button class="tab" data-bag="gear">Gear</button><button class="tab on">Crafting</button></div>
        <p class="sub">Gathering skill <b>${p.gatherSkill}</b> / ${MAX_GATHER_SKILL}. Frostmarch materials need 60, Sunscar 140.</p>
        <div class="mats">${mats}</div>
        <div class="questList">${recipes}</div>
      </div>`,
    );
    void reforgeSlot;
    on(el, '#x', this.close);
    on(el, '[data-bag]', () => this.bag());
    on(el, '[data-craft]', (_e, b) => {
      const res = craft(p, b.dataset.craft as RecipeId);
      if (res) {
        g.recomputeStats();
        g.checkAchievements();
        g.ui.toast(escapeHtml(res));
        play('loot');
        g.save();
      }
      this.crafting();
    });
    on(el, '[data-reforge]', (_e, b) => {
      const it = p.gear[b.dataset.reforge as GearSlot];
      const res = it ? craft(p, 'reforge', it) : null;
      if (res) {
        g.recomputeStats();
        g.checkAchievements();
        g.ui.toast(escapeHtml(res));
        play('levelup');
        g.save();
      }
      this.crafting();
    });
  }

  bag(selected?: string) {
    const g = this.game;
    const p = g.progress;
    const st = g.stats;
    const card = (it: Item | null, slot?: GearSlot) => {
      if (!it) return `<div class="itemCard empty">${slot ? SLOT_LABEL[slot] : ''} - empty</div>`;
      const diff = slot ? 0 : Math.round(upgradeValue(it, p.gear));
      const cmp = slot ? '' : diff > 0 ? `<span class="up">+${diff} upgrade</span>` : diff < 0 ? `<span class="down">${diff}</span>` : '';
      return `<button class="itemCard ${selected === it.id ? 'sel' : ''}" data-id="${it.id}" ${slot ? 'disabled' : ''}>
        <span class="in" style="color:${RARITY_COLORS[it.rarity]}">${escapeHtml(it.name)}</span>
        <span class="is">${slot ? SLOT_LABEL[slot] : TYPE_LABEL[it.type]} - ${RARITY_NAMES[it.rarity]} - ilvl ${it.ilvl}</span>
        <span class="is">${statLine(it)}</span>
        ${cmp}
      </button>`;
    };
    const sel = p.bag.find((i) => i.id === selected);
    const sorted = [...p.bag].sort((a, b) => upgradeValue(b, p.gear) - upgradeValue(a, p.gear));
    const el = show(
      `<div class="panel bagPanel">
        <button class="close" id="x" aria-label="Close">&times;</button>
        <h2>${escapeHtml(p.name)}${p.title ? ` <small class="ptitle">${escapeHtml(p.title)}</small>` : ''}</h2>
        <div class="tabs"><button class="tab on">Gear</button><button class="tab" data-bag="craft">Crafting</button></div>
        <p class="sub">Level ${p.level} ${p.spec ? SPECS[p.spec].name : CLASSES[p.cls].name} - <span class="gold">${p.gold} gold</span> - ${p.potions} healing draught${p.potions === 1 ? '' : 's'}</p>
        <div class="bagCols">
          <div>
            <h3>Stats</h3>
            <div class="stats">
              <div><span>Health</span><b>${st.maxHp}</b></div>
              <div><span>Power</span><b>${st.power}</b></div>
              <div><span>Armor</span><b>${st.armor}</b></div>
              <div><span>Critical chance</span><b>${Math.round(st.critChance * 100)}%</b></div>
              <div><span>Damage bonus</span><b>+${Math.round((st.damageScale - 1) * 100)}%</b></div>
            </div>
            <h3>Equipped</h3>
            <div class="slots">${GEAR_SLOTS.map((s) => card(p.gear[s], s)).join('')}</div>
          </div>
          <div>
            <h3>Bag (${p.bag.length}/24)</h3>
            ${sel ? `<div class="row" style="margin-bottom:8px"><button class="btn" id="equip">Equip as ${SLOT_LABEL[slotFor(sel, p.gear)]}</button><button class="btn secondary" id="sell">Sell for ${sel.value}g</button></div>` : `<div class="row" style="margin-bottom:8px"><button class="btn secondary" id="best" ${sorted.some((i) => upgradeValue(i, p.gear) > 0) ? '' : 'disabled'}>Equip best</button><span class="sub">Tap an item to equip or sell it.</span></div>`}
            <div class="bag">${sorted.map((i) => card(i)).join('') || '<p class="sub">Your bag is empty. Defeated enemies sometimes drop items.</p>'}</div>
          </div>
        </div>
      </div>`,
    );
    on(el, '#x', this.close);
    on(el, '.bag .itemCard', (_e, b) => this.bag(b.dataset.id === selected ? undefined : b.dataset.id));
    on(el, '[data-bag]', () => this.crafting());
    on(el, '#equip', () => {
      if (sel) g.equip(sel);
      this.bag();
    });
    on(el, '#sell', () => {
      if (sel) g.sell(sel);
      this.bag();
    });
    on(el, '#best', () => {
      for (let guard = 0; guard < 30; guard++) {
        const best = [...p.bag].sort((a, b) => upgradeValue(b, p.gear) - upgradeValue(a, p.gear))[0];
        if (!best || upgradeValue(best, p.gear) <= 0) break;
        g.equip(best);
      }
      this.bag();
    });
    void itemScore;
  }

  // ----- Menu, death, act complete, victory -----

  menu() {
    const g = this.game;
    const mins = Math.floor(g.progress.playSeconds / 60);
    const el = show(
      `<div class="panel" style="width:min(440px,100%)">
        <button class="close" id="x" aria-label="Close">&times;</button>
        <h2>Paused</h2>
        <p class="sub">Played for ${mins} minute${mins === 1 ? '' : 's'}. Progress is saved automatically.</p>
        <div class="toggle"><span>Sound</span><button class="btn secondary" id="snd">${isSoundEnabled() ? 'On' : 'Off'}</button></div>
        <div class="toggle"><span>Graphics</span><button class="btn secondary" id="gfx">${g.quality === 'high' ? 'High' : 'Battery saver'}</button></div>
        <h3>Online</h3>
        <p class="sub">See other heroes on your map and chat with them. Everything else stays on your device. Status: <b>${g.online ? g.online.status : 'off'}</b></p>
        <div class="row"><input class="name srv" id="srv" placeholder="Server address, e.g. wss://play.example.com" value="${escapeHtml(this.hooks.serverUrl())}" aria-label="Server address" /></div>
        <div class="row" style="margin-top:8px"><button class="btn secondary" id="onl">${g.online && g.online.status !== 'off' ? 'Go offline' : 'Go online'}</button></div>
        <h3>How to play</h3>
        <p style="font-size:13px">Move with your left thumb and drag with your right to look around. Tap an enemy to target it and press <b>Attack</b>. Your abilities are around the Attack button. Spend talent points in <b>Skills</b>. Step out of coloured circles on the ground. When hurt, drink a healing draught (the red button).</p>
        <div class="row end"><button class="btn secondary" id="quit">Save and quit</button><button class="btn" id="resume">Resume</button></div>
      </div>`,
    );
    on(el, '#x', this.close);
    on(el, '#resume', this.close);
    on(el, '#snd', () => {
      setSoundEnabled(!isSoundEnabled());
      saveSetting('sound', isSoundEnabled() ? '1' : '0');
      this.menu();
    });
    on(el, '#gfx', () => {
      g.setQuality(g.quality === 'high' ? 'low' : 'high');
      saveSetting('quality', g.quality);
      this.menu();
    });
    on(el, '#quit', () => {
      g.save();
      this.onQuit();
    });
    on(el, '#onl', () => {
      const url = (el.querySelector<HTMLInputElement>('#srv')!.value || '').trim();
      this.hooks.setOnline(!(g.online && g.online.status !== 'off'), url);
      setTimeout(() => this.menu(), 300);
    });
  }

  chat() {
    const g = this.game;
    const sess = g.online;
    if (!sess) return;
    const lines = sess.chat.map((l) => l.system
      ? `<div class="chatLine"><span class="gold">${escapeHtml(l.text)}</span></div>`
      : `<div class="chatLine"><b>${escapeHtml(l.name)}:</b><span>${escapeHtml(l.text)}</span>${l.from !== sess.myId ? `<span class="acts"><button data-mute="${escapeHtml(l.name)}">Mute</button><button data-report="${l.from}">Report</button></span>` : ''}</div>`).join('');
    const el = show(
      `<div class="panel dialog wide">
        <button class="close" id="x" aria-label="Close">&times;</button>
        <h2>Chat</h2>
        <p class="sub">Heroes on ${escapeHtml(g.map.name)}. Be kind: offensive words are filtered, and you can mute or report anyone.</p>
        <div class="chatLog" id="log">${lines || '<p class="sub">No messages yet.</p>'}</div>
        <div class="row"><input class="name chatIn" id="msg" maxlength="140" placeholder="Say something" aria-label="Message" /><button class="btn" id="send">Send</button></div>
      </div>`,
    );
    const log = el.querySelector('#log')!;
    log.scrollTop = log.scrollHeight;
    const input = el.querySelector<HTMLInputElement>('#msg')!;
    const send = () => {
      if (sess.say(input.value)) input.value = '';
      setTimeout(() => this.chat(), 250);
    };
    on(el, '#x', this.close);
    on(el, '#send', send);
    input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') send();
    });
    on(el, '[data-mute]', (_e, b) => {
      sess.mute(b.dataset.mute!);
      this.chat();
    });
    on(el, '[data-report]', (_e, b) => {
      sess.report(Number(b.dataset.report), 'chat');
      g.ui.message('Report sent');
    });
  }

  death(onRelease: () => void) {
    const hub = this.game.map.zones[0].name;
    const el = show(
      `<div class="center-col">
        <h1 class="bigTitle">You have fallen</h1>
        <p>Your spirit returns to ${escapeHtml(hub)}. Enemies that defeated you have returned to their posts.</p>
        <button class="btn" id="rel">Return to ${escapeHtml(hub)}</button>
      </div>`,
    );
    on(el, '#rel', () => {
      closeScreens();
      onRelease();
    });
  }

  dungeonComplete(map: MapDef, first: boolean) {
    const el = show(
      `<div class="center-col panel" style="max-width:560px">
        <h1 class="bigTitle win">Dungeon cleared</h1>
        <p>${escapeHtml(map.name)} is quiet at last.${first ? ' First clear: bonus experience!' : ''} Check your bag for the spoils.</p>
        <div class="row"><button class="btn" id="leave">Return to ${escapeHtml(MAPS[map.act].zones[0].name)}</button><button class="btn secondary" id="stay">Stay</button></div>
      </div>`,
    );
    on(el, '#stay', this.close);
    on(el, '#leave', () => {
      closeScreens();
      this.onTravel(map.act);
    });
  }

  actComplete(map: MapDef, unlocked: MapId | null) {
    const next = unlocked ? MAPS[unlocked] : map.next ? MAPS[map.next] : null;
    const el = show(
      `<div class="center-col panel" style="max-width:560px">
        <h1 class="bigTitle win">${escapeHtml(map.subtitle)} complete</h1>
        <p>${escapeHtml(map.name)} is safe.${next ? ` A new land is open: <b>${escapeHtml(next.name)}</b> (levels ${next.levels[0]}-${next.levels[1]}).` : ''}</p>
        ${next ? `<p class="sub">Speak with the Wayfinder beside the glowing waystone in ${escapeHtml(map.zones[0].name)} to travel.</p>` : ''}
        <div class="row">${next ? `<button class="btn" id="go">Travel to ${escapeHtml(next.name)}</button>` : ''}<button class="btn secondary" id="stay">Stay a while</button></div>
      </div>`,
    );
    on(el, '#stay', this.close);
    on(el, '#go', () => {
      closeScreens();
      if (next) this.onTravel(next.id);
    });
  }

  victory() {
    const g = this.game;
    const p = g.progress;
    const mins = Math.max(1, Math.round(p.playSeconds / 60));
    const el = show(
      `<div class="center-col panel" style="max-width:560px">
        <h1 class="bigTitle win">Victory</h1>
        <p>From the Vale to the Sunscar Dunes, the Wardens have returned. ${escapeHtml(p.name)} the ${p.spec ? SPECS[p.spec].name : CLASSES[p.cls].name} will be remembered.</p>
        <p class="sub">Completed in ${mins} minutes of play at level ${p.level}.</p>
        <p>Keep exploring, collect legendary gear, or start a new hero on another path.</p>
        <div class="row"><button class="btn" id="cont">Keep playing</button></div>
      </div>`,
    );
    on(el, '#cont', this.close);
  }
}

function cap(s: string) {
  return s[0].toUpperCase() + s.slice(1);
}

export function saveSetting(k: string, v: string) {
  try {
    localStorage.setItem(`ashenveil.${k}`, v);
  } catch {
    /* ignore */
  }
}

export function loadSetting(k: string): string | null {
  try {
    return localStorage.getItem(`ashenveil.${k}`);
  } catch {
    return null;
  }
}
