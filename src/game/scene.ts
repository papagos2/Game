// Builds the static 3D world of a map from its theme: terrain, water, scenery, hub village, camps, boss lair.
// Visual-only: nothing here affects game rules except the collider list.
import * as THREE from 'three';
import { mergeGeometries } from 'three/examples/jsm/utils/BufferGeometryUtils.js';
import { MOBS, WORLD_LIMIT, type FloraKind, type FloraRule, type MapDef } from '../data/world';
import { WATER_LEVEL, valueNoise, type Terrain } from './terrain';
import { makeRng } from './rules';

export interface Collider { x: number; z: number; r: number }

export interface WorldScene {
  root: THREE.Group;
  colliders: Collider[];
  minimap: HTMLCanvasElement;
  glowMaterials: THREE.MeshStandardMaterial[];
  water: THREE.Mesh | null;
  waystone: THREE.Object3D | null;
}

const SIZE = WORLD_LIMIT * 2 + 40;
const SEGMENTS = 150;

function lambert(color: string, extra: THREE.MeshLambertMaterialParameters = {}) {
  return new THREE.MeshLambertMaterial({ color, flatShading: true, ...extra });
}

function buildTerrain(t: Terrain): THREE.Mesh {
  const geo = new THREE.PlaneGeometry(SIZE, SIZE, SEGMENTS, SEGMENTS);
  geo.rotateX(-Math.PI / 2);
  const pos = geo.attributes.position as THREE.BufferAttribute;
  for (let i = 0; i < pos.count; i++) pos.setY(i, t.heightAt(pos.getX(i), pos.getZ(i)));
  const flat = geo.toNonIndexed();
  flat.computeVertexNormals();
  const p = flat.attributes.position as THREE.BufferAttribute;
  const n = flat.attributes.normal as THREE.BufferAttribute;
  const colors = new Float32Array(p.count * 3);
  for (let i = 0; i < p.count; i += 3) {
    const cx = (p.getX(i) + p.getX(i + 1) + p.getX(i + 2)) / 3;
    const cy = (p.getY(i) + p.getY(i + 1) + p.getY(i + 2)) / 3;
    const cz = (p.getZ(i) + p.getZ(i + 1) + p.getZ(i + 2)) / 3;
    const c = t.colorAt(cx, cz, cy, (1 - n.getY(i)) * 3);
    const jitter = 0.94 + valueNoise(cx * 0.7, cz * 0.7) * 0.1;
    for (let k = 0; k < 3; k++) {
      colors[(i + k) * 3] = c.r * jitter;
      colors[(i + k) * 3 + 1] = c.g * jitter;
      colors[(i + k) * 3 + 2] = c.b * jitter;
    }
  }
  flat.setAttribute('color', new THREE.BufferAttribute(colors, 3));
  const mesh = new THREE.Mesh(flat, new THREE.MeshLambertMaterial({ vertexColors: true }));
  mesh.receiveShadow = true;
  mesh.name = 'terrain';
  return mesh;
}

function buildMinimap(t: Terrain): HTMLCanvasElement {
  const res = 320;
  const cv = document.createElement('canvas');
  cv.width = res;
  cv.height = res;
  const ctx = cv.getContext('2d')!;
  const img = ctx.createImageData(res, res);
  const half = WORLD_LIMIT;
  const water = new THREE.Color(t.map.theme.water);
  for (let j = 0; j < res; j++) {
    for (let i = 0; i < res; i++) {
      const x = -half + (i / res) * half * 2;
      const z = -half + (j / res) * half * 2;
      const h = t.heightAt(x, z);
      const hx = t.heightAt(x + 3, z) - h;
      let c = t.colorAt(x, z, h, Math.min(1, Math.abs(hx) * 0.8));
      if (t.map.lake && h < WATER_LEVEL) c = { r: water.r * 0.8, g: water.g * 0.8, b: water.b * 0.9 };
      const shade = 1 + Math.max(-0.25, Math.min(0.25, -hx * 0.15));
      const o = (j * res + i) * 4;
      img.data[o] = Math.min(255, c.r * 255 * shade);
      img.data[o + 1] = Math.min(255, c.g * 255 * shade);
      img.data[o + 2] = Math.min(255, c.b * 255 * shade);
      img.data[o + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  return cv;
}

interface Placement { x: number; z: number; s: number; rot: number; tint: number }

function instanced(t: Terrain, geo: THREE.BufferGeometry, mat: THREE.Material, items: Placement[], yOff: number, scaleY = 1, colors?: THREE.Color[]) {
  const mesh = new THREE.InstancedMesh(geo, mat, Math.max(1, items.length));
  mesh.count = items.length;
  const m = new THREE.Matrix4();
  const q = new THREE.Quaternion();
  const e = new THREE.Euler();
  items.forEach((it, i) => {
    e.set(0, it.rot, 0);
    q.setFromEuler(e);
    m.compose(new THREE.Vector3(it.x, t.heightAt(it.x, it.z) + yOff * it.s, it.z), q, new THREE.Vector3(it.s, it.s * scaleY, it.s));
    mesh.setMatrixAt(i, m);
    if (colors) mesh.setColorAt(i, colors[i % colors.length].clone().multiplyScalar(0.85 + it.tint * 0.3));
  });
  mesh.castShadow = true;
  mesh.receiveShadow = true;
  return mesh;
}

function hubPos(map: MapDef) {
  return { x: map.zones[0].x, z: map.zones[0].z };
}

function nearSettlement(map: MapDef, x: number, z: number): boolean {
  const hub = hubPos(map);
  if (Math.hypot(x - hub.x, z - hub.z) < 42) return true;
  for (const c of map.camps) {
    const d = MOBS[c.kind];
    if (d.boss && Math.hypot(x - c.x, z - c.z) < 34) return true;
    if (d.elite && !d.boss && Math.hypot(x - c.x, z - c.z) < 22) return true;
  }
  return false;
}

const COLLIDE: Partial<Record<FloraKind, number>> = {
  pine: 0.8, snowpine: 0.8, oak: 0.8, dead: 0.5, rock: 1.1, spire: 1.2, cactus: 0.6, palm: 0.6, obelisk: 1.2, iceshard: 0.9,
};

function floraGeometry(kind: FloraKind): { parts: { geo: THREE.BufferGeometry; color: string; y: number; scaleY?: number; glow?: boolean; tinted?: boolean; opacity?: number }[] } {
  switch (kind) {
    case 'pine': {
      const cone = new THREE.ConeGeometry(1.8, 5, 6);
      cone.translate(0, 2.5, 0);
      return { parts: [{ geo: new THREE.CylinderGeometry(0.25, 0.35, 2, 5), color: '#5a4030', y: 1 }, { geo: cone, color: '#ffffff', y: 1.3, tinted: true }] };
    }
    case 'snowpine': {
      const cone = new THREE.ConeGeometry(1.8, 5, 6);
      cone.translate(0, 2.5, 0);
      const cap = new THREE.ConeGeometry(1.1, 2.4, 6);
      cap.translate(0, 5.2, 0);
      return { parts: [
        { geo: new THREE.CylinderGeometry(0.25, 0.35, 2, 5), color: '#4a3a30', y: 1 },
        { geo: cone, color: '#ffffff', y: 1.3, tinted: true },
        { geo: cap, color: '#f4f8fc', y: 1.3 },
      ] };
    }
    case 'oak':
      return { parts: [{ geo: new THREE.CylinderGeometry(0.3, 0.45, 3, 5), color: '#6a4a33', y: 1.5 }, { geo: new THREE.IcosahedronGeometry(2.4, 0), color: '#ffffff', y: 4.2, scaleY: 0.85, tinted: true }] };
    case 'dead': {
      const g = new THREE.ConeGeometry(0.35, 7, 4);
      g.translate(0, 3.5, 0);
      return { parts: [{ geo: g, color: '#4a4450', y: 0 }] };
    }
    case 'rock':
      return { parts: [{ geo: new THREE.DodecahedronGeometry(1.2, 0), color: '#ffffff', y: 0.3, scaleY: 0.7, tinted: true }] };
    case 'spire': {
      const g = new THREE.ConeGeometry(1.4, 6, 5);
      g.translate(0, 3, 0);
      return { parts: [{ geo: g, color: '#ffffff', y: 0, tinted: true }] };
    }
    case 'ember':
      return { parts: [{ geo: new THREE.OctahedronGeometry(0.6, 0), color: '#ffffff', y: 0.1, scaleY: 0.4, glow: true }] };
    case 'iceshard': {
      const g = new THREE.OctahedronGeometry(1, 0);
      g.scale(0.7, 2.6, 0.7);
      g.translate(0, 1.8, 0);
      return { parts: [{ geo: g, color: '#bfe8ff', y: 0, tinted: true, opacity: 0.85 }] };
    }
    case 'cactus': {
      const main = new THREE.CylinderGeometry(0.35, 0.4, 3.4, 6);
      main.translate(0, 1.7, 0);
      const armA = new THREE.CylinderGeometry(0.22, 0.22, 1.3, 5);
      armA.translate(0.55, 2.2, 0);
      const armB = new THREE.CylinderGeometry(0.22, 0.22, 1, 5);
      armB.translate(-0.5, 1.6, 0);
      return { parts: [{ geo: mergeGeometries([main, armA, armB])!, color: '#5a8a3a', y: 0 }] };
    }
    case 'palm': {
      const trunk = new THREE.CylinderGeometry(0.22, 0.32, 6, 5);
      trunk.translate(0, 3, 0);
      trunk.rotateZ(0.12);
      const leaves: THREE.BufferGeometry[] = [];
      for (let i = 0; i < 6; i++) {
        const l = new THREE.ConeGeometry(0.6, 3.2, 3);
        l.rotateZ(Math.PI / 2 + 0.35);
        l.translate(1.5, 0, 0);
        l.rotateY((i / 6) * Math.PI * 2);
        l.translate(0.7, 6, 0);
        leaves.push(l);
      }
      return { parts: [{ geo: trunk, color: '#8a6a4a', y: 0 }, { geo: mergeGeometries(leaves)!, color: '#3a8a3a', y: 0 }] };
    }
    case 'obelisk': {
      const g = new THREE.CylinderGeometry(0.5, 1.1, 7, 4);
      g.translate(0, 3.5, 0);
      const cap = new THREE.OctahedronGeometry(0.55, 0);
      cap.translate(0, 7.4, 0);
      return { parts: [{ geo: g, color: '#c8a870', y: 0 }, { geo: cap, color: '#ffffff', y: 0, glow: true }] };
    }
    case 'dune': {
      const g = new THREE.SphereGeometry(6, 8, 5, 0, Math.PI * 2, 0, Math.PI / 2);
      g.scale(1.6, 0.35, 1);
      return { parts: [{ geo: g, color: '#e8cc90', y: -0.4 }] };
    }
    case 'web':
    default:
      return { parts: [] };
  }
}

function buildFlora(t: Terrain, root: THREE.Group, colliders: Collider[], glow: THREE.MeshStandardMaterial) {
  const map = t.map;
  const rng = makeRng(1337 + t.seed);
  const lake = map.lake;
  const baseOk = (x: number, z: number) =>
    t.roadDistance(x, z) > 7 && !nearSettlement(map, x, z) && t.heightAt(x, z) > WATER_LEVEL + 0.6
    && (!lake || Math.hypot(x - lake.x, z - lake.z) > lake.r + 2);
  const zoneOk = (rule: FloraRule, x: number, z: number) => {
    const zid = t.sceneryZone(x, z);
    if (rule.zones && !rule.zones.includes(zid)) return false;
    if (rule.notZones && rule.notZones.includes(zid)) return false;
    return true;
  };
  rule: for (const rule of map.theme.flora) {
    const items: Placement[] = [];
    let tries = 0;
    const noiseSeed = rule.kind.length * 3.1;
    while (items.length < rule.count && tries < rule.count * 25) {
      tries++;
      const x = (rng() * 2 - 1) * (WORLD_LIMIT + 10);
      const z = (rng() * 2 - 1) * (WORLD_LIMIT + 10);
      if (!baseOk(x, z) || !zoneOk(rule, x, z)) continue;
      if (rule.noise !== undefined && valueNoise(x * 0.03 + noiseSeed, z * 0.03) < rule.noise) continue;
      items.push({ x, z, s: 0.75 + rng() * 0.6, rot: rng() * Math.PI * 2, tint: rng() });
    }
    if (!items.length) continue;
    if (rule.kind === 'web') {
      const webGeo = new THREE.CircleGeometry(2.2, 8);
      const webMat = new THREE.MeshBasicMaterial({ color: '#e8e6f0', transparent: true, opacity: 0.25, side: THREE.DoubleSide, depthWrite: false });
      for (const w of items) {
        const m = new THREE.Mesh(webGeo, webMat);
        m.position.set(w.x, t.heightAt(w.x, w.z) + 2.2, w.z);
        m.rotation.set(0.3, w.rot, 0);
        root.add(m);
      }
      continue rule;
    }
    const colors = (rule.colors ?? ['#ffffff']).map((c) => new THREE.Color(c));
    for (const part of floraGeometry(rule.kind).parts) {
      const mat = part.glow
        ? glow
        : part.opacity
          ? new THREE.MeshLambertMaterial({ color: part.color, flatShading: true, transparent: true, opacity: part.opacity, emissive: '#2a5a7a', emissiveIntensity: 0.4 })
          : lambert(part.color);
      root.add(instanced(t, part.geo, mat, items, part.y, part.scaleY ?? 1, part.tinted ? colors : undefined));
    }
    const r = COLLIDE[rule.kind];
    if (r) for (const it of items) colliders.push({ x: it.x, z: it.z, r: r * it.s });
  }
}

function house(t: Terrain, x: number, z: number, rot: number, w: number, d: number, wallColor: string, roofColor: string) {
  const g = new THREE.Group();
  const walls = new THREE.Mesh(new THREE.BoxGeometry(w, 3.2, d), lambert(wallColor));
  walls.position.y = 1.6;
  const beam = new THREE.Mesh(new THREE.BoxGeometry(w + 0.2, 0.35, d + 0.2), lambert('#4a3526'));
  beam.position.y = 3.2;
  const roof = new THREE.Mesh(new THREE.ConeGeometry(Math.max(w, d) * 0.8, 2.8, 4), lambert(roofColor));
  roof.position.y = 4.7;
  roof.rotation.y = Math.PI / 4;
  roof.scale.set(w / Math.max(w, d), 1, d / Math.max(w, d));
  const door = new THREE.Mesh(new THREE.BoxGeometry(1.1, 2, 0.1), lambert('#3b2a1e'));
  door.position.set(0, 1, d / 2 + 0.05);
  const win = new THREE.Mesh(new THREE.BoxGeometry(0.8, 0.7, 0.1), new THREE.MeshBasicMaterial({ color: '#ffcf7a' }));
  win.position.set(w / 4 + 0.4, 2, d / 2 + 0.06);
  g.add(walls, beam, roof, door, win);
  g.traverse((o) => {
    o.castShadow = true;
    o.receiveShadow = true;
  });
  g.position.set(x, t.heightAt(x, z), z);
  g.rotation.y = rot;
  return g;
}

function brazier(t: Terrain, x: number, z: number, glow: THREE.Material) {
  const g = new THREE.Group();
  const post = new THREE.Mesh(new THREE.CylinderGeometry(0.15, 0.2, 2.2, 5), lambert('#3a3330'));
  post.position.y = 1.1;
  const bowl = new THREE.Mesh(new THREE.CylinderGeometry(0.5, 0.3, 0.4, 6), lambert('#554a40'));
  bowl.position.y = 2.3;
  const flame = new THREE.Mesh(new THREE.ConeGeometry(0.35, 0.9, 5), glow);
  flame.position.y = 2.9;
  g.add(post, bowl, flame);
  g.position.set(x, t.heightAt(x, z), z);
  return g;
}

function buildHub(t: Terrain, root: THREE.Group, colliders: Collider[], glow: THREE.MeshStandardMaterial): THREE.Object3D | null {
  const map = t.map;
  const hub = hubPos(map);
  const theme = map.theme;
  const houses: [number, number, number, number, number][] = [
    [-22, -15, 0.4, 7, 6], [22, -18, -0.3, 8, 6], [-26, 10, 1.3, 6, 6], [26, 12, -1.4, 7, 7],
    [-8, 26, 0.1, 9, 6], [12, 28, -0.1, 6, 5], [-34, -2, 1.6, 5, 5],
  ];
  houses.forEach(([dx, dz, r, w, d], i) => {
    const x = hub.x + dx;
    const z = hub.z + dz;
    root.add(house(t, x, z, r, w, d, theme.house.wall, theme.house.roofs[i % theme.house.roofs.length]));
    colliders.push({ x, z, r: Math.max(w, d) * 0.62 });
  });
  // Well (or fountain) in the square.
  const well = new THREE.Group();
  const ring = new THREE.Mesh(new THREE.CylinderGeometry(1.4, 1.5, 1, 10, 1, true), lambert('#8b857a', { side: THREE.DoubleSide }));
  ring.position.y = 0.5;
  const water = new THREE.Mesh(new THREE.CircleGeometry(1.3, 10), new THREE.MeshBasicMaterial({ color: theme.water }));
  water.rotation.x = -Math.PI / 2;
  water.position.y = 0.6;
  const roof = new THREE.Mesh(new THREE.ConeGeometry(2, 1.2, 4), lambert(theme.house.roofs[0]));
  roof.position.y = 3.2;
  roof.rotation.y = Math.PI / 4;
  const p1 = new THREE.Mesh(new THREE.BoxGeometry(0.2, 2.6, 0.2), lambert('#5a4030'));
  p1.position.set(1.3, 1.3, 0);
  const p2 = p1.clone();
  p2.position.x = -1.3;
  well.add(ring, water, roof, p1, p2);
  well.position.set(hub.x, t.heightAt(hub.x, hub.z), hub.z);
  root.add(well);
  colliders.push({ x: hub.x, z: hub.z, r: 1.8 });
  for (const [dx, dz] of [[-8, -8], [8, -8], [-8, 8], [8, 8], [-4, -32], [6, -32]]) root.add(brazier(t, hub.x + dx, hub.z + dz, glow));
  const postGeo = new THREE.BoxGeometry(0.25, 1.4, 0.25);
  const postMat = lambert('#6a4d33');
  for (let a = 0; a < Math.PI * 2; a += Math.PI / 28) {
    const x = hub.x + Math.cos(a) * 42;
    const z = hub.z + Math.sin(a) * 42;
    if (t.roadDistance(x, z) < 6) continue;
    const p = new THREE.Mesh(postGeo, postMat);
    p.position.set(x, t.heightAt(x, z) + 0.7, z);
    root.add(p);
  }
  const vendor = map.npcs.find((n) => n.vendor);
  if (vendor) {
    const stall = new THREE.Group();
    const table = new THREE.Mesh(new THREE.BoxGeometry(3.5, 1, 1.4), lambert('#7a5a3a'));
    table.position.set(0, 0.5, -1.4);
    const awn = new THREE.Mesh(new THREE.BoxGeometry(4, 0.15, 2.4), lambert(theme.tent[0]));
    awn.position.set(0, 2.8, -1.2);
    const crates = new THREE.Mesh(new THREE.BoxGeometry(0.9, 0.9, 0.9), lambert('#9a7a4a'));
    crates.position.set(1.4, 1.45, -1.4);
    stall.add(table, awn, crates);
    stall.position.set(vendor.x, t.heightAt(vendor.x, vendor.z), vendor.z);
    root.add(stall);
  }
  // Waystone beside the Wayfinder: the portal between maps.
  const travel = map.npcs.find((n) => n.travel);
  if (!travel) return null;
  const stone = new THREE.Group();
  const base = new THREE.Mesh(new THREE.CylinderGeometry(1.1, 1.4, 0.6, 6), lambert('#5a5460'));
  base.position.y = 0.3;
  const pillar = new THREE.Mesh(new THREE.CylinderGeometry(0.35, 0.6, 3.6, 5), lambert('#6a6470'));
  pillar.position.y = 2.2;
  const crystal = new THREE.Mesh(new THREE.OctahedronGeometry(0.55, 0), new THREE.MeshBasicMaterial({ color: '#9fe8ff' }));
  crystal.position.y = 4.5;
  crystal.name = 'crystal';
  stone.add(base, pillar, crystal);
  const sx = travel.x + 3;
  const sz = travel.z + 2;
  stone.position.set(sx, t.heightAt(sx, sz), sz);
  root.add(stone);
  colliders.push({ x: sx, z: sz, r: 1.3 });
  return stone;
}

function buildCamps(t: Terrain, root: THREE.Group, colliders: Collider[], glow: THREE.MeshStandardMaterial) {
  const map = t.map;
  const rng = makeRng(7 + t.seed);
  const tentMats = map.theme.tent.map((c) => lambert(c));
  for (const camp of map.camps) {
    const def = MOBS[camp.kind];
    if (def.boss) {
      buildBossLair(t, root, colliders, glow, camp.x, camp.z);
      continue;
    }
    if (def.model !== 'humanoid') continue;
    for (let i = 0; i < 3; i++) {
      const a = rng() * Math.PI * 2;
      const r = 8 + rng() * 8;
      const x = camp.x + Math.cos(a) * r;
      const z = camp.z + Math.sin(a) * r;
      const tent = new THREE.Mesh(new THREE.ConeGeometry(2.6, 3.6, 5), tentMats[i % tentMats.length]);
      tent.position.set(x, t.heightAt(x, z) + 1.8, z);
      tent.castShadow = true;
      root.add(tent);
      colliders.push({ x, z, r: 2.4 });
    }
    root.add(brazier(t, camp.x + 3, camp.z + 3, glow));
    if (def.elite) {
      // Palisade with a gate facing the hub.
      const hub = hubPos(map);
      const gate = Math.atan2(hub.z - camp.z, hub.x - camp.x);
      const stakeGeo = new THREE.ConeGeometry(0.3, 3, 5);
      const stakeMat = lambert('#5a3f28');
      for (let a = 0; a < Math.PI * 2; a += Math.PI / 16) {
        let diff = Math.abs(a - gate) % (Math.PI * 2);
        if (diff > Math.PI) diff = Math.PI * 2 - diff;
        if (diff < 0.45) continue;
        const x = camp.x + Math.cos(a) * 19;
        const z = camp.z + Math.sin(a) * 19;
        const s = new THREE.Mesh(stakeGeo, stakeMat);
        s.position.set(x, t.heightAt(x, z) + 1.5, z);
        root.add(s);
        colliders.push({ x, z, r: 1.1 });
      }
    }
  }
}

function buildBossLair(t: Terrain, root: THREE.Group, colliders: Collider[], glow: THREE.MeshStandardMaterial, bx: number, bz: number) {
  const pillarGeo = new THREE.BoxGeometry(1.6, 9, 1.6);
  const pillarMat = lambert(t.map.theme.rock);
  for (let i = 0; i < 10; i++) {
    const a = (i / 10) * Math.PI * 2;
    if (Math.abs(a - Math.PI / 2) < 0.4) continue; // entrance from the south
    const x = bx + Math.cos(a) * 26;
    const z = bz + Math.sin(a) * 26;
    const p = new THREE.Mesh(pillarGeo, pillarMat);
    p.position.set(x, t.heightAt(x, z) + 4, z);
    p.rotation.z = ((i % 3) - 1) * 0.08;
    p.castShadow = true;
    root.add(p);
    const cap = new THREE.Mesh(new THREE.OctahedronGeometry(0.7, 0), glow);
    cap.position.set(x, t.heightAt(x, z) + 9, z);
    root.add(cap);
    colliders.push({ x, z, r: 1.4 });
  }
  const pool = new THREE.Mesh(new THREE.RingGeometry(18, 21, 24), glow);
  pool.rotation.x = -Math.PI / 2;
  pool.position.set(bx, t.heightAt(bx, bz) + 0.08, bz);
  root.add(pool);
}

export function buildWorld(t: Terrain): WorldScene {
  const root = new THREE.Group();
  const colliders: Collider[] = [];
  const theme = t.map.theme;
  const glow = new THREE.MeshStandardMaterial({ color: theme.glow, emissive: theme.glow, emissiveIntensity: 1.5, flatShading: true });
  root.add(buildTerrain(t));
  let water: THREE.Mesh | null = null;
  if (t.map.lake) {
    const lake = t.map.lake;
    water = new THREE.Mesh(
      new THREE.CircleGeometry(lake.r * 1.1, 32),
      new THREE.MeshPhongMaterial({ color: theme.water, transparent: true, opacity: 0.78, shininess: 90, specular: '#bfe3ff' }),
    );
    water.rotation.x = -Math.PI / 2;
    water.position.set(lake.x, WATER_LEVEL, lake.z);
    root.add(water);
  }
  buildFlora(t, root, colliders, glow);
  const waystone = buildHub(t, root, colliders, glow);
  buildCamps(t, root, colliders, glow);
  return { root, colliders, minimap: buildMinimap(t), glowMaterials: [glow], water, waystone };
}

/** Grid for fast "which colliders are near me" queries. */
export class ColliderGrid {
  private cells = new Map<number, Collider[]>();
  constructor(list: Collider[], private cell = 10) {
    for (const c of list) {
      const k = this.key(Math.floor(c.x / cell), Math.floor(c.z / cell));
      let arr = this.cells.get(k);
      if (!arr) this.cells.set(k, (arr = []));
      arr.push(c);
    }
  }
  private key(i: number, j: number) {
    return (i + 1000) * 4000 + (j + 1000);
  }
  /** Pushes a circle at (x,z) with radius r out of any colliders; returns corrected position. */
  resolve(x: number, z: number, r: number): { x: number; z: number } {
    const ci = Math.floor(x / this.cell);
    const cj = Math.floor(z / this.cell);
    for (let i = ci - 1; i <= ci + 1; i++) {
      for (let j = cj - 1; j <= cj + 1; j++) {
        const arr = this.cells.get(this.key(i, j));
        if (!arr) continue;
        for (const c of arr) {
          const dx = x - c.x;
          const dz = z - c.z;
          const min = c.r + r;
          const d2 = dx * dx + dz * dz;
          if (d2 < min * min && d2 > 1e-6) {
            const d = Math.sqrt(d2);
            x = c.x + (dx / d) * min;
            z = c.z + (dz / d) * min;
          }
        }
      }
    }
    return { x, z };
  }
}
