// Beaconhold simulation core — world setup, queries, rules and commands.
#include "BhWorld.h"

#include <algorithm>

namespace bh
{
World::World()
{
	Entities.reserve(MaxEntities);
}

void World::Reset(int MapWidth, int MapHeight, uint32_t Seed)
{
	Map.Init(MapWidth, MapHeight);
	Entities.clear();
	Entities.reserve(MaxEntities);
	Index.clear();
	Projectiles.clear();
	Events.clear();
	for (TeamState& T : Teams)
	{
		T = TeamState();
	}
	Random.SetState(Seed);
	Time = 0.f;
	NextId = 1;
	NextProjectileId = 1;
	bFrozen = false;
	HashW = (MapWidth + HashCell - 1) / HashCell;
	HashH = (MapHeight + HashCell - 1) / HashCell;
	Hash.assign(static_cast<size_t>(HashW * HashH), std::vector<EntityId>());
}

void World::RestoreCounters(uint32_t InNextId, uint32_t InNextProjectileId, float InTime, uint32_t RngState)
{
	NextId = InNextId;
	NextProjectileId = InNextProjectileId;
	Time = InTime;
	Random.SetState(RngState);
}

void World::RebuildIndex()
{
	if (Entities.capacity() < static_cast<size_t>(MaxEntities))
	{
		Entities.reserve(MaxEntities);
	}
	Index.clear();
	for (size_t I = 0; I < Entities.size(); ++I)
	{
		Index[Entities[I].Id] = I;
	}
	HashW = (Map.GetWidth() + HashCell - 1) / HashCell;
	HashH = (Map.GetHeight() + HashCell - 1) / HashCell;
	Hash.assign(static_cast<size_t>(HashW * HashH), std::vector<EntityId>());
}

// ---------------------------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------------------------

Entity* World::SpawnEntity(Archetype Type, Team Owner)
{
	if (Entities.size() >= static_cast<size_t>(MaxEntities) || Type == Archetype::None || Type >= Archetype::Count)
	{
		return nullptr;
	}
	const ArchetypeDef& D = GetDef(Type);
	Entities.emplace_back();
	Entity& E = Entities.back();
	E.Id = NextId++;
	E.Type = Type;
	E.Kind = D.Kind;
	E.Owner = Owner;
	E.Radius = D.Radius;
	E.SpawnTime = Time;
	E.Facing = Pi * 0.5f; // face the camera (south)
	Index[E.Id] = Entities.size() - 1;
	return &E;
}

EntityId World::SpawnUnit(Archetype Type, Team Owner, const Vec2& Pos)
{
	if (!IsUnit(Type))
	{
		return NoEntity;
	}
	Entity* E = SpawnEntity(Type, Owner);
	if (E == nullptr)
	{
		return NoEntity;
	}
	const TeamState& T = GetTeam(Owner);
	E->MaxHp = GetDef(Type).MaxHp * T.HpMult;
	E->Hp = E->MaxHp;
	Vec2 P = Pos;
	if (!Map.IsWalkablePos(P))
	{
		Tile Free;
		if (Map.FindNearestWalkable(Tile::FromPos(P), 8, Free))
		{
			P = Free.Center();
		}
	}
	E->Pos = P;
	E->PrevPos = P;
	E->LeashPoint = P;
	E->OrderPoint = P;
	E->ScanTimer = Random.Range(0.f, 0.25f);
	EmitSimple(EventType::EntitySpawned, Owner, P, E->Id, Type);
	return E->Id;
}

EntityId World::SpawnBuilding(Archetype Type, Team Owner, const Tile& TopLeft, bool bConstructed)
{
	if (!IsBuilding(Type))
	{
		return NoEntity;
	}
	Entity* E = SpawnEntity(Type, Owner);
	if (E == nullptr)
	{
		return NoEntity;
	}
	const ArchetypeDef& D = GetDef(Type);
	const int F = MaxI(D.Footprint, 1);
	E->Rect = TileRect(TopLeft.X, TopLeft.Y, TopLeft.X + F, TopLeft.Y + F);
	E->Pos = E->Rect.Center();
	E->PrevPos = E->Pos;
	E->Radius = static_cast<float>(F) * 0.5f;
	E->MaxHp = D.MaxHp * GetTeam(Owner).BuildingHpMult;
	E->bConstructed = bConstructed;
	E->BuildProgress = bConstructed ? 1.f : 0.f;
	E->Hp = bConstructed ? E->MaxHp : E->MaxHp * 0.1f;
	const EntityId Id = E->Id;
	Map.SetOccupant(E->Rect, Id);
	PushOutUnits(E->Rect);
	EmitSimple(EventType::EntitySpawned, Owner, E->Pos, Id, Type);
	return Id;
}

EntityId World::SpawnResourceNode(Archetype Type, const Tile& TopLeft, int Amount)
{
	if (Type == Archetype::None || Type >= Archetype::Count || GetDef(Type).Kind != EntityKind::Resource)
	{
		return NoEntity;
	}
	Entity* E = SpawnEntity(Type, Team::Neutral);
	if (E == nullptr)
	{
		return NoEntity;
	}
	const int F = MaxI(GetDef(Type).Footprint, 1);
	E->Rect = TileRect(TopLeft.X, TopLeft.Y, TopLeft.X + F, TopLeft.Y + F);
	E->Pos = E->Rect.Center();
	E->PrevPos = E->Pos;
	E->Radius = static_cast<float>(F) * 0.5f;
	E->Amount = Amount > 0 ? Amount : GetDef(Type).ResourceAmount;
	E->MaxHp = static_cast<float>(E->Amount);
	E->Hp = E->MaxHp;
	const EntityId Id = E->Id;
	Map.SetOccupant(E->Rect, Id);
	EmitSimple(EventType::EntitySpawned, Team::Neutral, E->Pos, Id, Type);
	return Id;
}

void World::PushOutUnits(const TileRect& Rect)
{
	for (Entity& U : Entities)
	{
		if (!U.bAlive || !U.IsUnit())
		{
			continue;
		}
		const Tile T = Tile::FromPos(U.Pos);
		if (!Rect.Contains(T))
		{
			continue;
		}
		Tile Free;
		if (Map.FindNearestWalkable(T, 8, Free))
		{
			U.Pos = Free.Center();
			U.PrevPos = U.Pos;
			U.bHasPath = false;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------------------

Entity* World::Find(EntityId Id)
{
	if (Id == NoEntity)
	{
		return nullptr;
	}
	const auto It = Index.find(Id);
	if (It == Index.end() || It->second >= Entities.size())
	{
		return nullptr;
	}
	Entity& E = Entities[It->second];
	return E.Id == Id ? &E : nullptr;
}

const Entity* World::Find(EntityId Id) const
{
	if (Id == NoEntity)
	{
		return nullptr;
	}
	const auto It = Index.find(Id);
	if (It == Index.end() || It->second >= Entities.size())
	{
		return nullptr;
	}
	const Entity& E = Entities[It->second];
	return E.Id == Id ? &E : nullptr;
}

int World::CountOwned(Team T, Archetype A, bool bIncludeUnfinished) const
{
	int Count = 0;
	for (const Entity& E : Entities)
	{
		if (E.bAlive && E.Owner == T && E.Type == A && (bIncludeUnfinished || E.bConstructed))
		{
			++Count;
		}
	}
	return Count;
}

int World::CountUnits(Team T, bool bCombatOnly) const
{
	int Count = 0;
	for (const Entity& E : Entities)
	{
		if (E.bAlive && E.Owner == T && E.IsUnit() && (!bCombatOnly || IsCombatUnit(E)))
		{
			++Count;
		}
	}
	return Count;
}

bool World::HasCompleted(Team T, Archetype A) const
{
	for (const Entity& E : Entities)
	{
		if (E.bAlive && E.Owner == T && E.Type == A && E.bConstructed)
		{
			return true;
		}
	}
	return false;
}

EntityId World::FindNearestDropOff(Team T, const Vec2& Pos) const
{
	EntityId Best = NoEntity;
	float BestD = 1e9f;
	for (const Entity& E : Entities)
	{
		if (!E.bAlive || E.Owner != T || !E.IsBuilding() || !E.bConstructed || !GetDef(E.Type).IsDropOff)
		{
			continue;
		}
		const float D = E.Rect.DistanceTo(Pos);
		if (D < BestD)
		{
			BestD = D;
			Best = E.Id;
		}
	}
	return Best;
}

EntityId World::FindNearestNode(const Vec2& Pos, float MaxDist, EntityId Exclude) const
{
	EntityId Best = NoEntity;
	float BestD = MaxDist;
	for (const Entity& E : Entities)
	{
		if (!E.bAlive || !E.IsResourceNode() || E.Amount <= 0 || E.Id == Exclude)
		{
			continue;
		}
		const float D = E.Rect.DistanceTo(Pos);
		if (D < BestD)
		{
			BestD = D;
			Best = E.Id;
		}
	}
	return Best;
}

EntityId World::FindNearestEnemy(Team T, const Vec2& Pos, float MaxDist, bool bUnitsOnly) const
{
	std::vector<EntityId> Found;
	QueryRadius(Pos, MaxDist, Found);
	EntityId Best = NoEntity;
	float BestD = 1e9f;
	for (EntityId Id : Found)
	{
		const Entity* E = Find(Id);
		if (E == nullptr || !E->bAlive || !AreEnemies(T, E->Owner) || (bUnitsOnly && !E->IsUnit()))
		{
			continue;
		}
		const float D = EdgeDistanceToPoint(*E, Pos);
		if (D < BestD)
		{
			BestD = D;
			Best = Id;
		}
	}
	return Best;
}

void World::QueryRadius(const Vec2& P, float R, std::vector<EntityId>& Out) const
{
	Out.clear();
	if (HashW <= 0 || HashH <= 0)
	{
		return;
	}
	const float Cell = static_cast<float>(HashCell);
	const int CX0 = ClampI(static_cast<int>(std::floor((P.X - R - 2.f) / Cell)), 0, HashW - 1);
	const int CY0 = ClampI(static_cast<int>(std::floor((P.Y - R - 2.f) / Cell)), 0, HashH - 1);
	const int CX1 = ClampI(static_cast<int>(std::floor((P.X + R + 2.f) / Cell)), 0, HashW - 1);
	const int CY1 = ClampI(static_cast<int>(std::floor((P.Y + R + 2.f) / Cell)), 0, HashH - 1);
	Scratch.clear();
	for (int CY = CY0; CY <= CY1; ++CY)
	{
		for (int CX = CX0; CX <= CX1; ++CX)
		{
			const std::vector<EntityId>& Bucket = Hash[static_cast<size_t>(CY * HashW + CX)];
			Scratch.insert(Scratch.end(), Bucket.begin(), Bucket.end());
		}
	}
	std::sort(Scratch.begin(), Scratch.end());
	Scratch.erase(std::unique(Scratch.begin(), Scratch.end()), Scratch.end());
	for (EntityId Id : Scratch)
	{
		const Entity* E = Find(Id);
		if (E == nullptr || !E->bAlive)
		{
			continue;
		}
		if (EdgeDistanceToPoint(*E, P) <= R)
		{
			Out.push_back(Id);
		}
	}
}

float World::EdgeDistanceToPoint(const Entity& A, const Vec2& P)
{
	if (A.IsUnit())
	{
		return MaxF(0.f, Vec2::Dist(A.Pos, P) - A.Radius);
	}
	return A.Rect.DistanceTo(P);
}

float World::EdgeDistance(const Entity& A, const Entity& B) const
{
	float D = 0.f;
	if (!B.IsUnit())
	{
		D = B.Rect.DistanceTo(A.Pos) - (A.IsUnit() ? A.Radius : 0.f);
	}
	else if (!A.IsUnit())
	{
		D = A.Rect.DistanceTo(B.Pos) - B.Radius;
	}
	else
	{
		D = Vec2::Dist(A.Pos, B.Pos) - A.Radius - B.Radius;
	}
	return MaxF(0.f, D);
}

bool World::IsCombatUnit(const Entity& E) const
{
	if (!E.IsUnit())
	{
		return false;
	}
	const ArchetypeDef& D = GetDef(E.Type);
	return D.Damage > 0.f && !D.IsWorker;
}

float World::GetDamage(const Entity& E) const
{
	const ArchetypeDef& D = GetDef(E.Type);
	const TeamState& T = GetTeam(E.Owner);
	float Dmg = D.Damage;
	if (E.Owner == Team::Player && !D.IsWorker)
	{
		if (T.Researched[static_cast<int>(Research::Blades1)])
		{
			Dmg += 2.f;
		}
		if (T.Researched[static_cast<int>(Research::Blades2)])
		{
			Dmg += 2.f;
		}
	}
	return Dmg * T.DamageMult;
}

float World::GetArmor(const Entity& E) const
{
	const ArchetypeDef& D = GetDef(E.Type);
	float Armor = D.Armor;
	if (E.Owner == Team::Player && E.IsUnit() && !D.IsWorker)
	{
		const TeamState& T = GetTeam(E.Owner);
		if (T.Researched[static_cast<int>(Research::Plate1)])
		{
			Armor += 1.f;
		}
		if (T.Researched[static_cast<int>(Research::Plate2)])
		{
			Armor += 1.f;
		}
	}
	return Armor;
}

float World::GetRange(const Entity& E) const
{
	const ArchetypeDef& D = GetDef(E.Type);
	float Range = D.Range;
	if (E.Owner == Team::Player && (E.Type == Archetype::Ranger || E.Type == Archetype::Watchtower) &&
		GetTeam(E.Owner).Researched[static_cast<int>(Research::Fletching)])
	{
		Range += 1.f;
	}
	return Range;
}

float World::GetSpeed(const Entity& E) const
{
	const ArchetypeDef& D = GetDef(E.Type);
	float Speed = D.Speed;
	if (E.Buff == BuffType::Charge)
	{
		Speed *= GetAbilityDef(Ability::Charge).Amount;
	}
	return Speed;
}

float World::GetCooldown(const Entity& E) const
{
	const ArchetypeDef& D = GetDef(E.Type);
	float Cooldown = D.Cooldown;
	if (E.Buff == BuffType::Volley)
	{
		Cooldown *= GetAbilityDef(Ability::Volley).Amount;
	}
	return Cooldown;
}

float World::GetAbilityCooldownMax(const Entity& E) const
{
	const ArchetypeDef& D = GetDef(E.Type);
	if (D.AbilityId == Ability::None)
	{
		return 0.f;
	}
	float Cooldown = GetAbilityDef(D.AbilityId).Cooldown;
	if (GetTeam(E.Owner).Researched[static_cast<int>(Research::LanternWisdom)])
	{
		Cooldown *= 0.7f;
	}
	return Cooldown;
}

// ---------------------------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------------------------

PlaceResult World::CanPlace(Archetype Type, const Tile& TopLeft) const
{
	const ArchetypeDef& D = GetDef(Type);
	const int F = MaxI(D.Footprint, 1);
	const TileRect Rect(TopLeft.X, TopLeft.Y, TopLeft.X + F, TopLeft.Y + F);
	if (!Map.InBounds(Rect.X0, Rect.Y0) || !Map.InBounds(Rect.X1 - 1, Rect.Y1 - 1))
	{
		return PlaceResult::OutOfBounds;
	}
	if (D.NeedsBeaconSite)
	{
		bool bOnSite = false;
		for (const TileRect& Site : Map.BeaconSites)
		{
			if (Site.X0 == Rect.X0 && Site.Y0 == Rect.Y0)
			{
				bOnSite = true;
				break;
			}
		}
		if (!bOnSite)
		{
			return PlaceResult::NeedsBeaconSite;
		}
	}
	for (int Y = Rect.Y0; Y < Rect.Y1; ++Y)
	{
		for (int X = Rect.X0; X < Rect.X1; ++X)
		{
			if (!Map.IsWalkable(X, Y))
			{
				return PlaceResult::Blocked;
			}
			const MapTile& T = Map.At(X, Y);
			if (!D.NeedsBeaconSite && T.BeaconSite >= 0)
			{
				return PlaceResult::Blocked;
			}
			if (T.G == Ground::Blight)
			{
				return PlaceResult::Blight;
			}
		}
	}
	return PlaceResult::Ok;
}

Availability World::CheckBuild(Team T, Archetype Type) const
{
	const ArchetypeDef& D = GetDef(Type);
	if (!D.Buildable)
	{
		return Availability::Locked;
	}
	if (D.Requires != Archetype::None && !HasCompleted(T, D.Requires))
	{
		return Availability::Locked;
	}
	const TeamState& S = GetTeam(T);
	if (S.Res[0] < D.CostSunstone)
	{
		return Availability::NoSunstone;
	}
	if (S.Res[1] < D.CostTimber)
	{
		return Availability::NoTimber;
	}
	return Availability::Ok;
}

Availability World::CheckTrain(const Entity& Building, Archetype Unit) const
{
	if (!Building.bConstructed)
	{
		return Availability::NotConstructed;
	}
	const ArchetypeDef& BD = GetDef(Building.Type);
	bool bListed = false;
	for (Archetype A : BD.Trains)
	{
		if (A == Unit)
		{
			bListed = true;
		}
	}
	if (!bListed)
	{
		return Availability::Locked;
	}
	if (static_cast<int>(Building.Queue.size()) >= MaxQueueLength)
	{
		return Availability::QueueFull;
	}
	const ArchetypeDef& D = GetDef(Unit);
	const TeamState& S = GetTeam(Building.Owner);
	if (S.Res[0] < D.CostSunstone)
	{
		return Availability::NoSunstone;
	}
	if (S.Res[1] < D.CostTimber)
	{
		return Availability::NoTimber;
	}
	if (!S.bIgnoreSupply && S.SupplyUsed + D.SupplyCost > S.SupplyCap)
	{
		return Availability::NoSupply;
	}
	return Availability::Ok;
}

bool World::IsResearchInProgress(Team T, Research Tech) const
{
	for (const Entity& E : Entities)
	{
		if (!E.bAlive || E.Owner != T || !E.IsBuilding())
		{
			continue;
		}
		for (const ProductionItem& Item : E.Queue)
		{
			if (Item.bResearch && Item.Tech == Tech)
			{
				return true;
			}
		}
	}
	return false;
}

Research World::NextResearchLevel(Team T, Research Line) const
{
	Research R = Line;
	const TeamState& S = GetTeam(T);
	int Guard = 0;
	while (R != Research::None && S.Researched[static_cast<int>(R)] && GetResearchDef(R).NextLevel != Research::None && Guard++ < 8)
	{
		R = GetResearchDef(R).NextLevel;
	}
	return R;
}

Availability World::CheckResearch(const Entity& Building, Research Tech) const
{
	if (Tech == Research::None)
	{
		return Availability::Locked;
	}
	if (!Building.bConstructed)
	{
		return Availability::NotConstructed;
	}
	const ResearchDef& RD = GetResearchDef(Tech);
	if (RD.ResearchedAt != Building.Type)
	{
		return Availability::Locked;
	}
	const TeamState& S = GetTeam(Building.Owner);
	if (S.Researched[static_cast<int>(Tech)])
	{
		return Availability::Done;
	}
	if (RD.Requires != Research::None && !S.Researched[static_cast<int>(RD.Requires)])
	{
		return Availability::Locked;
	}
	if (IsResearchInProgress(Building.Owner, Tech))
	{
		return Availability::InProgress;
	}
	if (static_cast<int>(Building.Queue.size()) >= MaxQueueLength)
	{
		return Availability::QueueFull;
	}
	if (S.Res[0] < RD.CostSunstone)
	{
		return Availability::NoSunstone;
	}
	if (S.Res[1] < RD.CostTimber)
	{
		return Availability::NoTimber;
	}
	return Availability::Ok;
}

bool World::CanAfford(Team T, int Sunstone, int Timber) const
{
	const TeamState& S = GetTeam(T);
	return S.Res[0] >= Sunstone && S.Res[1] >= Timber;
}

void World::Spend(Team T, int Sunstone, int Timber)
{
	TeamState& S = GetTeam(T);
	S.Res[0] = MaxI(0, S.Res[0] - Sunstone);
	S.Res[1] = MaxI(0, S.Res[1] - Timber);
}

void World::Refund(Team T, int Sunstone, int Timber)
{
	TeamState& S = GetTeam(T);
	S.Res[0] += Sunstone;
	S.Res[1] += Timber;
}

void World::RecomputeSupply()
{
	for (int TI = 0; TI < NumTeams; ++TI)
	{
		Teams[TI].SupplyUsed = 0;
		Teams[TI].SupplyCap = 0;
	}
	for (const Entity& E : Entities)
	{
		if (!E.bAlive)
		{
			continue;
		}
		TeamState& S = Teams[TeamIndex(E.Owner)];
		const ArchetypeDef& D = GetDef(E.Type);
		if (E.IsUnit())
		{
			S.SupplyUsed += D.SupplyCost;
		}
		else if (E.IsBuilding())
		{
			if (E.bConstructed)
			{
				S.SupplyCap += D.SupplyProvided;
			}
			for (const ProductionItem& Item : E.Queue)
			{
				if (!Item.bResearch)
				{
					S.SupplyUsed += GetDef(Item.Unit).SupplyCost;
				}
			}
		}
	}
	for (int TI = 0; TI < NumTeams; ++TI)
	{
		Teams[TI].SupplyCap = MinI(Teams[TI].SupplyCap, SupplyHardCap);
	}
}

void World::EmitSimple(EventType Type, Team Owner, const Vec2& Pos, EntityId A, Archetype Arch, float Value, int Sub)
{
	GameEvent E;
	E.Type = Type;
	E.Owner = Owner;
	E.Pos = Pos;
	E.A = A;
	E.Arch = Arch;
	E.Value = Value;
	E.Sub = Sub;
	Events.push_back(E);
}

// ---------------------------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------------------------

namespace
{
std::vector<Vec2> WorldFormationSlots(const GameMap& Map, const Vec2& Center, int Count)
{
	std::vector<Vec2> Slots;
	Slots.reserve(static_cast<size_t>(Count));
	Vec2 C = Center;
	if (!Map.IsWalkablePos(C))
	{
		Tile Free;
		if (Map.FindNearestWalkable(Tile::FromPos(C), 6, Free))
		{
			C = Free.Center();
		}
	}
	Slots.push_back(C);
	const float Spacing = 0.8f;
	for (int Ring = 1; Ring <= 8 && static_cast<int>(Slots.size()) < Count; ++Ring)
	{
		const int N = 6 * Ring;
		for (int K = 0; K < N && static_cast<int>(Slots.size()) < Count; ++K)
		{
			const float Angle = TwoPi * static_cast<float>(K) / static_cast<float>(N) + static_cast<float>(Ring) * 0.3f;
			const Vec2 P = C + Vec2::FromAngle(Angle) * (Spacing * static_cast<float>(Ring));
			if (Map.IsWalkablePos(P))
			{
				Slots.push_back(P);
			}
		}
	}
	while (static_cast<int>(Slots.size()) < Count)
	{
		Slots.push_back(C);
	}
	return Slots;
}
} // namespace

void World::CmdMove(const std::vector<EntityId>& Units, const Vec2& Point, bool bAttackMove)
{
	std::vector<Entity*> Movers;
	for (EntityId Id : Units)
	{
		Entity* E = Find(Id);
		if (E != nullptr && E->bAlive && E->IsUnit())
		{
			Movers.push_back(E);
		}
	}
	if (Movers.empty())
	{
		return;
	}
	std::sort(Movers.begin(), Movers.end(), [&Point](const Entity* A, const Entity* B)
	{
		return Vec2::DistSq(A->Pos, Point) < Vec2::DistSq(B->Pos, Point);
	});
	std::vector<Vec2> Slots = WorldFormationSlots(Map, Point, static_cast<int>(Movers.size()));
	std::vector<bool> Taken(Slots.size(), false);
	for (Entity* E : Movers)
	{
		size_t Best = 0;
		float BestD = 1e9f;
		for (size_t S = 0; S < Slots.size(); ++S)
		{
			if (Taken[S])
			{
				continue;
			}
			const float D = Vec2::DistSq(Slots[S], E->Pos);
			if (D < BestD)
			{
				BestD = D;
				Best = S;
			}
		}
		Taken[Best] = true;
		const bool bWorker = GetDef(E->Type).IsWorker;
		E->Order = (bAttackMove && !bWorker) ? OrderType::AttackMove : OrderType::Move;
		E->OrderPoint = Slots[Best];
		E->OrderTarget = NoEntity;
		E->EngageTarget = NoEntity;
		E->WindupTimer = 0.f;
		E->bHasPath = false;
		E->bResumeGather = false;
		E->BuildTarget = NoEntity;
	}
	EmitSimple(EventType::CommandMove, Movers.front()->Owner, Point, Movers.front()->Id, Movers.front()->Type, 0.f, bAttackMove ? 1 : 0);
}

void World::CmdAttack(const std::vector<EntityId>& Units, EntityId Target)
{
	const Entity* T = Find(Target);
	if (T == nullptr || !T->bAlive)
	{
		return;
	}
	const Vec2 TargetPos = T->Pos;
	EntityId First = NoEntity;
	Team Owner = Team::Neutral;
	for (EntityId Id : Units)
	{
		Entity* E = Find(Id);
		if (E == nullptr || !E->bAlive || !E->IsUnit() || GetDef(E->Type).Damage <= 0.f || !AreEnemies(E->Owner, T->Owner))
		{
			continue;
		}
		E->Order = OrderType::Attack;
		E->OrderTarget = Target;
		E->EngageTarget = NoEntity;
		E->EngageTimer = 0.f;
		E->WindupTimer = 0.f;
		E->bHasPath = false;
		E->bResumeGather = false;
		E->BuildTarget = NoEntity;
		if (First == NoEntity)
		{
			First = Id;
			Owner = E->Owner;
		}
	}
	if (First != NoEntity)
	{
		EmitSimple(EventType::CommandAttack, Owner, TargetPos, First, Archetype::None, 0.f, 0);
	}
}

void World::SetGatherNode(Entity& W, EntityId Node)
{
	W.GatherType = Resource::Sunstone;
	W.GatherNode = Node;
	W.GatherTimer = 0.f;
	W.bHasPath = false;
	W.Order = OrderType::Gather;
	W.bResumeGather = true;
	W.OrderTarget = NoEntity;
	W.EngageTarget = NoEntity;
	W.WindupTimer = 0.f;
}

void World::SetGatherTree(Entity& W, const Tile& Tree)
{
	W.GatherType = Resource::Timber;
	W.GatherTile = Tree;
	W.GatherNode = NoEntity;
	W.GatherTimer = 0.f;
	W.bHasPath = false;
	W.Order = OrderType::Gather;
	W.bResumeGather = true;
	W.OrderTarget = NoEntity;
	W.EngageTarget = NoEntity;
	W.WindupTimer = 0.f;
}

void World::CmdGatherNode(const std::vector<EntityId>& Workers, EntityId Node)
{
	const Entity* N = Find(Node);
	if (N == nullptr || !N->bAlive || !N->IsResourceNode())
	{
		return;
	}
	const Vec2 NodePos = N->Pos;
	EntityId First = NoEntity;
	Team Owner = Team::Neutral;
	for (EntityId Id : Workers)
	{
		Entity* W = Find(Id);
		if (W == nullptr || !W->bAlive || !GetDef(W->Type).IsWorker)
		{
			continue;
		}
		SetGatherNode(*W, Node);
		W->BuildTarget = NoEntity;
		if (First == NoEntity)
		{
			First = Id;
			Owner = W->Owner;
		}
	}
	if (First != NoEntity)
	{
		EmitSimple(EventType::CommandGather, Owner, NodePos, First, Archetype::None, 0.f, static_cast<int>(Resource::Sunstone));
	}
}

void World::CmdGatherTree(const std::vector<EntityId>& Workers, const Tile& Tree)
{
	if (!Map.HasTree(Tree.X, Tree.Y))
	{
		return;
	}
	Tile Target = Tree;
	if (!Map.HasWalkableNeighbour(Tree.X, Tree.Y))
	{
		// Interior tree: pick the closest reachable one instead.
		Tile Edge;
		if (Map.FindNearestTree(Tree.Center(), 6.f, Edge))
		{
			Target = Edge;
		}
	}
	EntityId First = NoEntity;
	Team Owner = Team::Neutral;
	for (EntityId Id : Workers)
	{
		Entity* W = Find(Id);
		if (W == nullptr || !W->bAlive || !GetDef(W->Type).IsWorker)
		{
			continue;
		}
		SetGatherTree(*W, Target);
		W->BuildTarget = NoEntity;
		if (First == NoEntity)
		{
			First = Id;
			Owner = W->Owner;
		}
	}
	if (First != NoEntity)
	{
		EmitSimple(EventType::CommandGather, Owner, Target.Center(), First, Archetype::None, 0.f, static_cast<int>(Resource::Timber));
	}
}

EntityId World::CmdBuild(const std::vector<EntityId>& Workers, Archetype Type, const Tile& TopLeft)
{
	// The closest selected Lamplighter builds; the rest keep working.
	Entity* Builder = nullptr;
	const ArchetypeDef& D = GetDef(Type);
	const int F = MaxI(D.Footprint, 1);
	const Vec2 SiteCenter(static_cast<float>(TopLeft.X) + static_cast<float>(F) * 0.5f, static_cast<float>(TopLeft.Y) + static_cast<float>(F) * 0.5f);
	float BestD = 1e9f;
	for (EntityId Id : Workers)
	{
		Entity* W = Find(Id);
		if (W == nullptr || !W->bAlive || !GetDef(W->Type).IsWorker)
		{
			continue;
		}
		const float Dist = Vec2::DistSq(W->Pos, SiteCenter);
		if (Dist < BestD)
		{
			BestD = Dist;
			Builder = W;
		}
	}
	if (Builder == nullptr)
	{
		return NoEntity;
	}
	const Team Owner = Builder->Owner;
	const Availability Avail = CheckBuild(Owner, Type);
	if (Avail != Availability::Ok)
	{
		const EventType Feedback = Avail == Availability::NoSunstone ? EventType::NotEnoughSunstone
			: Avail == Availability::NoTimber ? EventType::NotEnoughTimber
			: EventType::RequirementMissing;
		EmitSimple(Feedback, Owner, SiteCenter, NoEntity, Type);
		return NoEntity;
	}
	if (CanPlace(Type, TopLeft) != PlaceResult::Ok)
	{
		EmitSimple(EventType::InvalidPlacement, Owner, SiteCenter, NoEntity, Type);
		return NoEntity;
	}
	const EntityId BuilderId = Builder->Id;
	Spend(Owner, D.CostSunstone, D.CostTimber);
	const EntityId Id = SpawnBuilding(Type, Owner, TopLeft, false);
	if (Id == NoEntity)
	{
		Refund(Owner, D.CostSunstone, D.CostTimber);
		return NoEntity;
	}
	Entity* W = Find(BuilderId);
	if (W != nullptr)
	{
		W->bResumeGather = W->GatherType != Resource::None && (W->Order == OrderType::Gather || W->Order == OrderType::Return);
		W->Order = OrderType::Build;
		W->BuildTarget = Id;
		W->bHasPath = false;
		W->EngageTarget = NoEntity;
		W->WindupTimer = 0.f;
	}
	EmitSimple(EventType::BuildingPlaced, Owner, SiteCenter, Id, Type);
	EmitSimple(EventType::CommandBuild, Owner, SiteCenter, BuilderId, Type);
	return Id;
}

void World::CmdHelpBuild(const std::vector<EntityId>& Workers, EntityId Building)
{
	const Entity* B = Find(Building);
	if (B == nullptr || !B->bAlive || !B->IsBuilding())
	{
		return;
	}
	const Team Owner = B->Owner;
	const Vec2 BPos = B->Pos;
	const bool bNeedsWork = !B->bConstructed || B->Hp < B->MaxHp;
	if (!bNeedsWork)
	{
		return;
	}
	EntityId First = NoEntity;
	for (EntityId Id : Workers)
	{
		Entity* W = Find(Id);
		if (W == nullptr || !W->bAlive || W->Owner != Owner || !GetDef(W->Type).IsWorker)
		{
			continue;
		}
		W->bResumeGather = W->GatherType != Resource::None && (W->Order == OrderType::Gather || W->Order == OrderType::Return);
		W->Order = OrderType::Build;
		W->BuildTarget = Building;
		W->bHasPath = false;
		W->EngageTarget = NoEntity;
		W->WindupTimer = 0.f;
		if (First == NoEntity)
		{
			First = Id;
		}
	}
	if (First != NoEntity)
	{
		EmitSimple(EventType::CommandBuild, Owner, BPos, First, Archetype::None);
	}
}

void World::CmdReturnCargo(const std::vector<EntityId>& Workers)
{
	for (EntityId Id : Workers)
	{
		Entity* W = Find(Id);
		if (W != nullptr && W->bAlive && W->CarryAmount > 0)
		{
			W->Order = OrderType::Return;
			W->bHasPath = false;
		}
	}
}

void World::CmdStop(const std::vector<EntityId>& Units)
{
	for (EntityId Id : Units)
	{
		Entity* E = Find(Id);
		if (E == nullptr || !E->bAlive || !E->IsUnit())
		{
			continue;
		}
		E->Order = OrderType::Idle;
		E->OrderTarget = NoEntity;
		E->EngageTarget = NoEntity;
		E->WindupTimer = 0.f;
		E->bHasPath = false;
		E->Path.clear();
		E->LeashPoint = E->Pos;
		E->bResumeGather = false;
		E->BuildTarget = NoEntity;
		E->Act = Activity::Idle;
	}
}

bool World::CmdTrain(EntityId BuildingId, Archetype Unit)
{
	Entity* B = Find(BuildingId);
	if (B == nullptr || !B->bAlive || !B->IsBuilding())
	{
		return false;
	}
	const Availability Avail = CheckTrain(*B, Unit);
	if (Avail != Availability::Ok)
	{
		EventType Feedback = EventType::RequirementMissing;
		switch (Avail)
		{
		case Availability::NoSunstone:
			Feedback = EventType::NotEnoughSunstone;
			break;
		case Availability::NoTimber:
			Feedback = EventType::NotEnoughTimber;
			break;
		case Availability::NoSupply:
			Feedback = EventType::SupplyBlocked;
			break;
		case Availability::QueueFull:
			Feedback = EventType::QueueFull;
			break;
		case Availability::Ok:
		case Availability::Locked:
		case Availability::Done:
		case Availability::InProgress:
		case Availability::NotConstructed:
			break;
		}
		EmitSimple(Feedback, B->Owner, B->Pos, B->Id, Unit);
		return false;
	}
	const ArchetypeDef& D = GetDef(Unit);
	Spend(B->Owner, D.CostSunstone, D.CostTimber);
	ProductionItem Item;
	Item.Unit = Unit;
	Item.Total = D.BuildTime;
	Item.PaidSunstone = D.CostSunstone;
	Item.PaidTimber = D.CostTimber;
	B->Queue.push_back(Item);
	RecomputeSupply();
	return true;
}

bool World::CmdResearch(EntityId BuildingId, Research Tech)
{
	Entity* B = Find(BuildingId);
	if (B == nullptr || !B->bAlive || !B->IsBuilding())
	{
		return false;
	}
	const Availability Avail = CheckResearch(*B, Tech);
	if (Avail != Availability::Ok)
	{
		const EventType Feedback = Avail == Availability::NoSunstone ? EventType::NotEnoughSunstone
			: Avail == Availability::NoTimber ? EventType::NotEnoughTimber
			: Avail == Availability::QueueFull ? EventType::QueueFull
			: EventType::RequirementMissing;
		EmitSimple(Feedback, B->Owner, B->Pos, B->Id, Archetype::None, 0.f, static_cast<int>(Tech));
		return false;
	}
	const ResearchDef& RD = GetResearchDef(Tech);
	Spend(B->Owner, RD.CostSunstone, RD.CostTimber);
	ProductionItem Item;
	Item.bResearch = true;
	Item.Tech = Tech;
	Item.Total = RD.Time;
	Item.PaidSunstone = RD.CostSunstone;
	Item.PaidTimber = RD.CostTimber;
	B->Queue.push_back(Item);
	return true;
}

void World::CmdCancelQueueItem(EntityId BuildingId, int QueueIndex)
{
	Entity* B = Find(BuildingId);
	if (B == nullptr || !B->bAlive || QueueIndex < 0 || QueueIndex >= static_cast<int>(B->Queue.size()))
	{
		return;
	}
	const ProductionItem Item = B->Queue[static_cast<size_t>(QueueIndex)];
	Refund(B->Owner, Item.PaidSunstone, Item.PaidTimber);
	B->Queue.erase(B->Queue.begin() + QueueIndex);
	RecomputeSupply();
}

void World::CmdCancelConstruction(EntityId BuildingId)
{
	Entity* B = Find(BuildingId);
	if (B == nullptr || !B->bAlive || !B->IsBuilding() || B->bConstructed)
	{
		return;
	}
	const ArchetypeDef& D = GetDef(B->Type);
	Refund(B->Owner, (D.CostSunstone * 3) / 4, (D.CostTimber * 3) / 4);
	for (Entity& W : Entities)
	{
		if (W.bAlive && W.Order == OrderType::Build && W.BuildTarget == BuildingId)
		{
			ResumeAfterWork(W);
		}
	}
	B->bAlive = false;
	Map.SetOccupant(B->Rect, NoEntity);
	EmitSimple(EventType::ConstructionCancelled, B->Owner, B->Pos, B->Id, B->Type);
}

void World::CmdSetRally(EntityId BuildingId, const Vec2& Point, EntityId Node, const Tile* Tree)
{
	Entity* B = Find(BuildingId);
	if (B == nullptr || !B->bAlive || !B->IsBuilding())
	{
		return;
	}
	B->bHasRally = true;
	B->RallyPoint = Point;
	B->RallyNode = Node;
	B->bRallyTree = Tree != nullptr;
	if (Tree != nullptr)
	{
		B->RallyTree = *Tree;
	}
}

int World::CmdAbility(const std::vector<EntityId>& Units, Ability A)
{
	if (A == Ability::None)
	{
		return 0;
	}
	const AbilityDef& AD = GetAbilityDef(A);
	int Cast = 0;
	std::vector<EntityId> Near;
	for (EntityId Id : Units)
	{
		Entity* E = Find(Id);
		if (E == nullptr || !E->bAlive || GetDef(E->Type).AbilityId != A || E->AbilityCooldown > 0.f)
		{
			continue;
		}
		switch (A)
		{
		case Ability::Brace:
			E->Buff = BuffType::Brace;
			E->BuffTimer = AD.Duration;
			break;
		case Ability::Volley:
			E->Buff = BuffType::Volley;
			E->BuffTimer = AD.Duration;
			E->AttackCooldown = MinF(E->AttackCooldown, 0.1f);
			break;
		case Ability::Charge:
			E->Buff = BuffType::Charge;
			E->BuffTimer = AD.Duration;
			E->bChargeReady = true;
			break;
		case Ability::Sunburst:
		{
			const Vec2 Center = E->Pos;
			const Team Owner = E->Owner;
			const EntityId CasterId = E->Id;
			const float Damage = AD.Amount * GetTeam(Owner).DamageMult;
			QueryRadius(Center, AD.Radius, Near);
			for (EntityId TargetId : Near)
			{
				Entity* T = Find(TargetId);
				if (T == nullptr || !T->bAlive || !T->IsUnit())
				{
					continue;
				}
				if (AreEnemies(Owner, T->Owner))
				{
					ApplyDamage(*T, Damage, CasterId, Owner, 1.f);
				}
				else if (T->Owner == Owner)
				{
					Heal(*T, AD.HealAmount, CasterId);
				}
			}
			E = Find(Id);
			break;
		}
		case Ability::None:
		case Ability::Count:
			break;
		}
		if (E == nullptr)
		{
			continue;
		}
		E->AbilityCooldown = GetAbilityCooldownMax(*E);
		EmitSimple(EventType::AbilityCast, E->Owner, E->Pos, E->Id, E->Type, 0.f, static_cast<int>(A));
		++Cast;
	}
	return Cast;
}

void World::Kill(EntityId Id, EntityId Killer)
{
	Entity* E = Find(Id);
	if (E != nullptr && E->bAlive)
	{
		DestroyEntity(*E, Killer);
	}
}

// ---------------------------------------------------------------------------------------------
// Text helpers
// ---------------------------------------------------------------------------------------------

const char* AvailabilityText(Availability A)
{
	switch (A)
	{
	case Availability::Ok:
		return "Ready";
	case Availability::Locked:
		return "Locked";
	case Availability::NoSunstone:
		return "Not enough Sunstone";
	case Availability::NoTimber:
		return "Not enough Timber";
	case Availability::NoSupply:
		return "Build more Cottages";
	case Availability::QueueFull:
		return "Queue is full";
	case Availability::Done:
		return "Researched";
	case Availability::InProgress:
		return "In progress";
	case Availability::NotConstructed:
		return "Under construction";
	}
	return "";
}

const char* OrderText(const Entity& E)
{
	if (E.IsBuilding())
	{
		if (!E.bConstructed)
		{
			return "Under construction";
		}
		return E.Queue.empty() ? "Ready" : "Working";
	}
	if (E.IsResourceNode())
	{
		return "Sunstone deposit";
	}
	switch (E.Order)
	{
	case OrderType::Idle:
		return E.EngageTarget != NoEntity ? "Fighting" : "Idle";
	case OrderType::Move:
		return "Moving";
	case OrderType::AttackMove:
		return E.EngageTarget != NoEntity ? "Fighting" : "Advancing";
	case OrderType::Attack:
		return "Attacking";
	case OrderType::Gather:
		return E.GatherType == Resource::Timber ? "Cutting Timber" : "Mining Sunstone";
	case OrderType::Return:
		return "Carrying cargo";
	case OrderType::Build:
		return "Building";
	}
	return "";
}

} // namespace bh
