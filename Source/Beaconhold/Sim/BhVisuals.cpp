// Beaconhold simulation core — primitive-based models.
#include "BhVisuals.h"

#include "BhData.h"
#include "BhMath.h"

#include <array>

namespace bh
{
namespace
{
using namespace palette;

// Small fluent helper to keep model definitions readable.
class ModelBuilder
{
public:
	explicit ModelBuilder(ModelDef& InModel) : M(InModel) {}

	ModelBuilder& Add(Shape S, float X, float Y, float Z, float SX, float SY, float SZ, Rgb C)
	{
		PartDef P;
		P.S = S;
		P.X = X;
		P.Y = Y;
		P.Z = Z;
		P.SX = SX;
		P.SY = SY;
		P.SZ = SZ;
		P.Color = C;
		M.Parts.push_back(P);
		M.Height = MaxF(M.Height, Z + SZ * 0.5f);
		return *this;
	}
	ModelBuilder& Box(float X, float Y, float Z, float SX, float SY, float SZ, Rgb C) { return Add(Shape::Cube, X, Y, Z, SX, SY, SZ, C); }
	ModelBuilder& Cyl(float X, float Y, float Z, float SX, float SY, float SZ, Rgb C) { return Add(Shape::Cylinder, X, Y, Z, SX, SY, SZ, C); }
	ModelBuilder& Ball(float X, float Y, float Z, float SX, float SY, float SZ, Rgb C) { return Add(Shape::Sphere, X, Y, Z, SX, SY, SZ, C); }
	ModelBuilder& Cone(float X, float Y, float Z, float SX, float SY, float SZ, Rgb C) { return Add(Shape::Cone, X, Y, Z, SX, SY, SZ, C); }

	ModelBuilder& Team(Paint P)
	{
		M.Parts.back().P = P;
		return *this;
	}
	ModelBuilder& Rot(float Pitch, float Yaw, float Roll)
	{
		PartDef& P = M.Parts.back();
		P.Pitch = Pitch;
		P.Yaw = Yaw;
		P.Roll = Roll;
		return *this;
	}
	ModelBuilder& Role(PartRole R)
	{
		M.Parts.back().Role = R;
		return *this;
	}
	ModelBuilder& Unlit()
	{
		M.Parts.back().bUnlit = true;
		return *this;
	}
	ModelBuilder& Shadow(float R)
	{
		M.ShadowRadius = R;
		return *this;
	}

	// Gable roof over walls of the given length (along the ridge) and width, with the eaves at EaveZ.
	// A 45-degree diamond as long as the walls fills the triangular gable ends (its lower half hides
	// inside the walls); two thin slabs form the roof with an overhang. Yaw 90 runs the ridge along Y.
	ModelBuilder& Gable(float X, float Y, float EaveZ, float WallLength, float WallWidth, Rgb RoofColor, Rgb WallColor, float Yaw = 0.f)
	{
		const float Half = WallWidth * 0.5f;
		const float Side = WallWidth * 0.7071f;
		const float Overhang = 0.12f;
		const float Thick = 0.08f;
		const bool bAlongY = Yaw > 45.f;
		Box(X, Y, EaveZ, WallLength, Side, Side, WallColor).Rot(0.f, Yaw, 45.f);
		for (int Sgn = -1; Sgn <= 1; Sgn += 2)
		{
			const float S = static_cast<float>(Sgn);
			const float Across = S * (Half * 0.5f + 0.7071f * Thick * 0.5f + 0.7071f * Overhang * 0.5f);
			const float Up = EaveZ + Half * 0.5f + 0.7071f * Thick * 0.5f - 0.7071f * Overhang * 0.5f;
			const float Px = bAlongY ? X - Across : X;
			const float Py = bAlongY ? Y : Y + Across;
			Box(Px, Py, Up, WallLength + Overhang * 2.f, Half * 1.4142f + Overhang, Thick, RoofColor).Rot(0.f, Yaw, S * 45.f);
			Team(RoofTeamPaint);
		}
		M.Height = MaxF(M.Height, EaveZ + Half + Thick);
		return *this;
	}
	// Paint applied to roof slabs created by Gable (team colour by default).
	Paint RoofTeamPaint = Paint::Team;

private:
	ModelDef& M;
};

// ------------------------------------------------------------------------------------------ Units

ModelDef MakeLamplighter()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Cyl(0.f, 0.f, 0.14f, 0.26f, 0.26f, 0.28f, Leather);
	B.Cyl(0.f, 0.f, 0.34f, 0.30f, 0.30f, 0.22f, Cloth);
	B.Box(0.f, 0.f, 0.34f, 0.31f, 0.31f, 0.06f, WardenBlue).Team(Paint::Team);
	B.Ball(0.f, 0.f, 0.56f, 0.26f, 0.26f, 0.26f, Skin);
	B.Cone(0.f, 0.f, 0.74f, 0.30f, 0.30f, 0.22f, WardenBlue).Team(Paint::Team);
	// Lantern pole (swings while working)
	B.Cyl(0.02f, 0.2f, 0.46f, 0.035f, 0.035f, 0.72f, Wood).Role(PartRole::Weapon);
	B.Ball(0.02f, 0.2f, 0.84f, 0.11f, 0.11f, 0.11f, LanternGlow).Unlit().Role(PartRole::Weapon);
	// Cargo
	B.Box(-0.17f, 0.f, 0.42f, 0.16f, 0.16f, 0.16f, Sunstone).Rot(20.f, 30.f, 15.f).Unlit().Role(PartRole::CarrySunstone);
	B.Cyl(-0.17f, -0.05f, 0.42f, 0.07f, 0.07f, 0.34f, Wood).Rot(90.f, 0.f, 0.f).Role(PartRole::CarryTimber);
	B.Cyl(-0.17f, 0.05f, 0.47f, 0.07f, 0.07f, 0.34f, WoodDark).Rot(90.f, 0.f, 0.f).Role(PartRole::CarryTimber);
	B.Shadow(0.32f);
	return M;
}

ModelDef MakeShieldbearer()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Cyl(0.f, 0.f, 0.15f, 0.30f, 0.30f, 0.30f, MetalDark);
	B.Box(0.f, 0.f, 0.36f, 0.34f, 0.34f, 0.30f, WardenBlue).Team(Paint::Team);
	B.Box(0.f, 0.f, 0.24f, 0.36f, 0.36f, 0.05f, WardenGold).Team(Paint::Accent);
	B.Ball(0.f, 0.f, 0.62f, 0.26f, 0.26f, 0.26f, Skin);
	B.Cyl(0.f, 0.f, 0.70f, 0.29f, 0.29f, 0.14f, Metal);
	B.Cone(0.f, 0.f, 0.83f, 0.08f, 0.08f, 0.14f, WardenGold).Team(Paint::Accent);
	// Shield (front-left) and sword (right)
	B.Cyl(0.18f, -0.17f, 0.38f, 0.38f, 0.38f, 0.05f, WardenBlue).Team(Paint::Team).Rot(90.f, 0.f, 0.f).Role(PartRole::Shield);
	B.Ball(0.21f, -0.17f, 0.38f, 0.09f, 0.09f, 0.09f, WardenGold).Team(Paint::Accent).Role(PartRole::Shield);
	B.Box(0.05f, 0.21f, 0.46f, 0.05f, 0.05f, 0.44f, Metal).Role(PartRole::Weapon);
	B.Box(0.05f, 0.21f, 0.27f, 0.06f, 0.16f, 0.04f, WardenGold).Team(Paint::Accent).Role(PartRole::Weapon);
	B.Shadow(0.36f);
	return M;
}

ModelDef MakeRanger()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Cyl(0.f, 0.f, 0.15f, 0.27f, 0.27f, 0.30f, Leather);
	B.Cyl(0.f, 0.f, 0.36f, 0.30f, 0.30f, 0.24f, Ranger);
	B.Box(0.f, 0.f, 0.30f, 0.31f, 0.31f, 0.05f, WardenBlue).Team(Paint::Team);
	B.Ball(0.f, 0.f, 0.58f, 0.24f, 0.24f, 0.24f, Skin);
	B.Cone(-0.02f, 0.f, 0.72f, 0.30f, 0.30f, 0.26f, WardenBlueDark).Team(Paint::TeamDark);
	B.Cone(-0.1f, 0.f, 0.38f, 0.34f, 0.34f, 0.50f, WardenBlueDark).Team(Paint::TeamDark).Rot(8.f, 0.f, 0.f);
	// Bow (left hand) and quiver
	B.Cyl(0.14f, -0.18f, 0.46f, 0.04f, 0.04f, 0.66f, Wood).Role(PartRole::Weapon);
	B.Box(0.11f, -0.18f, 0.46f, 0.012f, 0.012f, 0.6f, Cloth).Role(PartRole::Weapon);
	B.Cyl(-0.15f, 0.08f, 0.5f, 0.09f, 0.09f, 0.32f, Leather).Rot(0.f, 0.f, 15.f);
	B.Shadow(0.32f);
	return M;
}

ModelDef MakeStagRider()
{
	ModelDef M;
	ModelBuilder B(M);
	// Stag
	B.Box(0.f, 0.f, 0.46f, 0.72f, 0.30f, 0.30f, StagBrown).Role(PartRole::Mount);
	for (int I = 0; I < 4; ++I)
	{
		const float Lx = I < 2 ? 0.26f : -0.26f;
		const float Ly = (I % 2 == 0) ? 0.1f : -0.1f;
		B.Cyl(Lx, Ly, 0.16f, 0.07f, 0.07f, 0.34f, WoodDark).Role(PartRole::Mount);
	}
	B.Box(0.36f, 0.f, 0.66f, 0.16f, 0.15f, 0.34f, StagBrown).Rot(-25.f, 0.f, 0.f).Role(PartRole::Mount);
	B.Box(0.47f, 0.f, 0.82f, 0.24f, 0.14f, 0.14f, StagBrown).Role(PartRole::Mount);
	B.Box(0.44f, 0.09f, 0.98f, 0.03f, 0.03f, 0.26f, Bone).Rot(0.f, 0.f, 25.f).Role(PartRole::Mount);
	B.Box(0.44f, -0.09f, 0.98f, 0.03f, 0.03f, 0.26f, Bone).Rot(0.f, 0.f, -25.f).Role(PartRole::Mount);
	B.Box(0.40f, 0.14f, 1.06f, 0.14f, 0.03f, 0.03f, Bone).Role(PartRole::Mount);
	B.Box(0.40f, -0.14f, 1.06f, 0.14f, 0.03f, 0.03f, Bone).Role(PartRole::Mount);
	B.Box(-0.02f, 0.f, 0.63f, 0.32f, 0.34f, 0.05f, WardenBlue).Team(Paint::Team).Role(PartRole::Mount);
	// Rider
	B.Cyl(-0.02f, 0.f, 0.80f, 0.26f, 0.26f, 0.30f, WardenBlue).Team(Paint::Team).Role(PartRole::Mount);
	B.Ball(-0.02f, 0.f, 1.03f, 0.22f, 0.22f, 0.22f, Skin).Role(PartRole::Mount);
	B.Cone(-0.02f, 0.f, 1.17f, 0.24f, 0.24f, 0.2f, WardenGold).Team(Paint::Accent).Role(PartRole::Mount);
	B.Box(0.28f, 0.16f, 0.9f, 0.9f, 0.04f, 0.04f, Wood).Role(PartRole::Weapon);
	B.Cone(0.76f, 0.16f, 0.9f, 0.07f, 0.07f, 0.16f, Metal).Rot(-90.f, 0.f, 0.f).Role(PartRole::Weapon);
	B.Shadow(0.5f);
	return M;
}

ModelDef MakeSage()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Cone(0.f, 0.f, 0.32f, 0.44f, 0.44f, 0.64f, Cloth);
	B.Cyl(0.f, 0.f, 0.05f, 0.44f, 0.44f, 0.08f, WardenBlue).Team(Paint::Team);
	B.Cyl(0.f, 0.f, 0.46f, 0.2f, 0.2f, 0.08f, WardenGold).Team(Paint::Accent);
	B.Ball(0.f, 0.f, 0.66f, 0.24f, 0.24f, 0.24f, Skin);
	B.Cone(0.f, 0.f, 0.8f, 0.3f, 0.3f, 0.24f, WardenBlueLight).Team(Paint::TeamLight);
	B.Cyl(0.04f, 0.21f, 0.47f, 0.04f, 0.04f, 0.94f, Wood).Role(PartRole::Weapon);
	B.Ball(0.04f, 0.21f, 0.97f, 0.15f, 0.15f, 0.15f, LanternGlow).Unlit().Role(PartRole::Glow);
	B.Shadow(0.32f);
	return M;
}

ModelDef MakeGloomling()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Ball(0.f, 0.f, 0.22f, 0.38f, 0.34f, 0.36f, GloamSkin);
	B.Ball(0.1f, 0.f, 0.44f, 0.28f, 0.28f, 0.26f, GloamSkin);
	B.Ball(0.22f, 0.06f, 0.47f, 0.06f, 0.06f, 0.06f, GloamGreen).Unlit();
	B.Ball(0.22f, -0.06f, 0.47f, 0.06f, 0.06f, 0.06f, GloamGreen).Unlit();
	B.Cone(0.06f, 0.09f, 0.6f, 0.07f, 0.07f, 0.16f, GloamVioletDark).Rot(0.f, 0.f, 25.f);
	B.Cone(0.06f, -0.09f, 0.6f, 0.07f, 0.07f, 0.16f, GloamVioletDark).Rot(0.f, 0.f, -25.f);
	B.Box(0.16f, 0.17f, 0.24f, 0.16f, 0.05f, 0.05f, GloamViolet).Team(Paint::Team).Role(PartRole::Weapon);
	B.Box(0.16f, -0.17f, 0.24f, 0.16f, 0.05f, 0.05f, GloamViolet).Team(Paint::Team);
	B.Shadow(0.3f);
	return M;
}

ModelDef MakeThornback()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Box(0.f, 0.f, 0.42f, 0.62f, 0.56f, 0.46f, GloamBark).Rot(-8.f, 0.f, 0.f);
	B.Box(0.34f, 0.f, 0.44f, 0.28f, 0.3f, 0.26f, GloamBarkLight);
	B.Ball(0.48f, 0.07f, 0.48f, 0.06f, 0.06f, 0.06f, GloamGreen).Unlit();
	B.Ball(0.48f, -0.07f, 0.48f, 0.06f, 0.06f, 0.06f, GloamGreen).Unlit();
	const float SpikeX[5] = {-0.2f, 0.f, 0.18f, -0.08f, 0.1f};
	const float SpikeY[5] = {0.f, 0.12f, -0.1f, -0.16f, 0.16f};
	for (int I = 0; I < 5; ++I)
	{
		B.Cone(SpikeX[I], SpikeY[I], 0.76f, 0.13f, 0.13f, 0.32f, Thorn).Rot(-10.f + static_cast<float>(I) * 5.f, 0.f, SpikeY[I] * 80.f);
	}
	B.Cyl(0.08f, 0.36f, 0.3f, 0.15f, 0.15f, 0.48f, GloamViolet).Team(Paint::Team).Role(PartRole::Weapon);
	B.Cyl(0.08f, -0.36f, 0.3f, 0.15f, 0.15f, 0.48f, GloamViolet).Team(Paint::Team);
	B.Cyl(-0.12f, 0.16f, 0.12f, 0.16f, 0.16f, 0.24f, GloamBark);
	B.Cyl(-0.12f, -0.16f, 0.12f, 0.16f, 0.16f, 0.24f, GloamBark);
	B.Shadow(0.5f);
	return M;
}

ModelDef MakeHexer()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Cone(0.f, 0.f, 0.46f, 0.44f, 0.44f, 0.72f, GloamViolet).Team(Paint::Team);
	B.Cyl(0.f, 0.f, 0.14f, 0.3f, 0.3f, 0.06f, GloamVioletDark).Team(Paint::TeamDark);
	B.Ball(0.02f, 0.f, 0.84f, 0.22f, 0.22f, 0.22f, Bone);
	B.Cone(-0.02f, 0.f, 0.97f, 0.3f, 0.3f, 0.32f, GloamVioletDark).Team(Paint::TeamDark);
	B.Ball(0.13f, 0.05f, 0.86f, 0.04f, 0.04f, 0.04f, GloamGreen).Unlit();
	B.Ball(0.13f, -0.05f, 0.86f, 0.04f, 0.04f, 0.04f, GloamGreen).Unlit();
	B.Ball(0.2f, 0.2f, 0.58f, 0.15f, 0.15f, 0.15f, GloamGreen).Unlit().Role(PartRole::Weapon);
	B.Shadow(0.32f);
	return M;
}

ModelDef MakeBogTitan()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Ball(0.f, 0.f, 0.95f, 1.3f, 1.2f, 1.25f, Moss);
	B.Ball(0.45f, 0.f, 1.42f, 0.55f, 0.55f, 0.5f, GloamBark);
	B.Ball(0.68f, 0.12f, 1.47f, 0.1f, 0.1f, 0.1f, GloamGreen).Unlit();
	B.Ball(0.68f, -0.12f, 1.47f, 0.1f, 0.1f, 0.1f, GloamGreen).Unlit();
	B.Cyl(0.15f, 0.72f, 0.62f, 0.36f, 0.36f, 1.05f, Moss).Role(PartRole::Weapon);
	B.Cyl(0.15f, -0.72f, 0.62f, 0.36f, 0.36f, 1.05f, Moss);
	B.Cyl(-0.1f, 0.34f, 0.24f, 0.4f, 0.4f, 0.5f, GloamBark);
	B.Cyl(-0.1f, -0.34f, 0.24f, 0.4f, 0.4f, 0.5f, GloamBark);
	B.Ball(-0.35f, 0.3f, 1.45f, 0.3f, 0.3f, 0.3f, GloamViolet).Team(Paint::Team);
	B.Ball(-0.3f, -0.35f, 1.35f, 0.26f, 0.26f, 0.26f, GloamGreen).Unlit().Role(PartRole::Glow);
	B.Cone(-0.2f, 0.f, 1.65f, 0.22f, 0.22f, 0.5f, Thorn).Rot(-20.f, 0.f, 0.f);
	B.Cone(-0.05f, 0.35f, 1.6f, 0.2f, 0.2f, 0.42f, Thorn).Rot(0.f, 0.f, 25.f);
	B.Shadow(0.85f);
	return M;
}

// ------------------------------------------------------------------------------------------ Buildings

void RoundTower(ModelBuilder& B, float X, float Y, float BaseZ, float Diameter, float Height, float RoofScale)
{
	B.Cyl(X, Y, BaseZ + Height * 0.5f, Diameter, Diameter, Height, Stone);
	B.Cyl(X, Y, BaseZ + Height - 0.04f, Diameter * 1.08f, Diameter * 1.08f, 0.1f, StoneDark);
	B.Cone(X, Y, BaseZ + Height + 0.36f * RoofScale, Diameter * 1.25f, Diameter * 1.25f, 0.8f * RoofScale, WardenBlue).Team(Paint::Team);
	B.Ball(X, Y, BaseZ + Height + 0.8f * RoofScale, 0.1f, 0.1f, 0.1f, WardenGold).Team(Paint::Accent);
}

ModelDef MakeKeep()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Box(0.f, 0.f, 0.12f, 3.9f, 3.9f, 0.24f, StoneDark);
	B.Box(0.f, 0.f, 1.0f, 2.7f, 2.7f, 1.6f, Stone);
	B.Box(0.f, 0.f, 1.84f, 2.9f, 2.9f, 0.14f, StoneDark);
	for (int I = 0; I < 5; ++I)
	{
		const float T = -1.1f + static_cast<float>(I) * 0.55f;
		B.Box(T, 1.36f, 2.02f, 0.26f, 0.18f, 0.22f, Stone);
		B.Box(T, -1.36f, 2.02f, 0.26f, 0.18f, 0.22f, Stone);
		B.Box(1.36f, T, 2.02f, 0.18f, 0.26f, 0.22f, Stone);
		B.Box(-1.36f, T, 2.02f, 0.18f, 0.26f, 0.22f, Stone);
	}
	// Central lantern tower
	B.Cyl(0.f, 0.f, 2.55f, 1.15f, 1.15f, 1.3f, Stone);
	for (int I = 0; I < 4; ++I)
	{
		const float Px = (I < 2 ? 0.38f : -0.38f);
		const float Py = (I % 2 == 0 ? 0.38f : -0.38f);
		B.Box(Px, Py, 3.45f, 0.1f, 0.1f, 0.55f, WoodDark);
	}
	B.Ball(0.f, 0.f, 3.45f, 0.55f, 0.55f, 0.55f, LanternGlow).Unlit().Role(PartRole::Flame);
	B.Cone(0.f, 0.f, 4.15f, 1.45f, 1.45f, 0.95f, WardenBlue).Team(Paint::Team);
	B.Ball(0.f, 0.f, 4.68f, 0.2f, 0.2f, 0.2f, WardenGold).Team(Paint::Accent);
	// Corner towers
	for (int I = 0; I < 4; ++I)
	{
		const float Px = (I < 2 ? 1.45f : -1.45f);
		const float Py = (I % 2 == 0 ? 1.45f : -1.45f);
		RoundTower(B, Px, Py, 0.24f, 0.8f, 2.1f, 0.9f);
	}
	// Gate and banners (front = +X)
	B.Box(1.37f, 0.f, 0.7f, 0.12f, 0.7f, 0.95f, WoodDark);
	B.Box(1.4f, 0.f, 1.24f, 0.12f, 0.8f, 0.14f, WardenGold).Team(Paint::Accent);
	B.Box(1.38f, 0.72f, 1.3f, 0.05f, 0.34f, 0.7f, WardenBlue).Team(Paint::Team).Role(PartRole::Flag);
	B.Box(1.38f, -0.72f, 1.3f, 0.05f, 0.34f, 0.7f, WardenBlue).Team(Paint::Team).Role(PartRole::Flag);
	B.Shadow(2.2f);
	return M;
}

ModelDef MakeCottage()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Box(0.f, 0.f, 0.08f, 1.75f, 1.75f, 0.16f, StoneDark);
	B.Box(0.f, 0.f, 0.55f, 1.5f, 1.35f, 0.8f, Stone);
	B.Gable(0.f, 0.f, 0.95f, 1.5f, 1.35f, WardenBlue, Stone);
	B.Box(-0.4f, 0.35f, 1.35f, 0.22f, 0.22f, 0.6f, StoneDark);
	B.Box(0.76f, 0.f, 0.4f, 0.06f, 0.34f, 0.55f, WoodDark);
	B.Box(0.76f, 0.42f, 0.6f, 0.05f, 0.22f, 0.22f, LanternGlow).Unlit();
	B.Box(0.76f, -0.42f, 0.6f, 0.05f, 0.22f, 0.22f, LanternGlow).Unlit();
	B.Shadow(1.1f);
	return M;
}

ModelDef MakeStorehouse()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Box(0.f, 0.f, 0.08f, 1.8f, 1.8f, 0.16f, StoneDark);
	B.Box(-0.15f, 0.f, 0.52f, 1.3f, 1.5f, 0.75f, Wood);
	B.RoofTeamPaint = Paint::TeamDark;
	B.Gable(-0.15f, 0.f, 0.89f, 1.3f, 1.5f, WardenBlueDark, Wood);
	B.Box(0.53f, 0.f, 0.45f, 0.05f, 0.6f, 0.6f, WoodDark);
	B.Box(0.72f, 0.5f, 0.2f, 0.3f, 0.3f, 0.3f, Wood).Rot(0.f, 12.f, 0.f);
	B.Box(0.7f, 0.5f, 0.43f, 0.2f, 0.2f, 0.16f, Sunstone).Unlit();
	B.Cyl(0.72f, -0.45f, 0.12f, 0.16f, 0.16f, 0.62f, Wood).Rot(90.f, 0.f, 0.f);
	B.Cyl(0.72f, -0.45f, 0.28f, 0.16f, 0.16f, 0.62f, WoodDark).Rot(90.f, 0.f, 0.f);
	B.Shadow(1.1f);
	return M;
}

ModelDef MakeBarracks()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Box(0.f, 0.f, 0.1f, 2.8f, 2.8f, 0.2f, StoneDark);
	B.Box(-0.15f, 0.f, 0.75f, 2.3f, 1.8f, 1.1f, Stone);
	B.Gable(-0.15f, 0.f, 1.3f, 2.3f, 1.8f, WardenBlue, Stone);
	B.Box(1.0f, 0.f, 0.6f, 0.08f, 0.7f, 0.85f, WoodDark);
	B.Box(1.02f, 0.f, 1.08f, 0.06f, 0.9f, 0.12f, WardenGold).Team(Paint::Accent);
	B.Box(1.02f, 0.f, 1.62f, 0.05f, 0.3f, 0.3f, WardenGold).Team(Paint::Accent).Rot(0.f, 0.f, 45.f);
	for (int I = 0; I < 2; ++I)
	{
		const float Py = I == 0 ? 1.05f : -1.05f;
		B.Cyl(1.2f, Py, 0.9f, 0.07f, 0.07f, 1.6f, Wood);
		B.Box(1.2f, Py + (I == 0 ? -0.2f : 0.2f), 1.45f, 0.04f, 0.38f, 0.44f, WardenBlue).Team(Paint::Team).Role(PartRole::Flag);
	}
	B.Box(1.25f, 0.55f, 0.35f, 0.08f, 0.5f, 0.5f, Wood);
	B.Box(1.27f, 0.45f, 0.45f, 0.03f, 0.03f, 0.55f, Metal).Rot(0.f, 0.f, 12.f);
	B.Box(1.27f, 0.65f, 0.45f, 0.03f, 0.03f, 0.55f, Metal).Rot(0.f, 0.f, -12.f);
	B.Shadow(1.6f);
	return M;
}

ModelDef MakeForge()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Box(0.f, 0.f, 0.1f, 2.8f, 2.8f, 0.2f, StoneDark);
	B.Box(-0.2f, 0.f, 0.7f, 2.0f, 2.2f, 1.0f, StoneGrey);
	B.RoofTeamPaint = Paint::TeamDark;
	B.Gable(-0.2f, 0.f, 1.2f, 2.0f, 2.2f, WardenBlueDark, StoneGrey);
	B.Cyl(-0.75f, 0.75f, 1.6f, 0.55f, 0.55f, 1.8f, StoneDark);
	B.Cyl(-0.75f, 0.75f, 2.55f, 0.62f, 0.62f, 0.12f, MetalDark);
	B.Box(0.82f, 0.f, 0.55f, 0.06f, 0.8f, 0.55f, Flame).Unlit().Role(PartRole::Flame);
	B.Box(1.15f, 0.45f, 0.3f, 0.38f, 0.22f, 0.2f, MetalDark);
	B.Box(1.15f, 0.45f, 0.45f, 0.5f, 0.18f, 0.1f, Metal);
	B.Box(1.1f, -0.55f, 0.25f, 0.4f, 0.4f, 0.5f, Wood);
	B.Shadow(1.6f);
	return M;
}

ModelDef MakeStagLodge()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Box(0.f, 0.f, 0.08f, 2.8f, 2.8f, 0.16f, DirtDark);
	B.Box(-0.2f, 0.f, 0.62f, 2.2f, 1.7f, 0.9f, Wood);
	B.Gable(-0.2f, 0.f, 1.07f, 2.2f, 1.7f, WardenBlue, Wood);
	B.Box(0.92f, 0.f, 0.5f, 0.06f, 0.7f, 0.7f, WoodDark);
	B.Box(0.93f, 0.f, 1.3f, 0.05f, 0.16f, 0.14f, Bone);
	B.Box(0.94f, 0.2f, 1.45f, 0.03f, 0.03f, 0.36f, Bone).Rot(0.f, 0.f, 35.f);
	B.Box(0.94f, -0.2f, 1.45f, 0.03f, 0.03f, 0.36f, Bone).Rot(0.f, 0.f, -35.f);
	for (int I = 0; I < 5; ++I)
	{
		const float Py = -1.2f + static_cast<float>(I) * 0.6f;
		if (I == 2)
		{
			continue; // gate opening
		}
		B.Cyl(1.25f, Py, 0.3f, 0.08f, 0.08f, 0.6f, Wood);
	}
	B.Box(1.25f, 0.9f, 0.45f, 0.05f, 0.7f, 0.06f, Wood);
	B.Box(1.25f, -0.9f, 0.45f, 0.05f, 0.7f, 0.06f, Wood);
	B.Box(1.1f, 0.85f, 0.2f, 0.3f, 0.5f, 0.2f, Rgb::Hex(0xD9B45A));
	B.Shadow(1.6f);
	return M;
}

ModelDef MakeSanctum()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Cyl(0.f, 0.f, 0.15f, 2.7f, 2.7f, 0.3f, StoneDark);
	B.Cyl(0.f, 0.f, 0.34f, 2.3f, 2.3f, 0.1f, Stone);
	for (int I = 0; I < 8; ++I)
	{
		const float A = TwoPi * static_cast<float>(I) / 8.f;
		B.Cyl(std::cos(A) * 0.95f, std::sin(A) * 0.95f, 1.05f, 0.2f, 0.2f, 1.35f, Cloth);
	}
	B.Cyl(0.f, 0.f, 1.78f, 2.2f, 2.2f, 0.14f, WardenGold).Team(Paint::Accent);
	B.Ball(0.f, 0.f, 1.95f, 1.9f, 1.9f, 1.1f, WardenBlueLight).Team(Paint::TeamLight);
	B.Cone(0.f, 0.f, 2.75f, 0.35f, 0.35f, 0.6f, SunstoneLight).Unlit().Role(PartRole::Glow);
	B.Cyl(0.f, 0.f, 0.9f, 0.7f, 0.7f, 1.1f, Stone);
	B.Ball(0.f, 0.f, 1.2f, 0.4f, 0.4f, 0.4f, LanternGlow).Unlit().Role(PartRole::Glow);
	B.Shadow(1.5f);
	return M;
}

ModelDef MakeWatchtower()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Box(0.f, 0.f, 0.1f, 1.7f, 1.7f, 0.2f, StoneDark);
	B.Cyl(0.f, 0.f, 1.3f, 1.05f, 1.05f, 2.4f, Stone);
	B.Cyl(0.f, 0.f, 2.55f, 1.45f, 1.45f, 0.18f, Wood);
	for (int I = 0; I < 6; ++I)
	{
		const float A = TwoPi * static_cast<float>(I) / 6.f;
		B.Box(std::cos(A) * 0.62f, std::sin(A) * 0.62f, 2.78f, 0.18f, 0.18f, 0.3f, Stone);
	}
	B.Ball(0.f, 0.f, 2.9f, 0.3f, 0.3f, 0.3f, LanternGlow).Unlit().Role(PartRole::Flame);
	B.Cone(0.f, 0.f, 3.45f, 1.6f, 1.6f, 0.9f, WardenBlue).Team(Paint::Team);
	B.Ball(0.f, 0.f, 3.95f, 0.14f, 0.14f, 0.14f, WardenGold).Team(Paint::Accent);
	B.Box(0.53f, 0.f, 1.5f, 0.06f, 0.26f, 0.4f, WoodDark);
	B.Shadow(1.f);
	return M;
}

ModelDef MakeBeacon()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Box(0.f, 0.f, 0.14f, 1.8f, 1.8f, 0.28f, StoneDark);
	B.Cyl(0.f, 0.f, 0.36f, 1.4f, 1.4f, 0.16f, Stone);
	B.Cyl(0.f, 0.f, 1.25f, 0.62f, 0.62f, 1.7f, Stone);
	B.Cyl(0.f, 0.f, 2.14f, 1.05f, 1.05f, 0.24f, WardenGold).Team(Paint::Accent);
	B.Cone(0.f, 0.f, 2.72f, 0.78f, 0.78f, 1.0f, Flame).Unlit().Role(PartRole::Flame);
	B.Ball(0.f, 0.f, 2.52f, 0.5f, 0.5f, 0.5f, FlameCore).Unlit().Role(PartRole::Flame);
	for (int I = 0; I < 4; ++I)
	{
		const float A = TwoPi * static_cast<float>(I) / 4.f + Pi * 0.25f;
		B.Box(std::cos(A) * 0.72f, std::sin(A) * 0.72f, 0.62f, 0.16f, 0.16f, 0.5f, StoneDark);
	}
	B.Shadow(1.1f);
	return M;
}

ModelDef MakeGloamHeart()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Ball(0.f, 0.f, 0.45f, 3.7f, 3.7f, 1.3f, GloamBark);
	for (int I = 0; I < 7; ++I)
	{
		const float Yaw = static_cast<float>(I) * 51.4f;
		const float A = Yaw * Pi / 180.f;
		B.Cyl(std::cos(A) * 1.3f, std::sin(A) * 1.3f, 0.45f, 0.32f, 0.32f, 1.8f, GloamBarkLight).Rot(65.f, Yaw, 0.f);
	}
	B.Ball(0.f, 0.f, 1.75f, 1.5f, 1.5f, 1.5f, GloamCore).Unlit().Role(PartRole::Glow);
	for (int I = 0; I < 5; ++I)
	{
		const float Yaw = static_cast<float>(I) * 72.f;
		const float A = Yaw * Pi / 180.f;
		B.Cone(std::cos(A) * 0.95f, std::sin(A) * 0.95f, 2.1f, 0.34f, 0.34f, 2.0f, Thorn).Rot(-22.f, Yaw, 0.f);
	}
	B.Cone(0.f, 0.f, 3.3f, 0.55f, 0.55f, 1.6f, GloamBark);
	B.Ball(0.f, 0.f, 4.15f, 0.3f, 0.3f, 0.3f, GloamGreen).Unlit().Role(PartRole::Glow);
	B.Shadow(2.3f);
	return M;
}

ModelDef MakeBurrow()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Ball(0.f, 0.f, 0.35f, 2.7f, 2.7f, 1.2f, GloamBarkLight);
	B.Ball(0.95f, 0.f, 0.45f, 0.95f, 1.0f, 0.75f, Void).Unlit();
	B.Ball(1.2f, 0.18f, 0.5f, 0.08f, 0.08f, 0.08f, GloamGreen).Unlit();
	B.Ball(1.2f, -0.18f, 0.5f, 0.08f, 0.08f, 0.08f, GloamGreen).Unlit();
	for (int I = 0; I < 6; ++I)
	{
		const float Yaw = 40.f + static_cast<float>(I) * 55.f;
		const float A = Yaw * Pi / 180.f;
		B.Cone(std::cos(A) * 0.9f, std::sin(A) * 0.9f, 0.95f, 0.22f, 0.22f, 0.9f, Thorn).Rot(-25.f, Yaw, 0.f);
	}
	B.Ball(-0.4f, 0.5f, 0.95f, 0.35f, 0.35f, 0.35f, GloamViolet).Team(Paint::Team);
	B.Shadow(1.5f);
	return M;
}

ModelDef MakeHexroot()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Ball(0.f, 0.f, 0.15f, 2.5f, 2.5f, 0.5f, GloamBark);
	B.Cyl(0.f, 0.f, 1.2f, 0.75f, 0.75f, 2.1f, GloamBarkLight).Rot(0.f, 0.f, 6.f);
	for (int I = 0; I < 3; ++I)
	{
		const float Yaw = static_cast<float>(I) * 120.f;
		const float A = Yaw * Pi / 180.f;
		B.Cone(std::cos(A) * 0.55f, std::sin(A) * 0.55f, 0.4f, 0.35f, 0.35f, 0.9f, GloamBark).Rot(40.f, Yaw, 0.f);
		B.Cone(std::cos(A) * 0.7f, std::sin(A) * 0.7f, 2.2f, 0.9f, 0.9f, 0.9f, GloamViolet).Team(Paint::Team).Rot(-30.f, Yaw, 0.f);
		B.Ball(std::cos(A) * 1.05f, std::sin(A) * 1.05f, 1.55f, 0.22f, 0.22f, 0.22f, GloamGreen).Unlit().Role(PartRole::Glow);
	}
	B.Ball(0.f, 0.f, 2.55f, 1.1f, 1.1f, 0.8f, GloamVioletDark).Team(Paint::TeamDark);
	B.Ball(0.45f, 0.f, 0.55f, 0.3f, 0.3f, 0.3f, Bone);
	B.Shadow(1.5f);
	return M;
}

ModelDef MakeThornSpire()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Ball(0.f, 0.f, 0.15f, 1.7f, 1.7f, 0.45f, GloamBark);
	B.Cone(0.f, 0.f, 1.5f, 1.15f, 1.15f, 3.0f, GloamBarkLight);
	for (int I = 0; I < 4; ++I)
	{
		const float Yaw = static_cast<float>(I) * 90.f + 20.f;
		const float A = Yaw * Pi / 180.f;
		B.Cone(std::cos(A) * 0.4f, std::sin(A) * 0.4f, 1.2f + static_cast<float>(I) * 0.35f, 0.18f, 0.18f, 0.7f, Thorn).Rot(-60.f, Yaw, 0.f);
	}
	B.Ball(0.f, 0.f, 3.1f, 0.4f, 0.4f, 0.4f, GloamGreen).Unlit().Role(PartRole::Glow);
	B.Shadow(1.f);
	return M;
}

ModelDef MakeSunstoneNode()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Box(0.1f, -0.2f, 0.15f, 1.5f, 1.3f, 0.35f, StoneGrey).Rot(0.f, 20.f, 0.f);
	B.Box(-0.4f, 0.45f, 0.15f, 0.8f, 0.8f, 0.32f, RockLight).Rot(0.f, -15.f, 0.f);
	B.Cone(0.f, 0.f, 0.95f, 0.7f, 0.7f, 1.6f, Sunstone).Unlit().Rot(8.f, 0.f, 6.f);
	B.Cone(0.42f, 0.35f, 0.72f, 0.52f, 0.52f, 1.15f, SunstoneLight).Unlit().Rot(-10.f, 0.f, 22.f);
	B.Cone(-0.42f, -0.3f, 0.66f, 0.55f, 0.55f, 1.05f, SunstoneDeep).Rot(18.f, 0.f, -15.f);
	B.Cone(0.38f, -0.5f, 0.52f, 0.4f, 0.4f, 0.75f, Sunstone).Rot(-20.f, 0.f, -25.f);
	B.Box(-0.52f, 0.42f, 0.48f, 0.3f, 0.3f, 0.48f, SunstoneLight).Unlit().Rot(15.f, 30.f, 10.f);
	B.Shadow(1.1f);
	return M;
}

ModelDef MakePine()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Cyl(0.f, 0.f, 0.25f, 0.18f, 0.18f, 0.5f, Bark);
	B.Cone(0.f, 0.f, 0.8f, 1.05f, 1.05f, 0.85f, Pine);
	B.Cone(0.f, 0.f, 1.28f, 0.82f, 0.82f, 0.75f, PineLight);
	B.Cone(0.f, 0.f, 1.7f, 0.55f, 0.55f, 0.65f, Pine);
	B.Shadow(0.5f);
	return M;
}

ModelDef MakeOak()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Cyl(0.f, 0.f, 0.35f, 0.22f, 0.22f, 0.7f, Bark);
	B.Ball(0.f, 0.f, 1.05f, 1.05f, 1.05f, 0.95f, Oak);
	B.Ball(0.22f, 0.18f, 1.32f, 0.72f, 0.72f, 0.65f, OakLight);
	B.Ball(-0.2f, -0.22f, 1.25f, 0.66f, 0.66f, 0.6f, Oak);
	B.Shadow(0.55f);
	return M;
}

ModelDef MakeDeadTree()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Cyl(0.f, 0.f, 0.6f, 0.2f, 0.2f, 1.2f, DeadTree).Rot(0.f, 0.f, 5.f);
	B.Box(0.15f, 0.f, 0.95f, 0.06f, 0.06f, 0.55f, DeadTree).Rot(0.f, 0.f, 40.f);
	B.Box(-0.12f, 0.08f, 1.1f, 0.05f, 0.05f, 0.45f, DeadTree).Rot(0.f, 60.f, -35.f);
	B.Ball(0.f, 0.f, 1.25f, 0.12f, 0.12f, 0.12f, GloamGreen).Unlit();
	B.Shadow(0.35f);
	return M;
}

ModelDef MakeStump()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Cyl(0.f, 0.f, 0.08f, 0.26f, 0.26f, 0.16f, Bark);
	B.Cyl(0.f, 0.f, 0.165f, 0.2f, 0.2f, 0.01f, Rgb::Hex(0xD9B98A));
	return M;
}

ModelDef MakeBeaconRuin()
{
	ModelDef M;
	ModelBuilder B(M);
	B.Box(0.f, 0.f, 0.08f, 1.8f, 1.8f, 0.16f, StoneDark);
	B.Cyl(0.f, 0.f, 0.35f, 0.62f, 0.62f, 0.5f, StoneDark).Rot(6.f, 0.f, 4.f);
	B.Box(0.55f, 0.45f, 0.2f, 0.3f, 0.2f, 0.25f, StoneGrey).Rot(0.f, 25.f, 10.f);
	B.Box(-0.5f, -0.4f, 0.14f, 0.35f, 0.25f, 0.18f, StoneGrey).Rot(0.f, -30.f, 0.f);
	B.Box(-0.45f, 0.5f, 0.12f, 0.2f, 0.2f, 0.14f, Stone).Rot(0.f, 40.f, 0.f);
	B.Ball(0.f, 0.f, 0.64f, 0.16f, 0.16f, 0.16f, Rgb::Hex(0xFFB060)).Unlit().Role(PartRole::Glow);
	return M;
}

ModelDef MakeScaffold(int Footprint)
{
	ModelDef M;
	ModelBuilder B(M);
	const float H = static_cast<float>(Footprint) * 0.45f - 0.05f;
	for (int I = 0; I < 4; ++I)
	{
		const float Px = (I < 2 ? H : -H);
		const float Py = (I % 2 == 0 ? H : -H);
		B.Cyl(Px, Py, 0.6f, 0.08f, 0.08f, 1.2f, Wood);
	}
	B.Box(H, 0.f, 0.9f, 0.06f, H * 2.f, 0.06f, WoodDark);
	B.Box(-H, 0.f, 0.9f, 0.06f, H * 2.f, 0.06f, WoodDark);
	B.Box(0.f, H, 0.5f, H * 2.f, 0.06f, 0.06f, WoodDark);
	B.Box(0.f, -H, 0.5f, H * 2.f, 0.06f, 0.06f, WoodDark);
	return M;
}

std::array<ModelDef, NumArchetypes> BuildModels()
{
	std::array<ModelDef, NumArchetypes> T{};
	T[ArchIndex(Archetype::Lamplighter)] = MakeLamplighter();
	T[ArchIndex(Archetype::Shieldbearer)] = MakeShieldbearer();
	T[ArchIndex(Archetype::Ranger)] = MakeRanger();
	T[ArchIndex(Archetype::StagRider)] = MakeStagRider();
	T[ArchIndex(Archetype::Sage)] = MakeSage();
	T[ArchIndex(Archetype::Gloomling)] = MakeGloomling();
	T[ArchIndex(Archetype::Thornback)] = MakeThornback();
	T[ArchIndex(Archetype::Hexer)] = MakeHexer();
	T[ArchIndex(Archetype::BogTitan)] = MakeBogTitan();
	T[ArchIndex(Archetype::Keep)] = MakeKeep();
	T[ArchIndex(Archetype::Cottage)] = MakeCottage();
	T[ArchIndex(Archetype::Storehouse)] = MakeStorehouse();
	T[ArchIndex(Archetype::Barracks)] = MakeBarracks();
	T[ArchIndex(Archetype::Forge)] = MakeForge();
	T[ArchIndex(Archetype::StagLodge)] = MakeStagLodge();
	T[ArchIndex(Archetype::Sanctum)] = MakeSanctum();
	T[ArchIndex(Archetype::Watchtower)] = MakeWatchtower();
	T[ArchIndex(Archetype::Beacon)] = MakeBeacon();
	T[ArchIndex(Archetype::GloamHeart)] = MakeGloamHeart();
	T[ArchIndex(Archetype::Burrow)] = MakeBurrow();
	T[ArchIndex(Archetype::Hexroot)] = MakeHexroot();
	T[ArchIndex(Archetype::ThornSpire)] = MakeThornSpire();
	T[ArchIndex(Archetype::SunstoneNode)] = MakeSunstoneNode();
	for (int A = 0; A < NumArchetypes; ++A)
	{
		if (GetDef(static_cast<Archetype>(A)).Kind == EntityKind::Unit)
		{
			T[static_cast<size_t>(A)].Scale = static_cast<Archetype>(A) == Archetype::BogTitan ? 1.15f : 1.3f;
		}
	}
	return T;
}
} // namespace

Rgb TeamColor(Team T, Paint P)
{
	const bool bGloam = T == Team::Enemy;
	switch (P)
	{
	case Paint::Team:
		return bGloam ? GloamViolet : WardenBlue;
	case Paint::TeamDark:
		return bGloam ? GloamVioletDark : WardenBlueDark;
	case Paint::TeamLight:
		return bGloam ? GloamVioletLight : WardenBlueLight;
	case Paint::Accent:
		return bGloam ? GloamGreen : WardenGold;
	case Paint::Fixed:
		break;
	}
	return Rgb();
}

Rgb ResolveColor(const PartDef& Part, Team T)
{
	return Part.P == Paint::Fixed ? Part.Color : TeamColor(T, Part.P);
}

const ModelDef& GetModel(Archetype A)
{
	static const std::array<ModelDef, NumArchetypes> Models = BuildModels();
	const int Index = ArchIndex(A);
	return Models[static_cast<size_t>(Index >= 0 && Index < NumArchetypes ? Index : 0)];
}

const ModelDef& GetTreeModel(TreeKind K)
{
	static const ModelDef PineModel = MakePine();
	static const ModelDef OakModel = MakeOak();
	static const ModelDef DeadModel = MakeDeadTree();
	switch (K)
	{
	case TreeKind::Oak:
		return OakModel;
	case TreeKind::Dead:
		return DeadModel;
	case TreeKind::Pine:
	case TreeKind::None:
		break;
	}
	return PineModel;
}

const ModelDef& GetStumpModel()
{
	static const ModelDef Stump = MakeStump();
	return Stump;
}

const ModelDef& GetBeaconRuinModel()
{
	static const ModelDef Ruin = MakeBeaconRuin();
	return Ruin;
}

const ModelDef& GetScaffoldModel(int Footprint)
{
	static const ModelDef S2 = MakeScaffold(2);
	static const ModelDef S3 = MakeScaffold(3);
	static const ModelDef S4 = MakeScaffold(4);
	return Footprint <= 2 ? S2 : (Footprint == 3 ? S3 : S4);
}

Rgb GroundColor(Ground G, int X, int Y)
{
	const float H = Hash01(X, Y, 0xB3ACu);
	switch (G)
	{
	case Ground::Grass:
		return H < 0.33f ? GrassDark : (H < 0.8f ? palette::Grass : GrassLight);
	case Ground::Meadow:
		return H < 0.5f ? palette::Meadow : GrassLight;
	case Ground::Dirt:
		return H < 0.5f ? palette::Dirt : DirtDark;
	case Ground::Sand:
		return palette::Sand;
	case Ground::Water:
		return H < 0.5f ? palette::Water : WaterDeep;
	case Ground::Rock:
		return H < 0.5f ? palette::Rock : RockLight;
	case Ground::Blight:
		return H < 0.5f ? palette::Blight : BlightDark;
	case Ground::Count:
		break;
	}
	return palette::Grass;
}

void TreePlacement(int X, int Y, float& OutX, float& OutY, float& OutYaw, float& OutScale)
{
	OutX = static_cast<float>(X) + 0.5f + (Hash01(X, Y, 0x71u) - 0.5f) * 0.25f;
	OutY = static_cast<float>(Y) + 0.5f + (Hash01(X, Y, 0x72u) - 0.5f) * 0.25f;
	OutYaw = Hash01(X, Y, 0x73u) * 360.f;
	OutScale = 0.85f + Hash01(X, Y, 0x74u) * 0.35f;
}

void GenerateProps(const GameMap& Map, std::vector<PropInstance>& Out)
{
	Out.clear();
	static const Rgb FlowerColors[4] = {Rgb::Hex(0xF4E27A), Rgb::Hex(0xF29AB8), Rgb::Hex(0xFFFFFF), Rgb::Hex(0xB9A1F2)};
	for (int Y = 0; Y < Map.GetHeight(); ++Y)
	{
		for (int X = 0; X < Map.GetWidth(); ++X)
		{
			const MapTile& T = Map.At(X, Y);
			if (T.Occupant != NoEntity || T.Tree != TreeKind::None || T.BeaconSite >= 0)
			{
				continue;
			}
			const float H = Hash01(X, Y, 0x5151u);
			const float Fx = static_cast<float>(X);
			const float Fy = static_cast<float>(Y);
			switch (T.G)
			{
			case Ground::Rock:
			{
				const int Count = 1 + static_cast<int>(H * 2.99f);
				for (int I = 0; I < Count; ++I)
				{
					PropInstance P;
					P.S = Shape::Cube;
					const float S = 0.45f + Hash01(X * 3 + I, Y, 0x61u) * 0.5f;
					P.X = Fx + 0.25f + Hash01(X, Y * 3 + I, 0x62u) * 0.5f;
					P.Y = Fy + 0.25f + Hash01(X + I * 7, Y, 0x63u) * 0.5f;
					P.SX = S;
					P.SY = S * (0.7f + Hash01(X, Y + I, 0x64u) * 0.5f);
					P.SZ = S * (0.6f + Hash01(X + I, Y, 0x65u) * 0.9f);
					P.Z = P.SZ * 0.4f;
					P.Yaw = Hash01(X + I, Y + I, 0x66u) * 90.f;
					P.Pitch = (Hash01(X, Y, 0x67u + static_cast<uint32_t>(I)) - 0.5f) * 20.f;
					P.Color = (I % 2 == 0) ? palette::Rock : palette::RockLight;
					Out.push_back(P);
				}
				break;
			}
			case Ground::Meadow:
			{
				const int Count = 2 + static_cast<int>(H * 3.f);
				for (int I = 0; I < Count; ++I)
				{
					PropInstance P;
					P.S = Shape::Sphere;
					P.X = Fx + 0.1f + Hash01(X * 5 + I, Y, 0x81u) * 0.8f;
					P.Y = Fy + 0.1f + Hash01(X, Y * 5 + I, 0x82u) * 0.8f;
					P.SX = P.SY = P.SZ = 0.09f;
					P.Z = 0.06f;
					P.Color = FlowerColors[static_cast<int>(Hash01(X + I, Y, 0x83u) * 3.99f)];
					Out.push_back(P);
				}
				break;
			}
			case Ground::Grass:
				if (H < 0.22f)
				{
					PropInstance P;
					P.S = Shape::Cone;
					P.X = Fx + 0.2f + Hash01(X, Y, 0x91u) * 0.6f;
					P.Y = Fy + 0.2f + Hash01(X, Y, 0x92u) * 0.6f;
					P.SX = P.SY = 0.14f;
					P.SZ = 0.2f;
					P.Z = 0.09f;
					P.Color = palette::GrassDark;
					Out.push_back(P);
				}
				break;
			case Ground::Blight:
				if (H < 0.18f)
				{
					PropInstance P;
					P.S = Shape::Sphere;
					P.X = Fx + 0.2f + Hash01(X, Y, 0xA1u) * 0.6f;
					P.Y = Fy + 0.2f + Hash01(X, Y, 0xA2u) * 0.6f;
					P.SX = P.SY = 0.16f;
					P.SZ = 0.1f;
					P.Z = 0.06f;
					P.Color = palette::GloamGreen;
					P.bUnlit = true;
					Out.push_back(P);
				}
				break;
			case Ground::Dirt:
				if (H < 0.25f)
				{
					PropInstance P;
					P.S = Shape::Sphere;
					P.X = Fx + 0.2f + Hash01(X, Y, 0xB1u) * 0.6f;
					P.Y = Fy + 0.2f + Hash01(X, Y, 0xB2u) * 0.6f;
					P.SX = 0.14f;
					P.SY = 0.11f;
					P.SZ = 0.07f;
					P.Z = 0.02f;
					P.Color = palette::StoneGrey;
					Out.push_back(P);
				}
				break;
			case Ground::Sand:
			case Ground::Water:
			case Ground::Count:
				break;
			}
		}
	}
}

namespace
{
float VisValueNoise(float X, float Y, uint32_t Seed)
{
	const float Fx0 = std::floor(X);
	const float Fy0 = std::floor(Y);
	const int X0 = static_cast<int>(Fx0);
	const int Y0 = static_cast<int>(Fy0);
	const float Sx = SmoothStep(X - Fx0);
	const float Sy = SmoothStep(Y - Fy0);
	const float A = Hash01(X0, Y0, Seed);
	const float B = Hash01(X0 + 1, Y0, Seed);
	const float C = Hash01(X0, Y0 + 1, Seed);
	const float D = Hash01(X0 + 1, Y0 + 1, Seed);
	return LerpF(LerpF(A, B, Sx), LerpF(C, D, Sx), Sy);
}

bool VisSoftGround(Ground G)
{
	return G != Ground::Water && G != Ground::Rock;
}

Rgb VisBaseGround(Ground G)
{
	switch (G)
	{
	case Ground::Grass:
		return palette::Grass;
	case Ground::Meadow:
		return palette::Meadow;
	case Ground::Dirt:
		return palette::Dirt;
	case Ground::Sand:
		return palette::Sand;
	case Ground::Water:
		return palette::Water;
	case Ground::Rock:
		return palette::Rock;
	case Ground::Blight:
		return palette::Blight;
	case Ground::Count:
		break;
	}
	return palette::Grass;
}

Rgb VisShade(Rgb C, float F)
{
	auto Ch = [F](uint8_t V) { return static_cast<uint8_t>(ClampI(static_cast<int>(static_cast<float>(V) * F + 0.5f), 0, 255)); };
	return Rgb(Ch(C.R), Ch(C.G), Ch(C.B));
}
} // namespace

void BuildGround(const GameMap& Map, int Subdiv, std::vector<GroundCell>& OutCells, std::vector<Rgb>& OutColors)
{
	const int Sub = MaxI(1, Subdiv);
	const int NumGround = static_cast<int>(Ground::Count);
	const float Shades[3] = {0.94f, 1.f, 1.06f};
	OutColors.clear();
	for (int G = 0; G < NumGround; ++G)
	{
		for (float Shade : Shades)
		{
			OutColors.push_back(VisShade(VisBaseGround(static_cast<Ground>(G)), Shade));
		}
	}
	OutCells.clear();
	OutCells.reserve(static_cast<size_t>(Map.GetWidth() * Map.GetHeight() * Sub * Sub));
	const float Cell = 1.f / static_cast<float>(Sub);
	for (int CY = 0; CY < Map.GetHeight() * Sub; ++CY)
	{
		for (int CX = 0; CX < Map.GetWidth() * Sub; ++CX)
		{
			const float Px = (static_cast<float>(CX) + 0.5f) * Cell;
			const float Py = (static_cast<float>(CY) + 0.5f) * Cell;
			const Tile T = Tile::FromPos(Vec2(Px, Py));
			Ground G = Map.At(T).G;
			if (VisSoftGround(G))
			{
				const float Wx = Px + (VisValueNoise(Px * 0.8f, Py * 0.8f, 11u) - 0.5f) * 0.9f;
				const float Wy = Py + (VisValueNoise(Px * 0.8f, Py * 0.8f, 23u) - 0.5f) * 0.9f;
				const Tile T2(ClampI(static_cast<int>(std::floor(Wx)), 0, Map.GetWidth() - 1), ClampI(static_cast<int>(std::floor(Wy)), 0, Map.GetHeight() - 1));
				const Ground G2 = Map.At(T2).G;
				if (VisSoftGround(G2))
				{
					G = G2;
				}
			}
			const float N = VisValueNoise(Px * 0.3f, Py * 0.3f, 37u) * 0.85f + Hash01(CX, CY, 41u) * 0.15f;
			const int Variant = N < 0.4f ? 0 : (N < 0.68f ? 1 : 2);
			GroundCell C;
			C.X = Px;
			C.Y = Py;
			C.Size = Cell;
			C.Z = GroundHeight(G);
			C.ColorIndex = static_cast<uint16_t>(static_cast<int>(G) * 3 + Variant);
			OutCells.push_back(C);
		}
	}
}

void BuildTreeInstances(const GameMap& Map, std::vector<TreeInstance>& Out)
{
	Out.clear();
	for (int Y = 0; Y < Map.GetHeight(); ++Y)
	{
		for (int X = 0; X < Map.GetWidth(); ++X)
		{
			const TreeKind K = Map.At(X, Y).Tree;
			if (K == TreeKind::None)
			{
				continue;
			}
			TreeInstance T;
			T.Kind = K;
			T.TileX = X;
			T.TileY = Y;
			TreePlacement(X, Y, T.X, T.Y, T.Yaw, T.Scale);
			Out.push_back(T);
		}
	}
	// Decorative forest band around the map so the world does not end at a hard edge.
	const int Band = 8;
	for (int Y = -Band; Y < Map.GetHeight() + Band; ++Y)
	{
		for (int X = -Band; X < Map.GetWidth() + Band; ++X)
		{
			if (Map.InBounds(X, Y))
			{
				continue;
			}
			const int Dx = X < 0 ? -X : (X >= Map.GetWidth() ? X - Map.GetWidth() + 1 : 0);
			const int Dy = Y < 0 ? -Y : (Y >= Map.GetHeight() ? Y - Map.GetHeight() + 1 : 0);
			const int D = MaxI(Dx, Dy);
			if (Hash01(X, Y, 0xF0u) > 0.45f + 0.09f * static_cast<float>(D))
			{
				continue;
			}
			TreeInstance T;
			T.Kind = Hash01(X, Y, 0xF1u) < 0.72f ? TreeKind::Pine : TreeKind::Oak;
			TreePlacement(X, Y, T.X, T.Y, T.Yaw, T.Scale);
			T.Scale *= 1.1f + 0.05f * static_cast<float>(MinI(D, 4));
			Out.push_back(T);
		}
	}
}

float GroundHeight(Ground G)
{
	return G == Ground::Water ? -0.16f : 0.f;
}

Rgb MinimapColor(const MapTile& T, int X, int Y)
{
	if (T.Tree == TreeKind::Pine || T.Tree == TreeKind::Oak)
	{
		return Pine;
	}
	if (T.Tree == TreeKind::Dead)
	{
		return DeadTree;
	}
	if (T.BeaconSite >= 0)
	{
		return StoneDark;
	}
	return GroundColor(T.G, X, Y);
}

} // namespace bh
