// Unit tests for the world simulation: movement, gathering, building, training, combat.
#include "TestFramework.h"

#include "BhPath.h"
#include "BhWorld.h"

using namespace bh;

namespace
{
void OpenField(World& W, int Size = 32)
{
	W.Reset(Size, Size, 1234u);
}

void RunFor(World& W, float Seconds)
{
	const int Ticks = static_cast<int>(Seconds / World::TickSeconds);
	for (int I = 0; I < Ticks; ++I)
	{
		W.Tick(World::TickSeconds);
		W.Events.clear();
	}
}

std::vector<EntityId> Ids(EntityId A) { return std::vector<EntityId>(1, A); }
} // namespace

BH_TEST(Path_StraightLineOnOpenField)
{
	GameMap Map;
	Map.Init(20, 20);
	PathFinder PF;
	std::vector<Vec2> Path;
	bool bReached = false;
	const bool bOk = PF.FindPath(Map, Vec2(1.5f, 1.5f), TileRect(15, 15, 16, 16), Vec2(15.5f, 15.5f), Path, bReached);
	BH_EXPECT(bOk);
	BH_EXPECT(bReached);
	BH_EXPECT_MSG(Path.size() == 1, "smoothed path should be a single segment, got %d", static_cast<int>(Path.size()));
}

BH_TEST(Path_AroundWall)
{
	GameMap Map;
	Map.Init(20, 20);
	for (int Y = 0; Y < 18; ++Y)
	{
		Map.At(10, Y).G = Ground::Rock;
	}
	PathFinder PF;
	std::vector<Vec2> Path;
	bool bReached = false;
	PF.FindPath(Map, Vec2(5.5f, 5.5f), TileRect(15, 5, 16, 6), Vec2(15.5f, 5.5f), Path, bReached);
	BH_EXPECT(bReached);
	BH_EXPECT(Path.size() >= 2);
	Vec2 Prev(5.5f, 5.5f);
	for (const Vec2& P : Path)
	{
		BH_EXPECT(PathFinder::LineWalkable(Map, Prev, P, 0.1f));
		Prev = P;
	}
}

BH_TEST(Path_UnreachableGivesPartial)
{
	GameMap Map;
	Map.Init(20, 20);
	for (int Y = 0; Y < 20; ++Y)
	{
		Map.At(10, Y).G = Ground::Water;
	}
	PathFinder PF;
	std::vector<Vec2> Path;
	bool bReached = true;
	PF.FindPath(Map, Vec2(5.5f, 5.5f), TileRect(15, 5, 16, 6), Vec2(15.5f, 5.5f), Path, bReached);
	BH_EXPECT(!bReached);
	BH_EXPECT(!Path.empty());
	BH_EXPECT(Path.back().X < 10.f);
}

BH_TEST(World_UnitMovesToPoint)
{
	World W;
	OpenField(W);
	const EntityId U = W.SpawnUnit(Archetype::Shieldbearer, Team::Player, Vec2(3.5f, 3.5f));
	W.CmdMove(Ids(U), Vec2(20.5f, 12.5f), false);
	RunFor(W, 12.f);
	const Entity* E = W.Find(U);
	BH_EXPECT(E != nullptr);
	BH_EXPECT_MSG(Vec2::Dist(E->Pos, Vec2(20.5f, 12.5f)) < 0.5f, "pos %.2f %.2f", E->Pos.X, E->Pos.Y);
	BH_EXPECT(E->Order == OrderType::Idle);
}

BH_TEST(World_GatherSunstoneAndTimber)
{
	World W;
	OpenField(W);
	W.SpawnBuilding(Archetype::Keep, Team::Player, Tile(4, 4), true);
	const EntityId Node = W.SpawnResourceNode(Archetype::SunstoneNode, Tile(12, 4), 500);
	for (int X = 4; X < 10; ++X)
	{
		W.GetMap().At(X, 14).Tree = TreeKind::Pine;
		W.GetMap().At(X, 14).Wood = GatherTuning::WoodPerTree;
	}
	const EntityId A = W.SpawnUnit(Archetype::Lamplighter, Team::Player, Vec2(9.5f, 6.5f));
	const EntityId B = W.SpawnUnit(Archetype::Lamplighter, Team::Player, Vec2(6.5f, 10.5f));
	W.CmdGatherNode(Ids(A), Node);
	W.CmdGatherTree(Ids(B), Tile(6, 14));
	RunFor(W, 60.f);
	const TeamState& T = W.GetTeam(Team::Player);
	BH_EXPECT_MSG(T.Res[0] >= 40, "sunstone %d", T.Res[0]);
	BH_EXPECT_MSG(T.Res[1] >= 30, "timber %d", T.Res[1]);
	const Entity* N = W.Find(Node);
	BH_EXPECT(N != nullptr && N->Amount < 500);
}

BH_TEST(World_TreeFellsAndOpensTile)
{
	World W;
	OpenField(W);
	W.SpawnBuilding(Archetype::Keep, Team::Player, Tile(2, 2), true);
	W.GetMap().At(9, 9).Tree = TreeKind::Oak;
	W.GetMap().At(9, 9).Wood = 20;
	const EntityId A = W.SpawnUnit(Archetype::Lamplighter, Team::Player, Vec2(8.5f, 8.5f));
	W.CmdGatherTree(Ids(A), Tile(9, 9));
	RunFor(W, 40.f);
	BH_EXPECT(!W.GetMap().HasTree(9, 9));
	BH_EXPECT(W.GetMap().IsWalkable(9, 9));
	BH_EXPECT(W.GetTeam(Team::Player).Res[1] >= 20);
}

BH_TEST(World_BuildCottageRaisesSupply)
{
	World W;
	OpenField(W);
	W.SpawnBuilding(Archetype::Keep, Team::Player, Tile(4, 4), true);
	TeamState& T = W.GetTeam(Team::Player);
	T.Res[0] = 500;
	T.Res[1] = 500;
	const EntityId A = W.SpawnUnit(Archetype::Lamplighter, Team::Player, Vec2(9.5f, 6.5f));
	W.RecomputeSupply();
	BH_EXPECT(T.SupplyCap == 10);
	const EntityId Cottage = W.CmdBuild(Ids(A), Archetype::Cottage, Tile(12, 12));
	BH_EXPECT(Cottage != NoEntity);
	BH_EXPECT(T.Res[1] == 500 - GetDef(Archetype::Cottage).CostTimber);
	RunFor(W, 35.f);
	const Entity* C = W.Find(Cottage);
	BH_EXPECT(C != nullptr && C->bConstructed);
	BH_EXPECT_MSG(T.SupplyCap == 18, "cap %d", T.SupplyCap);
}

BH_TEST(World_PlacementRules)
{
	World W;
	OpenField(W);
	W.SpawnBuilding(Archetype::Keep, Team::Player, Tile(4, 4), true);
	BH_EXPECT(W.CanPlace(Archetype::Cottage, Tile(5, 5)) == PlaceResult::Blocked);
	BH_EXPECT(W.CanPlace(Archetype::Cottage, Tile(31, 31)) == PlaceResult::OutOfBounds);
	BH_EXPECT(W.CanPlace(Archetype::Cottage, Tile(12, 12)) == PlaceResult::Ok);
	BH_EXPECT(W.CanPlace(Archetype::Beacon, Tile(12, 12)) == PlaceResult::NeedsBeaconSite);
	W.GetMap().BeaconSites.push_back(TileRect(20, 20, 22, 22));
	for (int Y = 20; Y < 22; ++Y)
	{
		for (int X = 20; X < 22; ++X)
		{
			W.GetMap().At(X, Y).BeaconSite = 0;
		}
	}
	BH_EXPECT(W.CanPlace(Archetype::Beacon, Tile(20, 20)) == PlaceResult::Ok);
	BH_EXPECT(W.CanPlace(Archetype::Cottage, Tile(20, 20)) == PlaceResult::Blocked);
}

BH_TEST(World_TrainUnitSpendsAndSpawns)
{
	World W;
	OpenField(W);
	const EntityId Keep = W.SpawnBuilding(Archetype::Keep, Team::Player, Tile(4, 4), true);
	TeamState& T = W.GetTeam(Team::Player);
	T.Res[0] = 120;
	W.RecomputeSupply();
	BH_EXPECT(W.CmdTrain(Keep, Archetype::Lamplighter));
	BH_EXPECT(W.CmdTrain(Keep, Archetype::Lamplighter));
	BH_EXPECT(!W.CmdTrain(Keep, Archetype::Lamplighter)); // not enough sunstone
	BH_EXPECT(T.Res[0] == 20);
	RunFor(W, 26.f);
	BH_EXPECT_MSG(W.CountOwned(Team::Player, Archetype::Lamplighter, true) == 2, "count %d", W.CountOwned(Team::Player, Archetype::Lamplighter, true));
	BH_EXPECT(T.Stats.UnitsTrained == 2);
}

BH_TEST(World_SupplyBlocksTraining)
{
	World W;
	OpenField(W);
	const EntityId Keep = W.SpawnBuilding(Archetype::Keep, Team::Player, Tile(4, 4), true);
	TeamState& T = W.GetTeam(Team::Player);
	T.Res[0] = 5000;
	for (int I = 0; I < 10; ++I)
	{
		W.SpawnUnit(Archetype::Lamplighter, Team::Player, Vec2(12.5f + static_cast<float>(I % 5), 12.5f + static_cast<float>(I / 5)));
	}
	W.RecomputeSupply();
	BH_EXPECT(T.SupplyUsed == 10);
	BH_EXPECT(W.CheckTrain(*W.Find(Keep), Archetype::Lamplighter) == Availability::NoSupply);
}

BH_TEST(World_MeleeCombatKills)
{
	World W;
	OpenField(W);
	const EntityId A = W.SpawnUnit(Archetype::Shieldbearer, Team::Player, Vec2(5.5f, 5.5f));
	const EntityId B = W.SpawnUnit(Archetype::Gloomling, Team::Enemy, Vec2(9.5f, 5.5f));
	W.CmdAttack(Ids(A), B);
	RunFor(W, 20.f);
	BH_EXPECT(W.Find(B) == nullptr);
	const Entity* E = W.Find(A);
	BH_EXPECT(E != nullptr && E->Hp < E->MaxHp);
	BH_EXPECT(W.GetTeam(Team::Player).Stats.Kills == 1);
}

BH_TEST(World_IdleUnitsAutoAcquire)
{
	World W;
	OpenField(W);
	const EntityId A = W.SpawnUnit(Archetype::Ranger, Team::Player, Vec2(5.5f, 5.5f));
	const EntityId B = W.SpawnUnit(Archetype::Gloomling, Team::Enemy, Vec2(10.5f, 5.5f));
	RunFor(W, 25.f);
	BH_EXPECT(W.Find(B) == nullptr || W.Find(A) == nullptr);
	BH_EXPECT(W.GetTeam(Team::Player).Stats.Kills + W.GetTeam(Team::Enemy).Stats.Kills >= 1);
}

BH_TEST(World_RangedUsesProjectiles)
{
	World W;
	OpenField(W);
	const EntityId A = W.SpawnUnit(Archetype::Ranger, Team::Player, Vec2(5.5f, 5.5f));
	const EntityId B = W.SpawnUnit(Archetype::Thornback, Team::Enemy, Vec2(9.5f, 5.5f));
	W.CmdAttack(Ids(A), B);
	bool bSawProjectile = false;
	for (int I = 0; I < 60; ++I)
	{
		W.Tick(World::TickSeconds);
		if (!W.GetProjectiles().empty())
		{
			bSawProjectile = true;
		}
		W.Events.clear();
	}
	BH_EXPECT(bSawProjectile);
	const Entity* T = W.Find(B);
	BH_EXPECT(T != nullptr && T->Hp < T->MaxHp);
}

BH_TEST(World_TowerShootsEnemies)
{
	World W;
	OpenField(W);
	W.SpawnBuilding(Archetype::Watchtower, Team::Player, Tile(10, 10), true);
	const EntityId G = W.SpawnUnit(Archetype::Gloomling, Team::Enemy, Vec2(14.5f, 11.5f));
	W.CmdStop(Ids(G));
	RunFor(W, 20.f);
	BH_EXPECT(W.Find(G) == nullptr);
}

BH_TEST(World_AbilitiesWork)
{
	World W;
	OpenField(W);
	const EntityId S = W.SpawnUnit(Archetype::Shieldbearer, Team::Player, Vec2(5.5f, 5.5f));
	const EntityId Sage = W.SpawnUnit(Archetype::Sage, Team::Player, Vec2(6.5f, 5.5f));
	const EntityId G1 = W.SpawnUnit(Archetype::Gloomling, Team::Enemy, Vec2(7.5f, 6.5f));
	BH_EXPECT(W.CmdAbility(Ids(S), Ability::Brace) == 1);
	BH_EXPECT(W.Find(S)->Buff == BuffType::Brace);
	BH_EXPECT(W.CmdAbility(Ids(S), Ability::Brace) == 0); // on cooldown
	W.Find(S)->Hp = 50.f;
	W.RecomputeSupply();
	W.Tick(World::TickSeconds); // builds spatial hash
	const float GloomHpBefore = W.Find(G1)->Hp;
	BH_EXPECT(W.CmdAbility(Ids(Sage), Ability::Sunburst) == 1);
	const Entity* G = W.Find(G1);
	BH_EXPECT(G == nullptr || !G->bAlive || G->Hp < GloomHpBefore);
	BH_EXPECT(W.Find(S)->Hp > 50.f);
}

BH_TEST(World_BuildingDestroyedFreesTiles)
{
	World W;
	OpenField(W);
	const EntityId C = W.SpawnBuilding(Archetype::Cottage, Team::Player, Tile(10, 10), true);
	BH_EXPECT(!W.GetMap().IsWalkable(10, 10));
	W.Kill(C, NoEntity);
	W.Tick(World::TickSeconds);
	BH_EXPECT(W.GetMap().IsWalkable(10, 10));
	BH_EXPECT(W.Find(C) == nullptr);
}

BH_TEST(World_ResearchAppliesBonus)
{
	World W;
	OpenField(W);
	W.SpawnBuilding(Archetype::Barracks, Team::Player, Tile(2, 2), true);
	const EntityId Forge = W.SpawnBuilding(Archetype::Forge, Team::Player, Tile(8, 2), true);
	TeamState& T = W.GetTeam(Team::Player);
	T.Res[0] = 1000;
	T.Res[1] = 1000;
	const EntityId S = W.SpawnUnit(Archetype::Shieldbearer, Team::Player, Vec2(15.5f, 15.5f));
	const float Before = W.GetDamage(*W.Find(S));
	BH_EXPECT(W.CmdResearch(Forge, Research::Blades1));
	BH_EXPECT(!W.CmdResearch(Forge, Research::Blades1)); // in progress
	RunFor(W, 32.f);
	BH_EXPECT(T.Researched[static_cast<int>(Research::Blades1)]);
	BH_EXPECT(W.GetDamage(*W.Find(S)) > Before);
	BH_EXPECT(W.NextResearchLevel(Team::Player, Research::Blades1) == Research::Blades2);
}
