// Beaconhold simulation core - static game data (stats, costs, tech tree).
#pragma once

#include "BhTypes.h"

namespace bh
{
constexpr int MaxTrainOptions = 4;
constexpr int MaxResearchOptions = 4;
constexpr int SupplyHardCap = 100;

struct ArchetypeDef
{
	const char* Name = "";
	const char* Description = "";
	EntityKind Kind = EntityKind::Unit;
	Team Faction = Team::Neutral;
	Icon IconId = Icon::None;

	float MaxHp = 100.f;
	float Armor = 0.f;
	float Radius = 0.35f; // units: collision radius (tiles)
	int Footprint = 0;    // buildings & resource nodes: square side in tiles
	float Sight = 6.f;    // auto-acquire range

	int CostSunstone = 0; // Gloam units: cost in gloom
	int CostTimber = 0;
	int SupplyCost = 0;
	int SupplyProvided = 0;
	float BuildTime = 10.f; // seconds to train or construct

	float Speed = 0.f;           // tiles per second
	float Damage = 0.f;          // per hit
	float Range = 0.f;           // edge-to-edge attack range (tiles)
	float Cooldown = 1.f;        // seconds between attacks
	float Windup = 0.3f;         // seconds from swing start to impact / launch
	float ProjectileSpeed = 0.f; // 0 = melee (instant impact)
	float Splash = 0.f;          // splash radius around the target
	float BuildingDamageMult = 1.f;

	bool IsWorker = false;
	bool IsDropOff = false;
	bool IsTower = false;
	bool NeedsBeaconSite = false;
	bool Buildable = false; // can be placed by Lamplighters

	Ability AbilityId = Ability::None;
	float HealAmount = 0.f; // Sage heal per cast / Beacon aura per pulse
	float HealCooldown = 0.f;
	float HealRange = 0.f;
	float IncomePerSecond = 0.f; // Beacon passive Sunstone income

	Archetype Trains[MaxTrainOptions] = {Archetype::None, Archetype::None, Archetype::None, Archetype::None};
	Research Researches[MaxResearchOptions] = {Research::None, Research::None, Research::None, Research::None};
	Archetype Requires = Archetype::None;
	int ResourceAmount = 0;
};

struct ResearchDef
{
	const char* Name = "";
	const char* Description = "";
	Icon IconId = Icon::None;
	int CostSunstone = 0;
	int CostTimber = 0;
	float Time = 30.f;
	Archetype ResearchedAt = Archetype::None;
	Research Requires = Research::None;
	Research NextLevel = Research::None;
};

struct AbilityDef
{
	const char* Name = "";
	const char* Description = "";
	Icon IconId = Icon::None;
	float Cooldown = 20.f;
	float Duration = 0.f;
	float Radius = 0.f;
	float Amount = 0.f;
	float HealAmount = 0.f;
};

struct BoonDef
{
	const char* Name = "";
	const char* Description = ""; // per rank
	Icon IconId = Icon::None;
	float PerRank = 0.f;
};

// How strongly the Gloam commander favours a unit, when a building can train more than one, given
// the Warden army it faces: Base + the sum over Warden soldier types of (that type's share of
// the army's supply) x (the weight against it). Counters: Thornbacks break shield walls and
// cavalry, Gloomlings overrun archers and Sages.
struct GloamPickWeights
{
	float Base = 1.f;
	float VsShieldbearer = 0.f;
	float VsRanger = 0.f;
	float VsStagRider = 0.f;
	float VsSage = 0.f;
};

const ArchetypeDef& GetDef(Archetype A);
const GloamPickWeights& GetGloamPickWeights(Archetype GloamUnit);
const ResearchDef& GetResearchDef(Research R);
const AbilityDef& GetAbilityDef(Ability A);
const BoonDef& GetBoonDef(Boon B);
const char* GetResourceName(Resource R);

// Stable identifiers (enum names) used by tutorial highlights, save data and logs.
const char* ArchetypeKey(Archetype A);
const char* ResearchKey(Research R);
const char* AbilityKey(Ability A);

inline bool IsUnit(Archetype A) { return A != Archetype::None && A < Archetype::Count && GetDef(A).Kind == EntityKind::Unit; }
inline bool IsBuilding(Archetype A) { return A != Archetype::None && A < Archetype::Count && GetDef(A).Kind == EntityKind::Building; }

// Gathering tuning.
struct GatherTuning
{
	static constexpr int SunstonePerTrip = 8;
	static constexpr float MineTime = 2.6f;
	static constexpr int TimberPerTrip = 10;
	static constexpr float ChopTime = 3.4f;
	static constexpr int WoodPerTree = 40;
	static constexpr int MaxMinersPerNode = 6;
	static constexpr float RedirectRadius = 12.f; // a full outcrop sends workers to one this close
	static constexpr float GatherReach = 0.45f; // edge distance to start gathering
};

// Buildings the Lamplighter can place, in build-menu order.
inline constexpr Archetype BuildableStructures[] = {
	Archetype::Cottage, Archetype::Barracks, Archetype::Storehouse, Archetype::Watchtower,
	Archetype::Forge,   Archetype::Sanctum,  Archetype::StagLodge,  Archetype::Beacon,
};
inline constexpr int BuildableStructureCount = static_cast<int>(sizeof(BuildableStructures) / sizeof(BuildableStructures[0]));

} // namespace bh
