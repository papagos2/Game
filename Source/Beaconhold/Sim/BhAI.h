// Beaconhold simulation core — Gloam commander AI.
//
// The Gloam do not gather: they earn "gloom" over time, train from their buildings, keep a
// home guard, launch escalating attack waves (often at outlying Beacons) and strike back
// when their base is hit. Difficulty scales income, timing and wave size.
#pragma once

#include "BhMissions.h"
#include "BhWorld.h"

#include <vector>

namespace bh
{
class EnemyAI
{
public:
	void Start(const AIConfig& InConfig, Difficulty InDiff, World& W);
	void Tick(World& W, float Dt);

	bool IsEnabled() const { return Config.bEnabled; }
	float SecondsToNextAttack(const World& W) const { return NextAttack - W.GetTime(); }

	// State (public for serialization and tests).
	AIConfig Config;
	Difficulty Diff = Difficulty::Normal;
	float GloomAccum = 0.f;
	float ThinkTimer = 0.f;
	float NextAttack = 0.f;
	int WaveSize = 4;
	int WavesLaunched = 0;
	Vec2 Home;
	Vec2 Rally;
	std::vector<EntityId> Attackers;
	float LastDefenseTime = -100.f;

private:
	void Produce(World& W);
	void ManageAttackers(World& W);
	void Defend(World& W);
	void LaunchWave(World& W);
	float CurrentIncome(const World& W) const;
};

} // namespace bh
