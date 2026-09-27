// Beaconhold simulation core — shared enums and identifiers.
#pragma once

#include <cstdint>

namespace bh
{
using EntityId = uint32_t;
constexpr EntityId NoEntity = 0;

enum class Team : uint8_t
{
	Neutral = 0,
	Player = 1,
	Enemy = 2,
};
constexpr int NumTeams = 3;
inline int TeamIndex(Team T) { return static_cast<int>(T); }
inline bool AreEnemies(Team A, Team B) { return A != Team::Neutral && B != Team::Neutral && A != B; }

enum class EntityKind : uint8_t
{
	Unit,
	Building,
	Resource,
};

// Every kind of thing that can exist on the map as an entity.
enum class Archetype : uint8_t
{
	// Warden units
	Lamplighter,
	Shieldbearer,
	Ranger,
	StagRider,
	Sage,
	// Gloam units
	Gloomling,
	Thornback,
	Hexer,
	BogTitan,
	// Warden buildings
	Keep,
	Cottage,
	Storehouse,
	Barracks,
	Forge,
	StagLodge,
	Sanctum,
	Watchtower,
	Beacon,
	// Gloam buildings
	GloamHeart,
	Burrow,
	Hexroot,
	ThornSpire,
	// Neutral
	SunstoneNode,

	Count,
	None = 255,
};
constexpr int NumArchetypes = static_cast<int>(Archetype::Count);
inline int ArchIndex(Archetype A) { return static_cast<int>(A); }

enum class Resource : uint8_t
{
	Sunstone = 0,
	Timber = 1,
	Count = 2,
	None = 255,
};
constexpr int NumResources = 2;

enum class Research : uint8_t
{
	Blades1,
	Blades2,
	Plate1,
	Plate2,
	Fletching,
	LanternWisdom,
	Count,
	None = 255,
};
constexpr int NumResearch = static_cast<int>(Research::Count);

enum class Ability : uint8_t
{
	None,
	Brace,
	Volley,
	Charge,
	Sunburst,
	Count,
};
constexpr int NumAbilities = static_cast<int>(Ability::Count);

enum class Boon : uint8_t
{
	Hardy,       // +HP
	Keen,        // +damage
	Swift,       // faster gathering
	Stonewright, // faster construction, sturdier buildings
	Coffers,     // more starting resources
	Count,
};
constexpr int NumBoons = static_cast<int>(Boon::Count);
constexpr int MaxBoonRank = 3;

enum class Difficulty : uint8_t
{
	Easy = 0,
	Normal = 1,
	Hard = 2,
};

// Icons are painted procedurally (see BhPainter) and shared by UI and minimap.
enum class Icon : uint8_t
{
	None,
	Sunstone,
	Timber,
	Supply,
	Lamplighter,
	Shieldbearer,
	Ranger,
	StagRider,
	Sage,
	Gloomling,
	Thornback,
	Hexer,
	BogTitan,
	Keep,
	Cottage,
	Storehouse,
	Barracks,
	Forge,
	StagLodge,
	Sanctum,
	Watchtower,
	Beacon,
	GloamHeart,
	Burrow,
	Hexroot,
	ThornSpire,
	SunstoneNode,
	Tree,
	Build,
	Stop,
	Army,
	Worker,
	Home,
	Cancel,
	Confirm,
	Back,
	Pause,
	Play,
	Speed,
	Objectives,
	Settings,
	Star,
	StarEmpty,
	Lock,
	Trophy,
	Skull,
	Sword,
	Shield,
	Boot,
	Bow,
	Brace,
	Volley,
	Charge,
	Sunburst,
	Blades,
	Plate,
	Fletching,
	Wisdom,
	BoonHardy,
	BoonKeen,
	BoonSwift,
	BoonStonewright,
	BoonCoffers,
	Repair,
	Count,
};
constexpr int NumIcons = static_cast<int>(Icon::Count);

} // namespace bh
