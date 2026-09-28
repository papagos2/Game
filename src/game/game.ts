// The running game: world, units, combat, AI, abilities, camera and saving.
import * as THREE from 'three';
import { ABILITIES, CLASSES, POTION, type AbilityDef, type AbilityId } from '../data/classes';
import { QUEST_BY_ID, QUEST_ITEMS } from '../data/quests';
import { CAMPS, MOBS, NPCS, SPAWN, WORLD_LIMIT, DEFAULT_ZONE, type MobKind, type NpcDef } from '../data/world';
import { play } from './audio';
import { animateRig, buildMobModel, buildNpcModel, buildPlayerModel, buildSpiritWolf, flashRig } from './models';
import {
  BAG_SIZE, addXp, completeQuest, onItemLooted, onMobKilled, saveGame, wantsQuestItem, type Progress,
} from './progress';
import {
  MAX_LEVEL, levelColor, levelMod, makeItem, makeRng, mitigation, mobStats, mobXp, playerStats, rollMobLoot,
  RARITY_COLORS, type Item, type PlayerStats, type Rng,
} from './rules';
import { ColliderGrid, buildWorld, type WorldScene } from './scene';
import { heightAt, smoothstep, zoneAt } from './terrain';
import { Unit } from './units';
import type { Input } from '../ui/input';

export interface GameUI {
  message(text: string): void;
  banner(title: string, sub?: string): void;
  toast(html: string): void;
  floatText(pos: THREE.Vector3, text: string, color: string, cls?: string): void;
  openDialog(npc: NpcDef): void;
  questsChanged(): void;
  playerDied(): void;
  victory(): void;
}

interface Projectile { mesh: THREE.Mesh; target: Unit; speed: number; onHit: () => void }
interface Effect { obj: THREE.Object3D; t0: number; dur: number; update?: (k: number) => void }
interface Telegraph { x: number; z: number; r: number; t0: number; at: number; mesh: THREE.Mesh; fill: THREE.Mesh; owner: Unit | null; fire: () => void }

export type Quality = 'high' | 'low';

const PLAYER_SPEED = 7.5;
const TMP = new THREE.Vector3();

export class Game {
  renderer: THREE.WebGLRenderer;
  scene = new THREE.Scene();
  camera: THREE.PerspectiveCamera;
  sun: THREE.DirectionalLight;
  world: WorldScene;
  grid: ColliderGrid;
  player: Unit;
  pet: Unit | null = null;
  units: Unit[] = [];
  npcs: Unit[] = [];
  stats!: PlayerStats;
  resource = 0;
  cooldowns: Partial<Record<AbilityId | 'potion', number>> = {};
  autoAttack = false;
  now = 0;
  paused = true;
  cam = { yaw: 0, pitch: 0.55, dist: 13 };
  zoneName = '';
  rng: Rng = makeRng(Date.now() & 0xffffff);
  leap: { from: THREE.Vector3; to: THREE.Vector3; t: number; target: Unit } | null = null;
  bossPhase = 0;
  private projectiles: Projectile[] = [];
  private effects: Effect[] = [];
  private telegraphs: Telegraph[] = [];
  private sky: THREE.Mesh;
  private fogDefault = new THREE.Color('#a9bfd0');
  private fogScar = new THREE.Color('#5a3a30');
  private lastSave = 0;
  private raf = 0;
  private lastT = 0;
  private disposed = false;

  constructor(
    canvas: HTMLCanvasElement,
    private ui: GameUI,
    public progress: Progress,
    private input: Input,
    public quality: Quality,
  ) {
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: quality === 'high', powerPreference: 'high-performance' });
    this.renderer.outputColorSpace = THREE.SRGBColorSpace;
    this.camera = new THREE.PerspectiveCamera(60, 1, 0.3, 420);
    this.scene.fog = new THREE.Fog(this.fogDefault.clone(), 70, 230);
    this.scene.background = this.fogDefault.clone();

    const hemi = new THREE.HemisphereLight('#dbe8ff', '#5a4a36', 1.25);
    this.scene.add(hemi);
    this.sun = new THREE.DirectionalLight('#fff1d6', 2.1);
    this.sun.shadow.mapSize.set(1024, 1024);
    const sc = this.sun.shadow.camera;
    sc.left = -40; sc.right = 40; sc.top = 40; sc.bottom = -40; sc.near = 1; sc.far = 160;
    this.sun.shadow.bias = -0.0008;
    this.scene.add(this.sun, this.sun.target);

    this.sky = this.buildSky();
    this.scene.add(this.sky);

    this.world = buildWorld();
    this.scene.add(this.world.root);
    this.grid = new ColliderGrid(this.world.colliders);
    this.applyQuality();

    const cls = CLASSES[progress.cls];
    this.player = new Unit(progress.name, 'player', progress.level, 'player', buildPlayerModel(progress.cls));
    this.player.attackRange = cls.attackRange;
    this.player.attackSpeed = cls.attackSpeed;
    this.player.speed = PLAYER_SPEED;
    this.recomputeStats();
    this.player.hp = this.player.maxHp;
    this.resource = cls.resource === 'mana' ? cls.resourceMax : 0;
    this.placeUnit(this.player, progress.pos.x, progress.pos.z);
    this.cam.yaw = 0;
    this.scene.add(this.player.rig.root);

    this.spawnNpcs();
    this.spawnMobs();
    this.resize();
    window.addEventListener('resize', this.resize);
  }

  // ---------- Setup ----------

  private buildSky(): THREE.Mesh {
    const mat = new THREE.ShaderMaterial({
      side: THREE.BackSide,
      depthWrite: false,
      fog: false,
      uniforms: { top: { value: new THREE.Color('#5f8fc9') }, bottom: { value: this.fogDefault.clone() } },
      vertexShader: 'varying float vY; void main(){ vY = normalize(position).y; gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); }',
      fragmentShader: 'uniform vec3 top; uniform vec3 bottom; varying float vY; void main(){ float t = smoothstep(-0.05, 0.5, vY); gl_FragColor = vec4(mix(bottom, top, t), 1.0); }',
    });
    const m = new THREE.Mesh(new THREE.SphereGeometry(400, 24, 12), mat);
    m.renderOrder = -1;
    return m;
  }

  applyQuality() {
    const high = this.quality === 'high';
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, high ? 2 : 1.25));
    this.renderer.shadowMap.enabled = high;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    this.sun.castShadow = high;
    const fog = this.scene.fog as THREE.Fog;
    fog.near = high ? 70 : 50;
    fog.far = high ? 230 : 170;
    this.world.root.traverse((o) => {
      const m = o as THREE.Mesh;
      if (m.isMesh || (m as unknown as THREE.InstancedMesh).isInstancedMesh) {
        (m.material as THREE.Material).needsUpdate = true;
      }
    });
  }

  setQuality(q: Quality) {
    this.quality = q;
    this.applyQuality();
  }

  resize = () => {
    const w = window.innerWidth;
    const h = window.innerHeight;
    this.renderer.setSize(w, h, false);
    this.camera.aspect = w / h;
    this.camera.fov = w / h < 1.3 ? 70 : 60;
    this.camera.updateProjectionMatrix();
  };

  private placeUnit(u: Unit, x: number, z: number) {
    u.pos.set(x, heightAt(x, z), z);
    u.rig.root.position.copy(u.pos);
  }

  private spawnNpcs() {
    for (const n of NPCS) {
      const u = new Unit(n.name, 'npc', 10, 'npc', buildNpcModel(n.color), null, n);
      this.placeUnit(u, n.x, n.z);
      u.facing = Math.atan2(0 - n.x, 150 - n.z);
      u.hp = u.maxHp = 1000;
      u.rig.root.rotation.y = u.facing;
      this.scene.add(u.rig.root);
      this.npcs.push(u);
    }
  }

  private spawnMobs() {
    const rng = makeRng(4242);
    CAMPS.forEach((camp, ci) => {
      for (let i = 0; i < camp.count; i++) {
        const a = rng() * Math.PI * 2;
        const r = Math.sqrt(rng()) * camp.spread;
        const p = this.grid.resolve(camp.x + Math.cos(a) * r, camp.z + Math.sin(a) * r, 1);
        const u = this.createMob(camp.kind, p.x, p.z, rng);
        u.campId = ci;
      }
    });
  }

  private createMob(kind: MobKind, x: number, z: number, rng: Rng = this.rng): Unit {
    const def = MOBS[kind];
    const level = def.levels[0] + Math.floor(rng() * (def.levels[1] - def.levels[0] + 1));
    const u = new Unit(def.name, 'enemy', level, kind, buildMobModel(def), def);
    const st = mobStats(kind, level);
    u.maxHp = u.hp = st.maxHp;
    u.damage = st.damage;
    u.attackSpeed = st.attackSpeed;
    u.speed = def.speed;
    u.attackRange = 1.6 + u.radius;
    u.facing = rng() * Math.PI * 2;
    u.home.set(x, 0, z);
    u.wanderAt = rng() * 6;
    u.specialAt = 8;
    this.placeUnit(u, x, z);
    this.scene.add(u.rig.root);
    this.units.push(u);
    return u;
  }

  recomputeStats() {
    const p = this.progress;
    const before = this.stats?.maxHp ?? 0;
    this.stats = playerStats(p.cls, p.level, p.gear);
    this.player.level = p.level;
    this.player.maxHp = this.stats.maxHp;
    this.player.armor = this.stats.armor;
    if (before > 0 && this.stats.maxHp > before) this.player.hp += this.stats.maxHp - before;
    this.player.hp = Math.min(this.player.hp, this.player.maxHp);
  }

  // ---------- Loop ----------

  start() {
    this.paused = false;
    this.lastT = performance.now();
    const loop = (t: number) => {
      if (this.disposed) return;
      this.raf = requestAnimationFrame(loop);
      const dt = Math.min(0.05, (t - this.lastT) / 1000);
      this.lastT = t;
      if (!this.paused) this.update(dt);
      else this.input.consume();
      this.updateCamera(dt);
      this.renderer.render(this.scene, this.camera);
    };
    this.raf = requestAnimationFrame(loop);
  }

  dispose() {
    this.disposed = true;
    cancelAnimationFrame(this.raf);
    window.removeEventListener('resize', this.resize);
    this.renderer.dispose();
  }

  update(dt: number) {
    this.now += dt;
    this.progress.playSeconds += dt;
    this.handleInput();
    this.updatePlayer(dt);
    if (this.pet) this.updatePet(this.pet, dt);
    for (const u of this.units) this.updateMob(u, dt);
    for (const n of this.npcs) {
      animateRig(n.rig, dt, 0, -1, false);
      const d = n.distTo(this.player);
      if (d < 12) {
        const want = Math.atan2(this.player.pos.x - n.pos.x, this.player.pos.z - n.pos.z);
        n.facing = lerpAngle(n.facing, want, dt * 3);
        n.rig.root.rotation.y = n.facing;
      }
    }
    this.updateDots(dt);
    this.updateProjectiles(dt);
    this.updateTelegraphs();
    this.updateEffects();
    this.updateEnvironment(dt);
    if (this.now - this.lastSave > 15) this.save();
  }

  save() {
    this.lastSave = this.now;
    if (!this.player.dead) this.progress.pos = { x: this.player.pos.x, z: this.player.pos.z };
    else this.progress.pos = { ...SPAWN };
    this.progress.level = this.progress.level;
    saveGame(this.progress);
  }

  // ---------- Input and camera ----------

  private handleInput() {
    const inp = this.input.consume();
    this.cam.yaw += inp.yaw;
    this.cam.pitch = Math.min(1.25, Math.max(0.12, this.cam.pitch + inp.pitch));
    this.cam.dist = Math.min(24, Math.max(6, this.cam.dist + inp.zoom));
    if (this.input.keyDown('q')) this.cam.yaw += 0.04;
    if (this.input.keyDown('e')) this.cam.yaw -= 0.04;
    for (const k of inp.pressed) {
      if (k >= '1' && k <= '4') this.useAbilitySlot(Number(k) - 1);
      else if (k === '5' || k === 'r') this.usePotion();
      else if (k === 'tab' || k === 't') this.cycleTarget();
      else if (k === 'f') this.tryInteract();
      else if (k === ' ') this.mainAction();
    }
    for (const tap of inp.taps) this.handleTap(tap.x, tap.y);
  }

  private handleTap(x: number, y: number) {
    let best: Unit | null = null;
    let bestD = 60;
    const w = window.innerWidth;
    const h = window.innerHeight;
    for (const u of [...this.units, ...this.npcs, ...(this.pet ? [this.pet] : [])]) {
      if (u.dead || !u.rig.root.visible) continue;
      if (u.distTo(this.player) > 60) continue;
      TMP.copy(u.pos);
      TMP.y += u.rig.height * 0.5 * u.rig.root.scale.y;
      TMP.project(this.camera);
      if (TMP.z > 1) continue;
      const sx = (TMP.x * 0.5 + 0.5) * w;
      const sy = (-TMP.y * 0.5 + 0.5) * h;
      const d = Math.hypot(sx - x, sy - y) - u.rig.height * 4;
      if (d < bestD) {
        bestD = d;
        best = u;
      }
    }
    if (!best) return;
    play('click');
    if (best.team === 'npc') {
      this.player.target = best;
      if (best.distTo(this.player) < 7) this.ui.openDialog(best.npc!);
      else this.ui.message('Walk closer to talk');
      return;
    }
    if (this.player.target === best && best.isHostileTo(this.player)) this.autoAttack = true;
    this.player.target = best;
  }

  private updateCamera(dt: number) {
    const p = this.player.pos;
    const tx = p.x;
    const ty = p.y + 2.2;
    const tz = p.z;
    const cp = Math.cos(this.cam.pitch);
    let cx = tx + Math.sin(this.cam.yaw) * cp * this.cam.dist;
    let cz = tz + Math.cos(this.cam.yaw) * cp * this.cam.dist;
    let cy = ty + Math.sin(this.cam.pitch) * this.cam.dist;
    const ground = heightAt(cx, cz) + 1.2;
    if (cy < ground) cy = ground;
    const lim = WORLD_LIMIT + 50;
    cx = Math.max(-lim, Math.min(lim, cx));
    cz = Math.max(-lim, Math.min(lim, cz));
    const k = Math.min(1, dt * 12 + 0.0001);
    this.camera.position.lerp(TMP.set(cx, cy, cz), this.paused ? 1 : k);
    this.camera.lookAt(tx, ty, tz);
    this.sky.position.copy(this.camera.position);
    this.sun.position.set(p.x + 40, p.y + 80, p.z + 25);
    this.sun.target.position.set(p.x, p.y, p.z);
  }

  // ---------- Player ----------

  private updatePlayer(dt: number) {
    const pl = this.player;
    const cls = CLASSES[this.progress.cls];
    const inCombat = this.now - pl.lastCombatAt < 6;
    if (pl.dead) {
      animateRig(pl.rig, dt, 0, -1, true);
      return;
    }
    // Resource and regeneration.
    if (cls.resource === 'mana') this.resource = Math.min(cls.resourceMax, this.resource + cls.resourceRegen * (inCombat ? 1 : 3) * dt);
    else if (!inCombat) this.resource = Math.max(0, this.resource - 6 * dt);
    if (!inCombat && pl.hp < pl.maxHp) pl.hp = Math.min(pl.maxHp, pl.hp + pl.maxHp * 0.04 * dt);
    if (pl.shieldUntil < this.now) pl.shield = 0;

    // Leap in progress.
    if (this.leap) {
      const L = this.leap;
      L.t += dt / 0.4;
      const t = Math.min(1, L.t);
      pl.pos.lerpVectors(L.from, L.to, t);
      pl.pos.y = heightAt(pl.pos.x, pl.pos.z) + Math.sin(t * Math.PI) * 4;
      pl.rig.root.position.copy(pl.pos);
      animateRig(pl.rig, dt, 0.2, 0.3, false);
      if (t >= 1) {
        this.leap = null;
        pl.pos.y = heightAt(pl.pos.x, pl.pos.z);
        if (!L.target.dead) {
          this.dealDamage(pl, L.target, ABILITIES.skywardLeap.amount * this.stats.damageScale, { color: '#bfe3ff' });
          L.target.stunUntil = this.now + 2;
        }
        this.ringEffect(pl.pos, 4, '#bfe3ff', 0.35);
        play('boom');
      }
      return;
    }

    // Movement.
    const mv = this.input.moveVector();
    const moving = Math.hypot(mv.x, mv.y);
    if (moving > 0.01 && !pl.rooted(this.now)) {
      const s = Math.sin(this.cam.yaw);
      const c = Math.cos(this.cam.yaw);
      const dx = -s * mv.y + c * mv.x;
      const dz = -c * mv.y - s * mv.x;
      const speed = PLAYER_SPEED * Math.min(1, moving);
      this.moveUnit(pl, dx * speed * dt, dz * speed * dt);
      pl.facing = lerpAngle(pl.facing, Math.atan2(dx, dz), Math.min(1, dt * 14));
      pl.moving = Math.min(1, moving);
    } else {
      pl.moving = 0;
    }

    // Target housekeeping.
    const tgt = pl.target;
    if (tgt && tgt.dead && this.now > tgt.corpseUntil - 0.01) pl.target = null;
    if (tgt && tgt.distTo(pl) > 70) pl.target = null;

    // Auto-attack.
    if (pl.attackAnim >= 0) {
      pl.attackAnim += dt / 0.45;
      if (pl.attackAnim > 1) pl.attackAnim = -1;
    }
    pl.attackTimer -= dt;
    const t = pl.target;
    if (this.autoAttack && t && !t.dead && t.isHostileTo(pl) && !pl.stunned(this.now)) {
      const inRange = pl.distTo(t) <= pl.attackRange + t.radius;
      if (pl.moving < 0.1) pl.facing = lerpAngle(pl.facing, Math.atan2(t.pos.x - pl.pos.x, t.pos.z - pl.pos.z), Math.min(1, dt * 10));
      if (inRange && pl.attackTimer <= 0) {
        pl.attackTimer = pl.attackSpeed;
        pl.attackAnim = 0;
        const dmg = cls.attackDamage * this.stats.damageScale * (0.9 + this.rng() * 0.2);
        if (cls.ranged) {
          play('cast');
          this.fireProjectile(pl, t, cls.colors.glow, 26, () => this.dealDamage(pl, t, dmg, { color: '#fff' }));
        } else {
          play('swing');
          this.dealDamage(pl, t, dmg, { color: '#fff' });
          this.resource = Math.min(cls.resourceMax, this.resource + 12);
        }
      }
    } else if (!t || t.dead) {
      this.autoAttack = false;
    }

    pl.rig.root.position.copy(pl.pos);
    pl.rig.root.rotation.y = pl.facing;
    animateRig(pl.rig, dt, pl.moving, pl.attackAnim, false);

    // Zone changes.
    const zn = zoneAt(pl.pos.x, pl.pos.z)?.name ?? DEFAULT_ZONE.name;
    if (zn !== this.zoneName) {
      const first = this.zoneName === '';
      this.zoneName = zn;
      if (!first) this.ui.banner(zn);
    }
  }

  moveUnit(u: Unit, dx: number, dz: number) {
    let x = u.pos.x + dx;
    let z = u.pos.z + dz;
    const r = Math.min(0.8, u.radius * 0.7);
    const res = this.grid.resolve(x, z, r);
    x = Math.max(-WORLD_LIMIT, Math.min(WORLD_LIMIT, res.x));
    z = Math.max(-WORLD_LIMIT, Math.min(WORLD_LIMIT, res.z));
    u.pos.set(x, heightAt(x, z), z);
  }

  // ---------- Player actions (called by the HUD too) ----------

  abilityList(): AbilityDef[] {
    return CLASSES[this.progress.cls].abilities.map((id) => ABILITIES[id]);
  }

  cooldownLeft(id: AbilityId | 'potion'): number {
    return Math.max(0, (this.cooldowns[id] ?? 0) - this.now);
  }

  mainAction() {
    const pl = this.player;
    if (pl.dead) return;
    let t = pl.target;
    if (!t || t.dead || !t.isHostileTo(pl)) {
      t = this.nearestEnemy(35, true);
      if (!t) {
        const npc = this.nearestNpc(6);
        if (npc) {
          this.ui.openDialog(npc.npc!);
          return;
        }
        this.ui.message('No enemies nearby');
        return;
      }
      pl.target = t;
    }
    this.autoAttack = true;
  }

  cycleTarget() {
    const pl = this.player;
    const list = this.units
      .filter((u) => !u.dead && u.distTo(pl) < 40)
      .sort((a, b) => a.distTo(pl) - b.distTo(pl));
    if (!list.length) {
      this.ui.message('No enemies nearby');
      return;
    }
    const idx = pl.target ? list.indexOf(pl.target) : -1;
    pl.target = list[(idx + 1) % list.length];
    play('click');
  }

  nearestEnemy(range: number, preferFront = false): Unit | null {
    const pl = this.player;
    let best: Unit | null = null;
    let bestScore = Infinity;
    for (const u of this.units) {
      if (u.dead) continue;
      const d = u.distTo(pl);
      if (d > range) continue;
      let score = d;
      if (preferFront) {
        const ang = Math.atan2(u.pos.x - pl.pos.x, u.pos.z - pl.pos.z);
        score += Math.abs(angleDiff(ang, pl.facing)) * 4;
        if (u.target === pl || u.target === this.pet) score -= 20;
      }
      if (score < bestScore) {
        bestScore = score;
        best = u;
      }
    }
    return best;
  }

  nearestNpc(range: number): Unit | null {
    let best: Unit | null = null;
    let bd = range;
    for (const n of this.npcs) {
      const d = n.distTo(this.player);
      if (d < bd) {
        bd = d;
        best = n;
      }
    }
    return best;
  }

  tryInteract() {
    const n = this.nearestNpc(6);
    if (n) this.ui.openDialog(n.npc!);
  }

  usePotion() {
    const pl = this.player;
    if (pl.dead) return;
    if (this.progress.potions <= 0) return this.fail('No draughts left - buy more from Odo');
    if (this.cooldownLeft('potion') > 0) return this.fail('Not ready yet');
    if (pl.hp >= pl.maxHp) return this.fail('Already at full health');
    this.progress.potions -= 1;
    this.cooldowns.potion = this.now + POTION.cooldown;
    this.heal(pl, pl.maxHp * POTION.healFraction);
    this.sparkles(pl.pos, '#ff8fa0');
    play('heal');
  }

  private fail(msg: string) {
    this.ui.message(msg);
    play('error');
  }

  useAbilitySlot(i: number) {
    const id = CLASSES[this.progress.cls].abilities[i];
    if (id) this.useAbility(id);
  }

  useAbility(id: AbilityId): boolean {
    const pl = this.player;
    const ab = ABILITIES[id];
    const cls = CLASSES[this.progress.cls];
    if (pl.dead || this.leap) return false;
    if (this.progress.level < ab.unlockLevel) return this.fail(`Learned at level ${ab.unlockLevel}`), false;
    if (pl.stunned(this.now)) return this.fail('You are stunned'), false;
    if (this.cooldownLeft(id) > 0) return this.fail('Not ready yet'), false;
    if (this.resource < ab.cost) return this.fail(`Not enough ${cls.resourceName}`), false;
    let target = pl.target && !pl.target.dead && pl.target.isHostileTo(pl) ? pl.target : null;
    if (ab.needsTarget) {
      if (!target) {
        target = this.nearestEnemy(ab.range, true);
        if (!target) return this.fail('No target in range'), false;
        pl.target = target;
      }
      if (pl.distTo(target) - target.radius > ab.range) return this.fail('Out of range'), false;
    }
    if (target) {
      pl.facing = Math.atan2(target.pos.x - pl.pos.x, target.pos.z - pl.pos.z);
      this.autoAttack = true;
    }
    if (cls.resource === 'fury') this.resource -= ab.cost;
    else this.resource -= ab.cost;
    this.cooldowns[id] = this.now + ab.cooldown;
    pl.attackAnim = 0;
    const s = this.stats.damageScale;
    const amt = ab.amount * s;
    switch (id) {
      case 'thunderCleave': {
        if (!target) {
          const near = this.nearestEnemy(6, true);
          if (near) pl.facing = Math.atan2(near.pos.x - pl.pos.x, near.pos.z - pl.pos.z);
        }
        let hits = 0;
        for (const u of this.enemiesNear(pl.pos.x, pl.pos.z, 5 + 1)) {
          const ang = Math.atan2(u.pos.x - pl.pos.x, u.pos.z - pl.pos.z);
          if (Math.abs(angleDiff(ang, pl.facing)) < 1.3 || u.distTo(pl) < 2) {
            this.dealDamage(pl, u, amt, { color: '#bfe3ff' });
            hits++;
          }
        }
        this.resource = Math.min(cls.resourceMax, this.resource + 20);
        this.arcEffect(pl, '#8fd0ff');
        play(hits ? 'shock' : 'swing');
        break;
      }
      case 'skywardLeap': {
        const t = target!;
        const dir = TMP.set(pl.pos.x - t.pos.x, 0, pl.pos.z - t.pos.z).normalize();
        const to = new THREE.Vector3(t.pos.x + dir.x * (t.radius + 1), 0, t.pos.z + dir.z * (t.radius + 1));
        this.leap = { from: pl.pos.clone(), to, t: 0, target: t };
        play('swing');
        break;
      }
      case 'staticGuard':
        pl.shield = amt;
        pl.shieldUntil = this.now + 8;
        this.ringEffect(pl.pos, 2.5, '#c6f0ff', 0.5);
        play('shock');
        break;
      case 'tempest':
        for (const u of this.enemiesNear(pl.pos.x, pl.pos.z, 8)) {
          this.dealDamage(pl, u, amt, { color: '#d9c8ff' });
          this.lightning(u.pos);
        }
        this.ringEffect(pl.pos, 8, '#b9a2ff', 0.5);
        play('boom');
        break;
      case 'cinderLance': {
        const t = target!;
        play('fire');
        this.fireProjectile(pl, t, '#ffb347', 34, () => {
          this.dealDamage(pl, t, amt, { color: '#ffb347' });
          this.addDot(t, pl, amt * 0.25, 3, 1, '#ff9a3c', false);
        }, 0.45);
        break;
      }
      case 'ashRing':
        for (const u of this.enemiesNear(pl.pos.x, pl.pos.z, 7)) {
          this.dealDamage(pl, u, amt, { color: '#ffb08a' });
          u.rootUntil = this.now + 3;
        }
        this.ringEffect(pl.pos, 7, '#ff8a4a', 0.5);
        play('fire');
        break;
      case 'phoenixVeil': {
        const mv = this.input.moveVector();
        let ang = pl.facing;
        if (Math.hypot(mv.x, mv.y) > 0.2) {
          const s2 = Math.sin(this.cam.yaw);
          const c2 = Math.cos(this.cam.yaw);
          ang = Math.atan2(-s2 * mv.y + c2 * mv.x, -c2 * mv.y - s2 * mv.x);
        }
        this.ringEffect(pl.pos, 2, '#ffd27a', 0.4);
        for (let i = 0; i < 12; i++) this.moveUnit(pl, Math.sin(ang), Math.cos(ang));
        pl.facing = ang;
        pl.rootUntil = 0;
        this.heal(pl, amt);
        this.ringEffect(pl.pos, 2.5, '#ffd27a', 0.4);
        play('fire');
        break;
      }
      case 'meteor': {
        const t = target!;
        const x = t.pos.x;
        const z = t.pos.z;
        this.addTelegraph(x, z, 6, 1.2, '#ffb347', null, () => {
          for (const u of this.enemiesNear(x, z, 6)) this.dealDamage(pl, u, amt, { color: '#ffdf8a' });
          this.ringEffect(new THREE.Vector3(x, heightAt(x, z), z), 6, '#ff7a1a', 0.6);
          play('boom');
        });
        this.fallingStar(x, z);
        play('cast');
        break;
      }
      case 'brambleSnare': {
        const t = target!;
        this.fireProjectile(pl, t, '#b6e27a', 30, () => {
          t.rootUntil = this.now + 3;
          this.addDot(t, pl, amt / 6, 6, 1.5, '#b6e27a', false);
          this.ringEffect(t.pos, 1.8, '#7ac24a', 0.6);
        });
        play('cast');
        break;
      }
      case 'renewal':
        this.addDot(pl, pl, amt / 8, 8, 1, '#7dff8a', true);
        if (this.pet && !this.pet.dead) this.addDot(this.pet, pl, amt / 8, 8, 1, '#7dff8a', true);
        this.sparkles(pl.pos, '#7dff8a');
        play('heal');
        break;
      case 'spiritWolf':
        this.summonWolf();
        play('cast');
        break;
      case 'wildBloom': {
        const t = target!;
        for (const u of this.enemiesNear(t.pos.x, t.pos.z, 7)) this.addDot(u, pl, amt / 4, 4, 1.5, '#ff8ae0', false);
        this.ringEffect(t.pos, 7, '#ff8ae0', 0.6);
        play('cast');
        break;
      }
    }
    return true;
  }

  enemiesNear(x: number, z: number, r: number): Unit[] {
    return this.units.filter((u) => !u.dead && u.state !== 'evade' && u.distToXZ(x, z) <= r + u.radius * 0.5);
  }

  private summonWolf() {
    if (this.pet) this.removePet();
    const pl = this.player;
    const w = new Unit('Spirit Wolf', 'player', this.progress.level, 'pet', buildSpiritWolf());
    w.maxHp = w.hp = Math.round(this.stats.maxHp * 0.6);
    w.damage = 7 * this.stats.damageScale;
    w.attackSpeed = 1.5;
    w.attackRange = 2.2;
    w.speed = 9;
    w.armor = this.stats.armor;
    w.expiresAt = this.now + 25;
    this.placeUnit(w, pl.pos.x + Math.sin(pl.facing + 1.5) * 2, pl.pos.z + Math.cos(pl.facing + 1.5) * 2);
    this.scene.add(w.rig.root);
    this.pet = w;
    this.ringEffect(w.pos, 2.5, '#9fe8ff', 0.5);
  }

  private removePet() {
    if (!this.pet) return;
    this.scene.remove(this.pet.rig.root);
    for (const u of this.units) if (u.target === this.pet) u.target = this.player.dead ? null : this.player;
    this.pet = null;
  }

  private updatePet(w: Unit, dt: number) {
    const pl = this.player;
    if (w.dead || this.now > w.expiresAt || pl.dead) {
      this.ringEffect(w.pos, 2, '#9fe8ff', 0.4);
      this.removePet();
      return;
    }
    let t: Unit | null = null;
    if (pl.target && !pl.target.dead && pl.target.isHostileTo(w) && pl.target.distTo(pl) < 35 && (this.autoAttack || pl.target.target)) t = pl.target;
    if (!t) t = this.units.find((u) => !u.dead && (u.target === pl || u.target === w)) ?? null;
    w.target = t;
    if (w.attackAnim >= 0) {
      w.attackAnim += dt / 0.4;
      if (w.attackAnim > 1) w.attackAnim = -1;
    }
    w.attackTimer -= dt;
    let moving = 0;
    if (t) {
      const d = w.distTo(t);
      if (d > w.attackRange + t.radius) {
        moving = this.stepToward(w, t.pos.x, t.pos.z, w.speed * dt);
      } else if (w.attackTimer <= 0) {
        w.attackTimer = w.attackSpeed;
        w.attackAnim = 0;
        this.dealDamage(w, t, w.damage * (0.9 + this.rng() * 0.2), { color: '#bff4ff' });
      }
      if (moving === 0) w.facing = Math.atan2(t.pos.x - w.pos.x, t.pos.z - w.pos.z);
    } else {
      const fx = pl.pos.x + Math.sin(pl.facing + 2.4) * 2.5;
      const fz = pl.pos.z + Math.cos(pl.facing + 2.4) * 2.5;
      if (w.distToXZ(fx, fz) > 1.5) moving = this.stepToward(w, fx, fz, Math.min(w.speed, Math.max(PLAYER_SPEED, w.distToXZ(fx, fz) * 2)) * dt);
    }
    w.rig.root.position.copy(w.pos);
    w.rig.root.rotation.y = w.facing;
    animateRig(w.rig, dt, moving, w.attackAnim, false);
  }

  /** Moves a unit toward a point; returns 1 if it moved. */
  private stepToward(u: Unit, x: number, z: number, step: number): number {
    const dx = x - u.pos.x;
    const dz = z - u.pos.z;
    const d = Math.hypot(dx, dz);
    if (d < 0.05) return 0;
    const s = Math.min(step, d);
    this.moveUnit(u, (dx / d) * s, (dz / d) * s);
    u.facing = lerpAngle(u.facing, Math.atan2(dx, dz), 0.3);
    return 1;
  }

  // ---------- Combat ----------

  dealDamage(src: Unit, dst: Unit, amount: number, opts: { color?: string; noCrit?: boolean; dot?: boolean } = {}) {
    if (dst.dead || dst.team === 'npc' || dst.state === 'evade') return;
    let dmg = amount;
    let crit = false;
    if (src.team === 'player') {
      dmg *= levelMod(src.level, dst.level);
      if (!opts.noCrit && !opts.dot && this.rng() < 0.12) {
        dmg *= 1.6;
        crit = true;
      }
    } else {
      dmg *= mitigation(dst.armor) * levelMod(src.level, dst.level);
    }
    dmg = Math.max(1, Math.round(dmg));
    const now = this.now;
    // Shield absorbs first.
    if (dst.shield > 0 && dst.shieldUntil > now) {
      const absorbed = Math.min(dst.shield, dmg);
      dst.shield -= absorbed;
      dmg -= absorbed;
      if (dst === this.player && src.team === 'enemy' && !opts.dot) {
        this.dealDamage(this.player, src, 4 * this.stats.damageScale, { color: '#c6f0ff', noCrit: true });
      }
      if (dmg <= 0) {
        this.ui.floatText(dst.pos.clone().setY(dst.pos.y + dst.rig.height * dst.rig.root.scale.y), 'Absorb', '#c6f0ff', 'small');
        return;
      }
    }
    dst.hp -= dmg;
    src.lastCombatAt = now;
    dst.lastCombatAt = now;
    if (src === this.pet) this.player.lastCombatAt = now;
    flashRig(dst.rig);
    const top = dst.pos.clone().setY(dst.pos.y + dst.rig.height * dst.rig.root.scale.y + 0.4);
    if (dst.team === 'enemy') {
      this.ui.floatText(top, String(dmg), crit ? '#ffe066' : opts.color ?? '#fff', crit ? 'crit' : opts.dot ? 'small' : undefined);
      if (!opts.dot) play(crit ? 'crit' : 'hit');
      dst.taggedByPlayer = true;
      if (dst.state === 'idle') this.aggro(dst, src);
    } else {
      this.ui.floatText(top, `-${dmg}`, '#ff5a4a', opts.dot ? 'small' : undefined);
      if (dst === this.player) {
        if (CLASSES[this.progress.cls].resource === 'fury') this.resource = Math.min(100, this.resource + 3);
        if (!this.player.target || this.player.target.dead) {
          this.player.target = src;
          this.autoAttack = true;
        }
      }
    }
    if (dst.hp <= 0) this.kill(dst, src);
  }

  heal(u: Unit, amount: number) {
    if (u.dead) return;
    const a = Math.round(Math.min(amount, u.maxHp - u.hp));
    u.hp += a;
    if (a > 0) this.ui.floatText(u.pos.clone().setY(u.pos.y + u.rig.height * u.rig.root.scale.y + 0.3), `+${a}`, '#6ee07a', 'small');
  }

  addDot(u: Unit, src: Unit | null, perTick: number, ticks: number, interval: number, color: string, heal: boolean) {
    if (u.dead) return;
    u.dots = u.dots.filter((d) => !(d.source === src && d.color === color));
    u.dots.push({ perTick, next: this.now + interval, interval, until: this.now + ticks * interval + 0.01, source: src, color, heal });
  }

  private updateDots(_dt: number) {
    const all = [this.player, ...this.units, ...(this.pet ? [this.pet] : [])];
    for (const u of all) {
      if (!u.dots.length) continue;
      if (u.dead) {
        u.dots = [];
        continue;
      }
      for (const d of u.dots) {
        if (this.now >= d.next && d.next <= d.until) {
          d.next += d.interval;
          if (d.heal) this.heal(u, d.perTick);
          else if (d.source) this.dealDamage(d.source, u, d.perTick, { color: d.color, dot: true });
        }
      }
      u.dots = u.dots.filter((d) => d.next <= d.until && !u.dead);
    }
  }

  private aggro(mob: Unit, by: Unit) {
    if (mob.dead || mob.state === 'evade') return;
    const wasIdle = mob.state === 'idle';
    mob.target = by;
    mob.state = 'chase';
    mob.lastCombatAt = this.now;
    mob.specialAt = this.now + 5 + this.rng() * 4;
    if (wasIdle && mob.kind !== 'boss') {
      // Friends close by join in.
      for (const o of this.units) {
        if (o !== mob && !o.dead && o.state === 'idle' && o.campId === mob.campId && o.distTo(mob) < 7) {
          o.target = by;
          o.state = 'chase';
          o.lastCombatAt = this.now;
          o.specialAt = this.now + 6 + this.rng() * 4;
        }
      }
    }
  }

  private kill(u: Unit, killer: Unit) {
    u.dead = true;
    u.hp = 0;
    u.dots = [];
    u.state = 'dead';
    u.target = null;
    if (u === this.player) {
      this.onPlayerDeath();
      return;
    }
    if (u === this.pet) return;
    u.corpseUntil = this.now + 8;
    u.respawnAt = this.now + (u.kind === 'boss' ? 150 : u.kind === 'chieftain' ? 60 : u.kind === 'imp' ? 1e9 : 30);
    for (const o of this.units) if (o.target === u) o.target = null;
    if (!u.taggedByPlayer) return;
    void killer;
    const p = this.progress;
    // Experience.
    const xp = mobXp(u.level, p.level, u.elite);
    if (xp > 0) {
      this.ui.floatText(this.player.pos.clone().setY(this.player.pos.y + 3), `+${xp} XP`, '#c28bff', 'small');
      this.gainXp(xp);
    }
    // Loot.
    const loot = rollMobLoot(this.rng, p.cls, u.kind as MobKind, u.level);
    if (u.kind === 'imp') loot.item = null;
    p.gold += loot.gold;
    this.ui.toast(`<span class="gold">+${loot.gold} gold</span>`);
    if (loot.item) this.giveItem(loot.item);
    const qd = u.def?.questDrop;
    if (qd && wantsQuestItem(p, qd.item) && this.rng() < qd.chance) {
      onItemLooted(p, qd.item);
      this.ui.toast(`<span style="color:#ffd84a">${QUEST_ITEMS[qd.item]}</span>`);
      play('loot');
      this.ui.questsChanged();
    }
    if (onMobKilled(p, u.kind as MobKind).length) this.ui.questsChanged();
    if (u.kind === 'boss') {
      p.bossDefeated = true;
      for (const o of this.units) if (o.kind === 'imp' && !o.dead) this.kill(o, killer);
      this.ui.banner('Varkul has fallen!', 'Return to Warden Elra in Hearthmoor');
      play('quest');
      this.save();
    }
  }

  gainXp(xp: number) {
    const gained = addXp(this.progress, xp);
    if (gained > 0) {
      this.recomputeStats();
      this.player.hp = this.player.maxHp;
      const unlocked = this.abilityList().filter((a) => a.unlockLevel === this.progress.level);
      this.ui.banner(`Level ${this.progress.level}`, unlocked.length ? `New ability: ${unlocked.map((a) => a.name).join(', ')}` : 'You feel stronger');
      play('levelup');
      this.levelEffect();
      this.ui.questsChanged();
      this.save();
    }
  }

  giveItem(item: Item) {
    const p = this.progress;
    if (p.bag.length >= BAG_SIZE) {
      p.gold += item.value;
      this.ui.toast(`Bag full - sold <span style="color:${RARITY_COLORS[item.rarity]}">${item.name}</span> for <span class="gold">${item.value}g</span>`);
      return;
    }
    p.bag.push(item);
    this.ui.toast(`<span style="color:${RARITY_COLORS[item.rarity]}">[${item.name}]</span>`);
    play('loot');
  }

  equip(item: Item) {
    const p = this.progress;
    const idx = p.bag.findIndex((i) => i.id === item.id);
    if (idx < 0) return;
    p.bag.splice(idx, 1);
    const old = p.gear[item.slot];
    p.gear[item.slot] = item;
    if (old) p.bag.push(old);
    this.recomputeStats();
    play('loot');
    this.save();
  }

  sell(item: Item) {
    const p = this.progress;
    const idx = p.bag.findIndex((i) => i.id === item.id);
    if (idx < 0) return;
    p.bag.splice(idx, 1);
    p.gold += item.value;
    play('loot');
  }

  buyPotion(): boolean {
    const p = this.progress;
    if (p.gold < POTION.price) return false;
    p.gold -= POTION.price;
    p.potions += 1;
    play('loot');
    return true;
  }

  turnInQuest(id: string) {
    const q = completeQuest(this.progress, id);
    if (!q) return;
    play('quest');
    this.ui.banner('Quest complete', q.title);
    this.progress.gold += 0;
    if (q.rewardItemLevel > 0) {
      this.giveItem(makeItem(this.rng, this.progress.cls, q.rewardItemLevel, q.id === 'cindermaw' ? 3 : q.rewardItemLevel >= 7 ? 2 : 1));
    }
    this.gainXp(q.xp);
    this.ui.questsChanged();
    this.save();
    if (id === 'cindermaw') this.ui.victory();
  }

  private onPlayerDeath() {
    const pl = this.player;
    pl.target = null;
    this.autoAttack = false;
    this.leap = null;
    this.removePet();
    for (const u of this.units) if (u.target === pl) this.evade(u);
    play('death');
    this.ui.playerDied();
  }

  respawnPlayer() {
    const pl = this.player;
    pl.dead = false;
    pl.state = 'idle';
    pl.hp = Math.round(pl.maxHp * 0.6);
    pl.rig.deathT = 0;
    pl.stunUntil = pl.rootUntil = 0;
    this.resource = CLASSES[this.progress.cls].resource === 'mana' ? CLASSES[this.progress.cls].resourceMax * 0.6 : 0;
    this.placeUnit(pl, SPAWN.x, SPAWN.z);
    pl.facing = Math.PI;
    this.cam.yaw = 0;
    this.save();
  }

  private evade(u: Unit) {
    u.state = 'evade';
    u.target = null;
    u.dots = [];
    u.rootUntil = u.stunUntil = 0;
    if (u.kind === 'boss') this.resetBoss(u);
  }

  private resetBoss(boss: Unit) {
    this.bossPhase = 0;
    boss.attackSpeed = mobStats('boss', boss.level).attackSpeed;
    for (const o of this.units) {
      if (o.kind === 'imp' && !o.dead) {
        o.dead = true;
        o.state = 'dead';
        o.corpseUntil = this.now;
        o.respawnAt = 1e9;
      }
    }
    for (const t of this.telegraphs) if (t.owner === boss) t.at = -1;
  }

  // ---------- Enemy AI ----------

  private updateMob(u: Unit, dt: number) {
    const pl = this.player;
    const far = u.distTo(pl) > 150;
    u.rig.root.visible = !far && !(u.dead && this.now > u.corpseUntil);
    if (u.dead) {
      if (this.now > u.respawnAt) this.respawnMob(u);
      else if (u.rig.root.visible) animateRig(u.rig, dt, 0, -1, true);
      return;
    }
    if (far && u.state === 'idle') return;
    if (u.attackAnim >= 0) {
      u.attackAnim += dt / 0.5;
      if (u.attackAnim > 1) u.attackAnim = -1;
    }
    u.attackTimer -= dt;
    let moving = 0;
    const leash = u.kind === 'boss' ? 42 : 55;
    const stunned = u.stunned(this.now);

    if (u.state === 'idle') {
      // Aggro check.
      if (!pl.dead && levelColor(u.level, pl.level) !== '#9a9a9a') {
        const radius = u.kind === 'boss' ? 16 : Math.max(5, Math.min(15, 10 + (u.level - pl.level)));
        if (u.distTo(pl) < radius) this.aggro(u, pl);
        else if (this.pet && u.distTo(this.pet) < radius * 0.7) this.aggro(u, this.pet);
      }
      if (u.state === 'idle' && u.kind !== 'boss' && u.kind !== 'chieftain') {
        if (this.now > u.wanderAt) {
          u.wanderAt = this.now + 4 + this.rng() * 6;
          const a = this.rng() * Math.PI * 2;
          const r = this.rng() * 7;
          u.wanderTo = new THREE.Vector3(u.home.x + Math.cos(a) * r, 0, u.home.z + Math.sin(a) * r);
        }
        if (u.wanderTo && !stunned) {
          if (u.distToXZ(u.wanderTo.x, u.wanderTo.z) > 0.6) moving = this.stepToward(u, u.wanderTo.x, u.wanderTo.z, u.speed * 0.3 * dt) * 0.35;
          else u.wanderTo = null;
        }
      }
    } else if (u.state === 'evade') {
      u.hp = Math.min(u.maxHp, u.hp + u.maxHp * 0.5 * dt);
      moving = this.stepToward(u, u.home.x, u.home.z, u.speed * 1.4 * dt);
      if (u.distToXZ(u.home.x, u.home.z) < 1) {
        u.state = 'idle';
        u.hp = u.maxHp;
        u.taggedByPlayer = false;
        u.wanderAt = this.now + 3;
      }
    } else if (u.state === 'chase') {
      let t = u.target;
      if (!t || t.dead) {
        t = !pl.dead && (u.distTo(pl) < 25) ? pl : null;
        if (this.pet && !this.pet.dead && (!t || u.distTo(this.pet) < u.distTo(pl))) t = this.pet;
        u.target = t;
      }
      if (!t || u.distToXZ(u.home.x, u.home.z) > leash || (this.now - u.lastCombatAt > 12 && u.distTo(t) > 30)) {
        this.evade(u);
      } else {
        const reach = u.attackRange + t.radius * 0.6;
        const d = u.distTo(t);
        if (!stunned) {
          if (d > reach) {
            if (!u.rooted(this.now)) moving = this.stepToward(u, t.pos.x, t.pos.z, u.speed * dt);
          } else {
            u.facing = lerpAngle(u.facing, Math.atan2(t.pos.x - u.pos.x, t.pos.z - u.pos.z), Math.min(1, dt * 8));
            if (u.attackTimer <= 0) {
              u.attackTimer = u.attackSpeed;
              u.attackAnim = 0;
              this.dealDamage(u, t, u.damage * (0.85 + this.rng() * 0.3));
              if (u.kind === 'spider' && this.rng() < 0.3) this.addDot(t, u, u.damage * 0.3, 3, 1.5, '#9be26a', false);
            }
          }
          this.mobSpecial(u, t);
        }
      }
    }
    u.rig.root.position.copy(u.pos);
    u.rig.root.rotation.y = u.facing;
    animateRig(u.rig, dt, stunned ? 0 : moving, u.attackAnim, false);
  }

  private mobSpecial(u: Unit, t: Unit) {
    if (this.now < u.specialAt) return;
    if (u.kind === 'brute' || u.kind === 'chieftain') {
      u.specialAt = this.now + 10 + this.rng() * 4;
      const r = u.kind === 'chieftain' ? 6.5 : 5.5;
      const x = u.pos.x;
      const z = u.pos.z;
      this.addTelegraph(x, z, r, 1.4, '#ff3b2a', u, () => {
        if (u.dead || u.state !== 'chase') return;
        u.attackAnim = 0;
        for (const v of [this.player, ...(this.pet ? [this.pet] : [])]) {
          if (!v.dead && v.distToXZ(x, z) < r) this.dealDamage(u, v, u.damage * 1.8);
        }
        this.ringEffect(new THREE.Vector3(x, heightAt(x, z), z), r, '#ff6a2a', 0.4);
        play('boom');
      });
      if (u.kind === 'chieftain' && u.phase === 0 && u.hp < u.maxHp * 0.5) {
        u.phase = 1;
        this.heal(u, u.maxHp * 0.15);
        this.ui.message('Chieftain Gorran lets out a rallying roar!');
      }
    } else if (u.kind === 'boss') {
      u.specialAt = this.now + 7.5 + this.rng() * 2;
      const x = t.pos.x;
      const z = t.pos.z;
      this.addTelegraph(x, z, 5.5, 1.9, '#ff3b2a', u, () => {
        if (u.dead || u.state !== 'chase') return;
        for (const v of [this.player, ...(this.pet ? [this.pet] : [])]) {
          if (!v.dead && v.distToXZ(x, z) < 5.5) this.dealDamage(u, v, v.maxHp * 0.25 / mitigation(v.armor), { color: '#ff7a1a' });
        }
        this.ringEffect(new THREE.Vector3(x, heightAt(x, z), z), 5.5, '#ff5a1f', 0.6);
        this.fallingStar(x, z, '#ff5a1f');
        play('boom');
      });
      const frac = u.hp / u.maxHp;
      if ((this.bossPhase === 0 && frac < 0.65) || (this.bossPhase === 1 && frac < 0.35)) {
        this.bossPhase += 1;
        this.ui.message('Varkul calls forth Cinder Imps!');
        for (let i = 0; i < 2; i++) {
          const a = this.rng() * Math.PI * 2;
          const imp = this.createMob('imp', u.pos.x + Math.cos(a) * 6, u.pos.z + Math.sin(a) * 6);
          imp.campId = u.campId;
          imp.home.copy(u.home);
          this.aggro(imp, this.player.dead ? t : this.player);
          this.ringEffect(imp.pos, 2, '#ff7a1a', 0.5);
        }
      }
      if (this.bossPhase === 2 && frac < 0.2) {
        this.bossPhase = 3;
        u.attackSpeed = 1.35;
        this.ui.banner('Varkul is enraged!');
      }
    }
  }

  private respawnMob(u: Unit) {
    if (u.kind === 'imp') {
      this.scene.remove(u.rig.root);
      this.units.splice(this.units.indexOf(u), 1);
      return;
    }
    if (u.distToXZ(this.player.pos.x, this.player.pos.z) < 30 && u.kind !== 'boss') {
      u.respawnAt = this.now + 5;
      return;
    }
    const def = MOBS[u.kind as MobKind];
    u.level = def.levels[0] + Math.floor(this.rng() * (def.levels[1] - def.levels[0] + 1));
    const st = mobStats(u.kind as MobKind, u.level);
    u.maxHp = u.hp = st.maxHp;
    u.damage = st.damage;
    u.attackSpeed = st.attackSpeed;
    u.dead = false;
    u.state = 'idle';
    u.taggedByPlayer = false;
    u.phase = 0;
    u.rig.deathT = 0;
    u.stunUntil = u.rootUntil = 0;
    this.placeUnit(u, u.home.x, u.home.z);
    if (u.kind === 'boss') this.bossPhase = 0;
  }

  // ---------- Projectiles, telegraphs, effects ----------

  private fireProjectile(src: Unit, target: Unit, color: string, speed: number, onHit: () => void, size = 0.3) {
    const mesh = new THREE.Mesh(new THREE.IcosahedronGeometry(size, 0), new THREE.MeshBasicMaterial({ color }));
    const glow = new THREE.Mesh(new THREE.IcosahedronGeometry(size * 2, 0), new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 0.3, depthWrite: false }));
    mesh.add(glow);
    mesh.position.set(src.pos.x + Math.sin(src.facing) * 0.6, src.pos.y + 1.6, src.pos.z + Math.cos(src.facing) * 0.6);
    this.scene.add(mesh);
    this.projectiles.push({ mesh, target, speed, onHit });
  }

  private updateProjectiles(dt: number) {
    this.projectiles = this.projectiles.filter((p) => {
      const t = p.target;
      TMP.set(t.pos.x, t.pos.y + t.rig.height * 0.5 * t.rig.root.scale.y, t.pos.z);
      const d = p.mesh.position.distanceTo(TMP);
      const step = p.speed * dt;
      if (d <= step + 0.3 || t.dead) {
        this.scene.remove(p.mesh);
        if (!t.dead) p.onHit();
        return false;
      }
      p.mesh.position.lerp(TMP, step / d);
      p.mesh.rotation.x += dt * 8;
      p.mesh.rotation.y += dt * 6;
      return true;
    });
  }

  private addTelegraph(x: number, z: number, r: number, delay: number, color: string, owner: Unit | null, fire: () => void) {
    const y = heightAt(x, z) + 0.15;
    const ringMat = new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 0.9, depthWrite: false, side: THREE.DoubleSide });
    const mesh = new THREE.Mesh(new THREE.RingGeometry(r - 0.25, r, 40), ringMat);
    mesh.rotation.x = -Math.PI / 2;
    mesh.position.set(x, y, z);
    const fillMat = new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 0.28, depthWrite: false, side: THREE.DoubleSide });
    const fill = new THREE.Mesh(new THREE.CircleGeometry(r, 40), fillMat);
    fill.rotation.x = -Math.PI / 2;
    fill.position.set(x, y + 0.02, z);
    fill.scale.setScalar(0.01);
    this.scene.add(mesh, fill);
    this.telegraphs.push({ x, z, r, t0: this.now, at: this.now + delay, mesh, fill, owner, fire });
  }

  private updateTelegraphs() {
    this.telegraphs = this.telegraphs.filter((t) => {
      const cancelled = t.at < 0;
      if (cancelled || this.now >= t.at) {
        this.scene.remove(t.mesh, t.fill);
        if (!cancelled) t.fire();
        return false;
      }
      const k = (this.now - t.t0) / (t.at - t.t0);
      t.fill.scale.setScalar(Math.max(0.01, k));
      return true;
    });
  }

  private addEffect(obj: THREE.Object3D, dur: number, update?: (k: number) => void) {
    this.scene.add(obj);
    this.effects.push({ obj, t0: this.now, dur, update });
  }

  private updateEffects() {
    this.effects = this.effects.filter((e) => {
      const k = (this.now - e.t0) / e.dur;
      if (k >= 1) {
        this.scene.remove(e.obj);
        return false;
      }
      e.update?.(k);
      return true;
    });
  }

  ringEffect(pos: THREE.Vector3, r: number, color: string, dur: number) {
    const mat = new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 0.8, depthWrite: false, side: THREE.DoubleSide });
    const m = new THREE.Mesh(new THREE.RingGeometry(0.75, 1, 40), mat);
    m.rotation.x = -Math.PI / 2;
    m.position.set(pos.x, heightAt(pos.x, pos.z) + 0.25, pos.z);
    this.addEffect(m, dur, (k) => {
      m.scale.setScalar(Math.max(0.1, r * (0.3 + 0.7 * Math.sqrt(k))));
      mat.opacity = 0.85 * (1 - k);
    });
  }

  private arcEffect(u: Unit, color: string) {
    const mat = new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 0.8, depthWrite: false, side: THREE.DoubleSide });
    const m = new THREE.Mesh(new THREE.RingGeometry(1.5, 5, 24, 1, -1.2, 2.4), mat);
    m.rotation.x = -Math.PI / 2;
    m.rotation.z = u.facing - Math.PI / 2;
    m.position.set(u.pos.x, u.pos.y + 1.1, u.pos.z);
    this.addEffect(m, 0.3, (k) => (mat.opacity = 0.8 * (1 - k)));
  }

  private lightning(pos: THREE.Vector3) {
    const mat = new THREE.MeshBasicMaterial({ color: '#e8ddff', transparent: true, opacity: 1 });
    const g = new THREE.Group();
    let y = 16;
    let x = 0;
    let z = 0;
    for (let i = 0; i < 5; i++) {
      const nx = x + (this.rng() - 0.5) * 1.6;
      const nz = z + (this.rng() - 0.5) * 1.6;
      const ny = y - 3.2;
      const len = Math.hypot(nx - x, ny - y, nz - z);
      const seg = new THREE.Mesh(new THREE.CylinderGeometry(0.08, 0.08, len, 4), mat);
      seg.position.set((x + nx) / 2, (y + ny) / 2, (z + nz) / 2);
      seg.lookAt(nx, ny, nz);
      seg.rotateX(Math.PI / 2);
      g.add(seg);
      x = nx; y = ny; z = nz;
    }
    g.position.copy(pos);
    this.addEffect(g, 0.25, (k) => (mat.opacity = 1 - k));
  }

  private fallingStar(x: number, z: number, color = '#ffdf8a') {
    const mat = new THREE.MeshBasicMaterial({ color });
    const m = new THREE.Mesh(new THREE.IcosahedronGeometry(1, 0), mat);
    const y0 = heightAt(x, z);
    this.addEffect(m, 1.2, (k) => {
      m.position.set(x + (1 - k) * 12, y0 + (1 - k) * 30, z - (1 - k) * 6);
      m.rotation.x += 0.2;
    });
  }

  private sparkles(pos: THREE.Vector3, color: string) {
    const g = new THREE.Group();
    const mat = new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 1 });
    const geo = new THREE.OctahedronGeometry(0.15, 0);
    for (let i = 0; i < 10; i++) {
      const s = new THREE.Mesh(geo, mat);
      const a = (i / 10) * Math.PI * 2;
      s.position.set(Math.cos(a) * 0.9, 0.3 + (i % 3) * 0.5, Math.sin(a) * 0.9);
      g.add(s);
    }
    g.position.copy(pos);
    this.addEffect(g, 1, (k) => {
      g.position.set(this.player.pos.x, this.player.pos.y + k * 1.5, this.player.pos.z);
      g.rotation.y = k * 3;
      mat.opacity = 1 - k;
    });
  }

  private levelEffect() {
    const mat = new THREE.MeshBasicMaterial({ color: '#ffe08a', transparent: true, opacity: 0.6, depthWrite: false, side: THREE.DoubleSide });
    const m = new THREE.Mesh(new THREE.CylinderGeometry(1.4, 1.4, 12, 16, 1, true), mat);
    this.addEffect(m, 1.6, (k) => {
      m.position.set(this.player.pos.x, this.player.pos.y + 6, this.player.pos.z);
      m.scale.set(1 + k, 1, 1 + k);
      mat.opacity = 0.6 * (1 - k);
    });
  }

  private updateEnvironment(dt: number) {
    const pl = this.player;
    const scar = 1 - smoothstep(50, 110, Math.hypot(pl.pos.x - 30, pl.pos.z + 130));
    const fog = this.scene.fog as THREE.Fog;
    fog.color.copy(this.fogDefault).lerp(this.fogScar, scar);
    (this.scene.background as THREE.Color).copy(fog.color);
    const skyMat = this.sky.material as THREE.ShaderMaterial;
    skyMat.uniforms.bottom.value.copy(fog.color);
    skyMat.uniforms.top.value.set('#5f8fc9').lerp(new THREE.Color('#2a1a18'), scar);
    for (const m of this.world.lavaMaterials) m.emissiveIntensity = 1.3 + Math.sin(this.now * 3) * 0.3;
    this.world.water.position.y = -1.2 + Math.sin(this.now * 0.8) * 0.05;
    void dt;
  }

  // ---------- Queries for the HUD ----------

  questCounts() {
    return this.progress.active.map((a) => ({ state: a, def: QUEST_BY_ID[a.id] }));
  }

  get maxResource() {
    return CLASSES[this.progress.cls].resourceMax;
  }

  isMaxLevel() {
    return this.progress.level >= MAX_LEVEL;
  }
}


export function angleDiff(a: number, b: number) {
  let d = a - b;
  while (d > Math.PI) d -= Math.PI * 2;
  while (d < -Math.PI) d += Math.PI * 2;
  return d;
}

export function lerpAngle(a: number, b: number, t: number) {
  return a + angleDiff(b, a) * t;
}
