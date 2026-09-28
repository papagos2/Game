// Playtest matrix: scripted players play every mission and difficulty with several strategies;
// telemetry reports pacing, economy, unit balance and simulation health for each run, then a
// one-line-per-run table.
//
// Usage: bhplaytest [filter] [--brief] [--seeds N]
//   filter   only runs whose name contains it (e.g. "heart", "dusk/hard", "riders")
//   --brief  only the table
//   --seeds  games per run with different random seeds (default 3; seed 0 is the game's own)
#include "Bot.h"
#include "Playtest.h"

#include "BhHud.h"
#include "BhMissions.h"
#include "BhSession.h"

#include <cstdio>
#include <cstdlib>
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
	BotConfig DuskStrong = Dusk;
	DuskStrong.bSmartEconomy = true;
	DuskStrong.TowerCount = 5;
	DuskStrong.Army = BotArmy::Sages;
	Add("dusk/hard/strong", 1, Difficulty::Hard, DuskStrong);
	Add("dusk/normal/strong", 1, Difficulty::Normal, DuskStrong);

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

} // namespace

int main(int Argc, char** Argv)
{
	const char* Filter = nullptr;
	bool bBrief = false;
	int Seeds = 3;
	for (int I = 1; I < Argc; ++I)
	{
		if (std::strcmp(Argv[I], "--brief") == 0)
		{
			bBrief = true;
		}
		else if (std::strcmp(Argv[I], "--seeds") == 0 && I + 1 < Argc)
		{
			Seeds = std::atoi(Argv[++I]);
		}
		else
		{
			Filter = Argv[I];
		}
	}
	Seeds = Seeds < 1 ? 1 : Seeds;
	struct Row
	{
		std::string Name;
		int Wins = 0;
		int Losses = 0;
		int Runs = 0;
		float TimeSum = 0.f;
		float TimeMax = 0.f;
		int StarSum = 0;
		int LostSum = 0;
		int KillSum = 0;
		float LullMax = 0.f;
		size_t Stuck = 0;
		size_t Violations = 0;
		int PeakUnits = 0;
	};
	std::vector<Row> Rows;
	int Failures = 0;
	for (const Run& R : BuildRuns())
	{
		if (Filter != nullptr && R.Name.find(Filter) == std::string::npos)
		{
			continue;
		}
		Row Rw;
		Rw.Name = R.Name;
		for (int Seed = 0; Seed < Seeds; ++Seed)
		{
			Session S;
			SessionConfig C;
			C.MissionIndex = R.Mission;
			C.Diff = R.Diff;
			C.bTutorial = R.Mission == 0;
			C.Seed = static_cast<uint32_t>(Seed); // 0 is the game's own seed
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
				std::printf("== %s (seed %d)\n%s", R.Name.c_str(), Seed, T.Summary().c_str());
			}
			Failures += static_cast<int>(T.Violations.size());
			const TeamState& P = S.GetWorld().GetTeam(Team::Player);
			++Rw.Runs;
			Rw.Wins += T.Outcome == MissionOutcome::Won ? 1 : 0;
			Rw.Losses += T.Outcome == MissionOutcome::Lost ? 1 : 0;
			Rw.TimeSum += T.Duration;
			Rw.TimeMax = T.Duration > Rw.TimeMax ? T.Duration : Rw.TimeMax;
			Rw.StarSum += Report.Stars;
			Rw.LostSum += P.Stats.UnitsLost;
			Rw.KillSum += P.Stats.Kills;
			Rw.LullMax = T.LongestLull > Rw.LullMax ? T.LongestLull : Rw.LullMax;
			Rw.Stuck += T.Stuck.size();
			Rw.Violations += T.Violations.size();
			Rw.PeakUnits = T.PeakPlayerUnits + T.PeakEnemyUnits > Rw.PeakUnits ? T.PeakPlayerUnits + T.PeakEnemyUnits : Rw.PeakUnits;
		}
		Rows.push_back(Rw);
	}
	std::printf("\n%-34s %5s %5s %7s %7s %5s %5s %5s %6s %5s %4s %5s\n", "run", "won", "lost", "avg", "max", "stars", "died", "kills", "lull", "stuck", "bad",
		"units");
	for (const Row& Rw : Rows)
	{
		const float N = static_cast<float>(Rw.Runs > 0 ? Rw.Runs : 1);
		std::printf("%-34s %2d/%-2d %2d/%-2d %7s %7s %5.1f %5.0f %5.0f %6s %5zu %4zu %5d\n", Rw.Name.c_str(), Rw.Wins, Rw.Runs, Rw.Losses, Rw.Runs,
			FormatTime(Rw.TimeSum / N).c_str(), FormatTime(Rw.TimeMax).c_str(), static_cast<float>(Rw.StarSum) / N, static_cast<float>(Rw.LostSum) / N,
			static_cast<float>(Rw.KillSum) / N, FormatTime(Rw.LullMax).c_str(), Rw.Stuck, Rw.Violations, Rw.PeakUnits);
	}
	return Failures > 0 ? 1 : 0;
}
