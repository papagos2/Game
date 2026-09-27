// Tests for render preparation: palette, render meshes, animation poses and HUD helpers.
#include "TestFramework.h"

#include "BhHud.h"
#include "BhMissions.h"
#include "BhRender.h"
#include "BhSession.h"

#include <cmath>
#include <cstdio>

using namespace bh;

namespace
{
int Luma(Rgb C)
{
	return (C.R * 299 + C.G * 587 + C.B * 114) / 1000;
}

bool UvsInside(const RenderSection& S)
{
	for (float V : S.UVs)
	{
		if (V < 0.f || V > 1.f)
		{
			return false;
		}
	}
	return S.UVs.size() == S.Positions.size() * 2 && S.Normals.size() == S.Positions.size();
}

// Builds every mesh of every mission, the way the engine layer does at mission start.
void BuildEverything(ColorPalette& Palette, int& OutTris)
{
	OutTris = 0;
	for (int A = 0; A < NumArchetypes; ++A)
	{
		for (Team T : {Team::Player, Team::Enemy})
		{
			RenderMesh Body;
			BuildEntityBodyMesh(static_cast<Archetype>(A), T, Palette, Body);
			OutTris += Body.TriangleCount();
			for (int R = 1; R < NumPartRoles; ++R)
			{
				RenderMesh Part;
				Vec3 Pivot;
				BuildRoleMesh(GetModel(static_cast<Archetype>(A)), static_cast<PartRole>(R), T, UnitBakeYaw, Palette, Part, Pivot);
			}
		}
	}
	for (int F = 2; F <= 4; ++F)
	{
		RenderMesh M;
		BuildScaffoldMesh(F, Palette, M);
	}
	RenderMesh Ruin;
	BuildRuinMesh(Palette, Ruin);
	for (int K = 0; K < static_cast<int>(FxMesh::Count); ++K)
	{
		RenderMesh M;
		BuildFxMesh(static_cast<FxMesh>(K), Palette, M);
	}
	for (int Mission = 0; Mission < GetMissionCount(); ++Mission)
	{
		World W;
		std::string Err;
		if (!LoadMissionMap(GetMission(Mission), W, Err))
		{
			continue;
		}
		const GameMap& Map = W.GetMap();
		std::vector<GroundCell> Cells;
		std::vector<Rgb> Colors;
		BuildGround(Map, 2, Cells, Colors);
		std::vector<TreeInstance> Trees;
		BuildTreeInstances(Map, Trees);
		std::vector<PropInstance> Props;
		GenerateProps(Map, Props);
		for (int Y = -16; Y < Map.GetHeight() + 16; Y += 16)
		{
			for (int X = -16; X < Map.GetWidth() + 16; X += 16)
			{
				const TileRect R(X, Y, X + 16, Y + 16);
				RenderMesh Chunk;
				BuildTerrainChunk(Cells, Colors, R, Map.GetWidth(), Map.GetHeight(), Palette, Chunk);
				BuildTreeChunkMesh(Trees, Map, R, Palette, Chunk);
				BuildPropChunkMesh(Props, R, Palette, Chunk);
				OutTris += Chunk.TriangleCount();
			}
		}
		RenderMesh Floor;
		BuildFloorMesh(Map.GetWidth(), Map.GetHeight(), 48.f, Palette, Floor);
	}
}
} // namespace

BH_TEST(Render_PaletteHoldsAllContent)
{
	ColorPalette Palette;
	int Tris = 0;
	BuildEverything(Palette, Tris);
	std::printf("  palette uses %d of %d colours; %d triangles built\n", Palette.Count(), PaletteColumns, Tris);
	BH_EXPECT(Palette.Count() > 40);
	BH_EXPECT_MSG(Palette.Count() < PaletteColumns * 3 / 4, "palette nearly full: %d", Palette.Count());
	BH_EXPECT(Palette.GetPixels().size() == static_cast<size_t>(PaletteColumns * PaletteRows * 4));
	// Columns are stable: asking again returns the same column and does not grow the palette.
	const int Count = Palette.Count();
	const uint32_t Rev = Palette.GetRevision();
	BH_EXPECT(Palette.Column(palette::Grass, false) == Palette.Column(palette::Grass, false));
	BH_EXPECT(Palette.Count() == Count && Palette.GetRevision() == Rev);
	BH_EXPECT(Palette.Column(palette::Grass, false) != Palette.Column(palette::Grass, true));
}

BH_TEST(Render_LightingFollowsTheSun)
{
	const Vec3 Up(0.f, 0.f, 1.f);
	const Vec3 ToSun = GetLightRig().ToSun;
	const Vec3 AwayFlat(-ToSun.X, -ToSun.Y, 0.f);
	const Vec3 TowardFlat(ToSun.X, ToSun.Y, 0.f);
	const Rgb Base = palette::Stone;
	const Rgb Lit = ColorPalette::Shade(Base, false, ColorPalette::LightRow(TowardFlat));
	const Rgb Dark = ColorPalette::Shade(Base, false, ColorPalette::LightRow(AwayFlat));
	const Rgb Top = ColorPalette::Shade(Base, false, ColorPalette::LightRow(Up));
	const Rgb Bottom = ColorPalette::Shade(Base, false, ColorPalette::LightRow(Vec3(0.f, 0.f, -1.f)));
	std::printf("  stone: sun side %d, shade side %d, top %d, underside %d\n", Luma(Lit), Luma(Dark), Luma(Top), Luma(Bottom));
	BH_EXPECT(Luma(Lit) > Luma(Dark) + 30);
	BH_EXPECT(Luma(Top) > Luma(Dark));
	BH_EXPECT(Luma(Dark) > Luma(Bottom));
	// Shaded sides stay readable (no crushed blacks) and lit tops do not blow out.
	BH_EXPECT(Luma(Dark) > 60);
	BH_EXPECT(Luma(Top) < 250);
	// Self-coloured parts ignore the light row.
	BH_EXPECT(ColorPalette::Shade(palette::Sunstone, true, 0) == ColorPalette::Shade(palette::Sunstone, true, PaletteRows - 1));
	// Grass seen from above keeps its hue (green dominant).
	const Rgb Grass = ColorPalette::Shade(palette::Grass, false, ColorPalette::LightRow(Up));
	std::printf("  grass top: #%02X%02X%02X\n", Grass.R, Grass.G, Grass.B);
	BH_EXPECT(Grass.G > Grass.R && Grass.G > Grass.B);
}

BH_TEST(Render_MeshesUsePaletteCells)
{
	ColorPalette Palette;
	RenderMesh Body;
	BuildEntityBodyMesh(Archetype::Keep, Team::Player, Palette, Body);
	BH_EXPECT(!Body.Opaque.Empty());
	BH_EXPECT(Body.Shadow.TriangleCount() == 2);
	BH_EXPECT(UvsInside(Body.Opaque) && UvsInside(Body.Shadow));
	// Every opaque vertex samples the centre of a used palette column.
	bool bCentred = true;
	for (size_t I = 0; I < Body.Opaque.UVs.size(); I += 2)
	{
		const float Col = Body.Opaque.UVs[I] * static_cast<float>(PaletteColumns) - 0.5f;
		bCentred = bCentred && std::fabs(Col - std::round(Col)) < 1e-3f && std::round(Col) < static_cast<float>(Palette.Count());
	}
	BH_EXPECT(bCentred);
	// Unreal units: the keep fills its 4-tile footprint; its shadow falls away from the sun.
	RenderMesh OpaqueOnly;
	OpaqueOnly.Opaque = Body.Opaque;
	Vec3 Min;
	Vec3 Max;
	OpaqueOnly.Bounds(Min, Max);
	BH_EXPECT(Max.X - Min.X > 300.f && Max.X - Min.X <= 400.f);
	BH_EXPECT(Max.Z > 150.f);
	RenderMesh ShadowOnly;
	ShadowOnly.Shadow = Body.Shadow;
	Vec3 SMin;
	Vec3 SMax;
	ShadowOnly.Bounds(SMin, SMax);
	BH_EXPECT(SMax.X - SMin.X > Max.X - Min.X);

	// Shadows lie flat on the ground, facing up (counter-clockwise seen from above).
	for (size_t T = 0; T + 2 < Body.Shadow.Indices.size(); T += 3)
	{
		const Vec3 A = Body.Shadow.Positions[Body.Shadow.Indices[T]];
		const Vec3 B = Body.Shadow.Positions[Body.Shadow.Indices[T + 1]];
		const Vec3 C = Body.Shadow.Positions[Body.Shadow.Indices[T + 2]];
		BH_EXPECT(Vec3::Cross(B - A, C - A).Z > 0.f);
		BH_EXPECT(A.Z > 0.f && A.Z < 5.f);
	}

	// Decal quad spans one tile.
	RenderMesh Quad;
	BuildDecalQuad(Quad);
	Quad.Bounds(Min, Max);
	BH_EXPECT(Quad.Opaque.TriangleCount() == 2 && std::fabs(Max.X - Min.X - 100.f) < 1e-3f);
}

BH_TEST(Render_AnimatedPartsTurnAboutTheirPivot)
{
	ColorPalette Palette;
	const ModelDef& Model = GetModel(Archetype::Shieldbearer);
	RenderMesh Weapon;
	Vec3 Pivot;
	BH_EXPECT(BuildRoleMesh(Model, PartRole::Weapon, Team::Player, UnitBakeYaw, Palette, Weapon, Pivot));
	BH_EXPECT(!Weapon.Opaque.Empty());
	// The weapon's pieces (blade and guard) share one pivot near the grip: most geometry is above it.
	Vec3 Min;
	Vec3 Max;
	Weapon.Bounds(Min, Max);
	BH_EXPECT(Max.Z > -Min.Z);
	BH_EXPECT(Pivot.Z > 0.f && Pivot.Z < Model.Height);
	RenderMesh None;
	BH_EXPECT(!BuildRoleMesh(Model, PartRole::Spin, Team::Player, UnitBakeYaw, Palette, None, Pivot));
	BH_EXPECT(None.Empty());
}

BH_TEST(Render_AnimationPoses)
{
	AnimInput In;
	In.Type = Archetype::Shieldbearer;
	In.Owner = Team::Player;
	// A swing leaves and returns to rest.
	In.SwingAge = 0.f;
	BH_EXPECT(std::fabs(EvaluateRolePose(PartRole::Weapon, In).Pitch) < 1e-3f);
	In.SwingAge = SwingDuration * 0.45f;
	BH_EXPECT(EvaluateRolePose(PartRole::Weapon, In).Pitch < -30.f);
	In.SwingAge = SwingDuration * 0.999f;
	BH_EXPECT(std::fabs(EvaluateRolePose(PartRole::Weapon, In).Pitch) < 2.f);
	In.SwingAge = 99.f;
	// Cargo shows only while carrying the matching resource.
	BH_EXPECT(!EvaluateRolePose(PartRole::CarryTimber, In).bVisible);
	In.Carry = Resource::Timber;
	BH_EXPECT(EvaluateRolePose(PartRole::CarryTimber, In).bVisible);
	BH_EXPECT(!EvaluateRolePose(PartRole::CarrySunstone, In).bVisible);
	// Death: Wardens fall over and are removed after the animation.
	In.DeathAge = 0.5f;
	const EntityPose Falling = EvaluateEntityPose(In);
	BH_EXPECT(Falling.Roll > 60.f && Falling.bVisible);
	In.DeathAge = DeathDuration(In.Type) + 0.01f;
	BH_EXPECT(!EvaluateEntityPose(In).bVisible);
	// Buildings grow while under construction and hide animated parts.
	AnimInput B;
	B.Type = Archetype::Barracks;
	B.bConstructed = false;
	B.BuildProgress = 0.5f;
	const EntityPose Growing = EvaluateEntityPose(B);
	BH_EXPECT(Growing.ScaleZ > 0.4f && Growing.ScaleZ < 0.7f);
	BH_EXPECT(!EvaluateRolePose(PartRole::Flag, B).bVisible);
	B.bConstructed = true;
	BH_EXPECT(EvaluateRolePose(PartRole::Flag, B).bVisible);
	// Nodes shrink as they are mined out.
	AnimInput N;
	N.Type = Archetype::SunstoneNode;
	N.AmountRatio = 0.2f;
	BH_EXPECT(EvaluateEntityPose(N).ScaleZ < 0.7f);
}

BH_TEST(Render_TutorialMarkersAndMinimap)
{
	Session S;
	SessionConfig Cfg;
	Cfg.MissionIndex = 0;
	std::string Err;
	BH_EXPECT(S.Start(Cfg, Err));
	// Walk the tutorial: every step with a marker must find something to point at.
	const MissionDef& Def = GetMission(0);
	int WithMarker = 0;
	for (size_t I = 0; I < Def.Tutorial.size(); ++I)
	{
		S.GetMission().TutorialIndex = static_cast<int>(I);
		Vec2 P;
		float H = 0.f;
		if (Def.Tutorial[I].Marker != MarkerKind::None)
		{
			++WithMarker;
			const bool bFound = FindTutorialMarker(S, P, H);
			BH_EXPECT_MSG(bFound || Def.Tutorial[I].Marker == MarkerKind::OwnArch, "tutorial step %d marker not found", static_cast<int>(I));
		}
	}
	BH_EXPECT(WithMarker > 0);

	ImageRGBA Base;
	PaintMinimap(S.GetWorld().GetMap(), 2, Base);
	const Vec2 View[4] = {Vec2(2, 2), Vec2(12, 2), Vec2(14, 10), Vec2(0, 10)};
	std::vector<MinimapPing> Pings(1);
	Pings[0].Pos = Vec2(20, 15);
	Pings[0].Age = 0.5f;
	ImageRGBA Out;
	PaintMinimapOverlay(S, Base, 2, View, true, Pings, Out);
	BH_EXPECT(Out.W == Base.W && Out.H == Base.H);
	int Changed = 0;
	for (size_t I = 0; I < Out.Px.size(); ++I)
	{
		Changed += Out.Px[I] != Base.Px[I] ? 1 : 0;
	}
	BH_EXPECT(Changed > 50);
	const Vec2 Centre = MinimapToWorld(S.GetWorld().GetMap(), 0.5f, 0.5f);
	BH_EXPECT(std::fabs(Centre.X - static_cast<float>(S.GetWorld().GetMap().GetWidth()) * 0.5f) < 1e-3f);
}

BH_TEST(Render_UiTexturesHaveContent)
{
	for (int T = 0; T < NumUiTex; ++T)
	{
		ImageRGBA Img;
		PaintUiTexture(static_cast<UiTex>(T), 64, Img);
		BH_EXPECT(Img.W == 64 && Img.H == 64);
		int Visible = 0;
		int Clear = 0;
		for (size_t K = 3; K < Img.Px.size(); K += 4)
		{
			Visible += Img.Px[K] > 20 ? 1 : 0;
			Clear += Img.Px[K] == 0 ? 1 : 0;
		}
		BH_EXPECT_MSG(Visible > 300, "ui texture %d nearly empty", T);
		const float M = UiTexMargin(static_cast<UiTex>(T));
		BH_EXPECT(M >= 0.f && M < 0.5f);
		if (static_cast<UiTex>(T) == UiTex::Panel || static_cast<UiTex>(T) == UiTex::Circle)
		{
			BH_EXPECT(Clear > 0); // rounded corners are transparent
		}
	}
}
