// Playtest matrix: scripted players play every mission and difficulty with several strategies;
// telemetry reports pacing, economy, unit balance and simulation health for each run, then a
// one-line-per-run table.
//
// Usage: bhplaytest [filter] [--brief]
//   filter  only runs whose name contains it (e.g. "heart", "dusk/hard", "riders")
//   --brief only the table
#include "Bot.h"
#include "Playtest.h"

#include "BhHud.h"
#include "BhMissions.h"
#include "BhSession.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace bh;
using namespace bht;

namespace
{
struct Run
{
	std::string Name;
	int Mission = 0;
	Difficulty Diff = Difficulty::Normal;
	BotConfig Bot;
	float MaxSeconds = 45.f * 60.f;
};

std::vector<Run> BuildRuns()
{
	std::vector<Run> Runs;
	auto Add = [&Runs](const char* Name, int Mission, Difficulty Diff, BotConfig Bot)
	{
		Run R;
		R.Name = Name;
		R.Mission = Mission;
		R.Diff = Diff;
		R.Bot = Bot;
		Runs.push_back(R);
	};
	BotConfig Tutorial;
	Tutorial.TargetWorkers = 8;
	Add("kindling/normal/mixed", 0, Difficulty::Normal, Tutorial);

	BotConfig Dusk;
	Dusk.bBuildTowers = true;
	Dusk.TargetWorkers = 14;
	Add("dusk/easy/mixed", 1, Difficulty::Easy, Dusk);
	Add("dusk/normal/mixed", 1, Difficulty::Normal, Dusk);
	Add("dusk/hard/mixed", 1, Difficulty::Hard, Dusk);
	BotConfig DuskNoTowers = Dusk;
	DuskNoTowers.bBuildTowers = false;
	Add("dusk/normal/no-towers", 1, Difficulty::Normal, DuskNoTowers);

	BotConfig Heart;
	Heart.TargetWorkers = 16;
	Heart.AttackSupply = 30.f;
	Add("heart/easy/mixed", 2, Difficulty::Easy, Heart);
	Add("heart/normal/mixed", 2, Difficulty::Normal, Heart);
	Add("heart/hard/mixed", 2, Difficulty::Hard, Heart);
	BotConfig Smart = Heart;
	Smart.bSmartEconomy = true;
	Add("heart/normal/mixed-smart-economy", 2, Difficulty::Normal, Smart);
	const BotArmy Armies[] = {BotArmy::Shields, BotArmy::Rangers, BotArmy::Riders, BotArmy::Sages};
	for (BotArmy A : Armies)
	{
		BotConfig C = Heart;
		C.Army = A;
		C.bSmartEconomy = true;
		Add((std::string("heart/normal/") + BotArmyName(A)).c_str(), 2, Difficulty::Normal, C);
	}
	BotConfig Rush = Heart;
	Rush.AttackSupply = 10.f;
	Rush.bSmartEconomy = true;
	Add("heart/normal/early-rush", 2, Difficulty::Normal, Rush);
	BotConfig Turtle = Heart;
	Turtle.bPassive = true;
	Turtle.bBuildTowers = true;
	Turtle.TowerCount = 6;
	Turtle.bSmartEconomy = true;
	Add("heart/normal/turtle", 2, Difficulty::Normal, Turtle);
	return Runs;
}

const char* OutcomeText(MissionOutcome O)
{
	return O == MissionOutcome::Won ? "WON" : (O == MissionOutcome::Lost ? "LOST" : "open");
}
} // namespace

int main(int Argc, char** Argv)
{
	const char* Filter = nullptr;
	bool bBrief = false;
	for (int I = 1; I < Argc; ++I)
	{
		if (std::strcmp(Argv[I], "--brief") == 0)
		{
			bBrief = true;
		}
		else
		{
			Filter = Argv[I];
		}
	}
	struct Row
	{
		std::string Name;
		MissionOutcome Outcome;
		float Time;
		int Stars;
		int Lost;
		int Kills;
		float Lull;
		size_t Stuck;
		size_t Violations;
		int PeakUnits;
		float FloatSun;
		float FloatWood;
	};
	std::vector<Row> Rows;
	int Failures = 0;
	for (const Run& R : BuildRuns())
	{
		if (Filter != nullptr && R.Name.find(Filter) == std::string::npos)
		{
			continue;
		}
		Session S;
		SessionConfig C;
		C.MissionIndex = R.Mission;
		C.Diff = R.Diff;
		C.bTutorial = R.Mission == 0;
		std::string Err;
		if (!S.Start(C, Err))
		{
			std::printf("%s: start failed: %s\n", R.Name.c_str(), Err.c_str());
			++Failures;
			continue;
		}
		Telemetry T;
		const BotReport Report = PlayMission(S, R.Bot, R.MaxSeconds, &T);
		if (!bBrief)
		{
			std::printf("== %s\n%s", R.Name.c_str(), T.Summary().c_str());
		}
		Failures += static_cast<int>(T.Violations.size());
		const TeamState& P = S.GetWorld().GetTeam(Team::Player);
		Rows.push_back({R.Name, T.Outcome, T.Duration, Report.Stars, P.Stats.UnitsLost, P.Stats.Kills, T.LongestLull, T.Stuck.size(), T.Violations.size(),
			T.PeakPlayerUnits + T.PeakEnemyUnits, T.AverageFloat(Resource::Sunstone), T.AverageFloat(Resource::Timber)});
	}
	std::printf("\n%-34s %-5s %6s %5s %5s %5s %6s %5s %4s %5s %11s\n", "run", "end", "time", "stars", "lost", "kills", "lull", "stuck", "bad", "units",
		"float s/t");
	for (const Row& Rw : Rows)
	{
		std::printf("%-34s %-5s %6s %5d %5d %5d %6s %5zu %4zu %5d %5.0f/%-5.0f\n", Rw.Name.c_str(), OutcomeText(Rw.Outcome), FormatTime(Rw.Time).c_str(),
			Rw.Stars, Rw.Lost, Rw.Kills, FormatTime(Rw.Lull).c_str(), Rw.Stuck, Rw.Violations, Rw.PeakUnits, Rw.FloatSun, Rw.FloatWood);
	}
	return Failures > 0 ? 1 : 0;
}
