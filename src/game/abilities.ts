// The ability engine: runs the data-driven effects defined in src/data/classes.ts.
import * as THREE from 'three';
import { ABILITIES, CLASSES, type AbilityDef, type AbilityId, type Effect, type Fx } from '../data/classes';
import { play } from './audio';
import { abilityLearned } from './progress';
import type { Game } from './game';
import type { Unit } from './units';

interface Ctx {
  ab: AbilityDef;
  target: Unit | null;
  /** Ground point for area effects (defaults to the target's position). */
  center: { x: number; z: number } | null;
}

export function abilityMods(g: Game, id: AbilityId) {
  const a = g.tb.ability[id];
  return {
    dmg: 1 + (a?.dmg ?? 0) / 100,
    cd: Math.max(0.3, 1 - (a?.cd ?? 0) / 100),
    cost: Math.max(0, 1 - (a?.cost ?? 0) / 100),
  };
}

export function abilityCost(g: Game, id: AbilityId) {
  return Math.round(ABILITIES[id].cost * abilityMods(g, id).cost);
}

export function abilityCooldown(g: Game, id: AbilityId) {
  return ABILITIES[id].cooldown * abilityMods(g, id).cd;
}

/** Tries to cast an ability; returns true when it went off. */
export function castAbility(g: Game, id: AbilityId): boolean {
  const pl = g.player;
  const ab = ABILITIES[id];
  const cls = CLASSES[g.progress.cls];
  if (pl.dead || g.leap) return false;
  if (!abilityLearned(g.progress, id)) {
    g.fail(ab.spec && ab.spec !== g.progress.spec ? 'Part of another path' : `Learned at level ${ab.unlockLevel}`);
    return false;
  }
  if (pl.stunned(g.now)) return g.fail('You are stunned'), false;
  if (g.cooldownLeft(id) > 0) return g.fail('Not ready yet'), false;
  const cost = abilityCost(g, id);
  if (g.resource < cost) return g.fail(`Not enough ${cls.resourceName}`), false;
  let target = pl.target && !pl.target.dead && pl.target.isHostileTo(pl) ? pl.target : null;
  if (ab.needsTarget) {
    if (!target || pl.distTo(target) - target.radius > ab.range) {
      const near = g.nearestEnemy(ab.range, true);
      if (near) target = near;
    }
    if (!target) return g.fail('No target in range'), false;
    if (pl.distTo(target) - target.radius > ab.range) return g.fail('Out of range'), false;
    pl.target = target;
  }
  if (target) {
    pl.facing = Math.atan2(target.pos.x - pl.pos.x, target.pos.z - pl.pos.z);
    g.autoAttack = true;
  } else if (ab.effects.some((e) => 'area' in e && e.area === 'cone')) {
    const near = g.nearestEnemy(7, true);
    if (near) pl.facing = Math.atan2(near.pos.x - pl.pos.x, near.pos.z - pl.pos.z);
  }
  g.resource -= cost;
  g.cooldowns[id] = g.now + abilityCooldown(g, id);
  pl.attackAnim = 0;
  if (ab.sound) play(ab.sound === 'frost' ? 'shock' : ab.sound);

  const ctx: Ctx = { ab, target, center: null };
  if (ab.delivery === 'projectile' && target) {
    const pr = ab.projectile!;
    g.fireProjectile(pl, target, pr.color, pr.speed, () => {
      applyEffects(g, ctx, ab.effects);
      runFx(g, ctx, ab.fx);
    }, pr.size);
  } else if (ab.delivery === 'leap' && target) {
    const dir = new THREE.Vector3(pl.pos.x - target.pos.x, 0, pl.pos.z - target.pos.z).normalize();
    const to = new THREE.Vector3(target.pos.x + dir.x * (target.radius + 1), 0, target.pos.z + dir.z * (target.radius + 1));
    g.leap = {
      from: pl.pos.clone(), to, t: 0,
      land: () => {
        applyEffects(g, ctx, ab.effects);
        runFx(g, ctx, ab.fx);
      },
    };
  } else {
    applyEffects(g, ctx, ab.effects);
    runFx(g, ctx, ab.fx);
  }
  return true;
}

function centerOf(ctx: Ctx, g: Game) {
  if (ctx.center) return ctx.center;
  if (ctx.target) return { x: ctx.target.pos.x, z: ctx.target.pos.z };
  return { x: g.player.pos.x, z: g.player.pos.z };
}

function targets(g: Game, ctx: Ctx, area: string, radius = 0): Unit[] {
  const pl = g.player;
  switch (area) {
    case 'self':
      return [pl];
    case 'target':
      return ctx.target && !ctx.target.dead ? [ctx.target] : [];
    case 'aroundSelf':
      return g.enemiesNear(pl.pos.x, pl.pos.z, radius);
    case 'aroundTarget': {
      const c = centerOf(ctx, g);
      return g.enemiesNear(c.x, c.z, radius);
    }
    case 'cone':
      return g.enemiesNear(pl.pos.x, pl.pos.z, radius + 1).filter((u) => {
        const ang = Math.atan2(u.pos.x - pl.pos.x, u.pos.z - pl.pos.z);
        return Math.abs(angleDiff(ang, pl.facing)) < 1.3 || u.distTo(pl) < 2;
      });
    default:
      return [];
  }
}

function applyEffects(g: Game, ctx: Ctx, effects: Effect[]) {
  const pl = g.player;
  const mods = abilityMods(g, ctx.ab.id);
  const scale = g.stats.damageScale * mods.dmg;
  const color = ctx.ab.color[0];
  for (const e of effects) {
    switch (e.t) {
      case 'damage':
        for (const u of targets(g, ctx, e.area, e.radius)) {
          let amt = e.coef * scale;
          if (e.bonusVs && hasCondition(g, u, e.bonusVs)) amt *= e.bonusMul ?? 1;
          g.dealDamage(pl, u, amt, { color });
        }
        break;
      case 'dot':
        for (const u of targets(g, ctx, e.area, e.radius)) {
          g.addDot(u, pl, (e.coef * scale) / e.ticks, e.ticks, e.interval, e.color, false);
        }
        break;
      case 'status':
        for (const u of targets(g, ctx, e.area, e.radius)) {
          if (e.status === 'stun') u.stunUntil = Math.max(u.stunUntil, g.now + e.dur);
          else if (e.status === 'root') u.rootUntil = Math.max(u.rootUntil, g.now + e.dur);
          else u.slowUntil = Math.max(u.slowUntil, g.now + e.dur);
        }
        break;
      case 'heal':
        for (const u of [pl, ...g.pets]) g.heal(u, e.coef * scale * g.healMul, true);
        break;
      case 'hot':
        for (const u of [pl, ...g.pets]) g.addDot(u, pl, (e.coef * scale * g.healMul) / e.ticks, e.ticks, e.interval, '#7dff8a', true);
        break;
      case 'shield':
        pl.shield = e.coef * scale;
        pl.shieldUntil = g.now + e.dur;
        break;
      case 'guard':
        pl.guard = e.reduce;
        pl.guardUntil = g.now + e.dur;
        pl.reflect = (e.reflect ?? 0) * g.stats.damageScale;
        break;
      case 'resource':
        g.resource = Math.min(g.maxResource, g.resource + e.amount);
        break;
      case 'blink':
        g.blink(e.dist);
        break;
      case 'summon':
        g.summonPet(e.pet, e.dur);
        break;
      case 'petFrenzy':
        for (const p of g.pets) {
          p.frenzyUntil = g.now + e.dur;
          g.heal(p, p.maxHp * 0.3, true);
        }
        if (!g.pets.length) g.ui.message('You have no companion to rouse');
        break;
      case 'chain': {
        const hit = new Set<Unit>();
        let from = ctx.target;
        let amt = e.coef * scale;
        let prev = pl.pos.clone().setY(pl.pos.y + 1.6);
        for (let i = 0; i <= e.jumps && from; i++) {
          hit.add(from);
          const to = from.pos.clone().setY(from.pos.y + 1.2);
          g.boltBetween(prev, to, '#dff4ff');
          g.dealDamage(pl, from, amt, { color: '#dff4ff' });
          prev = to;
          amt *= 0.88;
          const last: Unit = from;
          from = g.enemiesNear(last.pos.x, last.pos.z, e.radius).filter((u) => !hit.has(u)).sort((a, b) => a.distTo(last) - b.distTo(last))[0] ?? null;
        }
        break;
      }
      case 'detonate':
        for (const u of targets(g, ctx, e.area, e.radius)) {
          let rest = 0;
          u.dots = u.dots.filter((d) => {
            if (d.heal || d.source !== pl) return true;
            const ticksLeft = d.next <= d.until ? Math.floor((d.until - d.next) / d.interval) + 1 : 0;
            rest += ticksLeft * d.perTick;
            return false;
          });
          g.dealDamage(pl, u, rest * e.mult + e.coef * scale, { color });
        }
        break;
      case 'delayed': {
        const c = centerOf(ctx, g);
        const inner: Ctx = { ab: ctx.ab, target: ctx.target, center: { ...c } };
        g.addTelegraph(c.x, c.z, e.radius, e.delay, color, null, () => {
          applyEffects(g, inner, e.effects);
          g.ringEffect(new THREE.Vector3(c.x, 0, c.z), e.radius, color, 0.6);
          play('boom');
        });
        break;
      }
    }
  }
}

function hasCondition(g: Game, u: Unit, cond: 'slowed' | 'dotted' | 'rooted') {
  if (cond === 'slowed') return u.slowed(g.now) || u.stunned(g.now);
  if (cond === 'rooted') return u.rooted(g.now);
  return u.dots.some((d) => !d.heal && d.source === g.player);
}

function runFx(g: Game, ctx: Ctx, fx?: Fx[]) {
  if (!fx) return;
  for (const f of fx) {
    const at = f.at === 'target' ? centerOf(ctx, g) : { x: g.player.pos.x, z: g.player.pos.z };
    const pos = new THREE.Vector3(at.x, 0, at.z);
    switch (f.kind) {
      case 'ring':
        g.ringEffect(pos, f.radius ?? 3, f.color, 0.5);
        break;
      case 'nova':
        g.ringEffect(pos, f.radius ?? 4, f.color, 0.7);
        g.ringEffect(pos, (f.radius ?? 4) * 0.6, f.color, 0.5);
        break;
      case 'arc':
        g.arcEffect(g.player, f.color);
        break;
      case 'lightning':
        if (f.at === 'target' && ctx.target) g.lightning(ctx.target.pos, f.color);
        else for (const u of g.enemiesNear(g.player.pos.x, g.player.pos.z, 8)) g.lightning(u.pos, f.color);
        break;
      case 'star':
        g.fallingStar(at.x, at.z, f.color);
        break;
      case 'sparkle':
        g.sparkles(f.color);
        break;
    }
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
