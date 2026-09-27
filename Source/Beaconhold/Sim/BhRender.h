// Beaconhold simulation core — render preparation for the engine layer.
//
// Everything the renderer needs that can be computed without the engine lives here, so it is
// testable and the Unreal layer stays thin:
//  * a colour palette texture with baked lighting. Every opaque mesh uses one shared unlit
//    material that samples it, which keeps draw calls low on phones and reproduces the lighting
//    of the approved art-direction preview (warm sun, sky/ground hemisphere, ACES tone curve);
//  * final vertex buffers (palette UVs) and soft blob shadows;
//  * procedural animation poses for units, buildings and their animated parts;
//  * the minimap overlay, tutorial markers and procedurally painted UI textures.
#pragma once

#include "BhMeshGen.h"
#include "BhPainter.h"
#include "BhSession.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace bh
{
// ---------------------------------------------------------------- Lighting & palette

// Art-direction lighting in world space (Unreal axes: X east, Y south, Z up).
struct LightRig
{
	Vec3 ToSun = Vec3(-0.3346f, 0.5795f, 0.7431f); // unit vector towards the sun
	Rgb SunColor = Rgb::Hex(0xFFF1D6);
	float SunIntensity = 2.6f;
	Rgb SkyColor = Rgb::Hex(0xCFE3FF);
	Rgb GroundColor = Rgb::Hex(0x5C4A36);
	float HemiIntensity = 1.1f;
	float Exposure = 1.05f;
};
const LightRig& GetLightRig();

constexpr int PaletteColumns = 512;
constexpr int PaletteSunLevels = 16;
constexpr int PaletteSkyLevels = 16;
constexpr int PaletteRows = PaletteSunLevels * PaletteSkyLevels;

// Shared palette texture: one column per base colour, one row per light level.
class ColorPalette
{
public:
	ColorPalette();

	// Column of a base colour, lit or self-coloured. Adds it on first use; when the palette is
	// full the closest existing colour is used instead.
	int Column(Rgb Color, bool bUnlit);
	int Count() const { return static_cast<int>(Keys.size()); }
	// Changes whenever a colour is added (the engine re-uploads the texture).
	uint32_t GetRevision() const { return Revision; }
	// PaletteColumns x PaletteRows, RGBA8 (sRGB), row-major, row 0 first.
	const std::vector<uint8_t>& GetPixels() const { return Pixels; }

	// Final display colour of a cell (sRGB).
	static Rgb Shade(Rgb Base, bool bUnlit, int Row);
	// Light row for a world-space normal.
	static int LightRow(const Vec3& WorldNormal);
	static float U(int Column) { return (static_cast<float>(Column) + 0.5f) / static_cast<float>(PaletteColumns); }
	static float V(int Row) { return (static_cast<float>(Row) + 0.5f) / static_cast<float>(PaletteRows); }

private:
	std::vector<uint32_t> Keys;
	std::unordered_map<uint32_t, int> Index;
	std::vector<uint8_t> Pixels;
	uint32_t Revision = 1;
};

// ---------------------------------------------------------------- Render meshes

struct RenderSection
{
	std::vector<Vec3> Positions; // Unreal units (1 tile = 100)
	std::vector<Vec3> Normals;
	std::vector<float> UVs; // 2 per vertex
	std::vector<uint32_t> Indices; // counter-clockwise seen from the front, right-handed maths

	bool Empty() const { return Indices.empty(); }
	int TriangleCount() const { return static_cast<int>(Indices.size() / 3); }
};

// Section 0 is opaque palette geometry, section 1 translucent blob shadows (UVs span the
// shadow texture). The engine gives each section its own shared material.
struct RenderMesh
{
	RenderSection Opaque;
	RenderSection Shadow;

	bool Empty() const { return Opaque.Empty() && Shadow.Empty(); }
	int TriangleCount() const { return Opaque.TriangleCount() + Shadow.TriangleCount(); }
	void Bounds(Vec3& OutMin, Vec3& OutMax) const;
};

constexpr float UnrealUnitsPerTile = 100.f;

// Appends a mesh as palette geometry (scaled to Unreal units). Lighting is baked as if the mesh
// were rotated by BakeYaw degrees in the world — the yaw it is normally seen at.
void AppendRenderMesh(const MeshData& In, float BakeYaw, ColorPalette& Palette, RenderMesh& Out);

// Soft shadow on the ground, in tiles. Directional shadows stretch away from the sun with Height.
void AppendBlobShadow(RenderMesh& Out, float X, float Y, float Radius, float Height, bool bDirectional);

// Complete meshes used by the engine layer.
constexpr float BuildingYaw = 90.f; // buildings face the camera (south)
constexpr float UnitBakeYaw = 90.f; // units are lit as if facing the camera

// Static parts of a unit, building or resource node, with its blob shadow.
void BuildEntityBodyMesh(Archetype A, Team T, ColorPalette& Palette, RenderMesh& Out);
// Animated parts are grouped by role; each group becomes one component turning about a pivot.
// The geometry is relative to the pivot, which is returned in model space (tiles). Returns false
// when the model has no part with that role.
bool BuildRoleMesh(const ModelDef& M, PartRole Role, Team T, float BakeYaw, ColorPalette& Palette, RenderMesh& Out, Vec3& OutPivot);
constexpr int NumPartRoles = static_cast<int>(PartRole::Flame) + 1;
void BuildScaffoldMesh(int Footprint, ColorPalette& Palette, RenderMesh& Out);
void BuildRuinMesh(ColorPalette& Palette, RenderMesh& Out);
// Terrain, trees (with shadows) and props for one chunk of the map, in world space.
void BuildTerrainChunk(const std::vector<GroundCell>& Cells, const std::vector<Rgb>& Colors, const TileRect& Region, int MapW, int MapH, ColorPalette& Palette, RenderMesh& Out);
void BuildTreeChunkMesh(const std::vector<TreeInstance>& Trees, const GameMap& Map, const TileRect& Region, ColorPalette& Palette, RenderMesh& Out);
void BuildPropChunkMesh(const std::vector<PropInstance>& Props, const TileRect& Region, ColorPalette& Palette, RenderMesh& Out);
// Flat forest floor under and around the map.
void BuildFloorMesh(int MapW, int MapH, float Margin, ColorPalette& Palette, RenderMesh& Out);

// Small effect meshes (projectiles and particles), centred, in tiles scaled to Unreal units.
enum class FxMesh : uint8_t
{
	Arrow,
	Bolt,       // magic bolt (Sage, Hexer)
	TowerBolt,  // watchtower bolt
	ThornDart,  // Thornback / Thorn Spire
	Spark,      // bright chip
	Dust,       // soft puff
	Chip,       // wood chip
	Shard,      // sunstone shard
	Gloom,      // violet wisp
	Heal,       // green mote
	Ember,      // fire ember
	Smoke,      // grey puff
	RallyFlag,
	Count
};
void BuildFxMesh(FxMesh Kind, ColorPalette& Palette, RenderMesh& Out);

// Flat unit quad (1x1 tile, centred) with UVs 0..1 for decals, in Unreal units.
void BuildDecalQuad(RenderMesh& Out);

// ---------------------------------------------------------------- Animation

struct AnimInput
{
	Archetype Type = Archetype::None;
	Team Owner = Team::Neutral;
	Activity Act = Activity::Idle;
	BuffType Buff = BuffType::None;
	Resource Carry = Resource::None;
	bool bMoving = false;
	bool bConstructed = true;
	float BuildProgress = 1.f;
	float Time = 0.f;       // presentation clock (seconds)
	float SwingAge = 99.f;  // seconds since the last attack swing began
	float HitAge = 99.f;    // seconds since last damaged
	float SpawnAge = 99.f;  // seconds since the visual appeared
	float DeathAge = -1.f;  // seconds since death, negative while alive
	float AmountRatio = 1.f; // resource nodes: remaining / initial
	uint32_t Seed = 0;       // phase offset
};

struct EntityPose
{
	Vec3 Offset;        // tiles, world axes
	float Pitch = 0.f;  // degrees, entity frame
	float Roll = 0.f;
	float ScaleXY = 1.f;
	float ScaleZ = 1.f;
	bool bVisible = true;
};

struct PartPose
{
	Vec3 Offset; // tiles, model frame
	float Pitch = 0.f;
	float Yaw = 0.f;
	float Roll = 0.f;
	float Scale = 1.f;
	bool bVisible = true;
};

EntityPose EvaluateEntityPose(const AnimInput& In);
PartPose EvaluateRolePose(PartRole Role, const AnimInput& In);
// How long a death animation lasts before the visual is removed.
float DeathDuration(Archetype A);
// Seconds a weapon swing takes.
constexpr float SwingDuration = 0.45f;

// ---------------------------------------------------------------- HUD helpers

// Where the current tutorial step points in the world, if anywhere (tiles; height in tiles).
bool FindTutorialMarker(const Session& S, Vec2& OutPos, float& OutHeight);

struct MinimapPing
{
	Vec2 Pos;
	float Age = 0.f;
	bool bDanger = true;
};
constexpr float MinimapPingLife = 3.f;

// Base image plus entities, pings and the camera view outline (corners on the ground, tiles).
void PaintMinimapOverlay(const Session& S, const ImageRGBA& Base, int PixelsPerTile, const Vec2 (&View)[4], bool bViewValid,
	const std::vector<MinimapPing>& Pings, ImageRGBA& Out);

// Converts a minimap pixel position (0..1 across the image) to map tiles.
Vec2 MinimapToWorld(const GameMap& Map, float U, float V);

// Procedural UI textures (nine-slice panels, buttons, bars).
enum class UiTex : uint8_t
{
	Panel,          // dark translucent panel with a soft light rim
	PanelGold,      // dark panel with a gold rim (menus, dialogs)
	Parchment,      // light card
	Button,         // dark button
	ButtonHover,
	ButtonPressed,
	ButtonGold,     // primary action
	ButtonGoldHover,
	ButtonGoldPressed,
	Slot,           // command-card slot
	SlotHot,        // highlighted slot (tutorial)
	BarBack,        // rounded bar background
	BarFill,        // white rounded bar (tinted)
	Circle,         // white disc (tinted)
	Vignette,       // radial darkening for menus
	Gradient,       // vertical fade (top transparent, bottom dark)
	Highlight,      // glowing gold rim with a clear centre (tutorial highlight)
	Count
};
constexpr int NumUiTex = static_cast<int>(UiTex::Count);
// Nine-slice margin (fraction of the texture size) of each UI texture.
float UiTexMargin(UiTex T);
void PaintUiTexture(UiTex T, int Size, ImageRGBA& Out);

} // namespace bh
