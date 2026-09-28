// Playtest matrix: scripted players play every mission and difficulty with several strategies;
// telemetry reports pacing, economy, unit balance and simulation health for each run, then a
// one-line-per-run table.
//
// Usage: bhplaytest [filter] [--brief] [--seeds N] [--bench]
//   filter   only runs whose name contains it (e.g. "heart", "dusk/hard", "riders")
//   --brief  only the table
//   --seeds  games per run with different random seeds (default 3; seed 0 is the game's own)
//   --bench  CPU cost of every simulation tick (this machine's milliseconds) for each run and a
//            worst-case battle, instead of the gameplay table
#include "Bot.h"
#include "Playtest.h"

#include "BhHud.h"
#include "BhMissions.h"
#include "BhPainter.h"
#include "BhRender.h"
#include "BhSession.h"

#include <chrono>
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
	int BoonRanks[NumBoons] = {};
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

	// Progression: the strongest combat boons all Renown can buy (12: Hardy 3 and Keen 3), on Hard.
	auto AddBoosted = [&Runs, &Add](const char* Name, int Mission, BotConfig Bot)
	{
		Add(Name, Mission, Difficulty::Hard, Bot);
		Runs.back().BoonRanks[static_cast<int>(Boon::Hardy)] = MaxBoonRank;
		Runs.back().BoonRanks[static_cast<int>(Boon::Keen)] = MaxBoonRank;
	};
	AddBoosted("dusk/hard/mixed-max-boons", 1, Dusk);
	AddBoosted("heart/hard/mixed-max-boons", 2, Heart);
	// A first try at Hard after three stars on every Normal mission (9 Renown): Hardy 2, Keen 2,
	// and rank 1 of the other three.
	auto AddFirstHard = [&Runs, &Add](const char* Name, int Mission, BotConfig Bot)
	{
		Add(Name, Mission, Difficulty::Hard, Bot);
		const int Ranks[NumBoons] = {2, 2, 1, 1, 1};
		for (int B = 0; B < NumBoons; ++B)
		{
			Runs.back().BoonRanks[B] = Ranks[B];
		}
	};
	AddFirstHard("dusk/hard/mixed-9-renown", 1, Dusk);
	AddFirstHard("heart/hard/mixed-9-renown", 2, Heart);
	return Runs;
}

const Entity* FindFirst(const World& W, Archetype A)
{
	for (const Entity& E : W.GetEntities())
	{
		if (E.bAlive && E.Type == A)
		{
			return &E;
		}
	}
	return nullptr;
}

// Game-thread work the Unreal layer does with the simulation's own code: the HUD model and every
// entity's pose each frame, the minimap overlay five times a second and its base every 2 s.
struct FrameCost
{
	struct Item
	{
		double Sum = 0.0;
		float Max = 0.f;
		int Count = 0;
		void Add(float Ms)
		{
			Sum += static_cast<double>(Ms);
			Max = Ms > Max ? Ms : Max;
			++Count;
		}
	};
	Item Hud;
	Item Poses;
	Item Overlay;
	Item Base;
	int PeakEntities = 0;

	template <typename Fn>
	static float Time(Fn&& F)
	{
		const auto Start = std::chrono::steady_clock::now();
		F();
		return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - Start).count();
	}

	void Measure(const Session& S, float Now)
	{
		HudModel Model;
		Hud.Add(Time([&]() { BuildHudModel(S, Model); }));
		int Count = 0;
		Poses.Add(Time([&]()
		{
			for (const Entity& E : S.GetWorld().GetEntities())
			{
				AnimInput In;
				In.Type = E.Type;
				In.Owner = E.Owner;
				In.Act = E.Act;
				In.Buff = E.Buff;
				In.Carry = E.CarryAmount > 0 ? E.CarryType : Resource::None;
				In.bMoving = E.Act == Activity::Walking || E.Act == Activity::Carrying;
				In.bConstructed = E.bConstructed;
				In.BuildProgress = E.BuildProgress;
				In.Time = Now;
				In.Seed = E.Id;
				EvaluateEntityPose(In);
				for (int R = 1; R < NumPartRoles; ++R)
				{
					EvaluateRolePose(static_cast<PartRole>(R), In);
				}
				++Count;
			}
		}));
		PeakEntities = Count > PeakEntities ? Count : PeakEntities;
		ImageRGBA BaseImage;
		Base.Add(Time([&]() { PaintMinimap(S.GetWorld().GetMap(), 2, BaseImage); }));
		const Vec2 View[4] = {Vec2(10.f, 10.f), Vec2(30.f, 10.f), Vec2(30.f, 22.f), Vec2(10.f, 22.f)};
		ImageRGBA Image;
		Overlay.Add(Time([&]() { PaintMinimapOverlay(S, BaseImage, 2, View, true, {}, Image); }));
	}

	void Print() const
	{
		auto Row = [](const char* Name, const Item& I)
		{
			std::printf("%-34s %7d %8.3f %8s %8s %8.3f\n", Name, I.Count, I.Count > 0 ? static_cast<float>(I.Sum / I.Count) : 0.f, "", "", I.Max);
		};
		std::printf("\ngame-thread presentation work (same units; up to %d entities):\n", PeakEntities);
		Row("frame/hud model (every frame)", Hud);
		Row("frame/all poses (every frame)", Poses);
		Row("frame/minimap overlay (5 a second)", Overlay);
		Row("frame/minimap base (every 2 s)", Base);
	}
};

// The worst the rules allow at once: a full Warden army (supply 100) against the Gloam's army cap
// and a wave on top, all fighting across the Heart map while the enemy commander plays on.
void StressBattle(Telemetry& T, int Wardens, int Gloam, float Seconds, FrameCost* Frame = nullptr)
{
	Session S;
	SessionConfig C;
	C.MissionIndex = 2;
	C.bTutorial = false;
	std::string Err;
	if (!S.Start(C, Err))
	{
		return;
	}
	World& W = S.GetWorld();
	const Entity* Heart = FindFirst(W, Archetype::GloamHeart);
	const Vec2 Keep = S.GetKeepPos();
	const Vec2 Home = Heart != nullptr ? Heart->Pos : Keep + Vec2(20.f, -20.f);
	const Archetype WardenTypes[] = {Archetype::Shieldbearer, Archetype::Ranger, Archetype::StagRider, Archetype::Sage, Archetype::Shieldbearer};
	const Archetype GloamTypes[] = {Archetype::Gloomling, Archetype::Thornback, Archetype::Gloomling, Archetype::Hexer, Archetype::BogTitan};
	std::vector<EntityId> Army;
	std::vector<EntityId> Horde;
	const Vec2 Towards = (Home - Keep).Normalized();
	for (int I = 0; I < Wardens; ++I)
	{
		const Vec2 Offset = Towards * (4.f + static_cast<float>(I / 10) * 0.7f) + Vec2(-Towards.Y, Towards.X) * (static_cast<float>(I % 10) - 4.5f) * 0.7f;
		Army.push_back(W.SpawnUnit(WardenTypes[I % 5], Team::Player, Keep + Offset));
	}
	for (int I = 0; I < Gloam; ++I)
	{
		const Vec2 Offset = Towards * -(4.f + static_cast<float>(I / 10) * 0.7f) + Vec2(-Towards.Y, Towards.X) * (static_cast<float>(I % 10) - 4.5f) * 0.7f;
		Horde.push_back(W.SpawnUnit(GloamTypes[I % 5], Team::Enemy, Home + Offset));
	}
	T.Begin(S);
	std::vector<GameEvent> Events;
	float NextOrder = 0.f;
	while (W.GetTime() < Seconds && S.GetMission().Outcome == MissionOutcome::InProgress)
	{
		if (W.GetTime() >= NextOrder)
		{
			// Both sides keep pushing into each other (fresh paths for everyone at once).
			NextOrder = W.GetTime() + 15.f;
			W.CmdMove(Army, Home, true);
			W.CmdMove(Horde, Keep, true);
		}
		const auto Start = std::chrono::steady_clock::now();
		S.Update(World::TickSeconds, nullptr);
		const float Ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - Start).count();
		S.TakeEvents(Events);
		T.OnTickTime(S, Ms);
		T.OnTick(S, Events);
		if (Frame != nullptr && T.TickMs.size() % 10 == 0)
		{
			Frame->Measure(S, W.GetTime());
		}
	}
	T.End(S);
}

void PrintCpuRow(const char* Name, const Telemetry& T)
{
	double Sum = 0.0;
	for (float Ms : T.TickMs)
	{
		Sum += static_cast<double>(Ms);
	}
	const float Avg = T.TickMs.empty() ? 0.f : static_cast<float>(Sum / static_cast<double>(T.TickMs.size()));
	std::printf("%-34s %7zu %8.3f %8.3f %8.3f %8.3f  at %s with %d units\n", Name, T.TickMs.size(), Avg, T.TickPercentile(0.99f), T.TickPercentile(0.999f),
		T.WorstTickMs, FormatTime(T.WorstTickAt).c_str(), T.WorstTickUnits);
}

} // namespace

int main(int Argc, char** Argv)
{
	const char* Filter = nullptr;
	bool bBrief = false;
	bool bBench = false;
	int Seeds = 3;
	for (int I = 1; I < Argc; ++I)
	{
		if (std::strcmp(Argv[I], "--brief") == 0)
		{
			bBrief = true;
		}
		else if (std::strcmp(Argv[I], "--bench") == 0)
		{
			bBench = true;
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
	if (bBench)
	{
		std::printf("%-34s %7s %8s %8s %8s %8s  (milliseconds per simulation tick on this machine)\n", "run", "ticks", "avg", "p99", "p99.9", "max");
	}
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
			for (int B = 0; B < NumBoons; ++B)
			{
				C.BoonRanks[B] = R.BoonRanks[B];
			}
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
			if (bBench)
			{
				PrintCpuRow((R.Name + " #" + std::to_string(Seed)).c_str(), T);
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
	if (bBench)
	{
		// Worst cases: armies at the supply and army caps meet head on; then twice that.
		Telemetry Cap;
		FrameCost Frame;
		StressBattle(Cap, 80, 60, 180.f, &Frame);
		PrintCpuRow("stress/140-units", Cap);
		Telemetry Double;
		StressBattle(Double, 160, 120, 120.f);
		PrintCpuRow("stress/280-units (beyond the caps)", Double);
		Frame.Print();
		return Failures > 0 ? 1 : 0;
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
