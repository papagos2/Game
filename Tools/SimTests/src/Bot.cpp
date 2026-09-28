// Scripted player for mission playthrough tests.
#include "Bot.h"

#include "Playtest.h"

#include "BhHud.h"

#include <chrono>
#include <cstdio>
#include <map>

using namespace bh;

namespace bht
{
namespace
{
struct BotState
{
	float NextThink = 0.f;
	float NextRebalance = 0.f;
	float LastAttackOrder = -100.f;
	bool bAttacking = false;
	int TrainToggle = 0;
	std::map<EntityId, std::pair<Vec2, float>> LastSeen; // worker -> (pos, time moved)
};

std::vector<const Entity*> Owned(const World& W, Archetype A)
{
	std::vector<const Entity*> Out;
	for (const Entity& E : W.GetEntities())
	{
		if (E.bAlive && E.Owner == Team::Player && E.Type == A)
		{
			Out.push_back(&E);
		}
	}
	return Out;
}

bool UnderConstruction(const World& W, Archetype A)
{
	for (const Entity& E : W.GetEntities())
	{
		if (E.bAlive && E.Owner == Team::Player && E.Type == A && !E.bConstructed)
		{
			return true;
		}
	}
	return false;
}

int ArmySupply(const World& W)
{
	int Supply = 0;
	for (const Entity& E : W.GetEntities())
	{
		if (E.bAlive && E.Owner == Team::Player && W.IsCombatUnit(E))
		{
			Supply += GetDef(E.Type).SupplyCost;
		}
	}
	return Supply;
}

std::vector<EntityId> ArmyIds(const World& W)
{
	std::vector<EntityId> Out;
	for (const Entity& E : W.GetEntities())
	{
		if (E.bAlive && E.Owner == Team::Player && W.IsCombatUnit(E))
		{
			Out.push_back(E.Id);
		}
	}
	return Out;
}

Vec2 Centroid(const World& W, const std::vector<EntityId>& Ids)
{
	Vec2 Sum;
	int N = 0;
	for (EntityId Id : Ids)
	{
		if (const Entity* E = W.Find(Id))
		{
			Sum += E->Pos;
			++N;
		}
	}
	return N > 0 ? Sum / static_cast<float>(N) : Vec2();
}

// Picks a builder: a Lamplighter that is not carrying cargo if possible.
EntityId PickBuilder(const World& W, const Vec2& Near)
{
	EntityId Best = NoEntity;
	float BestScore = 1e9f;
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive || E.Owner != Team::Player || E.Type != Archetype::Lamplighter || E.Order == OrderType::Build)
		{
			continue;
		}
		const float Score = Vec2::Dist(E.Pos, Near) + (E.CarryAmount > 0 ? 6.f : 0.f);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = E.Id;
		}
	}
	return Best;
}

bool TryBuild(Session& S, Archetype Type, const Vec2& Near)
{
	World& W = S.GetWorld();
	PlayerControl& C = S.GetControl();
	if (W.CheckBuild(Team::Player, Type) != Availability::Ok)
	{
		return false;
	}
	const EntityId Builder = PickBuilder(W, Near);
	if (Builder == NoEntity)
	{
		return false;
	}
	C.CancelPlacement();
	C.SelectOne(W, Builder);
	C.BeginPlacement(W, Type, Near);
	if (!C.bPlacing)
	{
		return false;
	}
	const bool bOk = C.ConfirmPlacement(W);
	C.CancelPlacement();
	return bOk;
}

bool TryTrain(Session& S, const Entity& Building, Archetype Unit, int MaxQueue)
{
	World& W = S.GetWorld();
	if (static_cast<int>(Building.Queue.size()) >= MaxQueue || W.CheckTrain(Building, Unit) != Availability::Ok)
	{
		return false;
	}
	S.GetControl().SelectOne(W, Building.Id);
	S.ExecuteAction(ActionId(ActionKind::Train, static_cast<int>(Unit)));
	return true;
}

// Closest enemy unit near any of our buildings (base defence).
bool FindThreat(const World& W, Vec2& Out)
{
	float Best = 1e9f;
	bool bFound = false;
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive || E.Owner != Team::Enemy || !E.IsUnit())
		{
			continue;
		}
		for (const Entity& B : W.GetEntities())
		{
			if (!B.bAlive || B.Owner != Team::Player || !B.IsBuilding())
			{
				continue;
			}
			const float D = B.Rect.DistanceTo(E.Pos);
			if (D < 11.f && D < Best)
			{
				Best = D;
				Out = E.Pos;
				bFound = true;
			}
		}
	}
	return bFound;
}

void ManageWorkers(Session& S, const BotConfig& Cfg, BotState& St)
{
	World& W = S.GetWorld();
	PlayerControl& C = S.GetControl();
	const Vec2 Keep = S.GetKeepPos();
	int OnSun = 0;
	int OnWood = 0;
	std::vector<EntityId> Idle;
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive || E.Owner != Team::Player || E.Type != Archetype::Lamplighter)
		{
			continue;
		}
		if (E.Order == OrderType::Idle)
		{
			Idle.push_back(E.Id);
		}
		else if (E.GatherType == Resource::Sunstone)
		{
			++OnSun;
		}
		else if (E.GatherType == Resource::Timber)
		{
			++OnWood;
		}
		auto It = St.LastSeen.find(E.Id);
		if (It == St.LastSeen.end() || Vec2::Dist(It->second.first, E.Pos) > 0.3f || E.Act == Activity::Gathering || E.Act == Activity::Building)
		{
			St.LastSeen[E.Id] = std::make_pair(E.Pos, W.GetTime());
		}
	}
	for (EntityId Id : Idle)
	{
		const Entity* E = W.Find(Id);
		if (E == nullptr)
		{
			continue;
		}
		C.SelectOne(W, Id);
		bool bWantSun = OnSun <= OnWood + 1;
		if (Cfg.bSmartEconomy)
		{
			const TeamState& T = W.GetTeam(Team::Player);
			bWantSun = T.Res[0] < T.Res[1] + 150 || OnWood > OnSun * 2 + 2;
		}
		const EntityId Node = W.FindNearestNode(Keep, 30.f, NoEntity);
		const EntityId FarNode = W.FindNearestNode(E->Pos, 60.f, NoEntity);
		Tile Tree;
		const bool bTree = W.GetMap().FindNearestTree(Keep, 14.f, Tree) || W.GetMap().FindNearestTree(E->Pos, 40.f, Tree);
		if (bWantSun && (Node != NoEntity || FarNode != NoEntity))
		{
			const Entity* N = W.Find(Node != NoEntity ? Node : FarNode);
			C.TapWorld(W, N->Pos, 0.2f, false, nullptr);
			++OnSun;
		}
		else if (bTree)
		{
			C.TapWorld(W, Tree.Center(), 0.05f, false, nullptr);
			++OnWood;
		}
		else if (FarNode != NoEntity)
		{
			C.TapWorld(W, W.Find(FarNode)->Pos, 0.2f, false, nullptr);
		}
	}
	// An attentive player moves workers when one resource piles up and the other runs dry.
	if (Cfg.bSmartEconomy && W.GetTime() >= St.NextRebalance)
	{
		St.NextRebalance = W.GetTime() + 10.f;
		const TeamState& T = W.GetTeam(Team::Player);
		Resource From = Resource::None;
		if (T.Res[0] > 400 && T.Res[1] < 120)
		{
			From = Resource::Sunstone;
		}
		else if (T.Res[1] > 400 && T.Res[0] < 120)
		{
			From = Resource::Timber;
		}
		if (From != Resource::None)
		{
			int Moved = 0;
			for (const Entity& E : W.GetEntities())
			{
				if (Moved >= 2)
				{
					break;
				}
				if (!E.bAlive || E.Owner != Team::Player || E.Type != Archetype::Lamplighter || E.Order != OrderType::Gather || E.GatherType != From ||
					E.CarryAmount > 0)
				{
					continue;
				}
				C.SelectOne(W, E.Id);
				if (From == Resource::Sunstone)
				{
					Tile Tree;
					if (W.GetMap().FindNearestTree(E.Pos, 16.f, Tree))
					{
						C.TapWorld(W, Tree.Center(), 0.05f, false, nullptr);
						++Moved;
					}
				}
				else
				{
					const EntityId Node = W.FindNearestNode(E.Pos, 40.f, NoEntity);
					if (const Entity* N = W.Find(Node))
					{
						C.TapWorld(W, N->Pos, 0.2f, false, nullptr);
						++Moved;
					}
				}
			}
			C.ClearSelection();
		}
	}
}

void ManageBase(Session& S, const BotConfig& Cfg, BotState& St)
{
	World& W = S.GetWorld();
	const TeamState& T = W.GetTeam(Team::Player);
	const Vec2 Keep = S.GetKeepPos();
	const int Workers = W.CountOwned(Team::Player, Archetype::Lamplighter, true);
	const float Time = W.GetTime();
	const int Mission = S.GetConfig().MissionIndex;

	// Supply
	if (T.SupplyCap < SupplyHardCap && T.SupplyCap - T.SupplyUsed < 5 && !UnderConstruction(W, Archetype::Cottage))
	{
		TryBuild(S, Archetype::Cottage, Keep + Vec2(-5.f, 5.f));
	}
	// Workers
	for (const Entity* K : Owned(W, Archetype::Keep))
	{
		if (Workers < Cfg.TargetWorkers)
		{
			TryTrain(S, *K, Archetype::Lamplighter, 2);
		}
	}
	// Buildings
	const int Barracks = W.CountOwned(Team::Player, Archetype::Barracks, true);
	if (Barracks == 0 && Workers >= 5)
	{
		TryBuild(S, Archetype::Barracks, Keep + Vec2(5.f, 4.f));
	}
	if (Barracks == 1 && Workers >= 11 && Time > 280.f && Mission != 0)
	{
		TryBuild(S, Archetype::Barracks, Keep + Vec2(-4.f, -5.f));
	}
	const bool bHaveBarracks = W.HasCompleted(Team::Player, Archetype::Barracks);
	const bool bRiders = Cfg.Army == BotArmy::Riders;
	const bool bSages = Cfg.Army == BotArmy::Sages;
	if (bHaveBarracks && Mission != 0 && W.CountOwned(Team::Player, Archetype::Forge, true) == 0 && (Time > 220.f || bRiders))
	{
		TryBuild(S, Archetype::Forge, Keep + Vec2(6.f, -3.f));
	}
	if (bRiders && Mission != 0 && W.HasCompleted(Team::Player, Archetype::Forge) && W.CountOwned(Team::Player, Archetype::StagLodge, true) < 2)
	{
		TryBuild(S, Archetype::StagLodge, Keep + Vec2(-7.f, 1.f + 4.f * static_cast<float>(W.CountOwned(Team::Player, Archetype::StagLodge, true))));
	}
	if (bSages && Mission != 0 && bHaveBarracks && W.CountOwned(Team::Player, Archetype::Sanctum, true) == 0)
	{
		TryBuild(S, Archetype::Sanctum, Keep + Vec2(7.f, 6.f));
	}
	if (Cfg.bBuildTowers && bHaveBarracks && W.CountOwned(Team::Player, Archetype::Watchtower, true) < Cfg.TowerCount)
	{
		TryBuild(S, Archetype::Watchtower, Keep + Vec2(-4.f + 4.f * static_cast<float>(W.CountOwned(Team::Player, Archetype::Watchtower, true)), -7.f));
	}
	if (Cfg.Army == BotArmy::Mixed && Mission == 2 && W.HasCompleted(Team::Player, Archetype::Forge) &&
		W.CountOwned(Team::Player, Archetype::StagLodge, true) == 0 && Time > 420.f)
	{
		TryBuild(S, Archetype::StagLodge, Keep + Vec2(-7.f, 1.f));
	}
	if (Cfg.Army == BotArmy::Mixed && Mission == 2 && bHaveBarracks && W.CountOwned(Team::Player, Archetype::Sanctum, true) == 0 && Time > 480.f)
	{
		TryBuild(S, Archetype::Sanctum, Keep + Vec2(7.f, 6.f));
	}
	// Expansion storehouse once home sunstone runs low.
	if (Mission == 2 && W.CountOwned(Team::Player, Archetype::Storehouse, true) == 0 && Time > 540.f)
	{
		const EntityId Near = W.FindNearestNode(Keep + Vec2(20.f, -12.f), 40.f, NoEntity);
		if (const Entity* N = W.Find(Near))
		{
			TryBuild(S, Archetype::Storehouse, N->Pos + Vec2(0.f, 3.f));
		}
	}
	// Beacons on safe sites.
	for (const TileRect& Site : W.GetMap().BeaconSites)
	{
		if (W.GetMap().At(Site.X0, Site.Y0).Occupant != NoEntity)
		{
			continue;
		}
		bool bSafe = true;
		for (const Entity& E : W.GetEntities())
		{
			if (E.bAlive && E.Owner == Team::Enemy && Site.DistanceTo(E.Pos) < 9.f)
			{
				bSafe = false;
				break;
			}
		}
		if (bSafe && Time > 60.f)
		{
			TryBuild(S, Archetype::Beacon, Site.Center());
			break;
		}
	}

	// Army production (keep some reserve for buildings early on).
	const int Reserve = Time < 200.f ? 60 : 0;
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive || E.Owner != Team::Player || !E.IsBuilding() || !E.bConstructed || T.Res[0] < Reserve)
		{
			continue;
		}
		if (E.Type == Archetype::Barracks)
		{
			Archetype Pick = (St.TrainToggle++ % 2 == 0) ? Archetype::Shieldbearer : Archetype::Ranger;
			switch (Cfg.Army)
			{
			case BotArmy::Shields:
			case BotArmy::Sages:
				Pick = Archetype::Shieldbearer;
				break;
			case BotArmy::Rangers:
				Pick = Archetype::Ranger;
				break;
			case BotArmy::Riders:
				// A small guard until the Lodge is up.
				if (W.HasCompleted(Team::Player, Archetype::StagLodge) || W.CountOwned(Team::Player, Archetype::Shieldbearer, true) >= 4)
				{
					continue;
				}
				Pick = Archetype::Shieldbearer;
				break;
			case BotArmy::Mixed:
				break;
			}
			if (bSages && W.CountOwned(Team::Player, Archetype::Shieldbearer, true) > W.CountOwned(Team::Player, Archetype::Sage, true) + 3 &&
				W.HasCompleted(Team::Player, Archetype::Sanctum))
			{
				continue; // let the Sanctum catch up
			}
			TryTrain(S, E, Pick, 2);
		}
		else if (E.Type == Archetype::StagLodge)
		{
			TryTrain(S, E, Archetype::StagRider, bRiders ? 2 : 1);
		}
		else if (E.Type == Archetype::Sanctum && (bSages || W.CountOwned(Team::Player, Archetype::Sage, true) < 3))
		{
			TryTrain(S, E, Archetype::Sage, bSages ? 2 : 1);
		}
		else if (E.Type == Archetype::Forge && E.Queue.empty())
		{
			const Research Lines[3] = {Research::Blades1, Research::Plate1, Research::Fletching};
			for (Research Line : Lines)
			{
				const Research R = W.NextResearchLevel(Team::Player, Line);
				if (W.CheckResearch(E, R) == Availability::Ok)
				{
					S.GetControl().SelectOne(W, E.Id);
					S.ExecuteAction(ActionId(ActionKind::Research, static_cast<int>(R)));
					break;
				}
			}
		}
	}
	S.GetControl().ClearSelection();
}

void ManageArmy(Session& S, const BotConfig& Cfg, BotState& St)
{
	World& W = S.GetWorld();
	PlayerControl& C = S.GetControl();
	const std::vector<EntityId> Army = ArmyIds(W);
	if (Army.empty())
	{
		St.bAttacking = false;
		return;
	}
	const Vec2 Center = Centroid(W, Army);

	// Abilities when enemies are close.
	if (Cfg.bUseAbilities && W.FindNearestEnemy(Team::Player, Center, 6.f, true) != NoEntity)
	{
		C.SelectMany(W, Army);
		for (int A = 1; A < NumAbilities; ++A)
		{
			S.ExecuteAction(ActionId(ActionKind::Ability, A));
		}
	}

	Vec2 Threat;
	if (FindThreat(W, Threat))
	{
		C.SelectMany(W, Army);
		C.TapWorld(W, Threat, 0.05f, false, nullptr);
		St.LastAttackOrder = W.GetTime();
		C.ClearSelection();
		return;
	}
	if (Cfg.bPassive)
	{
		return;
	}
	const int Supply = ArmySupply(W);
	const int Mission = S.GetConfig().MissionIndex;
	float Threshold = Cfg.AttackSupply;
	if (Mission == 0)
	{
		Threshold = 8.f;
	}
	else if (Mission == 1)
	{
		Threshold = 12.f; // clear the spire guarding the east Beacon
	}
	if (!St.bAttacking && Supply >= Threshold)
	{
		St.bAttacking = true;
	}
	if (St.bAttacking && Supply < Threshold * 0.4f)
	{
		St.bAttacking = false; // fall back and rebuild
		C.SelectMany(W, Army);
		C.TapWorld(W, S.GetKeepPos() + Vec2(0.f, -4.f), 0.05f, false, nullptr);
		C.ClearSelection();
		return;
	}
	if (!St.bAttacking || W.GetTime() - St.LastAttackOrder < 8.f)
	{
		return;
	}
	Vec2 Target;
	bool bHave = false;
	float Best = 1e9f;
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive || E.Owner != Team::Enemy || !E.IsBuilding())
		{
			continue;
		}
		const float D = Vec2::Dist(E.Pos, Center);
		if (D < Best)
		{
			Best = D;
			Target = E.Rect.ClosestPoint(Center) + (Center - E.Pos).Normalized() * 1.5f;
			bHave = true;
		}
	}
	if (!bHave)
	{
		// No buildings left: hunt units.
		const EntityId Enemy = W.FindNearestEnemy(Team::Player, Center, 200.f, true);
		const Entity* E = W.Find(Enemy);
		if (E == nullptr)
		{
			return;
		}
		Target = E->Pos;
	}
	C.SelectMany(W, Army);
	C.TapWorld(W, Target, 0.05f, false, nullptr);
	C.ClearSelection();
	St.LastAttackOrder = W.GetTime();
}
} // namespace

const char* BotArmyName(BotArmy A)
{
	switch (A)
	{
	case BotArmy::Mixed:
		return "mixed";
	case BotArmy::Shields:
		return "shields";
	case BotArmy::Rangers:
		return "rangers";
	case BotArmy::Riders:
		return "riders";
	case BotArmy::Sages:
		return "sages";
	}
	return "?";
}

BotReport PlayMission(Session& S, const BotConfig& Config, float MaxSeconds, Telemetry* Observer)
{
	BotReport R;
	BotState St;
	World& W = S.GetWorld();
	std::vector<GameEvent> Events;
	char Line[256];
	float NextLog = 0.f;
	if (Observer != nullptr)
	{
		Observer->Begin(S);
	}
	while (W.GetTime() < MaxSeconds && S.GetMission().Outcome == MissionOutcome::InProgress)
	{
		const auto Start = std::chrono::steady_clock::now();
		S.Update(World::TickSeconds, nullptr);
		const float Ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - Start).count();
		S.TakeEvents(Events);
		if (Observer != nullptr)
		{
			Observer->OnTickTime(S, Ms);
			Observer->OnTick(S, Events);
		}
		if (S.GetMission().IsTutorialActive())
		{
			S.ContinueTutorial();
		}
		if (W.GetTime() >= St.NextThink)
		{
			St.NextThink = W.GetTime() + 0.5f;
			ManageWorkers(S, Config, St);
			ManageBase(S, Config, St);
			ManageArmy(S, Config, St);
		}
		R.MaxArmySupply = MaxI(R.MaxArmySupply, ArmySupply(W));
		R.MaxEnemyUnits = MaxI(R.MaxEnemyUnits, W.CountUnits(Team::Enemy, false));
		if (W.GetTime() >= NextLog)
		{
			NextLog += 60.f;
			const TeamState& T = W.GetTeam(Team::Player);
			std::snprintf(Line, sizeof(Line), "  t=%4.0fs sun=%4d wood=%4d supply=%3d/%3d workers=%2d army=%3d enemyUnits=%3d gloom=%4d\n",
				W.GetTime(), T.Res[0], T.Res[1], T.SupplyUsed, T.SupplyCap, W.CountOwned(Team::Player, Archetype::Lamplighter, true),
				ArmySupply(W), W.CountUnits(Team::Enemy, false), W.GetTeam(Team::Enemy).Res[0]);
			R.Log += Line;
		}
	}
	if (Observer != nullptr)
	{
		Observer->End(S);
	}
	const TeamState& T = W.GetTeam(Team::Player);
	R.Outcome = S.GetMission().Outcome;
	R.Time = W.GetTime();
	R.Stars = S.GetMission().GetStarCount();
	R.UnitsTrained = T.Stats.UnitsTrained;
	R.UnitsLost = T.Stats.UnitsLost;
	R.Kills = T.Stats.Kills;
	R.Gathered[0] = T.Stats.Gathered[0];
	R.Gathered[1] = T.Stats.Gathered[1];
	R.BuildingsBuilt = T.Stats.BuildingsBuilt;
	for (const auto& Kv : St.LastSeen)
	{
		const Entity* E = W.Find(Kv.first);
		if (E != nullptr && E->Order != OrderType::Idle && W.GetTime() - Kv.second.second > 20.f)
		{
			++R.StuckWorkers;
		}
	}
	return R;
}

} // namespace bht
