// Beaconhold simulation core — visual models built from primitive shapes, and the art palette.
//
// Every unit, building, tree and prop is described as a list of primitive parts (cube,
// cylinder, sphere, cone) in the entity's local frame: X forward, Y right, Z up, units in
// tiles (1 tile = 100 UE units). Engine basic shapes are 1 tile in size and centred, so a
// part's Size is directly its scale. Colours are sRGB.
#pragma once

#include "BhMap.h"
#include "BhTypes.h"

#include <cstdint>
#include <vector>

namespace bh
{
enum class Shape : uint8_t
{
	Cube,
	Cylinder,
	Sphere,
	Cone,
	Plane,
};

enum class Paint : uint8_t
{
	Team,
	TeamDark,
	TeamLight,
	Accent,
	Fixed, // use PartDef::Color
};

enum class PartRole : uint8_t
{
	Static,
	Weapon,         // swings on attack / while working
	Shield,         // raised while bracing
	Glow,           // gentle pulse
	Flag,           // sways
	Spin,           // slow rotation
	Mount,          // gallop bob (stag)
	CarrySunstone,  // visible only when carrying Sunstone
	CarryTimber,    // visible only when carrying Timber
	Flame,          // flicker (beacons, forge)
};

struct Rgb
{
	uint8_t R = 255;
	uint8_t G = 255;
	uint8_t B = 255;
	constexpr Rgb() = default;
	constexpr Rgb(uint8_t InR, uint8_t InG, uint8_t InB) : R(InR), G(InG), B(InB) {}
	static constexpr Rgb Hex(uint32_t V) { return Rgb(static_cast<uint8_t>((V >> 16) & 0xFF), static_cast<uint8_t>((V >> 8) & 0xFF), static_cast<uint8_t>(V & 0xFF)); }
	bool operator==(const Rgb& O) const { return R == O.R && G == O.G && B == O.B; }
	bool operator!=(const Rgb& O) const { return !(*this == O); }
	uint32_t Key() const { return (static_cast<uint32_t>(R) << 16) | (static_cast<uint32_t>(G) << 8) | B; }
};

struct PartDef
{
	Shape S = Shape::Cube;
	float X = 0.f; // offset (tiles)
	float Y = 0.f;
	float Z = 0.f;
	float SX = 1.f; // size (tiles)
	float SY = 1.f;
	float SZ = 1.f;
	float Pitch = 0.f; // degrees
	float Yaw = 0.f;
	float Roll = 0.f;
	Paint P = Paint::Fixed;
	Rgb Color;
	PartRole Role = PartRole::Static;
	bool bUnlit = false; // emissive-looking
};

struct ModelDef
{
	std::vector<PartDef> Parts;
	float Height = 1.f;        // top of the model (tiles, before Scale), for health bars
	float ShadowRadius = 0.4f; // blob shadow under units
	float Scale = 1.f;         // visual scale (units are drawn larger than their footprint for readability)
};

namespace palette
{
// Team colours
constexpr Rgb WardenBlue = Rgb::Hex(0x3B6FD8);
constexpr Rgb WardenBlueDark = Rgb::Hex(0x28498F);
constexpr Rgb WardenBlueLight = Rgb::Hex(0x86ABF0);
constexpr Rgb WardenGold = Rgb::Hex(0xE3B64E);
constexpr Rgb GloamViolet = Rgb::Hex(0x7A3BA6);
constexpr Rgb GloamVioletDark = Rgb::Hex(0x45205F);
constexpr Rgb GloamVioletLight = Rgb::Hex(0xB27AD6);
constexpr Rgb GloamGreen = Rgb::Hex(0x9BE35A);
// Materials
constexpr Rgb Stone = Rgb::Hex(0xE6DCC6);
constexpr Rgb StoneDark = Rgb::Hex(0xB3A68C);
constexpr Rgb StoneGrey = Rgb::Hex(0x8F8D95);
constexpr Rgb Wood = Rgb::Hex(0x8C5C35);
constexpr Rgb WoodDark = Rgb::Hex(0x5C3B22);
constexpr Rgb Skin = Rgb::Hex(0xF0C8A2);
constexpr Rgb Cloth = Rgb::Hex(0xF2EDDD);
constexpr Rgb Leather = Rgb::Hex(0x7A4E2E);
constexpr Rgb Metal = Rgb::Hex(0xB9C3CE);
constexpr Rgb MetalDark = Rgb::Hex(0x6E7884);
constexpr Rgb Bone = Rgb::Hex(0xEDE3CC);
constexpr Rgb Ranger = Rgb::Hex(0x5E7D3A);
constexpr Rgb StagBrown = Rgb::Hex(0x94643D);
constexpr Rgb Sunstone = Rgb::Hex(0xFFC83A);
constexpr Rgb SunstoneLight = Rgb::Hex(0xFFE08A);
constexpr Rgb SunstoneDeep = Rgb::Hex(0xE39A22);
constexpr Rgb Flame = Rgb::Hex(0xFFB547);
constexpr Rgb FlameCore = Rgb::Hex(0xFFF1B8);
constexpr Rgb LanternGlow = Rgb::Hex(0xFFD86B);
constexpr Rgb Bark = Rgb::Hex(0x6E4A2E);
constexpr Rgb GloamBark = Rgb::Hex(0x3C2A40);
constexpr Rgb GloamBarkLight = Rgb::Hex(0x584060);
constexpr Rgb Thorn = Rgb::Hex(0x56663A);
constexpr Rgb GloamSkin = Rgb::Hex(0x2E1842);
constexpr Rgb GloamCore = Rgb::Hex(0xC65CFF);
constexpr Rgb Moss = Rgb::Hex(0x3F4F34);
constexpr Rgb Void = Rgb::Hex(0x160B1E);
// Terrain
constexpr Rgb Grass = Rgb::Hex(0x7FB257);
constexpr Rgb GrassDark = Rgb::Hex(0x6FA04C);
constexpr Rgb GrassLight = Rgb::Hex(0x8FC064);
constexpr Rgb Meadow = Rgb::Hex(0x96C866);
constexpr Rgb Dirt = Rgb::Hex(0xB48C5E);
constexpr Rgb DirtDark = Rgb::Hex(0x9E7A50);
constexpr Rgb Sand = Rgb::Hex(0xE4D19A);
constexpr Rgb Water = Rgb::Hex(0x3F8FC8);
constexpr Rgb WaterDeep = Rgb::Hex(0x2F76AE);
constexpr Rgb Rock = Rgb::Hex(0x8C8A92);
constexpr Rgb RockLight = Rgb::Hex(0xA7A5AD);
constexpr Rgb Blight = Rgb::Hex(0x7A6B8C);
constexpr Rgb BlightDark = Rgb::Hex(0x6D5F80);
constexpr Rgb Pine = Rgb::Hex(0x2F6B45);
constexpr Rgb PineLight = Rgb::Hex(0x3D8054);
constexpr Rgb Oak = Rgb::Hex(0x5B9B3F);
constexpr Rgb OakLight = Rgb::Hex(0x74B24C);
constexpr Rgb DeadTree = Rgb::Hex(0x4D3A4F);
constexpr Rgb Skirt = Rgb::Hex(0x2C4A2E);
// UI
constexpr Rgb UiInk = Rgb::Hex(0x1B1612);
constexpr Rgb UiParchment = Rgb::Hex(0xF3E7C9);
constexpr Rgb UiPanel = Rgb::Hex(0x2A2233);
constexpr Rgb UiPanelLight = Rgb::Hex(0x3A3046);
constexpr Rgb UiGold = Rgb::Hex(0xE7BE5C);
constexpr Rgb UiRed = Rgb::Hex(0xE0533D);
constexpr Rgb UiGreen = Rgb::Hex(0x6CCB5F);
} // namespace palette

Rgb TeamColor(Team T, Paint P);
Rgb ResolveColor(const PartDef& Part, Team T);

const ModelDef& GetModel(Archetype A);
const ModelDef& GetTreeModel(TreeKind K);
const ModelDef& GetStumpModel();
const ModelDef& GetBeaconRuinModel();
const ModelDef& GetScaffoldModel(int Footprint);

// Small decorative props scattered over the terrain (rocks, flowers, grass, mushrooms).
struct PropInstance
{
	Shape S = Shape::Cube;
	float X = 0.f;
	float Y = 0.f;
	float Z = 0.f;
	float SX = 0.1f;
	float SY = 0.1f;
	float SZ = 0.1f;
	float Yaw = 0.f;
	float Pitch = 0.f;
	Rgb Color;
	bool bUnlit = false;
};
void GenerateProps(const GameMap& Map, std::vector<PropInstance>& Out);

// Stable per-tile placement of a tree (slight offset, rotation and size variation).
void TreePlacement(int X, int Y, float& OutX, float& OutY, float& OutYaw, float& OutScale);

struct TreeInstance
{
	TreeKind Kind = TreeKind::Pine;
	int TileX = -1; // -1 for decorative trees outside the playable map
	int TileY = -1;
	float X = 0.f;
	float Y = 0.f;
	float Yaw = 0.f;
	float Scale = 1.f;
};
// All trees to draw: harvestable trees on the map plus a decorative forest border around it.
void BuildTreeInstances(const GameMap& Map, std::vector<TreeInstance>& Out);

// The ground is drawn as cells smaller than the gameplay grid. Boundaries between walkable ground
// types are warped with noise so paths and meadows look organic; water and rock stay exact.
struct GroundCell
{
	float X = 0.f; // cell centre
	float Y = 0.f;
	float Size = 1.f;
	float Z = 0.f; // top height
	uint16_t ColorIndex = 0;
};
void BuildGround(const GameMap& Map, int Subdiv, std::vector<GroundCell>& OutCells, std::vector<Rgb>& OutColors);

// Terrain colour for a tile (with stable per-tile variation).
Rgb GroundColor(Ground G, int X, int Y);
// Top height of a ground tile (water sits lower).
float GroundHeight(Ground G);
// Minimap colour for a tile.
Rgb MinimapColor(const MapTile& T, int X, int Y);

} // namespace bh
