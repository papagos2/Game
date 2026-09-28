// Beaconhold simulation core - the world: entities, orders, economy, combat.
#pragma once

#include "BhData.h"
#include "BhMap.h"
#include "BhMath.h"
#include "BhPath.h"
#include "BhTypes.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace bh
{
enum class OrderType : uint8_t
{
	Idle,
	Move,
	AttackMove,
	Attack,
	Gather,
	Return,
	Build,
};

// What an entity is visibly doing (drives animation and UI text).
enum class Activity : uint8_t
{
	Idle,
	Walking,
	Attacking,
	Gathering,
	Building,
	Carrying, // walking with cargo
};

enum class BuffType : uint8_t
{
	None,
	Brace,
	Volley,
	Charge,
};

struct ProductionItem
{
	bool bResearch = false;
	Archetype Unit = Archetype::None;
	Research Tech = Research::None;
	float Elapsed = 0.f;
	float Total = 1.f;
	int PaidSunstone = 0;
	int PaidTimber = 0;
};

constexpr int MaxQueueLength = 5;

struct Entity
{
	EntityId Id = NoEntity;
	Archetype Type = Archetype::None;
	EntityKind Kind = EntityKind::Unit;
	Team Owner = Team::Neutral;
	bool bAlive = true;
	uint8_t Tag = 0; // mission-defined marker (e.g. objective target)

	Vec2 Pos;
	Vec2 PrevPos;
	float Facing = 0.f; // radians, sim XY plane
	float Radius = 0.3f;
	float Hp = 1.f;
	float MaxHp = 1.f;
	TileRect Rect; // footprint (buildings, resource nodes)

	// Orders & movement
	OrderType Order = OrderType::Idle;
	Vec2 OrderPoint;
	EntityId OrderTarget = NoEntity;
	EntityId EngageTarget = NoEntity; // auto-acquired target (idle / attack-move)
	float EngageTimer = 0.f;          // time spent chasing without landing a blow
	Vec2 LeashPoint;
	std::vector<Vec2> Path;
	size_t PathIndex = 0;
	Vec2 PathGoal;
	bool bHasPath = false;
	float RepathTimer = 0.f;
	float StuckTimer = 0.f;
	float BestWaypointDist = 1e9f;

	// Combat
	float AttackCooldown = 0.f;
	float WindupTimer = 0.f;
	EntityId WindupTarget = NoEntity;
	float ScanTimer = 0.f;
	float LastDamagedTime = -1000.f;
	EntityId LastAttacker = NoEntity;
	uint32_t AttackSerial = 0; // increments on every swing (presentation)
	int Kills = 0;

	// Worker
	Resource CarryType = Resource::None;
	int CarryAmount = 0;
	float GatherTimer = 0.f;
	float GatherFxTimer = 0.f;
	Resource GatherType = Resource::None; // what we were last gathering (to resume)
	Tile GatherTile;
	EntityId GatherNode = NoEntity;
	EntityId BuildTarget = NoEntity;
	bool bResumeGather = false;

	// Abilities
	float AbilityCooldown = 0.f;
	BuffType Buff = BuffType::None;
	float BuffTimer = 0.f;
	bool bChargeReady = false;
	float HealTimer = 0.f;

	// Buildings
	bool bConstructed = true;
	float BuildProgress = 1.f; // 0..1
	int Builders = 0;          // builders working this tick
	std::vector<ProductionItem> Queue;
	bool bHasRally = false;
	Vec2 RallyPoint;
	EntityId RallyNode = NoEntity;
	bool bRallyTree = false;
	Tile RallyTree;
	float IncomeAccum = 0.f;

	// Resource nodes
	int Amount = 0;
	int Miners = 0;

	// Presentation
	Activity Act = Activity::Idle;
	float SpawnTime = 0.f;

	bool IsUnit() const { return Kind == EntityKind::Unit; }
	bool IsBuilding() const { return Kind == EntityKind::Building; }
	bool IsResourceNode() const { return Kind == EntityKind::Resource; }
	float HpRatio() const { return MaxHp > 0.f ? Hp / MaxHp : 0.f; }
};

struct Projectile
{
	uint32_t Id = 0;
	Vec2 Pos;
	Vec2 PrevPos;
	Vec2 Start;
	EntityId Target = NoEntity;
	Vec2 TargetPos;
	float Speed = 10.f;
	float Damage = 0.f;
	float Splash = 0.f;
	float BuildingMult = 1.f;
	EntityId Source = NoEntity;
	Team Owner = Team::Neutral;
	Archetype SourceType = Archetype::None;
	float Travelled = 0.f;
	float TotalDist = 1.f;
	bool bAlive = true;
};

enum class EventType : uint8_t
{
	EntitySpawned,
	EntityDied,
	BuildingPlaced,
	BuildingCompleted,
	ConstructionCancelled,
	AttackStarted,
	ProjectileFired,
	Hit,
	Healed,
	GatherStrike,
	ResourceDelivered,
	TreeFelled,
	NodeDepleted,
	UnitTrained,
	ResearchCompleted,
	AbilityCast,
	UnderAttack,
	NotEnoughSunstone,
	NotEnoughTimber,
	SupplyBlocked,
	InvalidPlacement,
	RequirementMissing,
	QueueFull,
	CommandMove,
	CommandAttack,
	CommandGather,
	CommandBuild,
	WaveIncoming,
	WaveSpawned,
	ObjectiveCompleted,
	ObjectiveFailed,
	MissionWon,
	MissionLost,
	TutorialStep,
	Notice,
};

struct GameEvent
{
	EventType Type = EventType::Notice;
	EntityId A = NoEntity;
	EntityId B = NoEntity;
	Archetype Arch = Archetype::None;
	Team Owner = Team::Neutral;
	Vec2 Pos;
	float Value = 0.f;
	int Sub = 0;
};

struct TeamStats
{
	int UnitsTrained = 0;
	int UnitsLost = 0;
	int Kills = 0;
	int BuildingsBuilt = 0;
	int BuildingsLost = 0;
	int Gathered[NumResources] = {0, 0};
};

struct TeamState
{
	int Res[NumResources] = {0, 0}; // Sunstone, Timber. The Gloam uses Sunstone as "gloom".
	int SupplyUsed = 0;
	int SupplyCap = 0;
	bool Researched[NumResearch] = {};
	float HpMult = 1.f;
	float DamageMult = 1.f;
	float GatherMult = 1.f;
	float BuildTimeMult = 1.f;
	float BuildingHpMult = 1.f;
	bool bIgnoreSupply = false;
	// Recent "under attack" alarms: each fight raises one every 15 s, a raid elsewhere its own.
	float AlertTime[4] = {-1000.f, -1000.f, -1000.f, -1000.f};
	Vec2 AlertPos[4];
	TeamStats Stats;
};

enum class Availability : uint8_t
{
	Ok,
	Locked,
	NoSunstone,
	NoTimber,
	NoSupply,
	QueueFull,
	Done,
	InProgress,
	NotConstructed,
};

enum class PlaceResult : uint8_t
{
	Ok,
	OutOfBounds,
	Blocked,
	NeedsBeaconSite,
	Blight,
};

class World
{
public:
	static constexpr float TickSeconds = 0.05f;
	static constexpr int MaxEntities = 3000;
	// No unit is wider than this (a Bog Titan is 0.8): spatial queries look this far past their
	// range for unit centres. Checked by a test against the unit table.
	static constexpr float MaxUnitRadius = 1.f;

	World();

	void Reset(int MapWidth, int MapHeight, uint32_t Seed);

	// ------------------------------------------------------------------ Spawning
	EntityId SpawnUnit(Archetype Type, Team Owner, const Vec2& Pos);
	EntityId SpawnBuilding(Archetype Type, Team Owner, const Tile& TopLeft, bool bConstructed);
	EntityId SpawnResourceNode(Archetype Type, const Tile& TopLeft, int Amount);

	// ------------------------------------------------------------------ Queries
	Entity* Find(EntityId Id);
	const Entity* Find(EntityId Id) const;
	std::vector<Entity>& GetEntities() { return Entities; }
	const std::vector<Entity>& GetEntities() const { return Entities; }
	const std::vector<Projectile>& GetProjectiles() const { return Projectiles; }
	TeamState& GetTeam(Team T) { return Teams[TeamIndex(T)]; }
	const TeamState& GetTeam(Team T) const { return Teams[TeamIndex(T)]; }
	float GetTime() const { return Time; }
	Rng& GetRng() { return Random; }
	uint32_t GetRngState() const { return Random.GetState(); }
	GameMap& GetMap() { return Map; }
	const GameMap& GetMap() const { return Map; }

	int CountOwned(Team T, Archetype A, bool bIncludeUnfinished) const;
	int CountUnits(Team T, bool bCombatOnly) const;
	bool HasCompleted(Team T, Archetype A) const;
	EntityId FindNearestDropOff(Team T, const Vec2& Pos) const;
	EntityId FindNearestNode(const Vec2& Pos, float MaxDist, EntityId Exclude) const;
	EntityId FindNearestEnemy(Team T, const Vec2& Pos, float MaxDist, bool bUnitsOnly) const;
	void QueryRadius(const Vec2& P, float R, std::vector<EntityId>& Out) const;
	float EdgeDistance(const Entity& A, const Entity& B) const;
	static float EdgeDistanceToPoint(const Entity& A, const Vec2& P);
	bool IsCombatUnit(const Entity& E) const;

	// Effective stats (research + boons + buffs).
	float GetDamage(const Entity& E) const;
	float GetArmor(const Entity& E) const;
	float GetRange(const Entity& E) const;
	float GetSpeed(const Entity& E) const;
	float GetCooldown(const Entity& E) const;
	float GetAbilityCooldownMax(const Entity& E) const;

	// ------------------------------------------------------------------ Rules checks
	PlaceResult CanPlace(Archetype Type, const Tile& TopLeft) const;
	Availability CheckBuild(Team T, Archetype Type) const;
	Availability CheckTrain(const Entity& Building, Archetype Unit) const;
	Availability CheckResearch(const Entity& Building, Research Tech) const;
	Research NextResearchLevel(Team T, Research Line) const;
	bool IsResearchInProgress(Team T, Research Tech) const;

	// ------------------------------------------------------------------ Commands
	void CmdMove(const std::vector<EntityId>& Units, const Vec2& Point, bool bAttackMove);
	void CmdAttack(const std::vector<EntityId>& Units, EntityId Target);
	void CmdGatherNode(const std::vector<EntityId>& Workers, EntityId Node);
	void CmdGatherTree(const std::vector<EntityId>& Workers, const Tile& Tree);
	EntityId CmdBuild(const std::vector<EntityId>& Workers, Archetype Type, const Tile& TopLeft);
	void CmdHelpBuild(const std::vector<EntityId>& Workers, EntityId Building);
	void CmdReturnCargo(const std::vector<EntityId>& Workers);
	void CmdStop(const std::vector<EntityId>& Units);
	bool CmdTrain(EntityId Building, Archetype Unit);
	bool CmdResearch(EntityId Building, Research Tech);
	void CmdCancelQueueItem(EntityId Building, int Index);
	void CmdCancelConstruction(EntityId Building);
	void CmdSetRally(EntityId Building, const Vec2& Point, EntityId Node, const Tile* Tree);
	int CmdAbility(const std::vector<EntityId>& Units, Ability A);

	// Kills an entity immediately (mission scripting / tests).
	void Kill(EntityId Id, EntityId Killer);

	// ------------------------------------------------------------------ Simulation
	void Tick(float Dt);

	std::vector<GameEvent> Events;
	void Emit(const GameEvent& E) { Events.push_back(E); }
	void EmitSimple(EventType Type, Team Owner, const Vec2& Pos, EntityId A = NoEntity, Archetype Arch = Archetype::None, float Value = 0.f, int Sub = 0);

	bool CanAfford(Team T, int Sunstone, int Timber) const;
	void Spend(Team T, int Sunstone, int Timber);
	void Refund(Team T, int Sunstone, int Timber);
	void RecomputeSupply();

	// Global switch used when a mission ends (units freeze, nothing progresses).
	bool bFrozen = false;

	// Serialization support.
	uint32_t GetNextId() const { return NextId; }
	uint32_t GetNextProjectileId() const { return NextProjectileId; }
	void RestoreCounters(uint32_t InNextId, uint32_t InNextProjectileId, float InTime, uint32_t RngState);
	std::vector<Entity>& MutableEntities() { return Entities; }
	std::vector<Projectile>& MutableProjectiles() { return Projectiles; }
	void RebuildIndex();

private:
	// Tick stages (BhWorldTick.cpp)
	void RebuildSpatialHash();
	void UpdateUnit(Entity& E, float Dt);
	void UpdateBuilding(Entity& E, float Dt);
	void UpdateProjectiles(float Dt);
	void ResolveSeparation();
	void RemoveDead();

	// Behaviours
	void UpdateIdle(Entity& E, float Dt);
	void UpdateMove(Entity& E, float Dt, bool bAttackMove);
	void UpdateAttackOrder(Entity& E, float Dt);
	void UpdateGather(Entity& E, float Dt);
	void UpdateReturn(Entity& E, float Dt);
	void UpdateBuild(Entity& E, float Dt);
	bool EngageWith(Entity& E, EntityId TargetId, float Dt); // false if target lost
	void ResolveStrike(Entity& E);
	void TryHeal(Entity& E);
	EntityId ScanForTarget(const Entity& E, float Radius) const;
	EntityId FindEnemyInReach(const Entity& E) const; // closest enemy within attack range
	EntityId FindOutcropWithRoom(const Entity& Worker, EntityId Exclude, float MaxDist) const;
	void AssistAllies(const Entity& Victim, EntityId Attacker);

	// Movement
	bool PathTo(Entity& E, const TileRect& Goal, const Vec2& GoalPoint);
	bool PathToPoint(Entity& E, const Vec2& Point);
	bool PathToRect(Entity& E, const TileRect& Rect, int Expand);
	bool FollowPath(Entity& E, float Dt); // true when arrived
	bool StepTowards(Entity& E, const Vec2& Aim, float Dt); // straight step, sliding along walls
	void StopMoving(Entity& E);
	void FaceTowards(Entity& E, const Vec2& Point, float Dt);

	// Damage
	void ApplyDamage(Entity& Target, float Amount, EntityId SourceId, Team SourceTeam, float BuildingMult, Archetype SourceType);
	void Heal(Entity& Target, float Amount, EntityId SourceId);
	void DestroyEntity(Entity& E, EntityId Killer);

	// Workers
	void SetGatherNode(Entity& W, EntityId Node);
	void SetGatherTree(Entity& W, const Tile& Tree);
	void ResumeAfterWork(Entity& W);
	void FinishConstruction(Entity& B);

	// Production
	Tile FindSpawnTile(const Entity& Building, const Vec2& Towards) const;
	void CompleteProduction(Entity& Building, const ProductionItem& Item);
	void PushOutUnits(const TileRect& Rect);

	Entity* SpawnEntity(Archetype Type, Team Owner);

	GameMap Map;
	PathFinder Paths;
	std::vector<Entity> Entities;
	std::unordered_map<EntityId, size_t> Index;
	std::vector<Projectile> Projectiles;
	TeamState Teams[NumTeams];
	Rng Random;
	float Time = 0.f;
	uint32_t NextId = 1;
	uint32_t NextProjectileId = 1;
	int PathBudget = 0; // path searches allowed this tick

	// Spatial hash (cells of HashCell tiles)
	static constexpr int HashCell = 2;
	int HashW = 0;
	int HashH = 0;
	std::vector<std::vector<EntityId>> Hash;
};

const char* AvailabilityText(Availability A);
const char* OrderText(const Entity& E);

} // namespace bh
