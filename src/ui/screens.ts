// Full-screen and panel UI: title, character creation, NPC dialogs, bag, quest log, menu, death, victory.
import { ABILITIES, CLASSES, POTION, type ClassId } from '../data/classes';
import { QUEST_BY_ID, type QuestDef } from '../data/quests';
import type { NpcDef } from '../data/world';
import type { Game } from '../game/game';
import { acceptQuest, questsForNpc, type Progress } from '../game/progress';
import { RARITY_COLORS, SLOTS, itemScore, type Item } from '../game/rules';
import { isSoundEnabled, play, setSoundEnabled } from '../game/audio';
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
       ${hasSave ? `<button class="btn" id="cont">Continue<br><small style="font-weight:600">${escapeHtml(hasSave.name)} - Level ${hasSave.level} ${CLASSES[hasSave.cls].name}</small></button>` : ''}
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
  return `${c.blurb} Abilities: ${c.abilities.map((a) => ABILITIES[a].name).join(', ')}.`;
}

function randomName() {
  const a = ['Aren', 'Kaela', 'Doran', 'Lysa', 'Torvin', 'Mirel', 'Cass', 'Edrin', 'Sera', 'Bryn'];
  return a[Math.floor(Math.random() * a.length)];
}

// ---------- In-game panels ----------

export class Panels {
  constructor(private game: Game, private onClose: () => void, private onQuit: () => void) {}

  private close = () => {
    closeScreens();
    this.onClose();
  };

  dialog(npc: NpcDef) {
    const g = this.game;
    const list = questsForNpc(g.progress, npc.id);
    const items = list
      .map(({ quest, status }) => `<button class="questItem ${status === 'active' ? 'dim' : ''}" data-q="${quest.id}">
         <span class="mk">${status === 'available' ? '!' : '?'}</span>
         <span><b>${escapeHtml(quest.title)}</b><br><small style="color:var(--ink-dim)">${status === 'available' ? 'New quest' : status === 'ready' ? 'Ready to complete' : 'In progress'}</small></span></button>`)
      .join('');
    const greet = npc.vendor
      ? 'Supplies for the road? Healing draughts, fresh from the herbalist. I also buy anything you do not need.'
      : list.length
        ? 'Well met, traveller.'
        : g.progress.bossDefeated
          ? 'The Vale owes you everything.'
          : 'Nothing for you right now. Grow stronger and come back to me.';
    const el = show(
      `<div class="panel dialog">
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
    const p = this.game.progress;
    const junk = p.bag.filter((i) => this.isJunk(i));
    return `<h3>Trade</h3>
      <div class="row">
        <button class="btn" id="buy" ${p.gold < POTION.price ? 'disabled' : ''}>Buy ${POTION.name} (${POTION.price}g)</button>
        <button class="btn secondary" id="sellJunk" ${junk.length ? '' : 'disabled'}>Sell ${junk.length} unneeded item${junk.length === 1 ? '' : 's'}</button>
      </div>
      <p class="sub" style="margin-top:8px">You have <span class="gold">${p.gold} gold</span> and ${p.potions} draught${p.potions === 1 ? '' : 's'}.</p>`;
  }

  /** An item is junk if it is worse than what is equipped in its slot. */
  private isJunk(i: Item) {
    return itemScore(i) <= itemScore(this.game.progress.gear[i.slot]);
  }

  private bindVendor(el: HTMLElement, npc: NpcDef) {
    on(el, '#buy', () => {
      this.game.buyPotion();
      this.dialog(npc);
    });
    on(el, '#sellJunk', () => {
      for (const i of this.game.progress.bag.filter((it) => this.isJunk(it))) this.game.sell(i);
      this.dialog(npc);
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
        <div class="reward"><span>Reward:</span><b>${q.xp} XP</b><span class="gold">${q.gold} gold</span>${q.rewardItemLevel ? '<b>+ an item</b>' : ''}</div>
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

  questLog() {
    const g = this.game;
    const active = g.progress.active
      .map((a) => {
        const q = QUEST_BY_ID[a.id];
        return `<div class="questItem" style="display:block"><b>${escapeHtml(q.title)}</b>
          <p style="margin:4px 0">${a.done ? `Complete. Return to ${escapeHtml(npcName(q.turnIn ?? q.giver))}.` : escapeHtml(q.progress)}</p>
          <small style="color:var(--ink-dim)">${escapeHtml(q.objective.label)}: ${a.progress}/${q.objective.count}</small></div>`;
      })
      .join('');
    const el = show(
      `<div class="panel dialog">
        <button class="close" id="x" aria-label="Close">&times;</button>
        <h2>Quest Log</h2>
        <p class="sub">${g.progress.completed.length} of 7 quests completed. Gold markers on the minimap show where to go.</p>
        <div class="questList">${active || '<p>No active quests. Look for villagers with a <b style="color:#ffd84a">!</b> above their heads.</p>'}</div>
      </div>`,
    );
    on(el, '#x', this.close);
  }

  bag(selected?: string) {
    const g = this.game;
    const p = g.progress;
    const st = g.stats;
    const card = (it: Item | null, slotLabel?: string) => {
      if (!it) return `<div class="itemCard empty">${slotLabel ?? ''} - empty</div>`;
      const cur = p.gear[it.slot];
      const diff = slotLabel ? 0 : Math.round(itemScore(it) - itemScore(cur));
      const cmp = slotLabel ? '' : diff > 0 ? `<span class="up">+${diff} upgrade</span>` : diff < 0 ? `<span class="down">${diff}</span>` : '';
      return `<button class="itemCard ${selected === it.id ? 'sel' : ''}" data-id="${it.id}" ${slotLabel ? 'disabled' : ''}>
        <span class="in" style="color:${RARITY_COLORS[it.rarity]}">${escapeHtml(it.name)}</span>
        <span class="is">${slotLabel ?? cap(it.slot)} - item level ${it.ilvl}</span>
        <span class="is">${[it.power ? `+${it.power} Power` : '', it.stamina ? `+${it.stamina} Stamina` : '', it.armor ? `+${it.armor} Armor` : ''].filter(Boolean).join(', ')}</span>
        ${cmp}
      </button>`;
    };
    const sel = p.bag.find((i) => i.id === selected);
    const el = show(
      `<div class="panel bagPanel">
        <button class="close" id="x" aria-label="Close">&times;</button>
        <h2>${escapeHtml(p.name)}</h2>
        <p class="sub">Level ${p.level} ${CLASSES[p.cls].name} - <span class="gold">${p.gold} gold</span> - ${p.potions} healing draught${p.potions === 1 ? '' : 's'}</p>
        <div class="bagCols">
          <div>
            <h3>Stats</h3>
            <div class="stats">
              <div><span>Health</span><b>${st.maxHp}</b></div>
              <div><span>Power</span><b>${st.power}</b></div>
              <div><span>Armor</span><b>${st.armor}</b></div>
              <div><span>Damage bonus</span><b>+${Math.round((st.damageScale - 1) * 100)}%</b></div>
            </div>
            <h3>Equipped</h3>
            <div class="slots">${SLOTS.map((s) => card(p.gear[s], cap(s))).join('')}</div>
          </div>
          <div>
            <h3>Bag (${p.bag.length}/16)</h3>
            ${sel ? `<div class="row" style="margin-bottom:8px"><button class="btn" id="equip">Equip</button><button class="btn secondary" id="sell">Sell for ${sel.value}g</button></div>` : '<p class="sub" style="margin-bottom:8px">Tap an item to equip or sell it.</p>'}
            <div class="bag">${p.bag.map((i) => card(i)).join('') || '<p class="sub">Your bag is empty. Defeated enemies sometimes drop items.</p>'}</div>
          </div>
        </div>
      </div>`,
    );
    on(el, '#x', this.close);
    on(el, '.bag .itemCard', (_e, b) => this.bag(b.dataset.id === selected ? undefined : b.dataset.id));
    on(el, '#equip', () => {
      if (sel) g.equip(sel);
      this.bag();
    });
    on(el, '#sell', () => {
      if (sel) g.sell(sel);
      this.bag();
    });
  }

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
        <h3>How to play</h3>
        <p style="font-size:13px">Move with your left thumb and drag with your right to look around. Tap an enemy to target it and press <b>Attack</b>. Your abilities are around the Attack button. Step out of red circles on the ground. When hurt, drink a healing draught (the red button).</p>
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
  }

  death(onRelease: () => void) {
    const el = show(
      `<div class="center-col">
        <h1 class="bigTitle">You have fallen</h1>
        <p>Your spirit returns to Hearthmoor. Enemies that defeated you have returned to their posts.</p>
        <button class="btn" id="rel">Return to Hearthmoor</button>
      </div>`,
    );
    on(el, '#rel', () => {
      closeScreens();
      onRelease();
    });
  }

  victory() {
    const g = this.game;
    const mins = Math.max(1, Math.round(g.progress.playSeconds / 60));
    const el = show(
      `<div class="center-col panel" style="max-width:560px">
        <h1 class="bigTitle win">Victory</h1>
        <p>Varkul the Cindermaw is no more, and the Vale of Ashenveil is safe. ${escapeHtml(g.progress.name)} the ${CLASSES[g.progress.cls].name} will be remembered.</p>
        <p class="sub">Completed in ${mins} minutes of play at level ${g.progress.level}.</p>
        <p>You can keep exploring, gather better gear, or start a new hero with a different class.</p>
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
