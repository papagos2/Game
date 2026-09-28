// Scripted player used to play full missions in tests. It only uses the player-facing
// control API (select, tap, build menu, placement, train), like a human would.
#pragma once

#include "BhSession.h"

#include <string>

namespace bht
{
// Army the bot builds (for balance comparisons).
enum class BotArmy
{
	Mixed,   // Shieldbearers and Rangers, Stag Riders and Sages later
	Shields, // Shieldbearers only
	Rangers, // Rangers only
	Riders,  // rushes the Stag Lodge, then Stag Riders only
	Sages,   // Shieldbearers screening Sages
};

struct BotConfig
{
	int TargetWorkers = 14;
	float AttackSupply = 22.f;   // army supply before the first push
	bool bUseAbilities = true;
	bool bBuildTowers = false;
	int TowerCount = 3;
	bool bPassive = false;       // never attacks (used to prove the AI can win)
	bool bVerbose = false;
	BotArmy Army = BotArmy::Mixed;
	// Moves workers between Sunstone and Timber by what is short, like an attentive player.
	bool bSmartEconomy = false;
};

class Telemetry;

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

// Plays the session until the mission ends or MaxSeconds of game time pass. Telemetry, when
// given, observes every tick.
BotReport PlayMission(bh::Session& S, const BotConfig& Config, float MaxSeconds, Telemetry* Observer = nullptr);

const char* BotArmyName(BotArmy A);

} // namespace bht
