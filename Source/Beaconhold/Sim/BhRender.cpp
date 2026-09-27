// Beaconhold simulation core — render preparation for the engine layer.
#include "BhRender.h"

#include "BhData.h"
#include "BhMath.h"

#include <cmath>

namespace bh
{
namespace
{
// ------------------------------------------------------------------------------------ Colour maths

float RenderSrgbToLinear(uint8_t C)
{
	const float V = static_cast<float>(C) / 255.f;
	return V <= 0.04045f ? V / 12.92f : std::pow((V + 0.055f) / 1.055f, 2.4f);
}

uint8_t RenderLinearToSrgb(float V)
{
	const float C = Saturate(V);
	const float S = C <= 0.0031308f ? C * 12.92f : 1.055f * std::pow(C, 1.f / 2.4f) - 0.055f;
	return static_cast<uint8_t>(ClampI(static_cast<int>(S * 255.f + 0.5f), 0, 255));
}

// The ACES filmic fit used by the three.js art-direction preview (sRGB primaries in and out).
void RenderAces(float& R, float& G, float& B, float Exposure)
{
	const float Scale = Exposure / 0.6f;
	R *= Scale;
	G *= Scale;
	B *= Scale;
	const float IR = 0.59719f * R + 0.35458f * G + 0.04823f * B;
	const float IG = 0.07600f * R + 0.90834f * G + 0.01566f * B;
	const float IB = 0.02840f * R + 0.13383f * G + 0.83777f * B;
	auto Fit = [](float V)
	{
		const float A = V * (V + 0.0245786f) - 0.000090537f;
		const float D = V * (0.983729f * V + 0.4329510f) + 0.238081f;
		return A / D;
	};
	const float FR = Fit(IR);
	const float FG = Fit(IG);
	const float FB = Fit(IB);
	R = Saturate(1.60475f * FR - 0.53108f * FG - 0.07367f * FB);
	G = Saturate(-0.10208f * FR + 1.10813f * FG - 0.00605f * FB);
	B = Saturate(-0.00327f * FR - 0.07276f * FG + 1.07602f * FB);
}

uint32_t RenderPaletteKey(Rgb C, bool bUnlit)
{
	return (C.Key() << 1) | (bUnlit ? 1u : 0u);
}

// ------------------------------------------------------------------------------------ Mesh helpers

void RenderPushTri(RenderSection& S, const Vec3 (&P)[3], const Vec3& N, float U, float V)
{
	const uint32_t Base = static_cast<uint32_t>(S.Positions.size());
	for (int K = 0; K < 3; ++K)
	{
		S.Positions.push_back(P[K] * UnrealUnitsPerTile);
		S.Normals.push_back(N);
		S.UVs.push_back(U);
		S.UVs.push_back(V);
	}
	S.Indices.push_back(Base);
	S.Indices.push_back(Base + 1);
	S.Indices.push_back(Base + 2);
}

// Quad on the ground (tiles), counter-clockwise seen from above, with UVs spanning 0..1.
void RenderPushGroundQuad(RenderSection& S, const Vec3& C, const Vec2& Major, const Vec2& Minor)
{
	const uint32_t Base = static_cast<uint32_t>(S.Positions.size());
	const Vec2 Corners[4] = {Vec2(-1.f, -1.f), Vec2(1.f, -1.f), Vec2(1.f, 1.f), Vec2(-1.f, 1.f)};
	for (const Vec2& K : Corners)
	{
		const Vec2 P = Vec2(C.X, C.Y) + Major * K.X + Minor * K.Y;
		S.Positions.push_back(Vec3(P.X, P.Y, C.Z) * UnrealUnitsPerTile);
		S.Normals.push_back(Vec3(0.f, 0.f, 1.f));
		S.UVs.push_back(K.X * 0.5f + 0.5f);
		S.UVs.push_back(K.Y * 0.5f + 0.5f);
	}
	// Keep counter-clockwise when the axes form a left-handed pair.
	const bool bFlip = (Major.X * Minor.Y - Major.Y * Minor.X) < 0.f;
	const uint32_t Order[6] = {0, 1, 2, 0, 2, 3};
	const uint32_t Flipped[6] = {0, 2, 1, 0, 3, 2};
	for (int I = 0; I < 6; ++I)
	{
		S.Indices.push_back(Base + (bFlip ? Flipped[I] : Order[I]));
	}
}

Vec2 RenderShadowDir()
{
	const Vec3& S = GetLightRig().ToSun;
	return Vec2(-S.X, -S.Y).Normalized();
}

bool RenderIsStanding(const TreeInstance& T, const GameMap& Map)
{
	return T.TileX < 0 || Map.HasTree(T.TileX, T.TileY);
}

float RenderHash(uint32_t Seed)
{
	return Hash01(static_cast<int>(Seed & 0xFFFF), static_cast<int>(Seed >> 16), 0x5EEDu);
}

float RenderEaseOutBack(float T)
{
	const float C = Saturate(T) - 1.f;
	return 1.f + C * C * (2.7f * C + 1.7f);
}

// Weapon swing: a quick wind-up, a hard strike forwards and a slower recovery (degrees of pitch;
// positive tips a vertical weapon backwards).
float RenderSwingCurve(float T)
{
	const float C = Saturate(T);
	if (C < 0.27f)
	{
		return 35.f * SmoothStep(C / 0.27f);
	}
	if (C < 0.49f)
	{
		return LerpF(35.f, -75.f, SmoothStep((C - 0.27f) / 0.22f));
	}
	return LerpF(-75.f, 0.f, SmoothStep((C - 0.49f) / 0.51f));
}

float RenderSwingAmplitude(Archetype A)
{
	switch (A)
	{
	case Archetype::Ranger:
	case Archetype::Hexer:
		return 0.35f;
	case Archetype::Sage:
		return 0.6f;
	default:
		return 1.f;
	}
}

// ------------------------------------------------------------------------------------ Minimap

void MiniPut(ImageRGBA& Img, int X, int Y, Rgb C, float A)
{
	if (X < 0 || Y < 0 || X >= Img.W || Y >= Img.H || A <= 0.f)
	{
		return;
	}
	uint8_t* D = &Img.Px[static_cast<size_t>((Y * Img.W + X) * 4)];
	const float K = Saturate(A);
	D[0] = static_cast<uint8_t>(static_cast<float>(D[0]) + (static_cast<float>(C.R) - static_cast<float>(D[0])) * K);
	D[1] = static_cast<uint8_t>(static_cast<float>(D[1]) + (static_cast<float>(C.G) - static_cast<float>(D[1])) * K);
	D[2] = static_cast<uint8_t>(static_cast<float>(D[2]) + (static_cast<float>(C.B) - static_cast<float>(D[2])) * K);
	D[3] = 255;
}

void MiniRect(ImageRGBA& Img, int X0, int Y0, int X1, int Y1, Rgb C, float A)
{
	for (int Y = Y0; Y < Y1; ++Y)
	{
		for (int X = X0; X < X1; ++X)
		{
			MiniPut(Img, X, Y, C, A);
		}
	}
}

void MiniLine(ImageRGBA& Img, float X0, float Y0, float X1, float Y1, Rgb C, float A)
{
	const float Len = std::sqrt((X1 - X0) * (X1 - X0) + (Y1 - Y0) * (Y1 - Y0));
	const int Steps = MaxI(1, static_cast<int>(Len * 1.5f));
	for (int I = 0; I <= Steps; ++I)
	{
		const float T = static_cast<float>(I) / static_cast<float>(Steps);
		MiniPut(Img, static_cast<int>(std::floor(LerpF(X0, X1, T))), static_cast<int>(std::floor(LerpF(Y0, Y1, T))), C, A);
	}
}

void MiniRing(ImageRGBA& Img, float Cx, float Cy, float R, Rgb C, float A)
{
	const int X0 = static_cast<int>(std::floor(Cx - R - 1.f));
	const int X1 = static_cast<int>(std::ceil(Cx + R + 1.f));
	const int Y0 = static_cast<int>(std::floor(Cy - R - 1.f));
	const int Y1 = static_cast<int>(std::ceil(Cy + R + 1.f));
	for (int Y = Y0; Y <= Y1; ++Y)
	{
		for (int X = X0; X <= X1; ++X)
		{
			const float Dx = static_cast<float>(X) + 0.5f - Cx;
			const float Dy = static_cast<float>(Y) + 0.5f - Cy;
			const float D = AbsF(std::sqrt(Dx * Dx + Dy * Dy) - R);
			MiniPut(Img, X, Y, C, A * Saturate(1.2f - D));
		}
	}
}

// ------------------------------------------------------------------------------------ UI textures

float UiRoundRectDist(float Px, float Py, float Half, float Radius)
{
	// Signed distance to a centred square of half-size Half with rounded corners.
	const float Qx = AbsF(Px) - (Half - Radius);
	const float Qy = AbsF(Py) - (Half - Radius);
	const float Ox = MaxF(Qx, 0.f);
	const float Oy = MaxF(Qy, 0.f);
	return std::sqrt(Ox * Ox + Oy * Oy) + MinF(MaxF(Qx, Qy), 0.f) - Radius;
}

struct UiPanelSpec
{
	Rgba Top;
	Rgba Bottom;
	Rgba Rim;
	float RimWidth = 2.f;
	Rgba Highlight = Rgba(1.f, 1.f, 1.f, 0.f); // inner top highlight line
	float Radius = 0.22f;                       // fraction of the size
	float InnerShadow = 0.f;                    // darkening towards the edges
};

void UiPaintPanel(ImageRGBA& Out, int Size, const UiPanelSpec& S)
{
	Out.Init(Size, Size);
	const float Half = static_cast<float>(Size) * 0.5f - 0.5f;
	const float Radius = S.Radius * static_cast<float>(Size);
	for (int Y = 0; Y < Size; ++Y)
	{
		for (int X = 0; X < Size; ++X)
		{
			const float Px = static_cast<float>(X) + 0.5f - static_cast<float>(Size) * 0.5f;
			const float Py = static_cast<float>(Y) + 0.5f - static_cast<float>(Size) * 0.5f;
			const float D = UiRoundRectDist(Px, Py, Half, Radius);
			const float Cover = Saturate(0.5f - D);
			if (Cover <= 0.f)
			{
				continue;
			}
			const float T = static_cast<float>(Y) / static_cast<float>(Size - 1);
			Rgba C(LerpF(S.Top.R, S.Bottom.R, T), LerpF(S.Top.G, S.Bottom.G, T), LerpF(S.Top.B, S.Bottom.B, T), LerpF(S.Top.A, S.Bottom.A, T));
			if (S.InnerShadow > 0.f)
			{
				const float Edge = Saturate(1.f + D / (static_cast<float>(Size) * 0.18f));
				const float K = S.InnerShadow * Edge * Edge;
				C.R *= 1.f - K;
				C.G *= 1.f - K;
				C.B *= 1.f - K;
			}
			if (S.Highlight.A > 0.f)
			{
				const float Line = Saturate(1.f - AbsF(D + S.RimWidth + 1.5f));
				if (Py < 0.f && Line > 0.f)
				{
					const float K = S.Highlight.A * Line * Saturate(-Py / Half);
					C.R = LerpF(C.R, S.Highlight.R, K);
					C.G = LerpF(C.G, S.Highlight.G, K);
					C.B = LerpF(C.B, S.Highlight.B, K);
				}
			}
			if (S.Rim.A > 0.f && S.RimWidth > 0.f)
			{
				const float RimCover = Saturate(D + S.RimWidth + 0.5f);
				C.R = LerpF(C.R, S.Rim.R, RimCover * S.Rim.A);
				C.G = LerpF(C.G, S.Rim.G, RimCover * S.Rim.A);
				C.B = LerpF(C.B, S.Rim.B, RimCover * S.Rim.A);
				C.A = LerpF(C.A, 1.f, RimCover * S.Rim.A);
			}
			uint8_t* P = &Out.Px[static_cast<size_t>((Y * Size + X) * 4)];
			P[0] = static_cast<uint8_t>(Saturate(C.R) * 255.f + 0.5f);
			P[1] = static_cast<uint8_t>(Saturate(C.G) * 255.f + 0.5f);
			P[2] = static_cast<uint8_t>(Saturate(C.B) * 255.f + 0.5f);
			P[3] = static_cast<uint8_t>(Saturate(C.A * Cover) * 255.f + 0.5f);
		}
	}
}
} // namespace

// ================================================================================== Palette

const LightRig& GetLightRig()
{
	static const LightRig Rig;
	return Rig;
}

ColorPalette::ColorPalette()
{
	Pixels.assign(static_cast<size_t>(PaletteColumns * PaletteRows * 4), 0);
	for (size_t I = 3; I < Pixels.size(); I += 4)
	{
		Pixels[I] = 255;
	}
}

int ColorPalette::Column(Rgb Color, bool bUnlit)
{
	const uint32_t Key = RenderPaletteKey(Color, bUnlit);
	const auto Found = Index.find(Key);
	if (Found != Index.end())
	{
		return Found->second;
	}
	if (static_cast<int>(Keys.size()) >= PaletteColumns)
	{
		// Full: reuse the closest colour with the same lighting mode.
		int Best = 0;
		int BestDist = 1 << 30;
		for (size_t I = 0; I < Keys.size(); ++I)
		{
			if ((Keys[I] & 1u) != (bUnlit ? 1u : 0u))
			{
				continue;
			}
			const Rgb O = Rgb::Hex(Keys[I] >> 1);
			const int Dr = static_cast<int>(O.R) - static_cast<int>(Color.R);
			const int Dg = static_cast<int>(O.G) - static_cast<int>(Color.G);
			const int Db = static_cast<int>(O.B) - static_cast<int>(Color.B);
			const int Dist = Dr * Dr + Dg * Dg + Db * Db;
			if (Dist < BestDist)
			{
				BestDist = Dist;
				Best = static_cast<int>(I);
			}
		}
		return Best;
	}
	const int Col = static_cast<int>(Keys.size());
	Keys.push_back(Key);
	Index[Key] = Col;
	for (int Row = 0; Row < PaletteRows; ++Row)
	{
		const Rgb C = Shade(Color, bUnlit, Row);
		uint8_t* P = &Pixels[static_cast<size_t>((Row * PaletteColumns + Col) * 4)];
		P[0] = C.R;
		P[1] = C.G;
		P[2] = C.B;
		P[3] = 255;
	}
	++Revision;
	return Col;
}

Rgb ColorPalette::Shade(Rgb Base, bool bUnlit, int Row)
{
	const LightRig& L = GetLightRig();
	float R = RenderSrgbToLinear(Base.R);
	float G = RenderSrgbToLinear(Base.G);
	float B = RenderSrgbToLinear(Base.B);
	if (!bUnlit)
	{
		const int SafeRow = ClampI(Row, 0, PaletteRows - 1);
		const float Sun = static_cast<float>(SafeRow / PaletteSkyLevels) / static_cast<float>(PaletteSunLevels - 1);
		const float Sky = static_cast<float>(SafeRow % PaletteSkyLevels) / static_cast<float>(PaletteSkyLevels - 1);
		const float SunC[3] = {RenderSrgbToLinear(L.SunColor.R), RenderSrgbToLinear(L.SunColor.G), RenderSrgbToLinear(L.SunColor.B)};
		const float SkyC[3] = {RenderSrgbToLinear(L.SkyColor.R), RenderSrgbToLinear(L.SkyColor.G), RenderSrgbToLinear(L.SkyColor.B)};
		const float GndC[3] = {RenderSrgbToLinear(L.GroundColor.R), RenderSrgbToLinear(L.GroundColor.G), RenderSrgbToLinear(L.GroundColor.B)};
		float Irr[3];
		for (int K = 0; K < 3; ++K)
		{
			Irr[K] = SunC[K] * L.SunIntensity * Sun + LerpF(GndC[K], SkyC[K], Sky) * L.HemiIntensity;
		}
		R *= Irr[0] / Pi;
		G *= Irr[1] / Pi;
		B *= Irr[2] / Pi;
	}
	RenderAces(R, G, B, L.Exposure);
	return Rgb(RenderLinearToSrgb(R), RenderLinearToSrgb(G), RenderLinearToSrgb(B));
}

int ColorPalette::LightRow(const Vec3& WorldNormal)
{
	const Vec3 N = WorldNormal.Normalized();
	const float Sun = MaxF(0.f, Vec3::Dot(N, GetLightRig().ToSun));
	const float Sky = 0.5f + 0.5f * N.Z;
	const int SunLevel = ClampI(static_cast<int>(Sun * static_cast<float>(PaletteSunLevels - 1) + 0.5f), 0, PaletteSunLevels - 1);
	const int SkyLevel = ClampI(static_cast<int>(Sky * static_cast<float>(PaletteSkyLevels - 1) + 0.5f), 0, PaletteSkyLevels - 1);
	return SunLevel * PaletteSkyLevels + SkyLevel;
}

// ================================================================================== Render meshes

void RenderMesh::Bounds(Vec3& OutMin, Vec3& OutMax) const
{
	OutMin = Vec3(1e30f, 1e30f, 1e30f);
	OutMax = Vec3(-1e30f, -1e30f, -1e30f);
	for (const RenderSection* S : {&Opaque, &Shadow})
	{
		for (const Vec3& P : S->Positions)
		{
			OutMin = Vec3(MinF(OutMin.X, P.X), MinF(OutMin.Y, P.Y), MinF(OutMin.Z, P.Z));
			OutMax = Vec3(MaxF(OutMax.X, P.X), MaxF(OutMax.Y, P.Y), MaxF(OutMax.Z, P.Z));
		}
	}
}

void AppendRenderMesh(const MeshData& In, float BakeYaw, ColorPalette& Palette, RenderMesh& Out)
{
	RenderSection& S = Out.Opaque;
	for (const MeshSection& Sec : In.Sections)
	{
		const int Col = Palette.Column(Sec.Color, Sec.bUnlit);
		const float U = ColorPalette::U(Col);
		for (size_t T = 0; T + 2 < Sec.Indices.size(); T += 3)
		{
			const uint32_t I0 = Sec.Indices[T];
			const uint32_t I1 = Sec.Indices[T + 1];
			const uint32_t I2 = Sec.Indices[T + 2];
			const Vec3 P[3] = {Sec.Positions[I0], Sec.Positions[I1], Sec.Positions[I2]};
			// Flat shading: one light level per triangle, from its averaged (outward) normal.
			const Vec3 N = (Sec.Normals[I0] + Sec.Normals[I1] + Sec.Normals[I2]).Normalized();
			const int Row = Sec.bUnlit ? 0 : ColorPalette::LightRow(Xform::Rotate(0.f, BakeYaw, 0.f, N));
			RenderPushTri(S, P, N, U, ColorPalette::V(Row));
		}
	}
}

void AppendBlobShadow(RenderMesh& Out, float X, float Y, float Radius, float Height, bool bDirectional)
{
	const float Lift = 0.025f;
	if (!bDirectional)
	{
		RenderPushGroundQuad(Out.Shadow, Vec3(X, Y, Lift), Vec2(Radius, 0.f), Vec2(0.f, Radius));
		return;
	}
	const Vec2 D = RenderShadowDir();
	const Vec2 Perp(-D.Y, D.X);
	const float Stretch = MaxF(0.f, Height) * 0.28f;
	const Vec2 C = Vec2(X, Y) + D * Stretch;
	RenderPushGroundQuad(Out.Shadow, Vec3(C.X, C.Y, Lift), D * (Radius + Stretch), Perp * Radius);
}

namespace
{
// Directional shadows for meshes drawn at a fixed yaw are authored in model space.
void RenderModelShadow(RenderMesh& Out, float Radius, float Height, float FrameYaw)
{
	const Vec2 World = RenderShadowDir();
	const float A = -FrameYaw * Pi / 180.f;
	const Vec2 D(World.X * std::cos(A) - World.Y * std::sin(A), World.X * std::sin(A) + World.Y * std::cos(A));
	const Vec2 Perp(-D.Y, D.X);
	const float Stretch = Height * 0.28f;
	const Vec2 C = D * Stretch;
	RenderPushGroundQuad(Out.Shadow, Vec3(C.X, C.Y, 0.025f), D * (Radius + Stretch), Perp * Radius);
}
} // namespace

void BuildEntityBodyMesh(Archetype A, Team T, ColorPalette& Palette, RenderMesh& Out)
{
	const ModelDef& M = GetModel(A);
	const ArchetypeDef& D = GetDef(A);
	const bool bUnit = D.Kind == EntityKind::Unit;
	AppendRenderMesh(BuildModelMesh(M, T, true), bUnit ? UnitBakeYaw : BuildingYaw, Palette, Out);
	if (bUnit)
	{
		// The body hides the dark centre of the blob, so units get a slightly wider one.
		AppendBlobShadow(Out, 0.f, 0.f, M.ShadowRadius * 1.35f, 0.f, false);
	}
	else
	{
		const float Radius = MaxF(0.4f, static_cast<float>(D.Footprint) * 0.46f);
		RenderModelShadow(Out, Radius, M.Height, BuildingYaw);
	}
}

bool BuildRoleMesh(const ModelDef& M, PartRole Role, Team T, float BakeYaw, ColorPalette& Palette, RenderMesh& Out, Vec3& OutPivot)
{
	// Pivot: weapons turn about the grip of their longest piece, everything else about the
	// centre of its pieces.
	const PartDef* Longest = nullptr;
	Vec3 Sum;
	int Count = 0;
	for (const PartDef& P : M.Parts)
	{
		if (P.Role != Role)
		{
			continue;
		}
		Sum = Sum + Vec3(P.X, P.Y, P.Z);
		++Count;
		if (Longest == nullptr || P.SZ > Longest->SZ)
		{
			Longest = &P;
		}
	}
	if (Count == 0 || Longest == nullptr)
	{
		return false;
	}
	if (Role == PartRole::Weapon)
	{
		OutPivot = PartXform(*Longest).Apply(Vec3(0.f, 0.f, -0.4f));
	}
	else
	{
		OutPivot = Sum * (1.f / static_cast<float>(Count));
	}
	MeshData Parts;
	for (const PartDef& P : M.Parts)
	{
		if (P.Role != Role)
		{
			continue;
		}
		Xform X = PartXform(P);
		X.Translation = X.Translation - OutPivot;
		AppendShape(Parts, P.S, X, ResolveColor(P, T), P.bUnlit);
	}
	AppendRenderMesh(Parts, BakeYaw, Palette, Out);
	return true;
}

void BuildScaffoldMesh(int Footprint, ColorPalette& Palette, RenderMesh& Out)
{
	AppendRenderMesh(BuildModelMesh(GetScaffoldModel(Footprint), Team::Player, false), BuildingYaw, Palette, Out);
}

void BuildRuinMesh(ColorPalette& Palette, RenderMesh& Out)
{
	AppendRenderMesh(BuildModelMesh(GetBeaconRuinModel(), Team::Neutral, false), BuildingYaw, Palette, Out);
	RenderModelShadow(Out, 1.f, 0.6f, BuildingYaw);
}

void BuildTerrainChunk(const std::vector<GroundCell>& Cells, const std::vector<Rgb>& Colors, const TileRect& Region, int MapW, int MapH, ColorPalette& Palette, RenderMesh& Out)
{
	AppendRenderMesh(BuildGroundChunk(Cells, Colors, Region, MapW, MapH), 0.f, Palette, Out);
}

void BuildTreeChunkMesh(const std::vector<TreeInstance>& Trees, const GameMap& Map, const TileRect& Region, ColorPalette& Palette, RenderMesh& Out)
{
	AppendRenderMesh(BuildTreeChunk(Trees, Map, Region), 0.f, Palette, Out);
	for (const TreeInstance& T : Trees)
	{
		if (!Region.Contains(Tile::FromPos(Vec2(T.X, T.Y))) || !RenderIsStanding(T, Map))
		{
			continue;
		}
		const ModelDef& M = GetTreeModel(T.Kind);
		AppendBlobShadow(Out, T.X, T.Y, M.ShadowRadius * T.Scale * 1.1f, M.Height * T.Scale, true);
	}
}

void BuildPropChunkMesh(const std::vector<PropInstance>& Props, const TileRect& Region, ColorPalette& Palette, RenderMesh& Out)
{
	AppendRenderMesh(BuildPropChunk(Props, Region), 0.f, Palette, Out);
}

void BuildFloorMesh(int MapW, int MapH, float Margin, ColorPalette& Palette, RenderMesh& Out)
{
	// Four slabs around the map (the map itself is covered by terrain chunks).
	const float Z = -0.36f;
	const float W = static_cast<float>(MapW);
	const float H = static_cast<float>(MapH);
	const float Rects[4][4] = {
		{-Margin, -Margin, W + Margin, 0.f},
		{-Margin, H, W + Margin, H + Margin},
		{-Margin, 0.f, 0.f, H},
		{W, 0.f, W + Margin, H},
	};
	const float U = ColorPalette::U(Palette.Column(palette::Skirt, false));
	const float V = ColorPalette::V(ColorPalette::LightRow(Vec3(0.f, 0.f, 1.f)));
	for (const auto& R : Rects)
	{
		const Vec3 A(R[0], R[1], Z);
		const Vec3 B(R[2], R[1], Z);
		const Vec3 C(R[2], R[3], Z);
		const Vec3 D(R[0], R[3], Z);
		const Vec3 T1[3] = {A, B, C};
		const Vec3 T2[3] = {A, C, D};
		RenderPushTri(Out.Opaque, T1, Vec3(0.f, 0.f, 1.f), U, V);
		RenderPushTri(Out.Opaque, T2, Vec3(0.f, 0.f, 1.f), U, V);
	}
}

void BuildFxMesh(FxMesh Kind, ColorPalette& Palette, RenderMesh& Out)
{
	MeshData M;
	auto Put = [&M](Shape S, float X, float Y, float Z, float SX, float SY, float SZ, Rgb C, bool bUnlit, float Pitch = 0.f, float Yaw = 0.f)
	{
		Xform T;
		T.Scale = Vec3(SX, SY, SZ);
		T.Pitch = Pitch;
		T.Yaw = Yaw;
		T.Translation = Vec3(X, Y, Z);
		AppendShape(M, S, T, C, bUnlit);
	};
	switch (Kind)
	{
	case FxMesh::Arrow:
		Put(Shape::Cube, 0.f, 0.f, 0.f, 0.46f, 0.025f, 0.025f, palette::Wood, false);
		Put(Shape::Cone, 0.26f, 0.f, 0.f, 0.06f, 0.06f, 0.09f, palette::Metal, false, -90.f);
		Put(Shape::Cube, -0.2f, 0.f, 0.f, 0.1f, 0.07f, 0.012f, palette::Cloth, false);
		break;
	case FxMesh::Bolt:
		Put(Shape::Sphere, 0.f, 0.f, 0.f, 0.16f, 0.16f, 0.16f, palette::LanternGlow, true);
		Put(Shape::Sphere, -0.12f, 0.f, 0.f, 0.1f, 0.1f, 0.1f, palette::FlameCore, true);
		break;
	case FxMesh::TowerBolt:
		Put(Shape::Cube, 0.f, 0.f, 0.f, 0.6f, 0.04f, 0.04f, palette::WoodDark, false);
		Put(Shape::Cone, 0.33f, 0.f, 0.f, 0.09f, 0.09f, 0.12f, palette::MetalDark, false, -90.f);
		break;
	case FxMesh::ThornDart:
		Put(Shape::Cone, 0.f, 0.f, 0.f, 0.08f, 0.08f, 0.34f, palette::Thorn, false, -90.f);
		Put(Shape::Sphere, -0.1f, 0.f, 0.f, 0.07f, 0.07f, 0.07f, palette::GloamGreen, true);
		break;
	case FxMesh::Spark:
		Put(Shape::Cube, 0.f, 0.f, 0.f, 0.07f, 0.07f, 0.07f, palette::FlameCore, true, 20.f, 35.f);
		break;
	case FxMesh::Dust:
		Put(Shape::Sphere, 0.f, 0.f, 0.f, 0.22f, 0.22f, 0.18f, Rgb::Hex(0xCBB28E), false);
		break;
	case FxMesh::Chip:
		Put(Shape::Cube, 0.f, 0.f, 0.f, 0.1f, 0.05f, 0.03f, palette::Wood, false, 15.f, 30.f);
		break;
	case FxMesh::Shard:
		Put(Shape::Cube, 0.f, 0.f, 0.f, 0.08f, 0.08f, 0.12f, palette::Sunstone, true, 25.f, 40.f);
		break;
	case FxMesh::Gloom:
		Put(Shape::Sphere, 0.f, 0.f, 0.f, 0.18f, 0.18f, 0.18f, palette::GloamVioletLight, true);
		break;
	case FxMesh::Heal:
		Put(Shape::Sphere, 0.f, 0.f, 0.f, 0.1f, 0.1f, 0.1f, Rgb::Hex(0xA8F29A), true);
		break;
	case FxMesh::Ember:
		Put(Shape::Cube, 0.f, 0.f, 0.f, 0.06f, 0.06f, 0.06f, palette::Flame, true, 30.f, 10.f);
		break;
	case FxMesh::Smoke:
		Put(Shape::Sphere, 0.f, 0.f, 0.f, 0.3f, 0.3f, 0.26f, Rgb::Hex(0x6E6A72), false);
		break;
	case FxMesh::RallyFlag:
		Put(Shape::Cylinder, 0.f, 0.f, 0.35f, 0.04f, 0.04f, 0.7f, palette::WoodDark, false);
		Put(Shape::Cube, 0.12f, 0.f, 0.58f, 0.22f, 0.02f, 0.16f, palette::WardenBlue, false);
		Put(Shape::Sphere, 0.f, 0.f, 0.72f, 0.06f, 0.06f, 0.06f, palette::WardenGold, false);
		break;
	case FxMesh::Count:
		break;
	}
	AppendRenderMesh(M, 0.f, Palette, Out);
}

void BuildDecalQuad(RenderMesh& Out)
{
	RenderPushGroundQuad(Out.Opaque, Vec3(0.f, 0.f, 0.f), Vec2(0.5f, 0.f), Vec2(0.f, 0.5f));
}

// ================================================================================== Animation

float DeathDuration(Archetype A)
{
	const ArchetypeDef& D = GetDef(A);
	if (D.Kind == EntityKind::Unit)
	{
		return D.Faction == Team::Enemy ? 0.8f : 1.9f;
	}
	return 1.4f;
}

EntityPose EvaluateEntityPose(const AnimInput& In)
{
	EntityPose P;
	if (In.Type == Archetype::None || In.Type >= Archetype::Count)
	{
		return P;
	}
	const ArchetypeDef& D = GetDef(In.Type);
	const float Phase = RenderHash(In.Seed) * 6.2831f;
	const float T = In.Time + Phase;

	if (D.Kind == EntityKind::Unit)
	{
		if (In.DeathAge >= 0.f)
		{
			if (D.Faction == Team::Enemy)
			{
				// The Gloam dissolve into the ground.
				const float K = Saturate(In.DeathAge / DeathDuration(In.Type));
				P.ScaleXY = 1.f + K * 0.3f;
				P.ScaleZ = MaxF(0.02f, 1.f - K);
				P.Offset.Z = -0.1f * K;
				P.bVisible = K < 1.f;
				return P;
			}
			// Wardens topple over, then sink away.
			P.Roll = 82.f * SmoothStep(In.DeathAge / 0.35f);
			P.Offset.Z = -0.7f * SmoothStep((In.DeathAge - 0.9f) / 1.f);
			P.bVisible = In.DeathAge < DeathDuration(In.Type);
			return P;
		}
		if (In.bMoving)
		{
			const float Freq = In.Type == Archetype::StagRider ? 3.2f : 2.4f;
			P.Offset.Z = AbsF(std::sin(T * Freq * Pi)) * (In.Type == Archetype::BogTitan ? 0.06f : 0.035f);
			P.Pitch = -4.f;
		}
		else
		{
			P.ScaleZ = 1.f + 0.015f * std::sin(T * 2.1f);
		}
		if (In.HitAge < 0.18f)
		{
			const float K = 1.f - In.HitAge / 0.18f;
			P.ScaleXY *= 1.f + 0.09f * K;
			P.ScaleZ *= 1.f - 0.09f * K;
		}
		if (In.SpawnAge < 0.35f)
		{
			const float S = 0.4f + 0.6f * RenderEaseOutBack(In.SpawnAge / 0.35f);
			P.ScaleXY *= S;
			P.ScaleZ *= S;
		}
		return P;
	}

	if (D.Kind == EntityKind::Building)
	{
		if (In.DeathAge >= 0.f)
		{
			const float K = Saturate(In.DeathAge / DeathDuration(In.Type));
			P.Offset.Z = -1.6f * K * K;
			P.Offset.X = 0.05f * std::sin(In.DeathAge * 55.f) * (1.f - K);
			P.Roll = 4.f * K;
			P.bVisible = K < 1.f;
			return P;
		}
		if (!In.bConstructed)
		{
			P.ScaleZ = 0.12f + 0.88f * Saturate(In.BuildProgress);
		}
		if (In.HitAge < 0.25f)
		{
			const float K = 1.f - In.HitAge / 0.25f;
			P.Offset.X = 0.035f * K * std::sin(In.HitAge * 70.f);
		}
		if (In.SpawnAge < 0.3f)
		{
			P.ScaleZ *= 0.6f + 0.4f * RenderEaseOutBack(In.SpawnAge / 0.3f);
		}
		return P;
	}

	// Resource node: shrinks as it is mined out.
	const float R = Saturate(In.AmountRatio);
	P.ScaleZ = 0.45f + 0.55f * R;
	P.ScaleXY = 0.75f + 0.25f * R;
	if (In.HitAge < 0.2f)
	{
		P.ScaleXY *= 1.f + 0.04f * (1.f - In.HitAge / 0.2f);
	}
	return P;
}

PartPose EvaluateRolePose(PartRole Role, const AnimInput& In)
{
	PartPose O;
	if (!In.bConstructed || In.DeathAge >= 0.f)
	{
		O.bVisible = Role != PartRole::CarrySunstone && Role != PartRole::CarryTimber && In.bConstructed;
		return O;
	}
	const float Phase = RenderHash(In.Seed + 17u) * 6.2831f;
	const float T = In.Time + Phase;
	switch (Role)
	{
	case PartRole::Static:
		break;
	case PartRole::Weapon:
		if (In.SwingAge < SwingDuration)
		{
			O.Pitch = RenderSwingCurve(In.SwingAge / SwingDuration) * RenderSwingAmplitude(In.Type);
		}
		else if (In.Act == Activity::Gathering || In.Act == Activity::Building)
		{
			const float Period = In.Act == Activity::Building ? 0.62f : 0.9f;
			O.Pitch = RenderSwingCurve(std::fmod(T, Period) / Period) * 0.85f;
		}
		else if (In.bMoving)
		{
			O.Pitch = 7.f * std::sin(T * 7.5f);
		}
		break;
	case PartRole::Shield:
		if (In.Buff == BuffType::Brace)
		{
			O.Offset = Vec3(0.07f, 0.f, 0.05f);
			O.Yaw = -12.f;
		}
		break;
	case PartRole::Glow:
		O.Scale = 1.f + 0.1f * std::sin(T * 3.f);
		break;
	case PartRole::Flag:
		O.Yaw = 9.f * std::sin(T * 2.2f);
		break;
	case PartRole::Spin:
		O.Yaw = std::fmod(T * 45.f, 360.f);
		break;
	case PartRole::Mount:
		if (In.bMoving)
		{
			O.Offset.Z = 0.035f * AbsF(std::sin(T * 3.2f * Pi + 0.6f));
			O.Pitch = 5.f * std::sin(T * 3.2f * Pi);
		}
		break;
	case PartRole::CarrySunstone:
		O.bVisible = In.Carry == Resource::Sunstone;
		break;
	case PartRole::CarryTimber:
		O.bVisible = In.Carry == Resource::Timber;
		break;
	case PartRole::Flame:
		O.Scale = 1.f + 0.14f * std::sin(T * 13.1f) * std::sin(T * 7.3f + 1.f);
		break;
	}
	return O;
}

// ================================================================================== HUD helpers

bool FindTutorialMarker(const Session& S, Vec2& OutPos, float& OutHeight)
{
	const MissionRuntime& M = S.GetMission();
	const TutorialStep* Step = M.CurrentTutorialStep();
	if (Step == nullptr || Step->Marker == MarkerKind::None)
	{
		return false;
	}
	const World& W = S.GetWorld();
	const Vec2 Home = S.GetKeepPos();
	float Best = 1e30f;
	bool bFound = false;
	auto Consider = [&](const Vec2& P, float Height)
	{
		const float D = Vec2::DistSq(P, Home);
		if (D < Best)
		{
			Best = D;
			OutPos = P;
			OutHeight = Height;
			bFound = true;
		}
	};
	switch (Step->Marker)
	{
	case MarkerKind::OwnArch:
	case MarkerKind::EnemyArch:
	{
		const Team Want = Step->Marker == MarkerKind::OwnArch ? Team::Player : Team::Enemy;
		for (const Entity& E : W.GetEntities())
		{
			if (E.bAlive && E.Owner == Want && E.Type == Step->MarkerArch)
			{
				const ModelDef& Model = GetModel(E.Type);
				Consider(E.Pos, Model.Height * Model.Scale);
			}
		}
		break;
	}
	case MarkerKind::SunstoneNode:
		for (const Entity& E : W.GetEntities())
		{
			if (E.bAlive && E.IsResourceNode() && E.Amount > 0)
			{
				Consider(E.Pos, GetModel(E.Type).Height);
			}
		}
		break;
	case MarkerKind::Tree:
	{
		Tile T;
		if (W.GetMap().FindNearestTree(Home, 30.f, T))
		{
			Consider(Vec2(static_cast<float>(T.X) + 0.5f, static_cast<float>(T.Y) + 0.5f), 2.f);
		}
		break;
	}
	case MarkerKind::BeaconSite:
		for (const TileRect& R : W.GetMap().BeaconSites)
		{
			const MapTile& T = W.GetMap().At(R.X0, R.Y0);
			if (T.Occupant == NoEntity)
			{
				Consider(R.Center(), 0.8f);
			}
		}
		break;
	case MarkerKind::None:
		break;
	}
	return bFound;
}

void PaintMinimapOverlay(const Session& S, const ImageRGBA& Base, int PixelsPerTile, const Vec2 (&View)[4], bool bViewValid,
	const std::vector<MinimapPing>& Pings, ImageRGBA& Out)
{
	Out = Base;
	const float Ppt = static_cast<float>(MaxI(1, PixelsPerTile));
	const Rgb Mine = Rgb::Hex(0x5FA8FF);
	const Rgb MineBuilding = Rgb::Hex(0x3B6FD8);
	const Rgb Foe = Rgb::Hex(0xE0533D);
	const Rgb FoeBuilding = Rgb::Hex(0xB02E5A);
	const Rgb Gold = palette::Sunstone;
	const Rgb Edge = Rgb::Hex(0x10141A);
	for (const Entity& E : S.GetWorld().GetEntities())
	{
		if (!E.bAlive)
		{
			continue;
		}
		if (E.IsResourceNode())
		{
			MiniRect(Out, static_cast<int>(static_cast<float>(E.Rect.X0) * Ppt), static_cast<int>(static_cast<float>(E.Rect.Y0) * Ppt),
				static_cast<int>(static_cast<float>(E.Rect.X1) * Ppt), static_cast<int>(static_cast<float>(E.Rect.Y1) * Ppt), Gold, 1.f);
			continue;
		}
		const bool bMine = E.Owner == Team::Player;
		if (E.IsBuilding())
		{
			const int X0 = static_cast<int>(static_cast<float>(E.Rect.X0) * Ppt);
			const int Y0 = static_cast<int>(static_cast<float>(E.Rect.Y0) * Ppt);
			const int X1 = static_cast<int>(static_cast<float>(E.Rect.X1) * Ppt);
			const int Y1 = static_cast<int>(static_cast<float>(E.Rect.Y1) * Ppt);
			MiniRect(Out, X0, Y0, X1, Y1, Edge, 1.f);
			MiniRect(Out, X0 + 1, Y0 + 1, X1 - 1, Y1 - 1, bMine ? MineBuilding : FoeBuilding, E.bConstructed ? 1.f : 0.6f);
			continue;
		}
		const int Cx = static_cast<int>(E.Pos.X * Ppt);
		const int Cy = static_cast<int>(E.Pos.Y * Ppt);
		const int R = E.Type == Archetype::BogTitan ? 2 : 1;
		MiniRect(Out, Cx - R, Cy - R, Cx + R + 1, Cy + R + 1, bMine ? Mine : Foe, 1.f);
	}
	for (const MinimapPing& P : Pings)
	{
		const float K = Saturate(P.Age / MinimapPingLife);
		const float Radius = (2.f + 9.f * std::fmod(P.Age, 1.f)) * Ppt * 0.5f;
		MiniRing(Out, P.Pos.X * Ppt, P.Pos.Y * Ppt, Radius, P.bDanger ? Foe : palette::UiGold, 1.f - K);
	}
	if (bViewValid)
	{
		const Rgb White(255, 255, 255);
		for (int I = 0; I < 4; ++I)
		{
			const Vec2& A = View[I];
			const Vec2& B = View[(I + 1) % 4];
			MiniLine(Out, A.X * Ppt, A.Y * Ppt, B.X * Ppt, B.Y * Ppt, White, 0.9f);
		}
	}
}

Vec2 MinimapToWorld(const GameMap& Map, float U, float V)
{
	return Vec2(Saturate(U) * static_cast<float>(Map.GetWidth()), Saturate(V) * static_cast<float>(Map.GetHeight()));
}

float UiTexMargin(UiTex T)
{
	switch (T)
	{
	case UiTex::Circle:
	case UiTex::Vignette:
	case UiTex::Gradient:
		return 0.f;
	case UiTex::Highlight:
		return 0.35f;
	case UiTex::BarBack:
	case UiTex::BarFill:
		return 0.45f;
	default:
		return 0.3f;
	}
}

void PaintUiTexture(UiTex T, int Size, ImageRGBA& Out)
{
	const Rgba Ink = Rgba::Hex(0x1B1612);
	const Rgba Panel = Rgba::Hex(0x2A2233, 0.9f);
	const Rgba PanelDeep = Rgba::Hex(0x1C1724, 0.94f);
	const Rgba PanelLight = Rgba::Hex(0x3A3046);
	const Rgba Gold = Rgba::Hex(0xE7BE5C);
	const Rgba GoldDeep = Rgba::Hex(0xB9862E);
	const Rgba Parch = Rgba::Hex(0xF3E7C9);
	UiPanelSpec S;
	switch (T)
	{
	case UiTex::Panel:
		S.Top = Panel;
		S.Bottom = PanelDeep;
		S.Rim = Rgba::Hex(0x6B5D7D, 0.9f);
		S.RimWidth = 2.f;
		S.Highlight = Rgba(1.f, 1.f, 1.f, 0.12f);
		S.Radius = 0.25f;
		break;
	case UiTex::PanelGold:
		S.Top = Rgba::Hex(0x2E2638, 0.96f);
		S.Bottom = Rgba::Hex(0x1A1522, 0.97f);
		S.Rim = Gold;
		S.RimWidth = 2.5f;
		S.Highlight = Rgba(1.f, 0.9f, 0.6f, 0.18f);
		S.Radius = 0.25f;
		break;
	case UiTex::Parchment:
		S.Top = Parch;
		S.Bottom = Rgba::Hex(0xE6D3AA);
		S.Rim = Rgba::Hex(0x8C6A3C);
		S.RimWidth = 2.f;
		S.Radius = 0.2f;
		S.InnerShadow = 0.12f;
		break;
	case UiTex::Button:
		S.Top = PanelLight;
		S.Bottom = Rgba::Hex(0x2B2335);
		S.Rim = Rgba::Hex(0x8F7FA6);
		S.RimWidth = 2.f;
		S.Highlight = Rgba(1.f, 1.f, 1.f, 0.2f);
		S.Radius = 0.25f;
		break;
	case UiTex::ButtonHover:
		S.Top = Rgba::Hex(0x4A3E5A);
		S.Bottom = Rgba::Hex(0x352C41);
		S.Rim = Gold;
		S.RimWidth = 2.f;
		S.Highlight = Rgba(1.f, 1.f, 1.f, 0.25f);
		S.Radius = 0.25f;
		break;
	case UiTex::ButtonPressed:
		S.Top = Rgba::Hex(0x221C2B);
		S.Bottom = Rgba::Hex(0x2E2638);
		S.Rim = GoldDeep;
		S.RimWidth = 2.f;
		S.Radius = 0.25f;
		S.InnerShadow = 0.3f;
		break;
	case UiTex::ButtonGold:
		S.Top = Rgba::Hex(0xF2D17E);
		S.Bottom = Rgba::Hex(0xD7A443);
		S.Rim = Rgba::Hex(0x7A5418);
		S.RimWidth = 2.f;
		S.Highlight = Rgba(1.f, 1.f, 0.9f, 0.5f);
		S.Radius = 0.25f;
		break;
	case UiTex::ButtonGoldHover:
		S.Top = Rgba::Hex(0xFFE39A);
		S.Bottom = Rgba::Hex(0xE7B652);
		S.Rim = Rgba::Hex(0x7A5418);
		S.RimWidth = 2.f;
		S.Highlight = Rgba(1.f, 1.f, 1.f, 0.6f);
		S.Radius = 0.25f;
		break;
	case UiTex::ButtonGoldPressed:
		S.Top = Rgba::Hex(0xC99A3E);
		S.Bottom = Rgba::Hex(0xE2B458);
		S.Rim = Rgba::Hex(0x6A4814);
		S.RimWidth = 2.f;
		S.Radius = 0.25f;
		S.InnerShadow = 0.2f;
		break;
	case UiTex::Slot:
		S.Top = Rgba::Hex(0x362C42, 0.95f);
		S.Bottom = Rgba::Hex(0x241D2D, 0.95f);
		S.Rim = Rgba::Hex(0x7D6E92);
		S.RimWidth = 2.f;
		S.Highlight = Rgba(1.f, 1.f, 1.f, 0.16f);
		S.Radius = 0.2f;
		S.InnerShadow = 0.25f;
		break;
	case UiTex::SlotHot:
		S.Top = Rgba::Hex(0x5A4630, 0.97f);
		S.Bottom = Rgba::Hex(0x3A2C1E, 0.97f);
		S.Rim = Rgba::Hex(0xFFD978);
		S.RimWidth = 3.f;
		S.Highlight = Rgba(1.f, 0.95f, 0.7f, 0.35f);
		S.Radius = 0.2f;
		break;
	case UiTex::BarBack:
		S.Top = Rgba::Hex(0x0E0B12, 0.85f);
		S.Bottom = Rgba::Hex(0x1A1522, 0.85f);
		S.Rim = Rgba::Hex(0x000000, 0.6f);
		S.RimWidth = 1.5f;
		S.Radius = 0.45f;
		break;
	case UiTex::BarFill:
		S.Top = Rgba(1.f, 1.f, 1.f, 1.f);
		S.Bottom = Rgba(0.82f, 0.82f, 0.82f, 1.f);
		S.Highlight = Rgba(1.f, 1.f, 1.f, 0.5f);
		S.Radius = 0.45f;
		break;
	case UiTex::Circle:
		S.Top = Rgba(1.f, 1.f, 1.f, 1.f);
		S.Bottom = Rgba(1.f, 1.f, 1.f, 1.f);
		S.Radius = 0.5f;
		break;
	case UiTex::Vignette:
	{
		Out.Init(Size, Size);
		for (int Y = 0; Y < Size; ++Y)
		{
			for (int X = 0; X < Size; ++X)
			{
				const float Dx = (static_cast<float>(X) + 0.5f) / static_cast<float>(Size) - 0.5f;
				const float Dy = (static_cast<float>(Y) + 0.5f) / static_cast<float>(Size) - 0.5f;
				const float D = std::sqrt(Dx * Dx + Dy * Dy) * 1.414f;
				uint8_t* P = &Out.Px[static_cast<size_t>((Y * Size + X) * 4)];
				P[0] = 12;
				P[1] = 9;
				P[2] = 16;
				P[3] = static_cast<uint8_t>(Saturate(SmoothStep((D - 0.35f) / 0.65f) * 0.85f) * 255.f);
			}
		}
		return;
	}
	case UiTex::Gradient:
	{
		Out.Init(Size, Size);
		for (int Y = 0; Y < Size; ++Y)
		{
			const float A = SmoothStep(static_cast<float>(Y) / static_cast<float>(Size - 1));
			for (int X = 0; X < Size; ++X)
			{
				uint8_t* P = &Out.Px[static_cast<size_t>((Y * Size + X) * 4)];
				P[0] = static_cast<uint8_t>(Ink.R * 255.f);
				P[1] = static_cast<uint8_t>(Ink.G * 255.f);
				P[2] = static_cast<uint8_t>(Ink.B * 255.f);
				P[3] = static_cast<uint8_t>(A * 0.8f * 255.f);
			}
		}
		return;
	}
	case UiTex::Highlight:
	{
		// Rim only: a bright line with a soft glow inside, fully clear in the middle.
		Out.Init(Size, Size);
		const float Half = static_cast<float>(Size) * 0.5f - 0.5f;
		const float Radius = 0.2f * static_cast<float>(Size);
		for (int Y = 0; Y < Size; ++Y)
		{
			for (int X = 0; X < Size; ++X)
			{
				const float Px = static_cast<float>(X) + 0.5f - static_cast<float>(Size) * 0.5f;
				const float Py = static_cast<float>(Y) + 0.5f - static_cast<float>(Size) * 0.5f;
				const float D = UiRoundRectDist(Px, Py, Half, Radius);
				if (D > 0.5f)
				{
					continue;
				}
				const float Line = Saturate(1.f - AbsF(D + 2.5f) / 2.f);
				const float Glow = Saturate(1.f + D / (static_cast<float>(Size) * 0.12f)) * 0.55f;
				const float A = MaxF(Line, Glow * Glow) * Saturate(0.5f - D);
				uint8_t* P = &Out.Px[static_cast<size_t>((Y * Size + X) * 4)];
				P[0] = 255;
				P[1] = static_cast<uint8_t>(LerpF(200.f, 240.f, Line));
				P[2] = static_cast<uint8_t>(LerpF(90.f, 160.f, Line));
				P[3] = static_cast<uint8_t>(Saturate(A) * 255.f);
			}
		}
		return;
	}
	case UiTex::Count:
		Out.Init(Size, Size);
		return;
	}
	UiPaintPanel(Out, Size, S);
}

} // namespace bh
