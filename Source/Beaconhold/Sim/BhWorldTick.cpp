// Beaconhold simulation core - per-tick behaviour: movement, combat, gathering, production.
#include "BhWorld.h"

#include <algorithm>

namespace bh
{
namespace
{
constexpr float TickLeashDistance = 9.f;
constexpr float TickChaseGiveUp = 8.f;
constexpr float TickTurnRate = 12.f;
constexpr int TickPathBudget = 32;
} // namespace

void World::Tick(float Dt)
{
	for (Entity& E : Entities)
	{
		E.PrevPos = E.Pos;
	}
	for (Projectile& P : Projectiles)
	{
		P.PrevPos = P.Pos;
	}
	if (bFrozen)
	{
		return;
	}
	Time += Dt;
	PathBudget = TickPathBudget;
	for (Entity& E : Entities)
	{
		E.Builders = 0;
		E.Miners = 0;
	}
	RebuildSpatialHash();

	// Entities spawned during this tick are appended and first updated next tick.
	const size_t Count = Entities.size();
	for (size_t I = 0; I < Count; ++I)
	{
		Entity& E = Entities[I];
		if (E.bAlive && E.IsUnit())
		{
			UpdateUnit(E, Dt);
		}
	}
	for (size_t I = 0; I < Count; ++I)
	{
		Entity& E = Entities[I];
		if (E.bAlive && E.IsBuilding())
		{
			UpdateBuilding(E, Dt);
		}
	}
	UpdateProjectiles(Dt);
	ResolveSeparation();
	RemoveDead();
	RecomputeSupply();
}

// ---------------------------------------------------------------------------------------------
// Spatial hash
// ---------------------------------------------------------------------------------------------

void World::RebuildSpatialHash()
{
	for (std::vector<EntityId>& Bucket : Hash)
	{
		Bucket.clear();
	}
	if (HashW <= 0 || HashH <= 0)
	{
		return;
	}
	for (const Entity& E : Entities)
	{
		if (!E.bAlive || E.IsResourceNode())
		{
			continue;
		}
		int X0 = 0;
		int Y0 = 0;
		int X1 = 0;
		int Y1 = 0;
		if (E.IsUnit())
		{
			X0 = X1 = static_cast<int>(E.Pos.X) / HashCell;
			Y0 = Y1 = static_cast<int>(E.Pos.Y) / HashCell;
		}
		else
		{
			X0 = E.Rect.X0 / HashCell;
			Y0 = E.Rect.Y0 / HashCell;
			X1 = (E.Rect.X1 - 1) / HashCell;
			Y1 = (E.Rect.Y1 - 1) / HashCell;
		}
		X0 = ClampI(X0, 0, HashW - 1);
		X1 = ClampI(X1, 0, HashW - 1);
		Y0 = ClampI(Y0, 0, HashH - 1);
		Y1 = ClampI(Y1, 0, HashH - 1);
		for (int Y = Y0; Y <= Y1; ++Y)
		{
			for (int X = X0; X <= X1; ++X)
			{
				Hash[static_cast<size_t>(Y * HashW + X)].push_back(E.Id);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Movement helpers
// ---------------------------------------------------------------------------------------------

bool World::PathTo(Entity& E, const TileRect& Goal, const Vec2& GoalPoint)
{
	if (PathBudget <= 0)
	{
		return false;
	}
	--PathBudget;
	bool bReached = false;
	Paths.FindPath(Map, E.Pos, Goal, GoalPoint, E.Path, bReached);
	E.PathIndex = 0;
	E.bHasPath = !E.Path.empty();
	E.PathGoal = GoalPoint;
	E.StuckTimer = 0.f;
	E.BestWaypointDist = 1e9f;
	return true;
}

bool World::PathToPoint(Entity& E, const Vec2& Point)
{
	Tile T = Tile::FromPos(Point);
	Vec2 GoalPoint = Point;
	if (!Map.IsWalkable(T))
	{
		Tile Free;
		if (Map.FindNearestWalkable(T, 8, Free))
		{
			T = Free;
			GoalPoint = Free.Center();
		}
	}
	return PathTo(E, TileRect(T.X, T.Y, T.X + 1, T.Y + 1), GoalPoint);
}

bool World::PathToRect(Entity& E, const TileRect& Rect, int Expand)
{
	return PathTo(E, Rect.Expanded(Expand), Rect.Center());
}

void World::StopMoving(Entity& E)
{
	E.bHasPath = false;
	E.Path.clear();
	E.PathIndex = 0;
}

void World::FaceTowards(Entity& E, const Vec2& Point, float Dt)
{
	const Vec2 D = Point - E.Pos;
	if (D.LengthSq() < 1e-6f)
	{
		return;
	}
	E.Facing = TurnTowards(E.Facing, D.Angle(), TickTurnRate * Dt);
}

bool World::FollowPath(Entity& E, float Dt)
{
	if (!E.bHasPath || E.PathIndex >= E.Path.size())
	{
		E.bHasPath = false;
		return true;
	}
	const Vec2 Target = E.Path[E.PathIndex];
	const Vec2 Delta = Target - E.Pos;
	const float Dist = Delta.Length();
	const float Step = GetSpeed(E) * Dt;
	E.Act = E.CarryAmount > 0 ? Activity::Carrying : Activity::Walking;
	FaceTowards(E, Target, Dt);
	if (Dist <= Step)
	{
		E.Pos = Target;
		++E.PathIndex;
		E.BestWaypointDist = 1e9f;
		E.StuckTimer = 0.f;
		if (E.PathIndex >= E.Path.size())
		{
			E.bHasPath = false;
			return true;
		}
		return false;
	}
	E.Pos += Delta * (Step / Dist);
	if (Dist < E.BestWaypointDist - 0.05f)
	{
		E.BestWaypointDist = Dist;
		E.StuckTimer = 0.f;
	}
	else
	{
		E.StuckTimer += Dt;
		if (E.StuckTimer > 1.2f)
		{
			// Blocked by a crowd or a new building: re-plan.
			E.StuckTimer = 0.f;
			E.BestWaypointDist = 1e9f;
			E.bHasPath = false;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------------------------
// Units
// ---------------------------------------------------------------------------------------------

void World::UpdateUnit(Entity& E, float Dt)
{
	const ArchetypeDef& D = GetDef(E.Type);
	E.AttackCooldown -= Dt;
	E.AbilityCooldown = MaxF(0.f, E.AbilityCooldown - Dt);
	E.ScanTimer -= Dt;
	E.RepathTimer -= Dt;
	E.HealTimer -= Dt;
	if (E.BuffTimer > 0.f)
	{
		E.BuffTimer -= Dt;
		if (E.BuffTimer <= 0.f)
		{
			E.Buff = BuffType::None;
			E.bChargeReady = false;
		}
	}

	if (D.HealAmount > 0.f && E.HealTimer <= 0.f)
	{
		TryHeal(E);
	}

	if (E.WindupTimer > 0.f)
	{
		E.Act = Activity::Attacking;
		if (const Entity* T = Find(E.WindupTarget))
		{
			FaceTowards(E, T->Pos, Dt);
		}
		E.WindupTimer -= Dt;
		if (E.WindupTimer <= 0.f)
		{
			ResolveStrike(E);
		}
		return;
	}

	switch (E.Order)
	{
	case OrderType::Idle:
		UpdateIdle(E, Dt);
		break;
	case OrderType::Move:
		UpdateMove(E, Dt, false);
		break;
	case OrderType::AttackMove:
		UpdateMove(E, Dt, true);
		break;
	case OrderType::Attack:
		UpdateAttackOrder(E, Dt);
		break;
	case OrderType::Gather:
		UpdateGather(E, Dt);
		break;
	case OrderType::Return:
		UpdateReturn(E, Dt);
		break;
	case OrderType::Build:
		UpdateBuild(E, Dt);
		break;
	}
}

void World::UpdateIdle(Entity& E, float Dt)
{
	if (E.EngageTarget != NoEntity)
	{
		if (Vec2::Dist(E.Pos, E.LeashPoint) > TickLeashDistance)
		{
			// Chased too far from where we stood: walk back.
			E.EngageTarget = NoEntity;
			E.Order = OrderType::Move;
			E.OrderPoint = E.LeashPoint;
			E.bHasPath = false;
			return;
		}
		if (!EngageWith(E, E.EngageTarget, Dt))
		{
			E.EngageTarget = NoEntity;
			E.ScanTimer = 0.f;
		}
		return;
	}
	E.Act = Activity::Idle;
	if (IsCombatUnit(E) && E.ScanTimer <= 0.f)
	{
		E.ScanTimer = 0.25f;
		const EntityId Target = ScanForTarget(E, GetDef(E.Type).Sight);
		if (Target != NoEntity)
		{
			E.EngageTarget = Target;
			E.LeashPoint = E.Pos;
			E.EngageTimer = 0.f;
		}
	}
}

void World::UpdateMove(Entity& E, float Dt, bool bAttackMove)
{
	if (bAttackMove)
	{
		if (E.EngageTarget != NoEntity)
		{
			if (EngageWith(E, E.EngageTarget, Dt))
			{
				return;
			}
			E.EngageTarget = NoEntity;
			E.bHasPath = false;
			E.ScanTimer = 0.f;
		}
		if (E.ScanTimer <= 0.f)
		{
			E.ScanTimer = 0.25f;
			const EntityId Target = ScanForTarget(E, GetDef(E.Type).Sight);
			if (Target != NoEntity)
			{
				E.EngageTarget = Target;
				E.EngageTimer = 0.f;
				return;
			}
		}
	}
	if (!E.bHasPath)
	{
		if (Vec2::Dist(E.Pos, E.OrderPoint) < 0.15f)
		{
			E.Order = OrderType::Idle;
			E.LeashPoint = E.Pos;
			E.Act = Activity::Idle;
			return;
		}
		if (!PathToPoint(E, E.OrderPoint))
		{
			return; // out of path budget this tick
		}
		if (!E.bHasPath)
		{
			E.Order = OrderType::Idle;
			E.LeashPoint = E.Pos;
			E.Act = Activity::Idle;
			return;
		}
	}
	const float StuckBefore = E.StuckTimer;
	if (FollowPath(E, Dt))
	{
		E.Order = OrderType::Idle;
		E.LeashPoint = E.Pos;
		E.Act = Activity::Idle;
		return;
	}
	// Crowded destination: close enough counts as arrived.
	if (!E.bHasPath && StuckBefore > 0.f && Vec2::Dist(E.Pos, E.OrderPoint) < 1.5f)
	{
		E.Order = OrderType::Idle;
		E.LeashPoint = E.Pos;
		E.Act = Activity::Idle;
	}
}

void World::UpdateAttackOrder(Entity& E, float Dt)
{
	if (!EngageWith(E, E.OrderTarget, Dt))
	{
		E.Order = OrderType::Idle;
		E.OrderTarget = NoEntity;
		E.LeashPoint = E.Pos;
		E.ScanTimer = 0.f;
		E.Act = Activity::Idle;
	}
}

bool World::EngageWith(Entity& E, EntityId TargetId, float Dt)
{
	Entity* T = Find(TargetId);
	if (T == nullptr || !T->bAlive || T->IsResourceNode() || !AreEnemies(E.Owner, T->Owner))
	{
		return false;
	}
	const ArchetypeDef& D = GetDef(E.Type);
	const float Range = GetRange(E);
	const float Dist = EdgeDistance(E, *T);
	if (Dist <= Range + 0.05f)
	{
		StopMoving(E);
		FaceTowards(E, T->IsUnit() ? T->Pos : T->Rect.ClosestPoint(E.Pos), Dt);
		E.EngageTimer = 0.f;
		E.Act = Activity::Attacking;
		if (E.AttackCooldown <= 0.f)
		{
			E.WindupTimer = MaxF(D.Windup, 0.01f);
			E.WindupTarget = TargetId;
			E.AttackCooldown = GetCooldown(E);
			++E.AttackSerial;
			GameEvent Ev;
			Ev.Type = EventType::AttackStarted;
			Ev.A = E.Id;
			Ev.B = TargetId;
			Ev.Arch = E.Type;
			Ev.Owner = E.Owner;
			Ev.Pos = E.Pos;
			Emit(Ev);
		}
		return true;
	}

	E.EngageTimer += Dt;
	if (E.EngageTimer > TickChaseGiveUp)
	{
		return false;
	}

	// Close in directly when near; otherwise path.
	const Vec2 Aim = T->IsUnit() ? T->Pos : T->Rect.ClosestPoint(E.Pos);
	const float CenterDist = Vec2::Dist(E.Pos, Aim);
	if (CenterDist < 2.2f)
	{
		const Vec2 Dir = (Aim - E.Pos).Normalized();
		const Vec2 Next = E.Pos + Dir * (GetSpeed(E) * Dt);
		bool bMoved = false;
		if (Map.IsWalkablePos(Next))
		{
			E.Pos = Next;
			bMoved = true;
		}
		else
		{
			const Vec2 SlideX(Next.X, E.Pos.Y);
			const Vec2 SlideY(E.Pos.X, Next.Y);
			if (Map.IsWalkablePos(SlideX) && AbsF(Dir.X) > 0.2f)
			{
				E.Pos = SlideX;
				bMoved = true;
			}
			else if (Map.IsWalkablePos(SlideY) && AbsF(Dir.Y) > 0.2f)
			{
				E.Pos = SlideY;
				bMoved = true;
			}
		}
		if (bMoved)
		{
			StopMoving(E);
			FaceTowards(E, Aim, Dt);
			E.Act = Activity::Walking;
			return true;
		}
	}

	const bool bNeedPath = !E.bHasPath || (E.RepathTimer <= 0.f && Vec2::Dist(E.PathGoal, T->Pos) > 1.5f);
	if (bNeedPath)
	{
		const int Expand = 1 + static_cast<int>(Range);
		bool bPlanned = false;
		if (T->IsUnit())
		{
			const Tile TT = Tile::FromPos(T->Pos);
			bPlanned = PathTo(E, TileRect(TT.X, TT.Y, TT.X + 1, TT.Y + 1).Expanded(Expand), T->Pos);
		}
		else
		{
			bPlanned = PathToRect(E, T->Rect, Expand);
		}
		if (bPlanned)
		{
			E.RepathTimer = 0.5f;
		}
	}
	if (E.bHasPath && FollowPath(E, Dt))
	{
		E.bHasPath = false;
	}
	return true;
}

void World::ResolveStrike(Entity& E)
{
	Entity* T = Find(E.WindupTarget);
	E.WindupTarget = NoEntity;
	if (T == nullptr || !T->bAlive)
	{
		return;
	}
	const ArchetypeDef& D = GetDef(E.Type);
	if (EdgeDistance(E, *T) > GetRange(E) + 0.75f)
	{
		return; // target slipped away
	}
	float Dmg = GetDamage(E);
	if (E.bChargeReady)
	{
		Dmg *= 2.f;
		E.bChargeReady = false;
	}
	if (D.ProjectileSpeed > 0.f)
	{
		Projectile P;
		P.Id = NextProjectileId++;
		P.Start = E.Pos;
		P.Pos = E.Pos;
		P.PrevPos = E.Pos;
		P.Target = T->Id;
		P.TargetPos = T->IsUnit() ? T->Pos : T->Rect.ClosestPoint(E.Pos);
		P.Speed = D.ProjectileSpeed;
		P.Damage = Dmg;
		P.Splash = D.Splash;
		P.BuildingMult = D.BuildingDamageMult;
		P.Source = E.Id;
		P.Owner = E.Owner;
		P.SourceType = E.Type;
		P.TotalDist = MaxF(0.1f, Vec2::Dist(P.Start, P.TargetPos));
		Projectiles.push_back(P);
		EmitSimple(EventType::ProjectileFired, E.Owner, E.Pos, E.Id, E.Type, 0.f, static_cast<int>(P.Id));
		return;
	}
	const Vec2 ImpactPos = T->Pos;
	const Team TargetOwner = T->Owner;
	const EntityId TargetId = T->Id;
	ApplyDamage(*T, Dmg, E.Id, E.Owner, D.BuildingDamageMult, E.Type);
	if (D.Splash > 0.f)
	{
		std::vector<EntityId> Near;
		QueryRadius(ImpactPos, D.Splash, Near);
		for (EntityId Id : Near)
		{
			Entity* S = Find(Id);
			if (S != nullptr && S->bAlive && Id != TargetId && S->Owner == TargetOwner)
			{
				ApplyDamage(*S, Dmg * 0.6f, E.Id, E.Owner, D.BuildingDamageMult, E.Type);
			}
		}
	}
}

EntityId World::ScanForTarget(const Entity& E, float Radius) const
{
	std::vector<EntityId> Near;
	QueryRadius(E.Pos, Radius, Near);
	EntityId Best = NoEntity;
	float BestScore = 1e9f;
	for (EntityId Id : Near)
	{
		const Entity* T = Find(Id);
		if (T == nullptr || !T->bAlive || !AreEnemies(E.Owner, T->Owner) || T->IsResourceNode())
		{
			continue;
		}
		float Score = EdgeDistance(E, *T);
		if (T->IsBuilding())
		{
			Score += GetDef(T->Type).IsTower ? 3.f : 5.f;
		}
		else if (GetDef(T->Type).IsWorker)
		{
			Score += 1.5f;
		}
		if (T->LastAttacker == E.Id)
		{
			Score -= 1.f;
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Id;
		}
	}
	return Best;
}

void World::TryHeal(Entity& E)
{
	const ArchetypeDef& D = GetDef(E.Type);
	E.HealTimer = 0.3f;
	std::vector<EntityId> Near;
	QueryRadius(E.Pos, D.HealRange, Near);
	Entity* Best = nullptr;
	float BestRatio = 0.97f;
	for (EntityId Id : Near)
	{
		Entity* T = Find(Id);
		if (T == nullptr || !T->bAlive || !T->IsUnit() || T->Owner != E.Owner || T->Id == E.Id)
		{
			continue;
		}
		const float Ratio = T->HpRatio();
		if (Ratio < BestRatio)
		{
			BestRatio = Ratio;
			Best = T;
		}
	}
	if (Best == nullptr)
	{
		return;
	}
	float Amount = D.HealAmount;
	if (GetTeam(E.Owner).Researched[static_cast<int>(Research::LanternWisdom)])
	{
		Amount *= 1.5f;
	}
	Heal(*Best, Amount, E.Id);
	E.HealTimer = D.HealCooldown;
}

void World::AssistAllies(const Entity& Victim, EntityId Attacker)
{
	std::vector<EntityId> Near;
	QueryRadius(Victim.Pos, 5.f, Near);
	for (EntityId Id : Near)
	{
		Entity* A = Find(Id);
		if (A == nullptr || !A->bAlive || A->Owner != Victim.Owner || !IsCombatUnit(*A))
		{
			continue;
		}
		if (A->Order == OrderType::Idle && A->EngageTarget == NoEntity)
		{
			A->EngageTarget = Attacker;
			A->LeashPoint = A->Pos;
			A->EngageTimer = 0.f;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Damage
// ---------------------------------------------------------------------------------------------

void World::ApplyDamage(Entity& Target, float Amount, EntityId SourceId, Team SourceTeam, float BuildingMult, Archetype SourceType)
{
	if (!Target.bAlive || Target.IsResourceNode())
	{
		return;
	}
	float Dmg = Amount;
	if (Target.IsBuilding())
	{
		Dmg *= BuildingMult;
	}
	Dmg = MaxF(Dmg * 0.3f, Dmg - GetArmor(Target));
	if (Target.Buff == BuffType::Brace)
	{
		Dmg *= GetAbilityDef(Ability::Brace).Amount;
	}
	const float PreviousHitTime = Target.LastDamagedTime;
	Target.Hp -= Dmg;
	Target.LastDamagedTime = Time;
	Target.LastAttacker = SourceId;

	GameEvent Ev;
	Ev.Type = EventType::Hit;
	Ev.A = Target.Id;
	Ev.B = SourceId;
	Ev.Arch = Target.Type;
	Ev.Owner = Target.Owner;
	Ev.Pos = Target.Pos;
	Ev.Value = Dmg;
	Ev.Sub = static_cast<int>(SourceType);
	Emit(Ev);

	if (Target.Owner == Team::Player && SourceTeam == Team::Enemy)
	{
		TeamState& P = GetTeam(Team::Player);
		if (Time - P.LastAlertTime > 15.f)
		{
			P.LastAlertTime = Time;
			EmitSimple(EventType::UnderAttack, Team::Player, Target.Pos, Target.Id, Target.Type);
		}
	}

	if (Target.Hp <= 0.f)
	{
		DestroyEntity(Target, SourceId);
		return;
	}

	const Entity* Source = Find(SourceId);
	if (Source == nullptr || !Source->bAlive)
	{
		return;
	}
	if (Target.IsUnit() && IsCombatUnit(Target) && Target.Order == OrderType::Idle && Target.EngageTarget == NoEntity)
	{
		Target.EngageTarget = SourceId;
		Target.LeashPoint = Target.Pos;
		Target.EngageTimer = 0.f;
	}
	if (Time - PreviousHitTime > 1.f)
	{
		AssistAllies(Target, SourceId);
	}
}

void World::Heal(Entity& Target, float Amount, EntityId SourceId)
{
	if (!Target.bAlive || Target.Hp >= Target.MaxHp)
	{
		return;
	}
	const float Before = Target.Hp;
	Target.Hp = MinF(Target.MaxHp, Target.Hp + Amount);
	GameEvent Ev;
	Ev.Type = EventType::Healed;
	Ev.A = Target.Id;
	Ev.B = SourceId;
	Ev.Owner = Target.Owner;
	Ev.Pos = Target.Pos;
	Ev.Value = Target.Hp - Before;
	Emit(Ev);
}

void World::DestroyEntity(Entity& E, EntityId Killer)
{
	if (!E.bAlive)
	{
		return;
	}
	E.bAlive = false;
	E.Hp = 0.f;
	GameEvent Ev;
	Ev.Type = EventType::EntityDied;
	Ev.A = E.Id;
	Ev.B = Killer;
	Ev.Arch = E.Type;
	Ev.Owner = E.Owner;
	Ev.Pos = E.Pos;
	Ev.Value = E.Facing;
	Emit(Ev);

	TeamState& T = GetTeam(E.Owner);
	if (E.IsUnit())
	{
		++T.Stats.UnitsLost;
	}
	else
	{
		if (E.IsBuilding())
		{
			++T.Stats.BuildingsLost;
		}
		else
		{
			EmitSimple(EventType::NodeDepleted, Team::Neutral, E.Pos, E.Id, E.Type);
		}
		Map.SetOccupant(E.Rect, NoEntity);
	}
	if (Entity* K = Find(Killer))
	{
		++K->Kills;
		if (K->Owner != E.Owner && E.Owner != Team::Neutral)
		{
			++GetTeam(K->Owner).Stats.Kills;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Workers
// ---------------------------------------------------------------------------------------------

void World::ResumeAfterWork(Entity& W)
{
	W.BuildTarget = NoEntity;
	W.bHasPath = false;
	W.GatherTimer = 0.f;
	if (W.bResumeGather && W.GatherType != Resource::None)
	{
		W.Order = W.CarryAmount > 0 && W.CarryType != W.GatherType ? OrderType::Return : OrderType::Gather;
	}
	else
	{
		W.Order = OrderType::Idle;
		W.LeashPoint = W.Pos;
		W.Act = Activity::Idle;
	}
}

void World::UpdateGather(Entity& W, float Dt)
{
	const TeamState& Team_ = GetTeam(W.Owner);
	Entity* Node = nullptr;
	TileRect Rect;
	if (W.GatherType == Resource::Sunstone)
	{
		Node = Find(W.GatherNode);
		if (Node == nullptr || !Node->bAlive || Node->Amount <= 0)
		{
			const Vec2 From = Node != nullptr ? Node->Pos : W.Pos;
			const EntityId Alt = FindNearestNode(From, 14.f, W.GatherNode);
			if (Alt != NoEntity)
			{
				SetGatherNode(W, Alt);
			}
			else
			{
				W.Order = W.CarryAmount > 0 ? OrderType::Return : OrderType::Idle;
				W.GatherType = W.CarryAmount > 0 ? W.GatherType : Resource::None;
				W.Act = Activity::Idle;
			}
			return;
		}
		Rect = Node->Rect;
	}
	else if (W.GatherType == Resource::Timber)
	{
		if (!Map.HasTree(W.GatherTile.X, W.GatherTile.Y))
		{
			Tile Next;
			if (Map.FindNearestTree(W.GatherTile.Center(), 8.f, Next))
			{
				SetGatherTree(W, Next);
			}
			else
			{
				W.Order = W.CarryAmount > 0 ? OrderType::Return : OrderType::Idle;
				W.Act = Activity::Idle;
			}
			return;
		}
		Rect = TileRect(W.GatherTile.X, W.GatherTile.Y, W.GatherTile.X + 1, W.GatherTile.Y + 1);
	}
	else
	{
		W.Order = OrderType::Idle;
		return;
	}

	if (W.CarryAmount > 0 && W.CarryType != W.GatherType)
	{
		W.Order = OrderType::Return;
		W.bHasPath = false;
		return;
	}
	const int Capacity = W.GatherType == Resource::Sunstone ? GatherTuning::SunstonePerTrip : GatherTuning::TimberPerTrip;
	if (W.CarryAmount >= Capacity)
	{
		W.Order = OrderType::Return;
		W.bHasPath = false;
		return;
	}

	const float Dist = Rect.DistanceTo(W.Pos) - W.Radius;
	if (Dist <= GatherTuning::GatherReach)
	{
		if (Node != nullptr)
		{
			if (Node->Miners >= GatherTuning::MaxMinersPerNode)
			{
				W.Act = Activity::Idle; // wait for a free spot
				return;
			}
			++Node->Miners;
		}
		StopMoving(W);
		FaceTowards(W, Rect.Center(), Dt);
		W.Act = Activity::Gathering;
		W.GatherTimer += Dt * Team_.GatherMult;
		W.GatherFxTimer -= Dt;
		if (W.GatherFxTimer <= 0.f)
		{
			W.GatherFxTimer = 0.85f;
			EmitSimple(EventType::GatherStrike, W.Owner, Rect.Center(), W.Id, W.Type, 0.f, static_cast<int>(W.GatherType));
		}
		const float Needed = W.GatherType == Resource::Sunstone ? GatherTuning::MineTime : GatherTuning::ChopTime;
		if (W.GatherTimer >= Needed)
		{
			W.GatherTimer = 0.f;
			int Taken = 0;
			if (Node != nullptr)
			{
				Taken = MinI(Capacity, Node->Amount);
				Node->Amount -= Taken;
				Node->Hp = static_cast<float>(Node->Amount);
				if (Node->Amount <= 0)
				{
					DestroyEntity(*Node, NoEntity);
				}
			}
			else
			{
				bool bFelled = false;
				Taken = Map.HarvestTree(W.GatherTile.X, W.GatherTile.Y, Capacity, bFelled);
				if (bFelled)
				{
					EmitSimple(EventType::TreeFelled, Team::Neutral, W.GatherTile.Center(), NoEntity, Archetype::None, 0.f, W.GatherTile.Y * 4096 + W.GatherTile.X);
				}
			}
			if (Taken > 0)
			{
				W.CarryType = W.GatherType;
				W.CarryAmount = Taken;
				W.Order = OrderType::Return;
				W.bHasPath = false;
			}
		}
		return;
	}

	W.GatherTimer = 0.f;
	if (!W.bHasPath)
	{
		if (!PathToRect(W, Rect, 1))
		{
			return;
		}
		if (!W.bHasPath)
		{
			// Could not get closer. Trees: try another; nodes: give up.
			if (W.GatherType == Resource::Timber)
			{
				Tile Next;
				if (Map.FindNearestTree(W.Pos, 6.f, Next) && Next != W.GatherTile)
				{
					SetGatherTree(W, Next);
					return;
				}
			}
			W.Order = OrderType::Idle;
			W.Act = Activity::Idle;
			return;
		}
	}
	FollowPath(W, Dt);
}

void World::UpdateReturn(Entity& W, float Dt)
{
	if (W.CarryAmount <= 0)
	{
		W.Order = W.GatherType != Resource::None ? OrderType::Gather : OrderType::Idle;
		W.bHasPath = false;
		return;
	}
	const EntityId DropId = FindNearestDropOff(W.Owner, W.Pos);
	Entity* Drop = Find(DropId);
	if (Drop == nullptr)
	{
		W.Order = OrderType::Idle;
		W.Act = Activity::Idle;
		return;
	}
	const float Dist = Drop->Rect.DistanceTo(W.Pos) - W.Radius;
	if (Dist <= 0.5f)
	{
		TeamState& T = GetTeam(W.Owner);
		const int ResIndex = static_cast<int>(W.CarryType);
		if (ResIndex >= 0 && ResIndex < NumResources)
		{
			T.Res[ResIndex] += W.CarryAmount;
			T.Stats.Gathered[ResIndex] += W.CarryAmount;
		}
		EmitSimple(EventType::ResourceDelivered, W.Owner, W.Pos, W.Id, W.Type, static_cast<float>(W.CarryAmount), ResIndex);
		W.CarryAmount = 0;
		W.CarryType = Resource::None;
		W.bHasPath = false;
		if (W.GatherType != Resource::None)
		{
			W.Order = OrderType::Gather;
		}
		else
		{
			W.Order = OrderType::Idle;
			W.Act = Activity::Idle;
		}
		return;
	}
	if (!W.bHasPath || W.OrderTarget != DropId)
	{
		W.OrderTarget = DropId;
		if (!PathToRect(W, Drop->Rect, 1))
		{
			return;
		}
	}
	FollowPath(W, Dt);
}

void World::UpdateBuild(Entity& W, float Dt)
{
	Entity* B = Find(W.BuildTarget);
	if (B == nullptr || !B->bAlive || B->Owner != W.Owner || !B->IsBuilding())
	{
		ResumeAfterWork(W);
		return;
	}
	const bool bRepair = B->bConstructed;
	if (bRepair && B->Hp >= B->MaxHp)
	{
		ResumeAfterWork(W);
		return;
	}
	const float Dist = B->Rect.DistanceTo(W.Pos) - W.Radius;
	if (Dist <= 0.5f)
	{
		StopMoving(W);
		FaceTowards(W, B->Pos, Dt);
		W.Act = Activity::Building;
		++B->Builders;
		W.GatherFxTimer -= Dt;
		if (W.GatherFxTimer <= 0.f)
		{
			W.GatherFxTimer = 0.7f;
			EmitSimple(EventType::GatherStrike, W.Owner, B->Rect.ClosestPoint(W.Pos), W.Id, W.Type, 0.f, 2);
		}
		if (bRepair)
		{
			const float Rate = B->MaxHp / MaxF(1.f, GetDef(B->Type).BuildTime * 1.5f);
			B->Hp = MinF(B->MaxHp, B->Hp + Rate * Dt);
		}
		return;
	}
	if (!W.bHasPath)
	{
		if (!PathToRect(W, B->Rect, 1))
		{
			return;
		}
		if (!W.bHasPath)
		{
			ResumeAfterWork(W);
			return;
		}
	}
	FollowPath(W, Dt);
}

void World::FinishConstruction(Entity& B)
{
	B.bConstructed = true;
	B.BuildProgress = 1.f;
	B.Hp = MinF(B.Hp, B.MaxHp);
	++GetTeam(B.Owner).Stats.BuildingsBuilt;
	EmitSimple(EventType::BuildingCompleted, B.Owner, B.Pos, B.Id, B.Type);
	const EntityId Id = B.Id;
	for (Entity& W : Entities)
	{
		if (W.bAlive && W.IsUnit() && W.Order == OrderType::Build && W.BuildTarget == Id)
		{
			ResumeAfterWork(W);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Buildings
// ---------------------------------------------------------------------------------------------

void World::UpdateBuilding(Entity& B, float Dt)
{
	const ArchetypeDef& D = GetDef(B.Type);
	TeamState& T = GetTeam(B.Owner);

	if (!B.bConstructed)
	{
		if (B.Builders > 0)
		{
			const float Factor = B.Builders == 1 ? 1.f : (B.Builders == 2 ? 1.6f : 2.f);
			const float Rate = Factor / MaxF(1.f, D.BuildTime * T.BuildTimeMult);
			const float Delta = MinF(Rate * Dt, 1.f - B.BuildProgress);
			B.BuildProgress += Delta;
			B.Hp = MinF(B.MaxHp, B.Hp + B.MaxHp * 0.9f * Delta);
			if (B.BuildProgress >= 0.9999f)
			{
				FinishConstruction(B);
			}
		}
		return;
	}

	// Towers
	if (D.IsTower)
	{
		B.AttackCooldown -= Dt;
		B.ScanTimer -= Dt;
		if (B.WindupTimer > 0.f)
		{
			B.WindupTimer -= Dt;
			if (B.WindupTimer <= 0.f)
			{
				ResolveStrike(B);
			}
		}
		else
		{
			const float Range = GetRange(B);
			Entity* Target = Find(B.EngageTarget);
			if (Target == nullptr || !Target->bAlive || EdgeDistance(B, *Target) > Range)
			{
				B.EngageTarget = NoEntity;
				if (B.ScanTimer <= 0.f)
				{
					B.ScanTimer = 0.25f;
					B.EngageTarget = FindNearestEnemy(B.Owner, B.Pos, Range + B.Radius, true);
					Target = Find(B.EngageTarget);
					if (Target != nullptr && EdgeDistance(B, *Target) > Range)
					{
						B.EngageTarget = NoEntity;
						Target = nullptr;
					}
				}
			}
			if (Target != nullptr && B.EngageTarget != NoEntity && B.AttackCooldown <= 0.f)
			{
				B.WindupTimer = MaxF(D.Windup, 0.01f);
				B.WindupTarget = B.EngageTarget;
				B.AttackCooldown = GetCooldown(B);
				++B.AttackSerial;
				B.Facing = (Target->Pos - B.Pos).Angle();
				GameEvent Ev;
				Ev.Type = EventType::AttackStarted;
				Ev.A = B.Id;
				Ev.B = B.EngageTarget;
				Ev.Arch = B.Type;
				Ev.Owner = B.Owner;
				Ev.Pos = B.Pos;
				Emit(Ev);
			}
		}
	}

	// Beacon aura and income
	if (D.HealRange > 0.f && D.HealAmount > 0.f)
	{
		B.HealTimer -= Dt;
		if (B.HealTimer <= 0.f)
		{
			B.HealTimer = D.HealCooldown;
			std::vector<EntityId> Near;
			QueryRadius(B.Pos, D.HealRange, Near);
			for (EntityId Id : Near)
			{
				Entity* U = Find(Id);
				if (U != nullptr && U->bAlive && U->IsUnit() && U->Owner == B.Owner && U->Hp < U->MaxHp)
				{
					U->Hp = MinF(U->MaxHp, U->Hp + D.HealAmount);
				}
			}
		}
	}
	if (D.IncomePerSecond > 0.f)
	{
		B.IncomeAccum += D.IncomePerSecond * Dt;
		if (B.IncomeAccum >= 5.f)
		{
			B.IncomeAccum -= 5.f;
			T.Res[0] += 5;
			T.Stats.Gathered[0] += 5;
			EmitSimple(EventType::ResourceDelivered, B.Owner, B.Pos, B.Id, B.Type, 5.f, 0);
		}
	}

	// Production
	if (!B.Queue.empty())
	{
		ProductionItem& Item = B.Queue.front();
		Item.Elapsed += Dt;
		if (Item.Elapsed >= Item.Total)
		{
			const ProductionItem Done = Item;
			B.Queue.erase(B.Queue.begin());
			CompleteProduction(B, Done);
		}
	}
}

Tile World::FindSpawnTile(const Entity& Building, const Vec2& Towards) const
{
	for (int R = 1; R <= 4; ++R)
	{
		const TileRect Ring = Building.Rect.Expanded(R);
		float BestD = 1e9f;
		Tile Best;
		bool bFound = false;
		for (int Y = Ring.Y0; Y < Ring.Y1; ++Y)
		{
			for (int X = Ring.X0; X < Ring.X1; ++X)
			{
				const bool bEdge = X == Ring.X0 || X == Ring.X1 - 1 || Y == Ring.Y0 || Y == Ring.Y1 - 1;
				if (!bEdge || !Map.IsWalkable(X, Y))
				{
					continue;
				}
				const float D = Vec2::DistSq(Tile(X, Y).Center(), Towards);
				if (D < BestD)
				{
					BestD = D;
					Best = Tile(X, Y);
					bFound = true;
				}
			}
		}
		if (bFound)
		{
			return Best;
		}
	}
	return Tile::FromPos(Building.Pos);
}

void World::CompleteProduction(Entity& Building, const ProductionItem& Item)
{
	TeamState& T = GetTeam(Building.Owner);
	if (Item.bResearch)
	{
		T.Researched[static_cast<int>(Item.Tech)] = true;
		EmitSimple(EventType::ResearchCompleted, Building.Owner, Building.Pos, Building.Id, Building.Type, 0.f, static_cast<int>(Item.Tech));
		return;
	}
	const int F = MaxI(GetDef(Building.Type).Footprint, 1);
	const Vec2 Towards = Building.bHasRally ? Building.RallyPoint : Building.Pos + Vec2(0.f, static_cast<float>(F));
	const Tile SpawnAt = FindSpawnTile(Building, Towards);
	const bool bHasRally = Building.bHasRally;
	const Vec2 RallyPoint = Building.RallyPoint;
	const EntityId RallyNode = Building.RallyNode;
	const bool bRallyTree = Building.bRallyTree;
	const Tile RallyTree = Building.RallyTree;
	const Team Owner = Building.Owner;
	const Vec2 BuildingPos = Building.Pos;

	const EntityId U = SpawnUnit(Item.Unit, Owner, SpawnAt.Center());
	if (U == NoEntity)
	{
		return;
	}
	++T.Stats.UnitsTrained;
	EmitSimple(EventType::UnitTrained, Owner, SpawnAt.Center(), U, Item.Unit);

	const bool bWorker = GetDef(Item.Unit).IsWorker;
	const std::vector<EntityId> One(1, U);
	if (bHasRally)
	{
		const Entity* NodeE = Find(RallyNode);
		if (bWorker && NodeE != nullptr && NodeE->bAlive && NodeE->IsResourceNode())
		{
			CmdGatherNode(One, RallyNode);
		}
		else if (bWorker && bRallyTree && Map.HasTree(RallyTree.X, RallyTree.Y))
		{
			CmdGatherTree(One, RallyTree);
		}
		else
		{
			CmdMove(One, RallyPoint, !bWorker);
		}
	}
	else if (bWorker)
	{
		const EntityId Node = FindNearestNode(BuildingPos, 12.f, NoEntity);
		if (Node != NoEntity)
		{
			CmdGatherNode(One, Node);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Projectiles, separation, cleanup
// ---------------------------------------------------------------------------------------------

void World::UpdateProjectiles(float Dt)
{
	for (Projectile& P : Projectiles)
	{
		if (!P.bAlive)
		{
			continue;
		}
		Entity* T = Find(P.Target);
		if (T != nullptr && T->bAlive)
		{
			P.TargetPos = T->IsUnit() ? T->Pos : T->Rect.ClosestPoint(P.Start);
		}
		const Vec2 Delta = P.TargetPos - P.Pos;
		const float Dist = Delta.Length();
		const float Step = P.Speed * Dt;
		if (Dist <= Step + 0.05f)
		{
			P.Pos = P.TargetPos;
			P.bAlive = false;
			if (T != nullptr && T->bAlive)
			{
				const Team Victims = T->Owner;
				const EntityId TargetId = T->Id;
				const Vec2 Impact = T->Pos;
				ApplyDamage(*T, P.Damage, P.Source, P.Owner, P.BuildingMult, P.SourceType);
				if (P.Splash > 0.f)
				{
					std::vector<EntityId> Near;
					QueryRadius(Impact, P.Splash, Near);
					for (EntityId Id : Near)
					{
						Entity* S = Find(Id);
						if (S != nullptr && S->bAlive && Id != TargetId && S->Owner == Victims)
						{
							ApplyDamage(*S, P.Damage * 0.6f, P.Source, P.Owner, P.BuildingMult, P.SourceType);
						}
					}
				}
			}
			continue;
		}
		P.Pos += Delta * (Step / Dist);
		P.Travelled += Step;
		P.TotalDist = MaxF(P.TotalDist, P.Travelled + Dist);
	}
}

void World::ResolveSeparation()
{
	std::vector<EntityId> Near;
	for (Entity& A : Entities)
	{
		if (!A.bAlive || !A.IsUnit())
		{
			continue;
		}
		QueryRadius(A.Pos, A.Radius + 0.9f, Near);
		for (EntityId Id : Near)
		{
			if (Id <= A.Id)
			{
				continue;
			}
			Entity* B = Find(Id);
			if (B == nullptr || !B->bAlive || !B->IsUnit())
			{
				continue;
			}
			Vec2 Delta = A.Pos - B->Pos;
			const float MinDist = A.Radius + B->Radius - 0.02f;
			float Dist = Delta.Length();
			if (Dist >= MinDist)
			{
				continue;
			}
			if (Dist < 1e-4f)
			{
				const float Angle = static_cast<float>((A.Id * 2654435761u) % 628u) * 0.01f;
				Delta = Vec2::FromAngle(Angle);
				Dist = 1.f;
			}
			auto Weight = [](const Entity& E) -> float
			{
				switch (E.Act)
				{
				case Activity::Attacking:
				case Activity::Gathering:
				case Activity::Building:
					return 0.15f;
				case Activity::Idle:
					return 0.6f;
				case Activity::Walking:
				case Activity::Carrying:
					return 1.f;
				}
				return 1.f;
			};
			const float WA = Weight(A);
			const float WB = Weight(*B);
			const float Overlap = (MinDist - Dist) * 0.5f;
			const Vec2 N = Delta / Dist;
			const Vec2 NewA = A.Pos + N * (Overlap * WA / (WA + WB) * 2.f * 0.5f);
			const Vec2 NewB = B->Pos - N * (Overlap * WB / (WA + WB) * 2.f * 0.5f);
			if (Map.IsWalkablePos(NewA))
			{
				A.Pos = NewA;
			}
			if (Map.IsWalkablePos(NewB))
			{
				B->Pos = NewB;
			}
		}
		// Never leave a unit inside a blocked tile.
		if (!Map.IsWalkablePos(A.Pos))
		{
			Tile Free;
			if (Map.FindNearestWalkable(Tile::FromPos(A.Pos), 4, Free))
			{
				const Vec2 C = Free.Center();
				A.Pos = Vec2::Lerp(A.Pos, C, 0.5f);
				if (!Map.IsWalkablePos(A.Pos))
				{
					A.Pos = C;
				}
			}
		}
	}
}

void World::RemoveDead()
{
	const auto NewEnd = std::remove_if(Entities.begin(), Entities.end(), [](const Entity& E) { return !E.bAlive; });
	if (NewEnd != Entities.end())
	{
		Entities.erase(NewEnd, Entities.end());
		Index.clear();
		for (size_t I = 0; I < Entities.size(); ++I)
		{
			Index[Entities[I].Id] = I;
		}
	}
	Projectiles.erase(std::remove_if(Projectiles.begin(), Projectiles.end(), [](const Projectile& P) { return !P.bAlive; }), Projectiles.end());
}

} // namespace bh
