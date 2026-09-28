// Unit tests for the world simulation: movement, gathering, building, training, combat.
#include "TestFramework.h"

#include "BhPath.h"
#include "BhWorld.h"

#include <algorithm>

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

BH_TEST(World_SpatialQueryReachesEveryUnit)
{
	// Spatial queries rely on no unit being wider than MaxUnitRadius; a unit whose edge is in
	// range must be found wherever its centre sits in the grid.
	for (int A = 0; A < NumArchetypes; ++A)
	{
		const ArchetypeDef& D = GetDef(static_cast<Archetype>(A));
		BH_EXPECT_MSG(D.Kind != EntityKind::Unit || D.Radius <= World::MaxUnitRadius, "%s is wider than MaxUnitRadius", D.Name);
	}
	World W;
	W.Reset(32, 32, 1u);
	const EntityId Titan = W.SpawnUnit(Archetype::BogTitan, Team::Enemy, Vec2(10.95f, 10.95f));
	W.Tick(World::TickSeconds);
	const Entity* T = W.Find(Titan);
	BH_EXPECT(T != nullptr);
	if (T == nullptr)
	{
		return;
	}
	std::vector<EntityId> Near;
	for (int I = 0; I < 16; ++I)
	{
		const float Angle = static_cast<float>(I) * 0.3927f;
		const Vec2 From = T->Pos + Vec2::FromAngle(Angle) * (T->Radius + 2.95f);
		W.QueryRadius(From, 3.f, Near);
		BH_EXPECT_MSG(std::find(Near.begin(), Near.end(), Titan) != Near.end(), "the Titan's edge is in range from angle %d but was not found", I);
	}
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

BH_TEST(World_RangedUnitsCloseInFromTheDiagonal)
{
	// A target 6 tiles away on both axes is out of range (8.5) but inside the square around
	// it that ranged units once treated as "arrived": the archer must still walk in and shoot.
	const Archetype Shooters[] = {Archetype::Ranger, Archetype::Sage};
	for (Archetype Shooter : Shooters)
	{
		for (int Dir = 0; Dir < 4; ++Dir)
		{
			World W;
			OpenField(W);
			const float Sx = (Dir & 1) != 0 ? 1.f : -1.f;
			const float Sy = (Dir & 2) != 0 ? 1.f : -1.f;
			const Vec2 Target(16.5f, 16.5f);
			const EntityId A = W.SpawnUnit(Shooter, Team::Player, Target + Vec2(6.f * Sx, 6.f * Sy));
			const EntityId B = W.SpawnUnit(Archetype::Thornback, Team::Enemy, Target);
			W.CmdStop(Ids(B));
			W.CmdAttack(Ids(A), B);
			RunFor(W, 5.f);
			const Entity* T = W.Find(B);
			BH_EXPECT_MSG(T != nullptr && T->Hp < T->MaxHp, "%s from direction %d never hit its target", GetDef(Shooter).Name, Dir);
		}
	}
}

BH_TEST(World_WalkersPassHeadOn)
{
	// Two units walking straight at each other on the same line must not shove each other to a
	// standstill (they used to, on tile-centre waypoints next to buildings).
	World W;
	OpenField(W);
	const EntityId A = W.SpawnUnit(Archetype::Lamplighter, Team::Player, Vec2(16.5f, 10.5f));
	const EntityId B = W.SpawnUnit(Archetype::Shieldbearer, Team::Player, Vec2(16.5f, 20.5f));
	W.CmdMove(Ids(A), Vec2(16.5f, 22.5f), false);
	W.CmdMove(Ids(B), Vec2(16.5f, 8.5f), false);
	RunFor(W, 8.f);
	const Entity* EA = W.Find(A);
	const Entity* EB = W.Find(B);
	BH_EXPECT(EA != nullptr && EB != nullptr);
	if (EA != nullptr && EB != nullptr)
	{
		BH_EXPECT_MSG(Vec2::Dist(EA->Pos, Vec2(16.5f, 22.5f)) < 1.f, "first walker stopped at %.1f,%.1f", EA->Pos.X, EA->Pos.Y);
		BH_EXPECT_MSG(Vec2::Dist(EB->Pos, Vec2(16.5f, 8.5f)) < 1.f, "second walker stopped at %.1f,%.1f", EB->Pos.X, EB->Pos.Y);
	}
}

BH_TEST(World_RepeatedAttackTapsKeepSwinging)
{
	// Players tap the same enemy again and again; each tap used to cancel the swing in progress.
	World W;
	OpenField(W);
	const EntityId S = W.SpawnUnit(Archetype::Shieldbearer, Team::Player, Vec2(10.5f, 10.5f));
	const EntityId G = W.SpawnUnit(Archetype::Thornback, Team::Enemy, Vec2(11.4f, 10.5f));
	W.CmdStop(Ids(G));
	for (int I = 0; I < 30; ++I) // a tap every 0.2 s for 6 s
	{
		W.CmdAttack(Ids(S), G);
		RunFor(W, 0.2f);
	}
	const Entity* T = W.Find(G);
	BH_EXPECT_MSG(T == nullptr || T->Hp < T->MaxHp - 30.f, "only %.0f damage in 6 s of tapping", T != nullptr ? T->MaxHp - T->Hp : 0.f);
}

BH_TEST(World_BlockedAttackerStrikesWhatIsInReach)
{
	// Ordered onto a target it cannot reach (across water), a soldier with an enemy at its side
	// fights that enemy instead of standing in the fight.
	World W;
	OpenField(W);
	for (int Y = 0; Y < 32; ++Y)
	{
		W.GetMap().At(10, Y).G = Ground::Water;
		W.GetMap().At(11, Y).G = Ground::Water;
	}
	const EntityId S = W.SpawnUnit(Archetype::Shieldbearer, Team::Player, Vec2(8.5f, 16.5f));
	const EntityId Far = W.SpawnUnit(Archetype::Thornback, Team::Enemy, Vec2(13.5f, 16.5f));
	const EntityId Near = W.SpawnUnit(Archetype::Thornback, Team::Enemy, Vec2(8.5f, 17.4f));
	W.CmdStop(Ids(Far));
	W.CmdStop(Ids(Near));
	W.CmdAttack(Ids(S), Far);
	RunFor(W, 4.f);
	const Entity* N = W.Find(Near);
	BH_EXPECT_MSG(N == nullptr || N->Hp < N->MaxHp, "the soldier never struck the enemy beside it");
	const Entity* Me = W.Find(S);
	BH_EXPECT(Me != nullptr && Me->Order == OrderType::Attack && Me->OrderTarget == Far);
}

BH_TEST(World_FullOutcropSendsWorkersOn)
{
	// Ten workers sent to one outcrop: six mine it, the rest move on to a second outcrop nearby
	// instead of queueing (they used to wait, idle, for minutes).
	World W;
	OpenField(W);
	W.SpawnBuilding(Archetype::Keep, Team::Player, Tile(4, 4), true);
	const EntityId First = W.SpawnResourceNode(Archetype::SunstoneNode, Tile(12, 5), 1500);
	const EntityId Second = W.SpawnResourceNode(Archetype::SunstoneNode, Tile(12, 13), 1500);
	std::vector<EntityId> Workers;
	for (int I = 0; I < 10; ++I)
	{
		Workers.push_back(W.SpawnUnit(Archetype::Lamplighter, Team::Player, Vec2(9.5f + 0.4f * static_cast<float>(I % 3), 9.5f + 0.4f * static_cast<float>(I / 3))));
	}
	W.CmdGatherNode(Workers, First);
	RunFor(W, 20.f);
	int OnFirst = 0;
	int OnSecond = 0;
	for (EntityId Id : Workers)
	{
		const Entity* E = W.Find(Id);
		if (E != nullptr && E->GatherType == Resource::Sunstone)
		{
			OnFirst += E->GatherNode == First ? 1 : 0;
			OnSecond += E->GatherNode == Second ? 1 : 0;
		}
	}
	BH_EXPECT_MSG(OnFirst <= GatherTuning::MaxMinersPerNode && OnSecond >= 10 - GatherTuning::MaxMinersPerNode, "first %d, second %d", OnFirst, OnSecond);
	BH_EXPECT(W.GetTeam(Team::Player).Stats.Gathered[0] > 0);
}

BH_TEST(World_SunburstIsCastOnePerTap)
{
	// Four Sages selected: one tap casts one Sunburst, from the Sage with the most Gloam around
	// it; the others stay ready for the next taps.
	World W;
	OpenField(W);
	std::vector<EntityId> Sages;
	Sages.push_back(W.SpawnUnit(Archetype::Sage, Team::Player, Vec2(4.5f, 4.5f)));
	Sages.push_back(W.SpawnUnit(Archetype::Sage, Team::Player, Vec2(5.5f, 4.5f)));
	Sages.push_back(W.SpawnUnit(Archetype::Sage, Team::Player, Vec2(20.5f, 20.5f)));
	Sages.push_back(W.SpawnUnit(Archetype::Sage, Team::Player, Vec2(4.5f, 5.5f)));
	const EntityId G1 = W.SpawnUnit(Archetype::Gloomling, Team::Enemy, Vec2(21.5f, 20.5f));
	const EntityId G2 = W.SpawnUnit(Archetype::Gloomling, Team::Enemy, Vec2(20.5f, 21.5f));
	RunFor(W, World::TickSeconds); // area queries see units from the next tick on
	BH_EXPECT(W.CmdAbility(Sages, Ability::Sunburst) == 1);
	const Entity* Caster = W.Find(Sages[2]);
	BH_EXPECT_MSG(Caster != nullptr && Caster->AbilityCooldown > 0.f, "the Sage among the Gloam should have cast");
	int Ready = 0;
	for (EntityId Id : Sages)
	{
		const Entity* E = W.Find(Id);
		Ready += E != nullptr && E->AbilityCooldown <= 0.f ? 1 : 0;
	}
	BH_EXPECT(Ready == 3);
	const Entity* E1 = W.Find(G1);
	const Entity* E2 = W.Find(G2);
	BH_EXPECT(E1 != nullptr && E1->Hp < E1->MaxHp && E2 != nullptr && E2->Hp < E2->MaxHp);
	BH_EXPECT(W.CmdAbility(Sages, Ability::Sunburst) == 1);
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
