// Beaconhold simulation core - Gloam commander AI.
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
// A building's next unit, chosen when it fell idle and kept until the gloom for it is there, so
// cheap units cannot keep starving dear ones. Served first come, first served.
struct AIPendingPick
{
	EntityId Building = NoEntity;
	Archetype Unit = Archetype::None;
	float Since = 0.f;
};

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
	// Shares of the Warden army's supply: Shieldbearers, Rangers, Stag Riders, Sages. Measured
	// every think, so not saved.
	float ArmyShare[4] = {0.f, 0.f, 0.f, 0.f};
	std::vector<AIPendingPick> Pending;

	// The unit Building trains next, countering the Warden army.
	Archetype PickCounter(World& W, const Entity& Building) const;

private:
	void MeasurePlayerArmy(const World& W);
	void Produce(World& W);
	void ManageAttackers(World& W);
	void Defend(World& W);
	void LaunchWave(World& W);
	float CurrentIncome(const World& W) const;
};

} // namespace bh
