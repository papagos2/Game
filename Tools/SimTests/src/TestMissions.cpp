// Mission tests: maps load and are connected; each mission is winnable by a scripted player;
// the enemy AI is a real threat to a passive player.
#include "TestFramework.h"

#include "Bot.h"

#include "BhHud.h"
#include "BhMissions.h"
#include "BhPath.h"
#include "BhSession.h"

#include <cstdio>

using namespace bh;

namespace
{
void PrintReport(const char* Name, const bht::BotReport& R)
{
	std::printf("  %s: %s at %s, stars=%d, trained=%d lost=%d kills=%d, sun=%d wood=%d, maxArmy=%d, maxEnemy=%d, stuck=%d\n", Name,
		R.Outcome == MissionOutcome::Won ? "WON" : (R.Outcome == MissionOutcome::Lost ? "LOST" : "UNFINISHED"), FormatTime(R.Time).c_str(),
		R.Stars, R.UnitsTrained, R.UnitsLost, R.Kills, R.Gathered[0], R.Gathered[1], R.MaxArmySupply, R.MaxEnemyUnits, R.StuckWorkers);
	std::printf("%s", R.Log.c_str());
}

bht::BotReport Play(int Mission, Difficulty Diff, const bht::BotConfig& Cfg, float MaxSeconds)
{
	Session S;
	SessionConfig C;
	C.MissionIndex = Mission;
	C.Diff = Diff;
	C.bTutorial = Mission == 0;
	std::string Err;
	const bool bOk = S.Start(C, Err);
	BH_EXPECT_MSG(bOk, "start failed: %s", Err.c_str());
	return bht::PlayMission(S, Cfg, MaxSeconds);
}
} // namespace

BH_TEST(Missions_MapsLoadAndConnect)
{
	for (int I = 0; I < GetMissionCount(); ++I)
	{
		const MissionDef& M = GetMission(I);
		World W;
		std::string Err;
		const bool bOk = LoadMissionMap(M, W, Err);
		BH_EXPECT_MSG(bOk, "mission %d: %s", I, Err.c_str());
		if (!bOk)
		{
			continue;
		}
		BH_EXPECT_MSG(W.CountOwned(Team::Player, Archetype::Keep, false) == 1, "mission %d keep count", I);
		const Entity* Keep = nullptr;
		for (const Entity& E : W.GetEntities())
		{
			if (E.Type == Archetype::Keep)
			{
				Keep = &E;
			}
		}
		if (Keep == nullptr)
		{
			continue;
		}
		PathFinder PF;
		std::vector<Vec2> Path;
		bool bReached = false;
		Tile Start;
		W.GetMap().FindNearestWalkable(Tile::FromPos(Keep->Pos + Vec2(0.f, 2.6f)), 4, Start);
		int Tagged = 0;
		for (const Entity& E : W.GetEntities())
		{
			if (E.IsUnit() || E.Owner == Team::Player)
			{
				continue;
			}
			PF.FindPath(W.GetMap(), Start.Center(), E.Rect.Expanded(1), E.Pos, Path, bReached);
			BH_EXPECT_MSG(bReached, "mission %d: %s at %.0f,%.0f unreachable from keep", I, GetDef(E.Type).Name, E.Pos.X, E.Pos.Y);
			Tagged += E.Tag == 1 ? 1 : 0;
		}
		for (const TileRect& Site : W.GetMap().BeaconSites)
		{
			PF.FindPath(W.GetMap(), Start.Center(), Site.Expanded(1), Site.Center(), Path, bReached);
			BH_EXPECT_MSG(bReached, "mission %d: beacon site unreachable", I);
		}
		if (M.TaggedArch != Archetype::None)
		{
			BH_EXPECT_MSG(Tagged >= 1, "mission %d has no tagged target", I);
		}
		for (const WaveDef& Wave : M.Waves)
		{
			BH_EXPECT_MSG(!W.GetMap().SpawnPoints[Wave.SpawnPoint].empty(), "mission %d: missing spawn point %d", I, Wave.SpawnPoint);
		}
	}
}

BH_TEST(Missions_TutorialCompletesWithBot)
{
	Session S;
	SessionConfig C;
	C.MissionIndex = 0;
	C.bTutorial = true;
	std::string Err;
	BH_EXPECT(S.Start(C, Err));
	BH_EXPECT(S.GetMission().IsTutorialActive());
	bht::BotConfig Cfg;
	Cfg.TargetWorkers = 8;
	const bht::BotReport R = bht::PlayMission(S, Cfg, 20.f * 60.f);
	PrintReport("Kindling/Normal", R);
	BH_EXPECT(R.Outcome == MissionOutcome::Won);
	BH_EXPECT_MSG(S.GetMission().TutorialIndex >= static_cast<int>(GetMission(0).Tutorial.size()) - 1, "tutorial reached step %d", S.GetMission().TutorialIndex);
}

BH_TEST(Missions_LongDuskWinnable)
{
	bht::BotConfig Cfg;
	Cfg.bBuildTowers = true;
	Cfg.TargetWorkers = 14;
	const bht::BotReport R = Play(1, Difficulty::Normal, Cfg, 20.f * 60.f);
	PrintReport("LongDusk/Normal", R);
	BH_EXPECT(R.Outcome == MissionOutcome::Won);
}

BH_TEST(Missions_HeartWinnableNormal)
{
	bht::BotConfig Cfg;
	Cfg.TargetWorkers = 16;
	Cfg.AttackSupply = 30.f;
	const bht::BotReport R = Play(2, Difficulty::Normal, Cfg, 45.f * 60.f);
	PrintReport("Heart/Normal", R);
	BH_EXPECT(R.Outcome == MissionOutcome::Won);
}

BH_TEST(Missions_HeartEasy)
{
	bht::BotConfig Cfg;
	Cfg.TargetWorkers = 14;
	Cfg.AttackSupply = 24.f;
	const bht::BotReport R = Play(2, Difficulty::Easy, Cfg, 45.f * 60.f);
	PrintReport("Heart/Easy", R);
	BH_EXPECT(R.Outcome == MissionOutcome::Won);
}

BH_TEST(Missions_HeartHardReport)
{
	bht::BotConfig Cfg;
	Cfg.TargetWorkers = 16;
	Cfg.AttackSupply = 34.f;
	const bht::BotReport R = Play(2, Difficulty::Hard, Cfg, 45.f * 60.f);
	PrintReport("Heart/Hard (informational)", R);
	BH_EXPECT(R.Outcome != MissionOutcome::InProgress);
}

BH_TEST(Missions_AIThreatensPassivePlayer)
{
	bht::BotConfig Cfg;
	Cfg.bPassive = true;
	Cfg.TargetWorkers = 10;
	Session S;
	SessionConfig C;
	C.MissionIndex = 2;
	C.Diff = Difficulty::Normal;
	C.bTutorial = false;
	std::string Err;
	BH_EXPECT(S.Start(C, Err));
	const bht::BotReport R = bht::PlayMission(S, Cfg, 25.f * 60.f);
	PrintReport("Heart/Normal passive", R);
	BH_EXPECT(S.GetAI().WavesLaunched >= 3);
	BH_EXPECT_MSG(R.UnitsLost > 0 || R.Outcome == MissionOutcome::Lost, "the AI never hurt a passive player");
}

BH_TEST(Missions_StarGoals)
{
	// The Long Dusk ends at dawn for everyone, so its second star asks for a clean defence.
	BH_EXPECT(StarGoalText(GetMission(1), 1) == "Lose no buildings");
	BH_EXPECT(StarGoalText(GetMission(2), 1) == "Finish within 18:00");
	for (int Lose = 0; Lose < 2; ++Lose)
	{
		Session S;
		SessionConfig C;
		C.MissionIndex = 1;
		C.bTutorial = false;
		std::string Err;
		BH_EXPECT(S.Start(C, Err));
		World& W = S.GetWorld();
		for (const TileRect& Site : W.GetMap().BeaconSites)
		{
			W.SpawnBuilding(Archetype::Beacon, Team::Player, Tile(Site.X0, Site.Y0), true);
		}
		const EntityId Cottage = W.SpawnBuilding(Archetype::Cottage, Team::Player, Tile(30, 26), true);
		if (Lose == 1)
		{
			W.Kill(Cottage, NoEntity);
		}
		S.GetMission().Elapsed = 599.9f;
		for (int I = 0; I < 10 && S.GetMission().Outcome == MissionOutcome::InProgress; ++I)
		{
			S.Update(World::TickSeconds, nullptr);
		}
		BH_EXPECT(S.GetMission().Outcome == MissionOutcome::Won);
		BH_EXPECT_MSG(S.GetMission().Stars[1] == (Lose == 0), "second star %d with a building %s", S.GetMission().Stars[1] ? 1 : 0, Lose == 1 ? "lost" : "kept");
	}
}
