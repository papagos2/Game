import * as THREE from 'three';
import type { MobDef, MobKind, NpcDef } from '../data/world';
import type { Rig } from './models';

export type Team = 'player' | 'enemy' | 'npc';

export interface Dot {
  perTick: number;
  next: number;
  interval: number;
  until: number;
  source: Unit | null;
  color: string;
  heal: boolean;
}

let nextId = 1;

export class Unit {
  id = nextId++;
  pos = new THREE.Vector3();
  facing = 0;
  hp = 1;
  maxHp = 1;
  dead = false;
  target: Unit | null = null;
  attackTimer = 0;
  attackAnim = -1;
  attackSpeed = 2;
  attackRange = 2.5;
  damage = 5;
  armor = 0;
  speed = 6;
  moving = 0;
  stunUntil = 0;
  rootUntil = 0;
  slowUntil = 0;
  /** Damage reduction from Shield Wall / Barkskin (0..1) and thorns damage while it lasts. */
  guard = 0;
  guardUntil = 0;
  reflect = 0;
  frenzyUntil = 0;
  enraged = false;
  shield = 0;
  shieldUntil = 0;
  dots: Dot[] = [];
  lastCombatAt = -999;
  home = new THREE.Vector3();
  state: 'idle' | 'chase' | 'evade' | 'dead' = 'idle';
  wanderAt = 0;
  wanderTo: THREE.Vector3 | null = null;
  respawnAt = 0;
  corpseUntil = 0;
  taggedByPlayer = false;
  specialAt = 0;
  phase = 0;
  expiresAt = 0;
  campId = -1;

  constructor(
    public name: string,
    public team: Team,
    public level: number,
    public kind: MobKind | 'player' | 'pet' | 'npc',
    public rig: Rig,
    public def: MobDef | null = null,
    public npc: NpcDef | null = null,
  ) {}

  get radius() {
    return this.rig.radius;
  }

  get elite() {
    return !!this.def?.elite;
  }

  isHostileTo(o: Unit) {
    if (this.team === 'npc' || o.team === 'npc') return false;
    return this.team !== o.team;
  }

  distTo(o: Unit) {
    return Math.hypot(this.pos.x - o.pos.x, this.pos.z - o.pos.z);
  }

  distToXZ(x: number, z: number) {
    return Math.hypot(this.pos.x - x, this.pos.z - z);
  }

  stunned(now: number) {
    return now < this.stunUntil;
  }

  slowed(now: number) {
    return now < this.slowUntil;
  }

  rooted(now: number) {
    return now < this.rootUntil || now < this.stunUntil;
  }
}
