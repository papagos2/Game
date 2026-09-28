// Playtest telemetry: watches a played mission and measures pacing, economy, combat, AI
// behaviour and simulation health (stuck units, broken invariants, entity peaks).
//
// It only observes: it reads the session after every tick and consumes its events, so the
// same numbers come out whether a bot or a recorded input drives the session.
#pragma once

#include "BhHud.h"
#include "BhSession.h"

#include <map>
#include <string>
#include <vector>

namespace bht
{
struct Lull
{
	float Start = 0.f;
	float End = 0.f;
};

struct StuckIncident
{
	bh::EntityId Id = bh::NoEntity;
	bh::Archetype Type = bh::Archetype::None;
	bh::Team Owner = bh::Team::Neutral;
	bh::Vec2 Pos;
	float Time = 0.f;
	bh::OrderType Order = bh::OrderType::Idle;
	// What it was trying to reach.
	bh::Archetype TargetType = bh::Archetype::None;
	bh::Vec2 TargetPos;
	bool bHasPath = false;
	size_t PathLeft = 0;
	float EngageTimer = 0.f;
	float StuckTimer = 0.f;
};

struct TypeStats
{
	int Trained = 0;
	int Lost = 0;
	int Kills = 0;
	float DamageDealt = 0.f;
	float DamageTaken = 0.f;
};

class Telemetry
{
public:
	// Seconds without player-involved combat that count as a lull (after the first fight).
	float LullThreshold = 75.f;
	float SaveCheckInterval = 120.f; // game seconds between suspend/resume round trips
	// A unit trying to walk that stays within StuckRadius for StuckSeconds is stuck.
	float StuckRadius = 0.75f;
	float StuckSeconds = 8.f;

	void Begin(const bh::Session& S);
	// Call after every simulation tick with the events that tick produced.
	void OnTick(const bh::Session& S, const std::vector<bh::GameEvent>& Events);
	// Wall-clock cost of one simulation tick (world, mission script, enemy AI).
	void OnTickTime(const bh::Session& S, float Ms);
	void End(const bh::Session& S);

	// ---------------------------------------------------------------- Results
	bh::MissionOutcome Outcome = bh::MissionOutcome::InProgress;
	float Duration = 0.f;

	// Economy (player).
	int WorkerIncome[bh::NumResources] = {};
	int PassiveIncome[bh::NumResources] = {};
	float WorkerSeconds[bh::NumResources] = {}; // worker-seconds assigned to each resource
	float IdleWorkerSeconds = 0.f;
	float WaitingWorkerSeconds = 0.f; // assigned but queued at a full outcrop
	float FloatIntegral[bh::NumResources] = {};
	float SupplyBlockedSeconds = 0.f;
	int Spent[bh::NumResources] = {};

	// Milestones (seconds, -1 = never).
	float FirstSoldier = -1.f;
	float FirstCombat = -1.f;
	float FirstLoss = -1.f;
	float FirstBuildingLost = -1.f;

	// Pacing.
	std::vector<Lull> Lulls;
	float LongestLull = 0.f;
	float CombatSeconds = 0.f; // seconds with player-involved damage

	// Combat by archetype (both teams).
	TypeStats Types[bh::NumArchetypes];

	// Health.
	std::vector<StuckIncident> Stuck;
	std::vector<std::string> Violations;
	int SaveChecks = 0; // suspend saves written, restored and compared

	// Messages the player would see (MakeNotice, after the HUD's 3 s repeat filter).
	int Notices = 0;
	int PeakNoticesIn10s = 0;
	float PeakNoticesAt = 0.f;
	int NoticesPushedOut = 0; // toasts replaced before their time by newer ones (4 on screen)
	std::map<std::string, int> NoticeCounts;

	// CPU (milliseconds per simulation tick on this machine).
	std::vector<float> TickMs;
	float WorstTickMs = 0.f;
	float WorstTickAt = 0.f;
	int WorstTickUnits = 0;
	float TickPercentile(float P) const;
	int PeakPlayerUnits = 0;
	int PeakEnemyUnits = 0;
	int PeakEntities = 0;
	int PeakProjectiles = 0;
	int PeakEventsPerTick = 0;

	// Enemy.
	std::vector<float> WaveTimes; // AI waves launched and scripted waves spawned
	int PeakGloom = 0;
	float GloomFloatIntegral = 0.f;

	// Derived.
	float IncomePerWorkerMinute(bh::Resource R) const;
	float AverageFloat(bh::Resource R) const;
	std::string Summary() const;

private:
	struct Tracker
	{
		bh::Vec2 Anchor;
		float Since = 0.f;
		bool bReported = false;
		uint32_t Swings = 0; // a unit that keeps swinging is fighting, not stuck
	};
	void Sample(const bh::Session& S, float Dt);
	void CheckInvariants(const bh::Session& S);
	void CheckSaveRoundTrip(const bh::Session& S);
	void ShowNotice(const bh::Notice& N, float Now);
	void Violation(const std::string& Text);

	struct Shown
	{
		std::string Text;
		float Time = 0.f;
	};
	std::vector<Shown> RecentNotices; // the HUD drops a repeat of the same text within 3 s
	std::vector<float> NoticeWindow;  // times of the notices in the last 10 s
	std::vector<Shown> Toasts;        // on screen (4 at most, 3.5 s; danger 5 s)

	std::map<bh::EntityId, bh::Archetype> TypeOf;
	std::map<bh::EntityId, Tracker> Trackers;
	float NextSample = 0.f;
	float LastSampleTime = 0.f;
	float NextSaveCheck = 0.f;
	float LastCombat = -1.f;
	float LastCombatSecond = -1.f;
	int PrevRes[bh::NumResources] = {};
	int GatheredThisSample[bh::NumResources] = {};
	int Samples = 0;
};

} // namespace bht
