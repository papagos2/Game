// Beaconhold simulation core - mission definitions and mission runtime (objectives, waves, tutorial).
#pragma once

#include "BhWorld.h"

#include <string>
#include <vector>

namespace bh
{
enum class ObjectiveKind : uint8_t
{
	DestroyTagged, // destroy every enemy entity tagged by the mission
	BuildCount,    // own N completed buildings of a type
	TrainCount,    // have N units of a type alive
	Survive,       // hold out until the timer runs out
	GatherAmount,  // gather N of a resource
};

struct ObjectiveDef
{
	ObjectiveKind Kind = ObjectiveKind::DestroyTagged;
	const char* Text = "";
	Archetype Arch = Archetype::None;
	int Count = 0;
	float Seconds = 0.f;
	Resource Res = Resource::None;
	bool bOptional = false;
};

struct WaveUnit
{
	Archetype Arch = Archetype::Gloomling;
	int Count = 1;
};

struct WaveDef
{
	float Time = 0.f;
	int SpawnPoint = 1;
	std::vector<WaveUnit> Units;
	const char* Announce = "";
};

struct AIConfig
{
	bool bEnabled = false;
	int StartGloom = 0;
	float Income = 0.f;       // gloom per second at the start
	float IncomeGrowth = 0.f; // added per minute
	float MaxIncome = 0.f;
	float FirstAttack = 240.f;
	float AttackInterval = 120.f;
	int FirstWave = 4;
	int WaveGrowth = 2;
	int MaxWave = 20;
	int HomeGuard = 4;
	int ArmyCap = 40;
	float TitanAfter = -1.f; // seconds; negative disables Bog Titans
	float EliteAfter = 150.f; // seconds before buildings train anything but their first unit
};

enum class TutorialCond : uint8_t
{
	Continue,      // player taps "Continue"
	CameraMoved,   // player panned the camera
	SelectArch,    // selection contains Arch
	GatherRes,     // Count workers gathering Res
	HaveCount,     // own Count of Arch (alive, queued or under construction)
	CompleteCount, // own Count of Arch completed / alive
	CombatUnits,   // Count combat units alive
	SelectArmy,    // Count combat units selected
	ObjectiveDone, // objective #Count completed
};

enum class MarkerKind : uint8_t
{
	None,
	OwnArch,
	EnemyArch,
	SunstoneNode,
	Tree,
	BeaconSite,
};

struct TutorialStep
{
	const char* Text = "";
	TutorialCond Cond = TutorialCond::Continue;
	Archetype Arch = Archetype::None;
	int Count = 1;
	Resource Res = Resource::None;
	MarkerKind Marker = MarkerKind::None;
	Archetype MarkerArch = Archetype::None;
	const char* Highlight = ""; // UI action id, e.g. "train:Lamplighter", "build", "place:Cottage", "army"
};

// What the second star asks for (the first is victory, the third a Keep kept above half health).
enum class StarGoal : uint8_t
{
	ParTime,        // win within ParTime
	NoBuildingLost, // win without losing a building
};

struct MissionDef
{
	const char* Id = "";
	const char* Title = "";
	const char* Tagline = "";
	const char* Briefing = "";
	const char* VictoryText = "";
	const char* DefeatText = "";
	const char* const* Rows = nullptr;
	int RowCount = 0;
	int StartSunstone = 200;
	int StartTimber = 150;
	int NodeAmount = 1500;
	Archetype TaggedArch = Archetype::None;
	std::vector<ObjectiveDef> Objectives;
	std::vector<WaveDef> Waves;
	AIConfig AI;
	std::vector<TutorialStep> Tutorial;
	float ParTime = 900.f;
	StarGoal SecondStar = StarGoal::ParTime;
	uint32_t Seed = 1;
};

// Player-facing text of star Index (0..2) of a mission.
std::string StarGoalText(const MissionDef& M, int Index);

int GetMissionCount();
const MissionDef& GetMission(int Index);

struct DifficultyTuning
{
	float EnemyIncome = 1.f;
	float EnemyTiming = 1.f; // multiplies attack intervals / first attack
	float WaveSize = 1.f;
	float EnemyHp = 1.f;
	float EnemyDamage = 1.f;
};
DifficultyTuning GetDifficultyTuning(Difficulty D);
const char* DifficultyName(Difficulty D);

// Parses the ASCII layout of a mission into the world (terrain, trees, sites, entities).
bool LoadMissionMap(const MissionDef& M, World& W, std::string& OutError);

// Per-frame information about the player's view the tutorial needs.
struct PlayerContext
{
	const std::vector<EntityId>* Selection = nullptr;
	uint64_t SelectionLatch = 0; // archetypes selected since the step began
	int ArmyLatch = 0;           // most soldiers selected at once since the step began
	bool bCameraMoved = false;
	bool bContinuePressed = false;
};

enum class MissionOutcome : uint8_t
{
	InProgress,
	Won,
	Lost,
};

struct ObjectiveState
{
	bool bDone = false;
	int Progress = 0;
	int Target = 0;
};

class MissionRuntime
{
public:
	void Start(int InMissionIndex, Difficulty InDiff, bool bWithTutorial, World& W);
	void Tick(World& W, float Dt, const PlayerContext& Ctx);

	const MissionDef& Def() const { return GetMission(MissionIndex); }
	bool IsTutorialActive() const { return bTutorialActive; }
	const TutorialStep* CurrentTutorialStep() const;
	void SkipTutorial() { bTutorialActive = false; }

	// Seconds until the next scripted wave arrives (negative if none pending).
	float NextWaveIn() const;
	int GetStarCount() const;

	int MissionIndex = 0;
	Difficulty Diff = Difficulty::Normal;
	MissionOutcome Outcome = MissionOutcome::InProgress;
	float Elapsed = 0.f;
	std::vector<ObjectiveState> Objectives;
	size_t NextWave = 0;
	std::vector<uint8_t> WaveAnnounced;
	std::vector<EntityId> WaveUnits;
	float RetargetTimer = 0.f;
	bool bTutorialActive = false;
	int TutorialIndex = 0;
	float TutorialStepTime = 0.f;
	EntityId KeepId = NoEntity;
	float KeepMinRatio = 1.f;
	bool Stars[3] = {false, false, false};
	std::string EndReason;

private:
	void UpdateWaves(World& W, float Dt);
	void UpdateObjectives(World& W);
	void UpdateTutorial(World& W, float Dt, const PlayerContext& Ctx);
	bool TutorialConditionMet(const TutorialStep& Step, World& W, const PlayerContext& Ctx) const;
	void Finish(World& W, MissionOutcome Result, const std::string& Reason);
};

// Closest player-owned building (or unit if none) to Pos; used to aim enemy waves.
bool FindAttackTarget(const World& W, const Vec2& From, Team Attacker, Vec2& OutPos, bool bPreferBeacons);

} // namespace bh
