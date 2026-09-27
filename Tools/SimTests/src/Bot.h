// Scripted player used to play full missions in tests. It only uses the player-facing
// control API (select, tap, build menu, placement, train), like a human would.
#pragma once

#include "BhSession.h"

#include <string>

namespace bht
{
struct BotConfig
{
	int TargetWorkers = 14;
	float AttackSupply = 22.f;   // army supply before the first push
	bool bUseAbilities = true;
	bool bBuildTowers = false;
	bool bPassive = false;       // never attacks (used to prove the AI can win)
	bool bVerbose = false;
};

struct BotReport
{
	bh::MissionOutcome Outcome = bh::MissionOutcome::InProgress;
	float Time = 0.f;
	int Stars = 0;
	int MaxArmySupply = 0;
	int UnitsTrained = 0;
	int UnitsLost = 0;
	int Kills = 0;
	int Gathered[2] = {0, 0};
	int BuildingsBuilt = 0;
	int MaxEnemyUnits = 0;
	int StuckWorkers = 0;
	std::string Log;
};

// Plays the session until the mission ends or MaxSeconds of game time pass.
BotReport PlayMission(bh::Session& S, const BotConfig& Config, float MaxSeconds);

} // namespace bht
