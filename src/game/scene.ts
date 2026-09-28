// Builds the static 3D world: terrain, water, sky, trees, rocks, village, camps and the boss lair.
import * as THREE from 'three';
import { CAMPS, NPCS, WORLD_LIMIT, ZONES } from '../data/world';
import { LAKE, WATER_LEVEL, colorAt, heightAt, roadDistance, valueNoise } from './terrain';
import { makeRng } from './rules';

export interface Collider { x: number; z: number; r: number }

export interface WorldScene {
  root: THREE.Group;
  colliders: Collider[];
  minimap: HTMLCanvasElement;
  lavaMaterials: THREE.MeshStandardMaterial[];
  water: THREE.Mesh;
}

const SIZE = WORLD_LIMIT * 2 + 40;
const SEGMENTS = 150;

function lambert(color: string, extra: THREE.MeshLambertMaterialParameters = {}) {
  return new THREE.MeshLambertMaterial({ color, flatShading: true, ...extra });
}

function buildTerrain(): THREE.Mesh {
  const geo = new THREE.PlaneGeometry(SIZE, SIZE, SEGMENTS, SEGMENTS);
  geo.rotateX(-Math.PI / 2);
  const pos = geo.attributes.position as THREE.BufferAttribute;
  for (let i = 0; i < pos.count; i++) {
    pos.setY(i, heightAt(pos.getX(i), pos.getZ(i)));
  }
  // Flat-shaded look: non-indexed so every triangle has its own colour.
  const flat = geo.toNonIndexed();
  flat.computeVertexNormals();
  const p = flat.attributes.position as THREE.BufferAttribute;
  const n = flat.attributes.normal as THREE.BufferAttribute;
  const colors = new Float32Array(p.count * 3);
  for (let i = 0; i < p.count; i += 3) {
    const cx = (p.getX(i) + p.getX(i + 1) + p.getX(i + 2)) / 3;
    const cy = (p.getY(i) + p.getY(i + 1) + p.getY(i + 2)) / 3;
    const cz = (p.getZ(i) + p.getZ(i + 1) + p.getZ(i + 2)) / 3;
    const slope = 1 - n.getY(i);
    const c = colorAt(cx, cz, cy, slope * 3);
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

function buildMinimap(): HTMLCanvasElement {
  const res = 320;
  const cv = document.createElement('canvas');
  cv.width = res;
  cv.height = res;
  const ctx = cv.getContext('2d')!;
  const img = ctx.createImageData(res, res);
  const half = WORLD_LIMIT;
  for (let j = 0; j < res; j++) {
    for (let i = 0; i < res; i++) {
      const x = -half + (i / res) * half * 2;
      const z = -half + (j / res) * half * 2;
      const h = heightAt(x, z);
      const hx = heightAt(x + 3, z) - h;
      let c = colorAt(x, z, h, Math.min(1, Math.abs(hx) * 0.8));
      if (h < WATER_LEVEL) c = { r: 0.2, g: 0.38, b: 0.55 };
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

function scatter(rng: () => number, count: number, accept: (x: number, z: number) => boolean): Placement[] {
  const out: Placement[] = [];
  let tries = 0;
  while (out.length < count && tries < count * 20) {
    tries++;
    const x = (rng() * 2 - 1) * (WORLD_LIMIT + 10);
    const z = (rng() * 2 - 1) * (WORLD_LIMIT + 10);
    if (!accept(x, z)) continue;
    out.push({ x, z, s: 0.75 + rng() * 0.6, rot: rng() * Math.PI * 2, tint: rng() });
  }
  return out;
}

function nearSettlement(x: number, z: number, pad: number): boolean {
  if (Math.hypot(x, z - 150) < 40 + pad) return true;
  for (const c of CAMPS) {
    if (c.kind === 'boss' && Math.hypot(x - c.x, z - c.z) < 34 + pad) return true;
    if (c.kind === 'chieftain' && Math.hypot(x - c.x, z - c.z) < 18 + pad) return true;
  }
  return false;
}

function instanced(geo: THREE.BufferGeometry, mat: THREE.Material, items: Placement[], yOff: number, scaleY = 1, colors?: THREE.Color[]) {
  const mesh = new THREE.InstancedMesh(geo, mat, items.length);
  const m = new THREE.Matrix4();
  const q = new THREE.Quaternion();
  const e = new THREE.Euler();
  items.forEach((it, i) => {
    e.set(0, it.rot, 0);
    q.setFromEuler(e);
    m.compose(new THREE.Vector3(it.x, heightAt(it.x, it.z) + yOff * it.s, it.z), q, new THREE.Vector3(it.s, it.s * scaleY, it.s));
    mesh.setMatrixAt(i, m);
    if (colors) mesh.setColorAt(i, colors[i % colors.length].clone().multiplyScalar(0.85 + it.tint * 0.3));
  });
  mesh.castShadow = true;
  mesh.receiveShadow = true;
  return mesh;
}

function zoneId(x: number, z: number): string {
  let best = '';
  let bd = Infinity;
  for (const zn of ZONES) {
    const d = Math.hypot(x - zn.x, z - zn.z) / zn.radius;
    if (d < 1.1 && d < bd) {
      bd = d;
      best = zn.id;
    }
  }
  return best;
}

function buildVegetation(root: THREE.Group, colliders: Collider[]) {
  const rng = makeRng(1337);
  const ok = (x: number, z: number) =>
    roadDistance(x, z) > 7 && !nearSettlement(x, z, 0) && heightAt(x, z) > WATER_LEVEL + 0.6 && Math.hypot(x - LAKE.x, z - LAKE.z) > LAKE.r + 2;

  // Pines everywhere except the Scar; denser in the glade.
  const pines = scatter(rng, 900, (x, z) => {
    const zid = zoneId(x, z);
    if (!ok(x, z) || zid === 'ashenscar' || zid === 'cindermaw' || zid === 'webhollow') return false;
    if (zid === 'duskglade') return true;
    return valueNoise(x * 0.03, z * 0.03) > 0.52;
  });
  const trunkGeo = new THREE.CylinderGeometry(0.25, 0.35, 2, 5);
  const pineGeo = new THREE.ConeGeometry(1.8, 5, 6);
  pineGeo.translate(0, 2.5, 0);
  const pineCols = [new THREE.Color('#2f5a2c'), new THREE.Color('#3a6b32'), new THREE.Color('#264d2a')];
  root.add(instanced(trunkGeo, lambert('#5a4030'), pines, 1));
  root.add(instanced(pineGeo, lambert('#ffffff'), pines, 1.3, 1, pineCols));

  // Broadleaf trees in the meadows.
  const oaks = scatter(rng, 260, (x, z) => ok(x, z) && zoneId(x, z) === '' && valueNoise(x * 0.02 + 9, z * 0.02) > 0.45);
  const crownGeo = new THREE.IcosahedronGeometry(2.4, 0);
  const oakCols = [new THREE.Color('#5d8f3a'), new THREE.Color('#77a043'), new THREE.Color('#4f7f33')];
  root.add(instanced(new THREE.CylinderGeometry(0.3, 0.45, 3, 5), lambert('#6a4a33'), oaks, 1.5));
  root.add(instanced(crownGeo, lambert('#ffffff'), oaks, 4.2, 0.85, oakCols));

  // Dead grey trees in Webhollow.
  const dead = scatter(rng, 160, (x, z) => ok(x, z) && zoneId(x, z) === 'webhollow');
  const deadGeo = new THREE.ConeGeometry(0.35, 7, 4);
  deadGeo.translate(0, 3.5, 0);
  root.add(instanced(deadGeo, lambert('#4a4450'), dead, 0));

  // Rocks everywhere, dark and glowing ones in the Scar.
  const rocks = scatter(rng, 380, (x, z) => ok(x, z) && valueNoise(x * 0.05 + 3, z * 0.05) > 0.5);
  const rockGeo = new THREE.DodecahedronGeometry(1.2, 0);
  const rockCols = [new THREE.Color('#7d786f'), new THREE.Color('#8b857a'), new THREE.Color('#6a655e')];
  root.add(instanced(rockGeo, lambert('#ffffff'), rocks, 0.3, 0.7, rockCols));

  for (const list of [pines, oaks]) for (const t of list) colliders.push({ x: t.x, z: t.z, r: 0.8 * t.s });
  for (const t of dead) colliders.push({ x: t.x, z: t.z, r: 0.5 });
  for (const t of rocks) colliders.push({ x: t.x, z: t.z, r: 1.1 * t.s });

  // Webs: flat translucent discs strung low in Webhollow.
  const webs = scatter(rng, 40, (x, z) => ok(x, z) && zoneId(x, z) === 'webhollow');
  const webGeo = new THREE.CircleGeometry(2.2, 8);
  const webMat = new THREE.MeshBasicMaterial({ color: '#e8e6f0', transparent: true, opacity: 0.25, side: THREE.DoubleSide, depthWrite: false });
  for (const w of webs) {
    const m = new THREE.Mesh(webGeo, webMat);
    m.position.set(w.x, heightAt(w.x, w.z) + 2.2, w.z);
    m.rotation.set(0.3, w.rot, 0);
    root.add(m);
  }
}

function buildScar(root: THREE.Group, colliders: Collider[], lavaMats: THREE.MeshStandardMaterial[]) {
  const rng = makeRng(99);
  const lava = new THREE.MeshStandardMaterial({ color: '#ff5a1f', emissive: '#ff3a0a', emissiveIntensity: 1.4, flatShading: true });
  lavaMats.push(lava);
  const spires = scatter(rng, 110, (x, z) => {
    const zid = zoneId(x, z);
    return (zid === 'ashenscar' || zid === 'cindermaw') && roadDistance(x, z) > 8 && !nearSettlement(x, z, 0);
  });
  const spireGeo = new THREE.ConeGeometry(1.4, 6, 5);
  spireGeo.translate(0, 3, 0);
  const spireCols = [new THREE.Color('#2b2523'), new THREE.Color('#3a302b'), new THREE.Color('#1f1a19')];
  root.add(instanced(spireGeo, lambert('#ffffff'), spires, 0, 1, spireCols));
  for (const s of spires) colliders.push({ x: s.x, z: s.z, r: 1.2 * s.s });
  // Glowing cracks: small emissive shards.
  const cracks = scatter(rng, 90, (x, z) => zoneId(x, z) === 'ashenscar' && roadDistance(x, z) > 5);
  const crackGeo = new THREE.OctahedronGeometry(0.6, 0);
  root.add(instanced(crackGeo, lava, cracks, 0.1, 0.4));
}

function house(x: number, z: number, rot: number, w: number, d: number, wallColor: string, roofColor: string) {
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
  g.position.set(x, heightAt(x, z), z);
  g.rotation.y = rot;
  return g;
}

function brazier(x: number, z: number, lavaMats: THREE.MeshStandardMaterial[]) {
  const g = new THREE.Group();
  const post = new THREE.Mesh(new THREE.CylinderGeometry(0.15, 0.2, 2.2, 5), lambert('#3a3330'));
  post.position.y = 1.1;
  const bowl = new THREE.Mesh(new THREE.CylinderGeometry(0.5, 0.3, 0.4, 6), lambert('#554a40'));
  bowl.position.y = 2.3;
  const flame = new THREE.Mesh(new THREE.ConeGeometry(0.35, 0.9, 5), lavaMats[0]);
  flame.position.y = 2.9;
  flame.name = 'flame';
  g.add(post, bowl, flame);
  g.position.set(x, heightAt(x, z), z);
  return g;
}

function buildVillage(root: THREE.Group, colliders: Collider[], lavaMats: THREE.MeshStandardMaterial[]) {
  const houses: [number, number, number, number, number][] = [
    [-22, 135, 0.4, 7, 6], [22, 132, -0.3, 8, 6], [-26, 160, 1.3, 6, 6], [26, 162, -1.4, 7, 7],
    [-8, 176, 0.1, 9, 6], [12, 178, -0.1, 6, 5], [-34, 148, 1.6, 5, 5],
  ];
  const roofs = ['#8a3b2e', '#6b4a8a', '#3b5f8a', '#8a6a2e'];
  houses.forEach(([x, z, r, w, d], i) => {
    root.add(house(x, z, r, w, d, '#d8ccb0', roofs[i % roofs.length]));
    colliders.push({ x, z, r: Math.max(w, d) * 0.62 });
  });
  // Well in the square.
  const well = new THREE.Group();
  const ring = new THREE.Mesh(new THREE.CylinderGeometry(1.4, 1.5, 1, 10, 1, true), lambert('#8b857a', { side: THREE.DoubleSide }));
  ring.position.y = 0.5;
  const water = new THREE.Mesh(new THREE.CircleGeometry(1.3, 10), new THREE.MeshBasicMaterial({ color: '#2f5f7f' }));
  water.rotation.x = -Math.PI / 2;
  water.position.y = 0.6;
  const roof = new THREE.Mesh(new THREE.ConeGeometry(2, 1.2, 4), lambert('#6a3a2a'));
  roof.position.y = 3.2;
  roof.rotation.y = Math.PI / 4;
  const p1 = new THREE.Mesh(new THREE.BoxGeometry(0.2, 2.6, 0.2), lambert('#5a4030'));
  p1.position.set(1.3, 1.3, 0);
  const p2 = p1.clone();
  p2.position.x = -1.3;
  well.add(ring, water, roof, p1, p2);
  well.position.set(0, heightAt(0, 150), 150);
  root.add(well);
  colliders.push({ x: 0, z: 150, r: 1.8 });
  // Braziers and banners around the square.
  for (const [x, z] of [[-8, 142], [8, 142], [-8, 158], [8, 158], [-4, 118], [6, 118]]) root.add(brazier(x, z, lavaMats));
  // Fence posts along the village edge.
  const postGeo = new THREE.BoxGeometry(0.25, 1.4, 0.25);
  const postMat = lambert('#6a4d33');
  for (let a = 0; a < Math.PI * 2; a += Math.PI / 28) {
    const x = Math.cos(a) * 42;
    const z = 150 + Math.sin(a) * 42;
    if (roadDistance(x, z) < 6) continue;
    const p = new THREE.Mesh(postGeo, postMat);
    p.position.set(x, heightAt(x, z) + 0.7, z);
    root.add(p);
  }
  // Market stall for the vendor.
  const vendor = NPCS.find((n) => n.vendor)!;
  const stall = new THREE.Group();
  const table = new THREE.Mesh(new THREE.BoxGeometry(3.5, 1, 1.4), lambert('#7a5a3a'));
  table.position.set(0, 0.5, -1.4);
  const awn = new THREE.Mesh(new THREE.BoxGeometry(4, 0.15, 2.4), lambert('#b8402e'));
  awn.position.set(0, 2.8, -1.2);
  const crates = new THREE.Mesh(new THREE.BoxGeometry(0.9, 0.9, 0.9), lambert('#9a7a4a'));
  crates.position.set(1.4, 1.45, -1.4);
  stall.add(table, awn, crates);
  stall.position.set(vendor.x, heightAt(vendor.x, vendor.z), vendor.z);
  root.add(stall);
}

function buildRaiderCamp(root: THREE.Group, colliders: Collider[], lavaMats: THREE.MeshStandardMaterial[]) {
  const rng = makeRng(7);
  const tentMat = lambert('#8a6a4a');
  const tentMat2 = lambert('#6a3a2a');
  for (const camp of CAMPS.filter((c) => c.kind === 'raider' || c.kind === 'chieftain')) {
    const n = camp.kind === 'chieftain' ? 3 : 3;
    for (let i = 0; i < n; i++) {
      const a = rng() * Math.PI * 2;
      const r = 8 + rng() * 8;
      const x = camp.x + Math.cos(a) * r;
      const z = camp.z + Math.sin(a) * r;
      const tent = new THREE.Mesh(new THREE.ConeGeometry(2.6, 3.6, 5), i % 2 ? tentMat : tentMat2);
      tent.position.set(x, heightAt(x, z) + 1.8, z);
      tent.castShadow = true;
      root.add(tent);
      colliders.push({ x, z, r: 2.4 });
    }
    root.add(brazier(camp.x + 3, camp.z + 3, lavaMats));
  }
  // Palisade around the chieftain.
  const chief = CAMPS.find((c) => c.kind === 'chieftain')!;
  const stakeGeo = new THREE.ConeGeometry(0.3, 3, 5);
  for (let a = 0; a < Math.PI * 2; a += Math.PI / 16) {
    if (Math.abs(a - 2.41) < 0.45) continue; // gate facing the road to Hearthmoor
    const x = chief.x + Math.cos(a) * 19;
    const z = chief.z + Math.sin(a) * 19;
    const s = new THREE.Mesh(stakeGeo, lambert('#5a3f28'));
    s.position.set(x, heightAt(x, z) + 1.5, z);
    root.add(s);
    colliders.push({ x, z, r: 1.1 });
  }
}

function buildBossLair(root: THREE.Group, colliders: Collider[], lavaMats: THREE.MeshStandardMaterial[]) {
  const boss = CAMPS.find((c) => c.kind === 'boss')!;
  const pillarGeo = new THREE.BoxGeometry(1.6, 9, 1.6);
  for (let i = 0; i < 10; i++) {
    const a = (i / 10) * Math.PI * 2;
    if (Math.abs(a - Math.PI / 2) < 0.4) continue; // entrance from the south
    const x = boss.x + Math.cos(a) * 26;
    const z = boss.z + Math.sin(a) * 26;
    const p = new THREE.Mesh(pillarGeo, lambert('#2e2826'));
    p.position.set(x, heightAt(x, z) + 4, z);
    p.rotation.z = (i % 3 - 1) * 0.08;
    p.castShadow = true;
    root.add(p);
    const cap = new THREE.Mesh(new THREE.OctahedronGeometry(0.7, 0), lavaMats[0]);
    cap.position.set(x, heightAt(x, z) + 9, z);
    root.add(cap);
    colliders.push({ x, z, r: 1.4 });
  }
  const pool = new THREE.Mesh(new THREE.RingGeometry(18, 21, 24), lavaMats[0]);
  pool.rotation.x = -Math.PI / 2;
  pool.position.set(boss.x, heightAt(boss.x, boss.z) + 0.08, boss.z);
  root.add(pool);
}

export function buildWorld(): WorldScene {
  const root = new THREE.Group();
  const colliders: Collider[] = [];
  const lavaMaterials: THREE.MeshStandardMaterial[] = [
    new THREE.MeshStandardMaterial({ color: '#ffb347', emissive: '#ff7a1a', emissiveIntensity: 1.6, flatShading: true }),
  ];
  root.add(buildTerrain());

  const water = new THREE.Mesh(
    new THREE.CircleGeometry(LAKE.r * 1.1, 32),
    new THREE.MeshPhongMaterial({ color: '#3b7ea6', transparent: true, opacity: 0.78, shininess: 90, specular: '#bfe3ff' }),
  );
  water.rotation.x = -Math.PI / 2;
  water.position.set(LAKE.x, WATER_LEVEL, LAKE.z);
  root.add(water);

  buildVegetation(root, colliders);
  buildScar(root, colliders, lavaMaterials);
  buildVillage(root, colliders, lavaMaterials);
  buildRaiderCamp(root, colliders, lavaMaterials);
  buildBossLair(root, colliders, lavaMaterials);
  return { root, colliders, minimap: buildMinimap(), lavaMaterials, water };
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
