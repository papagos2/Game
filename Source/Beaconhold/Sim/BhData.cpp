// Beaconhold simulation core - static game data tables.
#include "BhData.h"

#include <array>
#include <cstddef>

namespace bh
{
namespace
{
std::array<ArchetypeDef, NumArchetypes> BuildArchetypeTable()
{
	std::array<ArchetypeDef, NumArchetypes> T{};

	// ---------------------------------------------------------------- Warden units
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Lamplighter)];
		D.Name = "Lamplighter";
		D.Description = "Gathers Sunstone and Timber, builds and repairs.";
		D.Kind = EntityKind::Unit;
		D.Faction = Team::Player;
		D.IconId = Icon::Lamplighter;
		D.MaxHp = 60.f;
		D.Radius = 0.30f;
		D.Sight = 4.f;
		D.CostSunstone = 50;
		D.SupplyCost = 1;
		D.BuildTime = 12.f;
		D.Speed = 2.7f;
		D.Damage = 5.f;
		D.Range = 0.3f;
		D.Cooldown = 1.5f;
		D.Windup = 0.35f;
		D.IsWorker = true;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Shieldbearer)];
		D.Name = "Shieldbearer";
		D.Description = "Sturdy melee guard. Brace to shrug off blows.";
		D.Kind = EntityKind::Unit;
		D.Faction = Team::Player;
		D.IconId = Icon::Shieldbearer;
		D.MaxHp = 230.f;
		D.Armor = 2.f;
		D.Radius = 0.36f;
		D.Sight = 6.5f;
		D.CostSunstone = 65;
		D.CostTimber = 15;
		D.SupplyCost = 2;
		D.BuildTime = 15.f;
		D.Speed = 2.35f;
		D.Damage = 11.f;
		D.Range = 0.35f;
		D.Cooldown = 1.1f;
		D.Windup = 0.35f;
		D.AbilityId = Ability::Brace;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Ranger)];
		D.Name = "Ranger";
		D.Description = "Longbow archer. Volley doubles attack speed.";
		D.Kind = EntityKind::Unit;
		D.Faction = Team::Player;
		D.IconId = Icon::Ranger;
		D.MaxHp = 110.f;
		D.Radius = 0.32f;
		D.Sight = 7.f;
		D.CostSunstone = 55;
		D.CostTimber = 35;
		D.SupplyCost = 2;
		D.BuildTime = 15.f;
		D.Speed = 2.5f;
		D.Damage = 13.f;
		D.Range = 5.5f;
		D.Cooldown = 1.35f;
		D.Windup = 0.3f;
		D.ProjectileSpeed = 14.f;
		D.BuildingDamageMult = 0.8f;
		D.AbilityId = Ability::Volley;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::StagRider)];
		D.Name = "Stag Rider";
		D.Description = "Swift cavalry on a great stag. Charge into the fray.";
		D.Kind = EntityKind::Unit;
		D.Faction = Team::Player;
		D.IconId = Icon::StagRider;
		D.MaxHp = 300.f;
		D.Armor = 1.f;
		D.Radius = 0.46f;
		D.Sight = 7.f;
		D.CostSunstone = 110;
		D.CostTimber = 60;
		D.SupplyCost = 3;
		D.BuildTime = 22.f;
		D.Speed = 3.8f;
		D.Damage = 24.f;
		D.Range = 0.45f;
		D.Cooldown = 1.4f;
		D.Windup = 0.3f;
		D.AbilityId = Ability::Charge;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Sage)];
		D.Name = "Lumen Sage";
		D.Description = "Heals nearby allies. Sunburst scorches the Gloam.";
		D.Kind = EntityKind::Unit;
		D.Faction = Team::Player;
		D.IconId = Icon::Sage;
		D.MaxHp = 120.f;
		D.Radius = 0.32f;
		D.Sight = 7.f;
		D.CostSunstone = 80;
		D.CostTimber = 60;
		D.SupplyCost = 2;
		D.BuildTime = 20.f;
		D.Speed = 2.3f;
		D.Damage = 8.f;
		D.Range = 4.5f;
		D.Cooldown = 1.6f;
		D.Windup = 0.35f;
		D.ProjectileSpeed = 11.f;
		D.AbilityId = Ability::Sunburst;
		D.HealAmount = 12.f;
		D.HealCooldown = 1.4f;
		D.HealRange = 5.f;
	}

	// ---------------------------------------------------------------- Gloam units
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Gloomling)];
		D.Name = "Gloomling";
		D.Description = "Quick, spiteful shade. Dangerous in packs.";
		D.Kind = EntityKind::Unit;
		D.Faction = Team::Enemy;
		D.IconId = Icon::Gloomling;
		D.MaxHp = 80.f;
		D.Radius = 0.30f;
		D.Sight = 7.f;
		D.CostSunstone = 40;
		D.BuildTime = 8.f;
		D.Speed = 3.1f;
		D.Damage = 8.f;
		D.Range = 0.3f;
		D.Cooldown = 1.0f;
		D.Windup = 0.25f;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Thornback)];
		D.Name = "Thornback";
		D.Description = "Bramble-armored brute that batters walls.";
		D.Kind = EntityKind::Unit;
		D.Faction = Team::Enemy;
		D.IconId = Icon::Thornback;
		D.MaxHp = 320.f;
		D.Armor = 3.f;
		D.Radius = 0.48f;
		D.Sight = 7.f;
		D.CostSunstone = 110;
		D.BuildTime = 16.f;
		D.Speed = 2.1f;
		D.Damage = 16.f;
		D.Range = 0.4f;
		D.Cooldown = 1.6f;
		D.Windup = 0.45f;
		D.BuildingDamageMult = 1.3f;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Hexer)];
		D.Name = "Hexer";
		D.Description = "Bog witch that hurls cursed bolts from afar.";
		D.Kind = EntityKind::Unit;
		D.Faction = Team::Enemy;
		D.IconId = Icon::Hexer;
		D.MaxHp = 100.f;
		D.Radius = 0.32f;
		D.Sight = 7.5f;
		D.CostSunstone = 80;
		D.BuildTime = 14.f;
		D.Speed = 2.3f;
		D.Damage = 15.f;
		D.Range = 5.f;
		D.Cooldown = 1.8f;
		D.Windup = 0.4f;
		D.ProjectileSpeed = 9.f;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::BogTitan)];
		D.Name = "Bog Titan";
		D.Description = "A walking mire. Its blows crush everything nearby.";
		D.Kind = EntityKind::Unit;
		D.Faction = Team::Enemy;
		D.IconId = Icon::BogTitan;
		D.MaxHp = 1400.f;
		D.Armor = 4.f;
		D.Radius = 0.8f;
		D.Sight = 8.f;
		D.CostSunstone = 450;
		D.BuildTime = 40.f;
		D.Speed = 1.6f;
		D.Damage = 42.f;
		D.Range = 0.6f;
		D.Cooldown = 2.4f;
		D.Windup = 0.6f;
		D.Splash = 1.6f;
		D.BuildingDamageMult = 1.6f;
	}

	// ---------------------------------------------------------------- Warden buildings
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Keep)];
		D.Name = "Beacon Keep";
		D.Description = "Heart of your settlement. Trains Lamplighters; accepts resources.";
		D.Kind = EntityKind::Building;
		D.Faction = Team::Player;
		D.IconId = Icon::Keep;
		D.MaxHp = 2000.f;
		D.Armor = 5.f;
		D.Footprint = 4;
		D.Sight = 8.f;
		D.CostSunstone = 400;
		D.CostTimber = 250;
		D.SupplyProvided = 10;
		D.BuildTime = 90.f;
		D.IsDropOff = true;
		D.Trains[0] = Archetype::Lamplighter;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Cottage)];
		D.Name = "Cottage";
		D.Description = "Houses your people. +8 supply.";
		D.Kind = EntityKind::Building;
		D.Faction = Team::Player;
		D.IconId = Icon::Cottage;
		D.MaxHp = 400.f;
		D.Armor = 1.f;
		D.Footprint = 2;
		D.CostSunstone = 30;
		D.CostTimber = 60;
		D.SupplyProvided = 8;
		D.BuildTime = 18.f;
		D.Buildable = true;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Storehouse)];
		D.Name = "Storehouse";
		D.Description = "Outpost where Lamplighters can drop off resources.";
		D.Kind = EntityKind::Building;
		D.Faction = Team::Player;
		D.IconId = Icon::Storehouse;
		D.MaxHp = 500.f;
		D.Armor = 1.f;
		D.Footprint = 2;
		D.CostSunstone = 50;
		D.CostTimber = 80;
		D.BuildTime = 20.f;
		D.IsDropOff = true;
		D.Buildable = true;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Barracks)];
		D.Name = "Muster Hall";
		D.Description = "Trains Shieldbearers and Rangers.";
		D.Kind = EntityKind::Building;
		D.Faction = Team::Player;
		D.IconId = Icon::Barracks;
		D.MaxHp = 900.f;
		D.Armor = 2.f;
		D.Footprint = 3;
		D.CostSunstone = 120;
		D.CostTimber = 100;
		D.BuildTime = 30.f;
		D.Buildable = true;
		D.Trains[0] = Archetype::Shieldbearer;
		D.Trains[1] = Archetype::Ranger;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Forge)];
		D.Name = "Forge";
		D.Description = "Researches weapon, armor and bow upgrades.";
		D.Kind = EntityKind::Building;
		D.Faction = Team::Player;
		D.IconId = Icon::Forge;
		D.MaxHp = 700.f;
		D.Armor = 2.f;
		D.Footprint = 3;
		D.CostSunstone = 120;
		D.CostTimber = 140;
		D.BuildTime = 30.f;
		D.Buildable = true;
		D.Requires = Archetype::Barracks;
		D.Researches[0] = Research::Blades1;
		D.Researches[1] = Research::Plate1;
		D.Researches[2] = Research::Fletching;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::StagLodge)];
		D.Name = "Stag Lodge";
		D.Description = "Trains Stag Riders.";
		D.Kind = EntityKind::Building;
		D.Faction = Team::Player;
		D.IconId = Icon::StagLodge;
		D.MaxHp = 900.f;
		D.Armor = 2.f;
		D.Footprint = 3;
		D.CostSunstone = 150;
		D.CostTimber = 150;
		D.BuildTime = 35.f;
		D.Buildable = true;
		D.Requires = Archetype::Forge;
		D.Trains[0] = Archetype::StagRider;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Sanctum)];
		D.Name = "Sanctum";
		D.Description = "Trains Lumen Sages. Researches Lantern Wisdom.";
		D.Kind = EntityKind::Building;
		D.Faction = Team::Player;
		D.IconId = Icon::Sanctum;
		D.MaxHp = 700.f;
		D.Armor = 1.f;
		D.Footprint = 3;
		D.CostSunstone = 140;
		D.CostTimber = 120;
		D.BuildTime = 35.f;
		D.Buildable = true;
		D.Requires = Archetype::Barracks;
		D.Trains[0] = Archetype::Sage;
		D.Researches[0] = Research::LanternWisdom;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Watchtower)];
		D.Name = "Watchtower";
		D.Description = "Shoots at nearby Gloam.";
		D.Kind = EntityKind::Building;
		D.Faction = Team::Player;
		D.IconId = Icon::Watchtower;
		D.MaxHp = 600.f;
		D.Armor = 3.f;
		D.Footprint = 2;
		D.Sight = 7.f;
		D.CostSunstone = 80;
		D.CostTimber = 100;
		D.BuildTime = 25.f;
		D.Damage = 15.f;
		D.Range = 7.f;
		D.Cooldown = 1.4f;
		D.Windup = 0.2f;
		D.ProjectileSpeed = 16.f;
		D.IsTower = true;
		D.Buildable = true;
		D.Requires = Archetype::Barracks;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Beacon)];
		D.Name = "Beacon";
		D.Description = "Relight an ancient Beacon site. Heals nearby allies and yields Sunstone.";
		D.Kind = EntityKind::Building;
		D.Faction = Team::Player;
		D.IconId = Icon::Beacon;
		D.MaxHp = 900.f;
		D.Armor = 3.f;
		D.Footprint = 2;
		D.Sight = 7.f;
		D.CostSunstone = 60;
		D.CostTimber = 140;
		D.BuildTime = 30.f;
		D.NeedsBeaconSite = true;
		D.Buildable = true;
		D.HealAmount = 4.f;
		D.HealCooldown = 1.f;
		D.HealRange = 5.f;
		D.IncomePerSecond = 0.4f;
	}

	// ---------------------------------------------------------------- Gloam buildings
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::GloamHeart)];
		D.Name = "Gloam Heart";
		D.Description = "The throbbing core of the Gloam. Destroy it to free the vale.";
		D.Kind = EntityKind::Building;
		D.Faction = Team::Enemy;
		D.IconId = Icon::GloamHeart;
		D.MaxHp = 2800.f;
		D.Armor = 5.f;
		D.Footprint = 4;
		D.Sight = 8.f;
		D.BuildTime = 90.f;
		D.Trains[0] = Archetype::Gloomling;
		D.Trains[1] = Archetype::BogTitan;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Burrow)];
		D.Name = "Burrow";
		D.Description = "A thorny warren that spews Gloam warriors.";
		D.Kind = EntityKind::Building;
		D.Faction = Team::Enemy;
		D.IconId = Icon::Burrow;
		D.MaxHp = 900.f;
		D.Armor = 2.f;
		D.Footprint = 3;
		D.BuildTime = 40.f;
		D.Trains[0] = Archetype::Gloomling;
		D.Trains[1] = Archetype::Thornback;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::Hexroot)];
		D.Name = "Hexroot";
		D.Description = "A twisted root-shrine where Hexers gather.";
		D.Kind = EntityKind::Building;
		D.Faction = Team::Enemy;
		D.IconId = Icon::Hexroot;
		D.MaxHp = 750.f;
		D.Armor = 1.f;
		D.Footprint = 3;
		D.BuildTime = 40.f;
		D.Trains[0] = Archetype::Hexer;
	}
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::ThornSpire)];
		D.Name = "Thorn Spire";
		D.Description = "Flings barbs at anything that comes close.";
		D.Kind = EntityKind::Building;
		D.Faction = Team::Enemy;
		D.IconId = Icon::ThornSpire;
		D.MaxHp = 700.f;
		D.Armor = 3.f;
		D.Footprint = 2;
		D.Sight = 7.f;
		D.BuildTime = 30.f;
		D.Damage = 17.f;
		D.Range = 6.5f;
		D.Cooldown = 1.6f;
		D.Windup = 0.25f;
		D.ProjectileSpeed = 12.f;
		D.IsTower = true;
	}

	// ---------------------------------------------------------------- Neutral
	{
		ArchetypeDef& D = T[ArchIndex(Archetype::SunstoneNode)];
		D.Name = "Sunstone Outcrop";
		D.Description = "Golden crystal. Lamplighters mine it for Sunstone.";
		D.Kind = EntityKind::Resource;
		D.Faction = Team::Neutral;
		D.IconId = Icon::SunstoneNode;
		D.MaxHp = 1.f;
		D.Footprint = 2;
		D.ResourceAmount = 1500;
	}
	return T;
}

std::array<ResearchDef, NumResearch> BuildResearchTable()
{
	std::array<ResearchDef, NumResearch> T{};
	{
		ResearchDef& R = T[static_cast<int>(Research::Blades1)];
		R.Name = "Tempered Blades";
		R.Description = "+2 damage for all Warden soldiers and towers.";
		R.IconId = Icon::Blades;
		R.CostSunstone = 100;
		R.CostTimber = 80;
		R.Time = 30.f;
		R.ResearchedAt = Archetype::Forge;
		R.NextLevel = Research::Blades2;
	}
	{
		ResearchDef& R = T[static_cast<int>(Research::Blades2)];
		R.Name = "Tempered Blades II";
		R.Description = "Another +2 damage for all Warden soldiers and towers.";
		R.IconId = Icon::Blades;
		R.CostSunstone = 175;
		R.CostTimber = 150;
		R.Time = 40.f;
		R.ResearchedAt = Archetype::Forge;
		R.Requires = Research::Blades1;
	}
	{
		ResearchDef& R = T[static_cast<int>(Research::Plate1)];
		R.Name = "Warden Plate";
		R.Description = "+1 armor for all Warden soldiers.";
		R.IconId = Icon::Plate;
		R.CostSunstone = 100;
		R.CostTimber = 100;
		R.Time = 30.f;
		R.ResearchedAt = Archetype::Forge;
		R.NextLevel = Research::Plate2;
	}
	{
		ResearchDef& R = T[static_cast<int>(Research::Plate2)];
		R.Name = "Warden Plate II";
		R.Description = "Another +1 armor for all Warden soldiers.";
		R.IconId = Icon::Plate;
		R.CostSunstone = 175;
		R.CostTimber = 175;
		R.Time = 40.f;
		R.ResearchedAt = Archetype::Forge;
		R.Requires = Research::Plate1;
	}
	{
		ResearchDef& R = T[static_cast<int>(Research::Fletching)];
		R.Name = "Fletching";
		R.Description = "+1 range for Rangers and Watchtowers.";
		R.IconId = Icon::Fletching;
		R.CostSunstone = 125;
		R.CostTimber = 100;
		R.Time = 35.f;
		R.ResearchedAt = Archetype::Forge;
	}
	{
		ResearchDef& R = T[static_cast<int>(Research::LanternWisdom)];
		R.Name = "Lantern Wisdom";
		R.Description = "Abilities recharge 30% faster. Sages heal 50% more.";
		R.IconId = Icon::Wisdom;
		R.CostSunstone = 150;
		R.CostTimber = 150;
		R.Time = 40.f;
		R.ResearchedAt = Archetype::Sanctum;
	}
	return T;
}

std::array<AbilityDef, NumAbilities> BuildAbilityTable()
{
	std::array<AbilityDef, NumAbilities> T{};
	{
		AbilityDef& A = T[static_cast<int>(Ability::Brace)];
		A.Name = "Brace";
		A.Description = "Take 50% less damage for 6 seconds.";
		A.IconId = Icon::Brace;
		A.Cooldown = 20.f;
		A.Duration = 6.f;
		A.Amount = 0.5f;
	}
	{
		AbilityDef& A = T[static_cast<int>(Ability::Volley)];
		A.Name = "Volley";
		A.Description = "Shoot twice as fast for 5 seconds.";
		A.IconId = Icon::Volley;
		A.Cooldown = 18.f;
		A.Duration = 5.f;
		A.Amount = 0.5f;
	}
	{
		AbilityDef& A = T[static_cast<int>(Ability::Charge)];
		A.Name = "Charge";
		A.Description = "+60% speed for 5 seconds; the next hit deals double damage.";
		A.IconId = Icon::Charge;
		A.Cooldown = 16.f;
		A.Duration = 5.f;
		A.Amount = 1.6f;
	}
	{
		AbilityDef& A = T[static_cast<int>(Ability::Sunburst)];
		A.Name = "Sunburst";
		A.Description = "Burst of light: 55 damage to nearby Gloam, heals nearby allies for 45.";
		A.IconId = Icon::Sunburst;
		A.Cooldown = 22.f;
		A.Radius = 3.5f;
		A.Amount = 55.f;
		A.HealAmount = 45.f;
	}
	return T;
}

std::array<BoonDef, NumBoons> BuildBoonTable()
{
	std::array<BoonDef, NumBoons> T{};
	T[static_cast<int>(Boon::Hardy)] = {"Hardy Folk", "+8% health for all units per rank.", Icon::BoonHardy, 0.08f};
	T[static_cast<int>(Boon::Keen)] = {"Keen Steel", "+8% damage for all units and towers per rank.", Icon::BoonKeen, 0.08f};
	T[static_cast<int>(Boon::Swift)] = {"Swift Hands", "+12% gathering speed per rank.", Icon::BoonSwift, 0.12f};
	T[static_cast<int>(Boon::Stonewright)] = {"Stonewright", "-12% build time and +10% building health per rank.", Icon::BoonStonewright, 0.12f};
	T[static_cast<int>(Boon::Coffers)] = {"Deep Coffers", "+75 starting Sunstone and Timber per rank.", Icon::BoonCoffers, 75.f};
	return T;
}
} // namespace

const ArchetypeDef& GetDef(Archetype A)
{
	static const std::array<ArchetypeDef, NumArchetypes> Table = BuildArchetypeTable();
	const int Index = ArchIndex(A);
	return Table[static_cast<size_t>(Index >= 0 && Index < NumArchetypes ? Index : 0)];
}

const ResearchDef& GetResearchDef(Research R)
{
	static const std::array<ResearchDef, NumResearch> Table = BuildResearchTable();
	const int Index = static_cast<int>(R);
	return Table[static_cast<size_t>(Index >= 0 && Index < NumResearch ? Index : 0)];
}

const AbilityDef& GetAbilityDef(Ability A)
{
	static const std::array<AbilityDef, NumAbilities> Table = BuildAbilityTable();
	const int Index = static_cast<int>(A);
	return Table[static_cast<size_t>(Index >= 0 && Index < NumAbilities ? Index : 0)];
}

const BoonDef& GetBoonDef(Boon B)
{
	static const std::array<BoonDef, NumBoons> Table = BuildBoonTable();
	const int Index = static_cast<int>(B);
	return Table[static_cast<size_t>(Index >= 0 && Index < NumBoons ? Index : 0)];
}

const char* ArchetypeKey(Archetype A)
{
	switch (A)
	{
	case Archetype::Lamplighter: return "Lamplighter";
	case Archetype::Shieldbearer: return "Shieldbearer";
	case Archetype::Ranger: return "Ranger";
	case Archetype::StagRider: return "StagRider";
	case Archetype::Sage: return "Sage";
	case Archetype::Gloomling: return "Gloomling";
	case Archetype::Thornback: return "Thornback";
	case Archetype::Hexer: return "Hexer";
	case Archetype::BogTitan: return "BogTitan";
	case Archetype::Keep: return "Keep";
	case Archetype::Cottage: return "Cottage";
	case Archetype::Storehouse: return "Storehouse";
	case Archetype::Barracks: return "Barracks";
	case Archetype::Forge: return "Forge";
	case Archetype::StagLodge: return "StagLodge";
	case Archetype::Sanctum: return "Sanctum";
	case Archetype::Watchtower: return "Watchtower";
	case Archetype::Beacon: return "Beacon";
	case Archetype::GloamHeart: return "GloamHeart";
	case Archetype::Burrow: return "Burrow";
	case Archetype::Hexroot: return "Hexroot";
	case Archetype::ThornSpire: return "ThornSpire";
	case Archetype::SunstoneNode: return "SunstoneNode";
	case Archetype::Count:
	case Archetype::None:
		break;
	}
	return "None";
}

const char* ResearchKey(Research R)
{
	switch (R)
	{
	case Research::Blades1: return "Blades1";
	case Research::Blades2: return "Blades2";
	case Research::Plate1: return "Plate1";
	case Research::Plate2: return "Plate2";
	case Research::Fletching: return "Fletching";
	case Research::LanternWisdom: return "LanternWisdom";
	case Research::Count:
	case Research::None:
		break;
	}
	return "None";
}

const char* AbilityKey(Ability A)
{
	switch (A)
	{
	case Ability::Brace: return "Brace";
	case Ability::Volley: return "Volley";
	case Ability::Charge: return "Charge";
	case Ability::Sunburst: return "Sunburst";
	case Ability::None:
	case Ability::Count:
		break;
	}
	return "None";
}

const char* GetResourceName(Resource R)
{
	switch (R)
	{
	case Resource::Sunstone:
		return "Sunstone";
	case Resource::Timber:
		return "Timber";
	case Resource::Count:
	case Resource::None:
		break;
	}
	return "";
}

} // namespace bh
