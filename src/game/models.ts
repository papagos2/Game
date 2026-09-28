// Low-poly character models built from primitives, with simple procedural animation.
import * as THREE from 'three';
import { CLASSES, type ClassId } from '../data/classes';
import type { MobDef } from '../data/world';

type Shape = 'humanoid' | 'quadruped' | 'spider';

export interface Rig {
  root: THREE.Group;
  /** Everything that tilts/bobs; child of root. */
  body: THREE.Group;
  legs: THREE.Object3D[];
  arms: THREE.Object3D[];
  head: THREE.Object3D | null;
  shape: Shape;
  height: number;
  radius: number;
  phase: number;
  deathT: number;
  flash: number;
  materials: THREE.MeshLambertMaterial[];
}

const matCache = new Map<string, THREE.MeshLambertMaterial>();

function mat(color: string, emissive?: string): THREE.MeshLambertMaterial {
  const key = color + (emissive ?? '');
  let m = matCache.get(key);
  if (!m) {
    m = new THREE.MeshLambertMaterial({ color, flatShading: true, emissive: emissive ?? '#000000', emissiveIntensity: emissive ? 1 : 0 });
    matCache.set(key, m);
  }
  return m;
}

function box(w: number, h: number, d: number, m: THREE.Material) {
  const mesh = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), m);
  mesh.castShadow = true;
  return mesh;
}

/** Creates a pivot at (x,y,z) with the given child hanging below/around it. */
function pivot(x: number, y: number, z: number, ...children: THREE.Object3D[]) {
  const g = new THREE.Group();
  g.position.set(x, y, z);
  g.add(...children);
  return g;
}

function makeRig(shape: Shape, height: number, radius: number): Rig {
  const root = new THREE.Group();
  const body = new THREE.Group();
  root.add(body);
  return { root, body, legs: [], arms: [], head: null, shape, height, radius, phase: Math.random() * 10, deathT: 0, flash: 0, materials: [] };
}

interface HumanoidOpts {
  body: string;
  trim: string;
  skin: string;
  scale?: number;
  bulk?: number;
  weapon?: 'sword' | 'staff' | 'crook' | 'axe' | 'club' | 'none';
  glow?: string;
  horns?: boolean;
  hood?: boolean;
  cape?: string;
}

function humanoid(o: HumanoidOpts): Rig {
  const s = o.scale ?? 1;
  const bulk = o.bulk ?? 1;
  const rig = makeRig('humanoid', 2.1 * s, 0.6 * s * bulk);
  const bm = mat(o.body);
  const tm = mat(o.trim);
  const sm = mat(o.skin);
  const b = rig.body;
  // Legs
  for (const side of [-1, 1]) {
    const leg = box(0.32 * bulk, 0.95, 0.34, tm);
    leg.position.y = -0.47;
    const boot = box(0.36 * bulk, 0.22, 0.46, mat('#3a2a20'));
    boot.position.set(0, -0.92, 0.06);
    const p = pivot(side * 0.2 * bulk, 1.0, 0, leg, boot);
    b.add(p);
    rig.legs.push(p);
  }
  // Torso and belt
  const torso = box(0.8 * bulk, 0.85, 0.46 * bulk, bm);
  torso.position.y = 1.42;
  const belt = box(0.84 * bulk, 0.14, 0.5 * bulk, mat('#3b2a1e'));
  belt.position.y = 1.04;
  const shoulders = box(1.0 * bulk, 0.22, 0.55 * bulk, tm);
  shoulders.position.y = 1.8;
  b.add(torso, belt, shoulders);
  // Head
  const headMesh = new THREE.Mesh(new THREE.IcosahedronGeometry(0.27, 1), sm);
  headMesh.castShadow = true;
  const head = pivot(0, 2.12, 0, headMesh);
  if (o.hood) {
    const hood = new THREE.Mesh(new THREE.ConeGeometry(0.38, 0.7, 6), bm);
    hood.position.y = 0.12;
    head.add(hood);
  }
  if (o.horns) {
    for (const side of [-1, 1]) {
      const horn = new THREE.Mesh(new THREE.ConeGeometry(0.08, 0.45, 4), mat('#e8dcc0'));
      horn.position.set(side * 0.2, 0.22, 0);
      horn.rotation.z = -side * 0.5;
      head.add(horn);
    }
  }
  if (o.glow) {
    const eyeM = new THREE.MeshBasicMaterial({ color: o.glow });
    for (const side of [-1, 1]) {
      const eye = new THREE.Mesh(new THREE.BoxGeometry(0.07, 0.05, 0.02), eyeM);
      eye.position.set(side * 0.1, 0.03, 0.26);
      head.add(eye);
    }
  }
  b.add(head);
  rig.head = head;
  if (o.cape) {
    const cape = box(0.75 * bulk, 1.2, 0.06, mat(o.cape));
    cape.position.set(0, 1.25, -0.28 * bulk);
    cape.rotation.x = 0.12;
    b.add(cape);
  }
  // Arms
  for (const side of [-1, 1]) {
    const arm = box(0.24, 0.8, 0.26, bm);
    arm.position.y = -0.38;
    const hand = box(0.22, 0.2, 0.22, sm);
    hand.position.y = -0.82;
    const p = pivot(side * 0.55 * bulk, 1.78, 0, arm, hand);
    if (side === 1 && o.weapon && o.weapon !== 'none') p.add(weapon(o.weapon, o.glow ?? '#ffffff'));
    b.add(p);
    rig.arms.push(p);
  }
  rig.root.scale.setScalar(s);
  return rig;
}

function weapon(kind: NonNullable<HumanoidOpts['weapon']>, glow: string): THREE.Object3D {
  const g = new THREE.Group();
  g.position.set(0, -0.85, 0.1);
  if (kind === 'sword') {
    const blade = box(0.1, 1.4, 0.04, mat('#d8e2ee', '#223344'));
    blade.position.set(0, 0, 0.75);
    blade.rotation.x = Math.PI / 2;
    const guard = box(0.45, 0.08, 0.1, mat('#b8962e'));
    guard.position.set(0, 0, 0.05);
    const edge = new THREE.Mesh(new THREE.BoxGeometry(0.03, 1.3, 0.05), new THREE.MeshBasicMaterial({ color: glow }));
    edge.position.set(0, 0, 0.75);
    edge.rotation.x = Math.PI / 2;
    g.add(blade, guard, edge);
  } else if (kind === 'staff' || kind === 'crook') {
    const shaft = new THREE.Mesh(new THREE.CylinderGeometry(0.05, 0.06, 2.2, 5), mat(kind === 'crook' ? '#5a4a2a' : '#4a2a1a'));
    shaft.position.y = 0.3;
    const orb = new THREE.Mesh(new THREE.IcosahedronGeometry(0.17, 0), new THREE.MeshBasicMaterial({ color: glow }));
    orb.position.y = 1.45;
    orb.name = 'orb';
    g.add(shaft, orb);
    if (kind === 'crook') {
      const hook = new THREE.Mesh(new THREE.TorusGeometry(0.2, 0.04, 4, 8, Math.PI * 1.3), mat('#5a4a2a'));
      hook.position.set(0.12, 1.45, 0);
      g.add(hook);
    }
  } else if (kind === 'axe') {
    const shaft = box(0.07, 1.1, 0.07, mat('#4a3020'));
    shaft.position.set(0, 0, 0.35);
    shaft.rotation.x = Math.PI / 2;
    const head = box(0.08, 0.45, 0.35, mat('#8a8a8a'));
    head.position.set(0, 0.15, 0.8);
    g.add(shaft, head);
  } else if (kind === 'club') {
    const c = new THREE.Mesh(new THREE.CylinderGeometry(0.2, 0.08, 1.3, 5), mat('#3b2a20'));
    c.rotation.x = Math.PI / 2;
    c.position.z = 0.6;
    g.add(c);
  }
  g.traverse((o) => (o.castShadow = true));
  return g;
}

function quadruped(color: string, accent: string, scale: number, glowEyes?: string): Rig {
  const rig = makeRig('quadruped', 1.2 * scale, 0.8 * scale);
  const m = mat(color);
  const am = mat(accent);
  const b = rig.body;
  const torso = box(0.7, 0.6, 1.6, m);
  torso.position.y = 0.95;
  const mane = box(0.78, 0.7, 0.6, am);
  mane.position.set(0, 1.05, 0.45);
  const headMesh = box(0.46, 0.44, 0.6, m);
  const snout = box(0.26, 0.24, 0.4, am);
  snout.position.set(0, -0.08, 0.45);
  const earL = new THREE.Mesh(new THREE.ConeGeometry(0.09, 0.28, 4), am);
  earL.position.set(-0.14, 0.3, -0.05);
  const earR = earL.clone();
  earR.position.x = 0.14;
  const head = pivot(0, 1.25, 0.95, headMesh, snout, earL, earR);
  if (glowEyes) {
    const em = new THREE.MeshBasicMaterial({ color: glowEyes });
    for (const s of [-1, 1]) {
      const e = new THREE.Mesh(new THREE.BoxGeometry(0.06, 0.05, 0.02), em);
      e.position.set(s * 0.12, 0.06, 0.3);
      head.add(e);
    }
  }
  const tail = box(0.14, 0.14, 0.7, am);
  tail.position.set(0, 1.1, -1.05);
  tail.rotation.x = 0.5;
  b.add(torso, mane, head, tail);
  rig.head = head;
  for (const [x, z] of [[-0.24, 0.55], [0.24, 0.55], [-0.24, -0.55], [0.24, -0.55]]) {
    const leg = box(0.16, 0.7, 0.18, m);
    leg.position.y = -0.35;
    const p = pivot(x, 0.72, z, leg);
    b.add(p);
    rig.legs.push(p);
  }
  rig.root.scale.setScalar(scale);
  return rig;
}

function spider(color: string, accent: string): Rig {
  const rig = makeRig('spider', 1.1, 1.0);
  const m = mat(color);
  const b = rig.body;
  const abdomen = new THREE.Mesh(new THREE.IcosahedronGeometry(0.65, 0), m);
  abdomen.position.set(0, 0.85, -0.6);
  abdomen.scale.set(1, 0.8, 1.2);
  const mark = new THREE.Mesh(new THREE.IcosahedronGeometry(0.25, 0), new THREE.MeshBasicMaterial({ color: accent }));
  mark.position.set(0, 1.3, -0.65);
  const thorax = new THREE.Mesh(new THREE.IcosahedronGeometry(0.38, 0), m);
  thorax.position.set(0, 0.75, 0.2);
  const headMesh = new THREE.Mesh(new THREE.IcosahedronGeometry(0.22, 0), m);
  const eyes = new THREE.Mesh(new THREE.BoxGeometry(0.26, 0.06, 0.05), new THREE.MeshBasicMaterial({ color: accent }));
  eyes.position.set(0, 0.06, 0.2);
  const head = pivot(0, 0.75, 0.55, headMesh, eyes);
  b.add(abdomen, mark, thorax, head);
  rig.head = head;
  for (let i = 0; i < 8; i++) {
    const side = i < 4 ? -1 : 1;
    const k = i % 4;
    const seg = new THREE.Group();
    const u = box(0.9, 0.08, 0.08, m);
    u.position.set(side * 0.45, 0.25, 0);
    u.rotation.z = side * -0.5;
    const l = box(0.08, 0.95, 0.08, m);
    l.position.set(side * 0.95, -0.15, 0);
    l.rotation.z = side * 0.35;
    seg.add(u, l);
    const p = pivot(side * 0.2, 0.75, 0.45 - k * 0.3, seg);
    p.rotation.y = side * (k - 1.5) * -0.35;
    b.add(p);
    rig.legs.push(p);
  }
  return rig;
}

function collectMaterials(rig: Rig) {
  const set = new Set<THREE.MeshLambertMaterial>();
  rig.root.traverse((o) => {
    const mesh = o as THREE.Mesh;
    if (mesh.isMesh && mesh.material instanceof THREE.MeshLambertMaterial) {
      // Clone so the hit flash only affects this unit.
      const clone = mesh.material.clone();
      mesh.material = clone;
      set.add(clone);
    }
  });
  rig.materials = [...set];
}

/** The hero. `glow` recolours weapon and eyes, e.g. to show the chosen specialization. */
export function buildPlayerModel(cls: ClassId, glow?: string): Rig {
  const c = CLASSES[cls].colors;
  const g = glow ?? c.glow;
  const rig =
    cls === 'stormblade'
      ? humanoid({ body: c.body, trim: c.trim, skin: '#e0b48f', weapon: 'sword', glow: g, bulk: 1.1, cape: '#1d2f55' })
      : cls === 'emberseer'
        ? humanoid({ body: c.body, trim: c.trim, skin: '#d9a57e', weapon: 'staff', glow: g, hood: true, cape: '#5a1a12' })
        : humanoid({ body: c.body, trim: c.trim, skin: '#c89370', weapon: 'crook', glow: g, horns: true, cape: '#26401f' });
  collectMaterials(rig);
  return rig;
}

export function buildNpcModel(color: string): Rig {
  const rig = humanoid({ body: color, trim: '#d8ccb0', skin: '#e0b48f', weapon: 'none' });
  collectMaterials(rig);
  return rig;
}

export function buildMobModel(def: MobDef): Rig {
  let rig: Rig;
  switch (def.model) {
    case 'wolf':
      rig = quadruped(def.color, def.accent, def.scale, '#ffcc55');
      break;
    case 'spider':
      rig = spider(def.color, def.accent);
      break;
    case 'humanoid':
      rig = humanoid({ body: def.color, trim: def.accent, skin: '#8a9a6a', weapon: 'axe', glow: '#ffdd55', scale: def.scale, horns: def.elite, cape: def.elite ? '#6a1a12' : undefined });
      break;
    case 'brute':
      rig = humanoid({ body: def.color, trim: '#2a2422', skin: '#4a3e3a', weapon: 'club', glow: def.accent, scale: 1.45, bulk: 1.35 });
      break;
    case 'imp':
      rig = humanoid({ body: def.color, trim: '#5a1a0a', skin: '#e0602a', weapon: 'none', glow: def.accent, scale: 0.7, horns: true });
      break;
    case 'boss':
    default:
      rig = humanoid({ body: def.color, trim: '#1a1210', skin: '#3a2a26', weapon: 'club', glow: def.accent, scale: 2.8, bulk: 1.4, horns: true });
      break;
  }
  collectMaterials(rig);
  return rig;
}

/** Companion models. */
export function buildPet(kind: 'spiritWolf' | 'bear'): Rig {
  if (kind === 'spiritWolf') return buildSpiritWolf();
  const rig = quadruped('#6a4a30', '#3a2a1a', 1.7, '#ffcc55');
  collectMaterials(rig);
  return rig;
}

function buildSpiritWolf(): Rig {
  const rig = quadruped('#9fe8ff', '#e0f8ff', 1.05, '#ffffff');
  rig.root.traverse((o) => {
    const mesh = o as THREE.Mesh;
    if (mesh.isMesh) {
      mesh.material = new THREE.MeshLambertMaterial({ color: '#a8eaff', emissive: '#3aa0c8', emissiveIntensity: 0.6, transparent: true, opacity: 0.8, flatShading: true });
    }
  });
  collectMaterials(rig);
  return rig;
}

/**
 * Animates a rig.
 * @param moving 0..1 movement speed fraction
 * @param attack 0..1 progress of an attack swing, or -1 for none
 */
export function animateRig(rig: Rig, dt: number, moving: number, attack: number, dead: boolean) {
  if (dead) {
    rig.deathT = Math.min(1, rig.deathT + dt * 2.2);
    const t = rig.deathT;
    rig.body.rotation.z = (rig.shape === 'humanoid' ? Math.PI / 2 : Math.PI) * t * 0.95;
    rig.body.position.y = rig.shape === 'humanoid' ? t * 0.3 : t * 0.9;
    return;
  }
  rig.deathT = 0;
  rig.body.rotation.z = 0;
  rig.phase += dt * (4 + moving * 7);
  const swing = Math.sin(rig.phase) * moving;
  if (rig.shape === 'humanoid') {
    rig.legs[0].rotation.x = swing * 0.8;
    rig.legs[1].rotation.x = -swing * 0.8;
    rig.arms[0].rotation.x = -swing * 0.6;
    rig.body.position.y = Math.abs(Math.cos(rig.phase)) * 0.08 * moving + Math.sin(rig.phase * 0.3) * 0.015;
    if (attack >= 0) {
      // Wind up then strike.
      const a = attack < 0.4 ? -2.4 * (attack / 0.4) : -2.4 + 3.0 * ((attack - 0.4) / 0.6);
      rig.arms[1].rotation.x = a;
      rig.body.rotation.y = Math.sin(attack * Math.PI) * 0.3;
    } else {
      rig.arms[1].rotation.x = swing * 0.6;
      rig.body.rotation.y = 0;
    }
  } else if (rig.shape === 'quadruped') {
    rig.legs[0].rotation.x = swing;
    rig.legs[3].rotation.x = swing;
    rig.legs[1].rotation.x = -swing;
    rig.legs[2].rotation.x = -swing;
    rig.body.position.y = Math.abs(Math.cos(rig.phase)) * 0.1 * moving;
    if (rig.head) rig.head.position.z = 0.95 + (attack >= 0 ? Math.sin(attack * Math.PI) * 0.45 : 0);
  } else {
    rig.legs.forEach((l, i) => (l.rotation.x = Math.sin(rig.phase * 1.6 + i * 1.3) * 0.35 * moving));
    rig.body.position.y = Math.sin(rig.phase * 2) * 0.03;
    if (rig.head) rig.head.position.z = 0.55 + (attack >= 0 ? Math.sin(attack * Math.PI) * 0.3 : 0);
  }
  // Hit flash.
  if (rig.flash > 0) rig.flash = Math.max(0, rig.flash - dt * 5);
  for (const m of rig.materials) m.emissiveIntensity = Math.max(m.userData.baseEmissive ?? 0, rig.flash);
}

export function flashRig(rig: Rig) {
  if (rig.flash <= 0) {
    for (const m of rig.materials) {
      if (m.userData.baseEmissive === undefined) m.userData.baseEmissive = m.emissiveIntensity;
      if (m.userData.baseEmissive === 0) m.emissive.set('#ff5040');
    }
  }
  rig.flash = 0.9;
}
