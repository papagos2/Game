// In-game HUD: unit frames, action buttons, minimap, quest tracker, nameplates and floating text.
import * as THREE from 'three';
import { ABILITIES, CLASSES, POTION, SPECS } from '../data/classes';
import { QUEST_BY_ID } from '../data/quests';
import { ALL_NPCS, MOBS, WORLD_LIMIT } from '../data/world';
import { abilityCost } from '../game/abilities';
import { MATERIALS } from '../data/economy';
import { activeBuffs } from '../game/economy';
import type { Game } from '../game/game';
import { abilityLearned, canChooseSpec, nextQuestLevel, npcMarker, pointsAvailable } from '../game/progress';
import { levelColor, xpToNext } from '../game/rules';
import type { Unit } from '../game/units';
import { iconHtml } from './icons';

const $ = <T extends HTMLElement = HTMLElement>(id: string) => document.getElementById(id) as T;

interface Floater { el: HTMLDivElement; pos: THREE.Vector3; t0: number; dx: number }
interface Plate { el: HTMLDivElement; name: HTMLDivElement; bar: HTMLDivElement | null; fill: HTMLDivElement | null; mk: HTMLDivElement | null; key: string }

const V = new THREE.Vector3();

export class Hud {
  private floaters: Floater[] = [];
  private plates = new Map<Unit, Plate>();
  private msgTimer = 0;
  private bannerTimer = 0;
  private buttons: { el: HTMLButtonElement; cd: HTMLDivElement; cdText: HTMLDivElement; kind: string; count?: HTMLDivElement }[] = [];
  private mini: CanvasRenderingContext2D;
  private last: Record<string, string | number> = {};
  private barKey = '';

  constructor(private game: Game, handlers: { bag: () => void; quests: () => void; menu: () => void; talk: () => void; skills: () => void }) {
    const cls = CLASSES[game.progress.cls];
    $('pRes').className = `fill ${cls.resource}`;
    this.mini = ($('minimap') as HTMLCanvasElement).getContext('2d')!;
    this.buildActionBar();
    $('btnSkills').onclick = handlers.skills;
    $('btnBag').onclick = handlers.bag;
    $('btnQuests').onclick = handlers.quests;
    $('btnMenu').onclick = handlers.menu;
    $('talkBtn').onclick = handlers.talk;
    // Tap a tracked quest to auto-travel to it.
    $('tracker').addEventListener('click', (e) => {
      const b = (e.target as HTMLElement).closest<HTMLElement>('[data-go]');
      if (b) game.travelTo(b.dataset.go || null);
    });
    $('targetFrame').onclick = () => {
      const t = game.player.target;
      if (t?.npc && t.distTo(game.player) < 7) handlers.talk();
    };
    this.refreshTracker();
  }

  private buildActionBar() {
    const bar = $('actionBar');
    bar.innerHTML = '';
    this.buttons = [];
    const g = this.game;
    const cls = CLASSES[g.progress.cls];
    const spec = g.progress.spec;
    this.barKey = `${spec}`;
    const portrait = spec ? iconHtml(SPECS[spec].glyph, [SPECS[spec].color, cls.colors.body]) : iconHtml(ABILITIES[cls.abilities[0]].glyph, [cls.colors.glow, cls.colors.body]);
    $('pPortrait').innerHTML = portrait;
    const make = (cls2: string, x: number, y: number, html: string, kind: string, onPress: () => void, label: string) => {
      const b = document.createElement('button');
      b.className = `ab ${cls2}`;
      b.style.right = `${x}px`;
      b.style.bottom = `${y}px`;
      b.setAttribute('aria-label', label);
      b.innerHTML = `${html}<div class="cd"></div><div class="cdText"></div>`;
      b.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        e.stopPropagation();
        b.classList.add('pressed');
        onPress();
      });
      const up = () => b.classList.remove('pressed');
      b.addEventListener('pointerup', up);
      b.addEventListener('pointerleave', up);
      b.addEventListener('pointercancel', up);
      bar.appendChild(b);
      const entry = { el: b, cd: b.querySelector('.cd') as HTMLDivElement, cdText: b.querySelector('.cdText') as HTMLDivElement, kind, count: undefined as HTMLDivElement | undefined };
      this.buttons.push(entry);
      return entry;
    };
    make('ab-main', 0, 0, `${iconHtml(cls.ranged ? 'bolt' : 'sword', [cls.colors.glow, '#2a2030'])}<div class="label">ATTACK</div>`, 'main', () => g.mainAction(), 'Attack');
    // Inner arc: four class abilities. Outer arc: the two abilities of the chosen path.
    const pos: [number, number][] = [[149, 14], [131, 82], [82, 131], [14, 149], [209, 86], [112, 197]];
    g.abilityList().forEach((ab, i) => {
      const e = make(i < 4 ? 'ab-s' : 'ab-s ab-spec', pos[i][0], pos[i][1], iconHtml(ab.glyph, ab.color), ab.id, () => g.useAbility(ab.id), ab.name);
      const lock = document.createElement('div');
      lock.className = 'lockText';
      lock.textContent = `Lv ${ab.unlockLevel}`;
      e.el.appendChild(lock);
    });
    const p = make('ab-p', 232, 10, iconHtml(POTION.glyph, POTION.color), 'potion', () => g.usePotion(), POTION.name);
    const c = document.createElement('div');
    c.className = 'count';
    p.el.appendChild(c);
    p.count = c;
    const m = make('ab-p', 292, 12, iconHtml('horse', ['#e0c8a0', '#4a3a2a']), 'mount', () => g.toggleMount(), 'Mount');
    const ml = document.createElement('div');
    ml.className = 'lockText';
    ml.textContent = `Lv 5`;
    m.el.appendChild(ml);
    const t = make('ab-p', 282, 150, iconHtml('leap', ['#d8d0c0', '#3a3440']), 'cycle', () => g.cycleTarget(), 'Next target');
    t.el.querySelector('.icon')!.innerHTML = '<svg viewBox="0 0 100 100"><circle cx="50" cy="50" r="26" fill="none" stroke="#fff" stroke-width="7"/><path d="M50 8v22M50 70v22M8 50h22M70 50h22" stroke="#fff" stroke-width="7" stroke-linecap="round"/></svg>';
  }

  private set(id: string, prop: 'text' | 'width' | 'html', value: string | number) {
    const key = id + prop;
    if (this.last[key] === value) return;
    this.last[key] = value;
    const el = $(id);
    if (prop === 'text') el.textContent = String(value);
    else if (prop === 'html') el.innerHTML = String(value);
    else el.style.transform = `scaleX(${value})`;
  }

  message(text: string) {
    const el = $('message');
    el.textContent = text;
    el.classList.add('show');
    this.msgTimer = 2;
  }

  banner(title: string, sub?: string) {
    const el = $('banner');
    el.innerHTML = `${escapeHtml(title)}${sub ? `<small>${escapeHtml(sub)}</small>` : ''}`;
    el.classList.add('show');
    this.bannerTimer = 3.2;
  }

  toast(html: string) {
    const box = $('toasts');
    const el = document.createElement('div');
    el.className = 'toast';
    el.innerHTML = html;
    box.prepend(el);
    while (box.children.length > 4) box.lastElementChild!.remove();
    setTimeout(() => el.remove(), 3300);
  }

  floatText(pos: THREE.Vector3, text: string, color: string, cls?: string) {
    const el = document.createElement('div');
    el.className = `float${cls ? ` ${cls}` : ''}`;
    el.textContent = text;
    el.style.color = color;
    $('plates').appendChild(el);
    this.floaters.push({ el, pos: pos.clone(), t0: performance.now(), dx: (Math.random() - 0.5) * 40 });
    if (this.floaters.length > 40) this.floaters.shift()!.el.remove();
  }

  refreshTracker() {
    const g = this.game;
    const list = g.questCounts();
    if (!list.length) {
      const p = g.progress;
      const hub = g.map.zones[0].name;
      const hasOffer = g.npcs.some((n) => npcMarker(p, n.npc!.id) !== '');
      const next = nextQuestLevel(p, g.map.act);
      let hint = '';
      if (hasOffer) hint = `<button class="tq go" data-go=""><b>${escapeHtml(hub)}</b><span>Talk to the villagers marked with <b style="display:inline;color:#ffd84a">!</b></span><i>GO</i></button>`;
      else if (next) hint = `<div class="tq"><b>Grow stronger</b><span>New quests at level ${next}. Hunt in the wilds.</span></div>`;
      $('tracker').innerHTML = hint;
      return;
    }
    $('tracker').innerHTML = list
      .map(({ state, def }) => `<button class="tq go ${state.done ? 'ready' : ''}" data-go="${def.id}"><b>${escapeHtml(def.title)}</b><span>${state.done ? 'Return to ' + npcName(def.turnIn ?? def.giver) : `${def.objective.label}: ${state.progress}/${def.objective.count}`}</span><i>GO</i></button>`)
      .join('');
  }

  update(dt: number) {
    const g = this.game;
    const pl = g.player;
    const p = g.progress;
    const cls = CLASSES[p.cls];
    if (this.barKey !== `${p.spec}`) this.buildActionBar();
    // Skills badge: unspent points or a path to choose.
    const pts = pointsAvailable(p);
    this.set('skillBadge', 'text', canChooseSpec(p) ? '!' : pts > 0 ? String(pts) : '');
    $('skillBadge').hidden = !(canChooseSpec(p) || pts > 0);
    // Player frame.
    this.set('pName', 'html', `${escapeHtml(p.name)}${p.title ? ` <small class="ptitle">${escapeHtml(p.title)}</small>` : ''}`);
    this.set('pLevel', 'text', `Lv ${p.level}`);
    this.set('pHp', 'width', (pl.hp / pl.maxHp).toFixed(3));
    this.set('pHpText', 'text', `${Math.ceil(pl.hp)} / ${pl.maxHp}`);
    const shieldFrac = pl.shield > 0 && pl.shieldUntil > g.now ? Math.min(1, pl.shield / pl.maxHp) : 0;
    this.set('pShield', 'width', shieldFrac.toFixed(3));
    this.set('pRes', 'width', (g.resource / g.maxResource).toFixed(3));
    this.set('pResText', 'text', `${cls.resourceName} ${Math.floor(g.resource)}`);
    const buffs: string[] = [];
    if (shieldFrac > 0) buffs.push(`<div class="buff">${iconHtml('shield', ABILITIES.staticGuard.color)}</div>`);
    if (pl.dots.some((d) => d.heal)) buffs.push(`<div class="buff">${iconHtml('leaf', ABILITIES.renewal.color)}</div>`);
    if (pl.dots.some((d) => !d.heal)) buffs.push(`<div class="buff">${iconHtml('thorn', ['#9be26a', '#3a1a3a'])}</div>`);
    for (const b of activeBuffs(p)) buffs.push(`<div class="buff">${iconHtml(b.id === 'might' ? 'flame' : 'shield', b.id === 'might' ? ['#ffb08a', '#7a1b0b'] : ['#d8d0c0', '#4a4450'])}</div>`);
    if (g.mounted) buffs.push(`<div class="buff">${iconHtml('horse', ['#e0c8a0', '#4a3a2a'])}</div>`);
    if (g.pets.length) buffs.push(`<div class="buff">${iconHtml('paw', ABILITIES.spiritWolf.color)}</div>`);
    if (pl.guardUntil > g.now) buffs.push(`<div class="buff">${iconHtml('shield', ['#e6d7b0', '#5a4a2a'])}</div>`);
    if (pl.slowed(g.now)) buffs.push(`<div class="buff">${iconHtml('snow', ['#bfefff', '#1a4a8a'])}</div>`);
    this.set('pBuffs', 'html', buffs.join(''));

    // Target frame.
    const t = pl.target;
    const tf = $('targetFrame');
    if (t && (!t.dead || g.now < t.corpseUntil)) {
      tf.hidden = false;
      this.set('tName', 'text', t.name);
      const lvColor = t.team === 'enemy' ? levelColor(t.level, p.level) : '#ffd66e';
      this.set('tLevel', 'html', t.team === 'npc' ? '' : `<span style="color:${lvColor}">${t.elite ? 'Elite ' : ''}Lv ${t.level}</span>`);
      const bar = tf.querySelector('.bar.hp')!;
      bar.className = `bar hp ${t.team === 'enemy' ? 'enemy' : t.team === 'npc' ? 'friendly' : ''}`;
      this.set('tHp', 'width', (t.hp / t.maxHp).toFixed(3));
      this.set('tHpText', 'text', t.team === 'npc' ? '' : t.dead ? 'Dead' : `${Math.ceil(t.hp)} / ${t.maxHp}`);
      this.set('tSub', 'text', t.npc ? t.npc.title : t.dead ? '' : t.stunned(g.now) ? 'Stunned' : t.rooted(g.now) ? 'Rooted' : '');
    } else {
      tf.hidden = true;
    }

    // Action buttons.
    for (const b of this.buttons) {
      if (b.kind === 'main') {
        b.el.style.boxShadow = g.autoAttack && t && !t.dead ? '0 0 0 3px #ffd84a, 0 0 18px rgba(255,200,80,0.6)' : '';
        continue;
      }
      if (b.kind === 'cycle') continue;
      if (b.kind === 'mount') {
        b.el.classList.toggle('locked', !g.canMount());
        b.el.style.boxShadow = g.mounted ? '0 0 0 3px #ffd84a' : '';
        continue;
      }
      let left = 0;
      let total = 1;
      if (b.kind === 'potion') {
        left = g.cooldownLeft('potion');
        total = POTION.cooldown;
        const c = String(p.potions);
        if (b.count && b.count.textContent !== c) b.count.textContent = c;
        b.el.classList.toggle('nores', p.potions <= 0);
      } else {
        const ab = ABILITIES[b.kind as keyof typeof ABILITIES];
        left = g.cooldownLeft(ab.id);
        total = g.cooldownTotal(ab.id);
        b.el.classList.toggle('locked', !abilityLearned(p, ab.id));
        b.el.classList.toggle('nores', g.resource < abilityCost(g, ab.id));
      }
      const frac = left > 0 ? left / total : 0;
      const bg = frac > 0 ? `conic-gradient(rgba(0,0,0,0.68) ${frac * 360}deg, transparent 0)` : 'none';
      if (b.cd.style.background !== bg) b.cd.style.background = bg;
      const txt = left > 0 ? (left > 1 ? String(Math.ceil(left)) : left.toFixed(1)) : '';
      if (b.cdText.textContent !== txt) b.cdText.textContent = txt;
    }

    // Talk button.
    const npc = !pl.dead ? g.nearestNpc(6) : null;
    const node = !pl.dead && !npc ? g.nearestNode(4) : null;
    const talk = $('talkBtn');
    talk.hidden = !npc && !node;
    if (npc) this.set('talkBtn', 'text', `Talk to ${npc.name}`);
    else if (node) this.set('talkBtn', 'text', `Gather ${MATERIALS[node.mat].name}`);

    // XP.
    const need = xpToNext(p.level);
    this.set('xpFill', 'width', need ? (p.xp / need).toFixed(3) : 1);
    this.set('xpText', 'text', need ? `Level ${p.level}  -  ${Math.floor(p.xp)} / ${need} XP` : `Level ${p.level} (max)`);
    this.set('zoneName', 'text', g.zoneName);

    // Timers.
    if (this.msgTimer > 0 && (this.msgTimer -= dt) <= 0) $('message').classList.remove('show');
    if (this.bannerTimer > 0 && (this.bannerTimer -= dt) <= 0) $('banner').classList.remove('show');

    this.updatePlates();
    this.updateFloaters();
    this.drawMinimap();
  }

  private project(pos: THREE.Vector3, yOff: number): { x: number; y: number } | null {
    V.set(pos.x, pos.y + yOff, pos.z).project(this.game.camera);
    if (V.z > 1 || V.z < -1) return null;
    return { x: (V.x * 0.5 + 0.5) * window.innerWidth, y: (-V.y * 0.5 + 0.5) * window.innerHeight };
  }

  private updatePlates() {
    const g = this.game;
    const pl = g.player;
    const seen = new Set<Unit>();
    const candidates: Unit[] = [...g.npcs];
    for (const u of g.units) if (!u.dead && u.distTo(pl) < 38) candidates.push(u);
    candidates.push(...g.pets);
    for (const u of candidates) {
      const d = u.distTo(pl);
      if (d > 45) continue;
      const scale = u.rig.root.scale.y;
      const s = this.project(u.pos, u.rig.height * scale + 0.55);
      if (!s) continue;
      seen.add(u);
      let plate = this.plates.get(u);
      if (!plate) plate = this.makePlate(u);
      const key = u.team === 'npc' ? npcMarker(g.progress, u.npc!.id) : `${u.level}|${u.name}|${levelColor(u.level, g.progress.level)}`;
      if (plate.key !== key) {
        plate.key = key;
        if (u.team === 'npc') {
          plate.mk!.textContent = key;
          plate.mk!.style.display = key ? '' : 'none';
        } else if (u.team === 'enemy') {
          plate.name.textContent = `${u.name}`;
          plate.name.style.color = levelColor(u.level, g.progress.level);
        }
      }
      if (plate.fill) plate.fill.style.transform = `scaleX(${Math.max(0, u.hp / u.maxHp).toFixed(3)})`;
      if (plate.bar) plate.bar.style.visibility = u.team === 'enemy' && (u.hp < u.maxHp || u === pl.target) ? 'visible' : 'hidden';
      plate.el.classList.toggle('targeted', pl.target === u);
      const k = Math.max(0.65, Math.min(1.1, 18 / Math.max(8, d)));
      plate.el.style.transform = `translate(${s.x}px, ${s.y}px) translate(-50%, -100%) scale(${k.toFixed(2)})`;
      plate.el.style.display = '';
    }
    for (const [u, plate] of this.plates) {
      if (!seen.has(u)) {
        if (!g.units.includes(u) && !g.npcs.includes(u) && !g.pets.includes(u)) {
          plate.el.remove();
          this.plates.delete(u);
        } else plate.el.style.display = 'none';
      }
    }
  }

  private makePlate(u: Unit): Plate {
    const el = document.createElement('div');
    el.className = 'plate';
    let mk: HTMLDivElement | null = null;
    if (u.team === 'npc') {
      mk = document.createElement('div');
      mk.className = 'mk';
      el.appendChild(mk);
    }
    const name = document.createElement('div');
    name.className = 'pn';
    name.textContent = u.name;
    name.style.color = u.team === 'npc' ? '#8fe08a' : u.team === 'player' ? '#9fe8ff' : '#ff9b8a';
    el.appendChild(name);
    if (u.npc) {
      const title = document.createElement('div');
      title.className = 'pt';
      title.textContent = `<${u.npc.title}>`;
      el.appendChild(title);
    }
    let bar: HTMLDivElement | null = null;
    let fill: HTMLDivElement | null = null;
    if (u.team !== 'npc') {
      bar = document.createElement('div');
      bar.className = 'pb';
      fill = document.createElement('div');
      if (u.team === 'player') fill.style.background = '#6ee0ff';
      bar.appendChild(fill);
      el.appendChild(bar);
    }
    $('plates').appendChild(el);
    const plate = { el, name, bar, fill, mk, key: '' };
    this.plates.set(u, plate);
    return plate;
  }

  private updateFloaters() {
    const now = performance.now();
    this.floaters = this.floaters.filter((f) => {
      const k = (now - f.t0) / 1100;
      if (k >= 1) {
        f.el.remove();
        return false;
      }
      const s = this.project(f.pos, k * 1.6);
      if (!s) {
        f.el.style.opacity = '0';
        return true;
      }
      f.el.style.opacity = String(k < 0.7 ? 1 : 1 - (k - 0.7) / 0.3);
      const pop = k < 0.12 ? 1 + (0.12 - k) * 4 : 1;
      f.el.style.transform = `translate(${s.x + f.dx * k}px, ${s.y}px) translate(-50%, -50%) scale(${pop.toFixed(2)})`;
      return true;
    });
  }

  private drawMinimap() {
    const g = this.game;
    const ctx = this.mini;
    const W = ctx.canvas.width;
    const base = g.world.minimap;
    const view = 170; // metres shown across
    const px = g.player.pos.x;
    const pz = g.player.pos.z;
    const scale = base.width / (WORLD_LIMIT * 2);
    const sx = (px + WORLD_LIMIT - view / 2) * scale;
    const sy = (pz + WORLD_LIMIT - view / 2) * scale;
    ctx.save();
    ctx.fillStyle = '#1b1a1a';
    ctx.fillRect(0, 0, W, W);
    ctx.beginPath();
    ctx.arc(W / 2, W / 2, W / 2, 0, Math.PI * 2);
    ctx.clip();
    ctx.imageSmoothingEnabled = true;
    ctx.drawImage(base, sx, sy, view * scale, view * scale, 0, 0, W, W);
    const toMap = (x: number, z: number) => ({ x: ((x - px) / view + 0.5) * W, y: ((z - pz) / view + 0.5) * W });

    // Quest objectives: highlight camps of mobs we still need, or the NPC to return to.
    const goals: { x: number; z: number; r: number }[] = [];
    for (const a of g.progress.active) {
      const q = QUEST_BY_ID[a.id];
      if (a.done) {
        const n = g.npcs.find((n2) => n2.npc!.id === (q.turnIn ?? q.giver));
        if (n) goals.push({ x: n.pos.x, z: n.pos.z, r: 0 });
        continue;
      }
      if (q.map !== g.map.id) continue;
      const o = q.objective;
      if (o.type === 'explore') {
        const zn = g.map.zones.find((z) => z.id === o.zone);
        if (zn) goals.push({ x: zn.x, z: zn.z, r: zn.radius * 0.5 });
        continue;
      }
      const kind = o.type === 'kill' ? o.mob : Object.values(MOBS).find((m) => m.questDrop?.item === o.item)?.kind;
      for (const c of g.map.camps) if (c.kind === kind) goals.push({ x: c.x, z: c.z, r: Math.max(8, c.spread) });
    }
    ctx.lineWidth = 2;
    for (const goal of goals) {
      const m = toMap(goal.x, goal.z);
      const r = (goal.r / view) * W;
      const inside = Math.hypot(m.x - W / 2, m.y - W / 2) < W / 2 - 6;
      if (inside) {
        ctx.fillStyle = 'rgba(255, 216, 74, 0.22)';
        ctx.strokeStyle = 'rgba(255, 216, 74, 0.9)';
        ctx.beginPath();
        ctx.arc(m.x, m.y, Math.max(5, r), 0, Math.PI * 2);
        ctx.fill();
        ctx.stroke();
      } else {
        const a = Math.atan2(m.y - W / 2, m.x - W / 2);
        const ex = W / 2 + Math.cos(a) * (W / 2 - 9);
        const ey = W / 2 + Math.sin(a) * (W / 2 - 9);
        ctx.fillStyle = '#ffd84a';
        ctx.beginPath();
        ctx.moveTo(ex + Math.cos(a) * 7, ey + Math.sin(a) * 7);
        ctx.lineTo(ex + Math.cos(a + 2.4) * 6, ey + Math.sin(a + 2.4) * 6);
        ctx.lineTo(ex + Math.cos(a - 2.4) * 6, ey + Math.sin(a - 2.4) * 6);
        ctx.fill();
      }
    }
    // Enemies.
    for (const u of g.units) {
      if (u.dead || Math.abs(u.pos.x - px) > view / 2 || Math.abs(u.pos.z - pz) > view / 2) continue;
      const m = toMap(u.pos.x, u.pos.z);
      ctx.fillStyle = u.state === 'chase' ? '#ff3b2a' : levelColor(u.level, g.progress.level);
      ctx.fillRect(m.x - 1.5, m.y - 1.5, 3, 3);
    }
    // NPC markers.
    ctx.font = 'bold 13px Cinzel, Georgia, serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    for (const n of g.npcs) {
      const m = toMap(n.pos.x, n.pos.z);
      const mk = npcMarker(g.progress, n.npc!.id);
      if (mk) {
        ctx.fillStyle = '#000';
        ctx.fillText(mk, m.x + 1, m.y + 1);
        ctx.fillStyle = '#ffd84a';
        ctx.fillText(mk, m.x, m.y);
      } else {
        ctx.fillStyle = '#8fe08a';
        ctx.fillRect(m.x - 1.5, m.y - 1.5, 3, 3);
      }
    }
    ctx.restore();
    // Player arrow (camera-relative heading on a north-up map).
    ctx.save();
    ctx.translate(W / 2, W / 2);
    ctx.rotate(-g.player.facing + Math.PI);
    ctx.fillStyle = '#fff';
    ctx.strokeStyle = '#000';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.moveTo(0, -7);
    ctx.lineTo(5, 5);
    ctx.lineTo(0, 2);
    ctx.lineTo(-5, 5);
    ctx.closePath();
    ctx.fill();
    ctx.stroke();
    ctx.restore();
    // North marker
    ctx.fillStyle = '#d8b36a';
    ctx.font = 'bold 10px system-ui';
    ctx.textAlign = 'center';
    ctx.fillText('N', W / 2, 9);
  }

  destroy() {
    for (const p of this.plates.values()) p.el.remove();
    this.plates.clear();
    for (const f of this.floaters) f.el.remove();
    this.floaters = [];
  }
}

export function npcName(id: string) {
  return ALL_NPCS.find((n) => n.id === id)?.name ?? id;
}

export function escapeHtml(s: string) {
  return s.replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]!);
}
