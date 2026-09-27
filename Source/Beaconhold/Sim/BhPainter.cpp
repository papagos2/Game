// Beaconhold simulation core - procedural painter (icons, decals, minimap).
#include "BhPainter.h"

#include "BhVisuals.h"

#include <cmath>

namespace bh
{
void ImageRGBA::Init(int InW, int InH)
{
	W = MaxI(1, InW);
	H = MaxI(1, InH);
	Px.assign(static_cast<size_t>(W * H * 4), 0);
}

void ImageRGBA::Blend(int X, int Y, const Rgba& C, float Coverage)
{
	if (X < 0 || Y < 0 || X >= W || Y >= H)
	{
		return;
	}
	const float A = Saturate(C.A * Coverage);
	if (A <= 0.f)
	{
		return;
	}
	uint8_t* P = &Px[static_cast<size_t>((Y * W + X) * 4)];
	const float DA = static_cast<float>(P[3]) / 255.f;
	const float OutA = A + DA * (1.f - A);
	if (OutA <= 1e-5f)
	{
		return;
	}
	auto Mix = [A, DA, OutA](float Src, uint8_t Dst)
	{
		const float D = static_cast<float>(Dst) / 255.f;
		const float V = (Src * A + D * DA * (1.f - A)) / OutA;
		return static_cast<uint8_t>(ClampI(static_cast<int>(V * 255.f + 0.5f), 0, 255));
	};
	P[0] = Mix(C.R, P[0]);
	P[1] = Mix(C.G, P[1]);
	P[2] = Mix(C.B, P[2]);
	P[3] = static_cast<uint8_t>(ClampI(static_cast<int>(OutA * 255.f + 0.5f), 0, 255));
}

// ---------------------------------------------------------------------------------------------
// Painter primitives
// ---------------------------------------------------------------------------------------------

template <typename Fn>
void Painter::Fill(float BX0, float BY0, float BX1, float BY1, const Rgba& C, Fn&& SignedDistancePx)
{
	const float S = Scale();
	const int X0 = MaxI(0, static_cast<int>(std::floor(BX0 * S)) - 3);
	const int Y0 = MaxI(0, static_cast<int>(std::floor(BY0 * S)) - 3);
	const int X1 = MinI(Img.W - 1, static_cast<int>(std::ceil(BX1 * S)) + 3);
	const int Y1 = MinI(Img.H - 1, static_cast<int>(std::ceil(BY1 * S)) + 3);
	for (int Y = Y0; Y <= Y1; ++Y)
	{
		for (int X = X0; X <= X1; ++X)
		{
			const Vec2 Q((static_cast<float>(X) + 0.5f) / S, (static_cast<float>(Y) + 0.5f) / S);
			const float D = SignedDistancePx(Q);
			const float Coverage = Saturate(0.5f - D);
			if (Coverage > 0.f)
			{
				Img.Blend(X, Y, C, Coverage);
			}
		}
	}
}

void Painter::Circle(float Cx, float Cy, float R, const Rgba& C, float Grow)
{
	const float S = Scale();
	const float Pad = Grow / S;
	Fill(Cx - R - Pad, Cy - R - Pad, Cx + R + Pad, Cy + R + Pad, C, [=](const Vec2& Q)
	{
		return (Vec2::Dist(Q, Vec2(Cx, Cy)) - R) * S - Grow;
	});
}

void Painter::Ring(float Cx, float Cy, float R, float Thickness, const Rgba& C, float Grow)
{
	const float S = Scale();
	const float Pad = Grow / S + Thickness;
	Fill(Cx - R - Pad, Cy - R - Pad, Cx + R + Pad, Cy + R + Pad, C, [=](const Vec2& Q)
	{
		return (AbsF(Vec2::Dist(Q, Vec2(Cx, Cy)) - R) - Thickness * 0.5f) * S - Grow;
	});
}

void Painter::RoundRect(float X0, float Y0, float X1, float Y1, float Radius, const Rgba& C, float Grow)
{
	const float S = Scale();
	const float Pad = Grow / S;
	const Vec2 Center((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f);
	const Vec2 Half((X1 - X0) * 0.5f, (Y1 - Y0) * 0.5f);
	const float R = MinF(Radius, MinF(Half.X, Half.Y));
	Fill(X0 - Pad, Y0 - Pad, X1 + Pad, Y1 + Pad, C, [=](const Vec2& Q)
	{
		const float Qx = AbsF(Q.X - Center.X) - (Half.X - R);
		const float Qy = AbsF(Q.Y - Center.Y) - (Half.Y - R);
		const float Outside = Vec2(MaxF(Qx, 0.f), MaxF(Qy, 0.f)).Length();
		const float Inside = MinF(MaxF(Qx, Qy), 0.f);
		return (Outside + Inside - R) * S - Grow;
	});
}

void Painter::Line(float X0, float Y0, float X1, float Y1, float Width, const Rgba& C, float Grow)
{
	const float S = Scale();
	const float Pad = Grow / S + Width;
	const Vec2 A(X0, Y0);
	const Vec2 B(X1, Y1);
	Fill(MinF(X0, X1) - Pad, MinF(Y0, Y1) - Pad, MaxF(X0, X1) + Pad, MaxF(Y0, Y1) + Pad, C, [=](const Vec2& Q)
	{
		const Vec2 E = B - A;
		const float T = Saturate(Vec2::Dot(Q - A, E) / MaxF(Vec2::Dot(E, E), 1e-8f));
		return (Vec2::Dist(Q, A + E * T) - Width * 0.5f) * S - Grow;
	});
}

namespace
{
float PainterPolySdf(const std::vector<Vec2>& P, const Vec2& Q, bool bStrokeOnly, float HalfWidth)
{
	float D = 1e9f;
	bool bInside = false;
	const size_t N = P.size();
	for (size_t I = 0, J = N - 1; I < N; J = I++)
	{
		const Vec2 E = P[I] - P[J];
		const Vec2 W = Q - P[J];
		const float T = Saturate(Vec2::Dot(W, E) / MaxF(Vec2::Dot(E, E), 1e-8f));
		D = MinF(D, (W - E * T).LengthSq());
		if (((P[I].Y > Q.Y) != (P[J].Y > Q.Y)) && (Q.X < (P[J].X - P[I].X) * (Q.Y - P[I].Y) / (P[J].Y - P[I].Y) + P[I].X))
		{
			bInside = !bInside;
		}
	}
	const float Dist = std::sqrt(D);
	if (bStrokeOnly)
	{
		return Dist - HalfWidth;
	}
	return bInside ? -Dist : Dist;
}
} // namespace

void Painter::Poly(std::initializer_list<Vec2> Points, const Rgba& C, float Grow)
{
	if (Points.size() < 3)
	{
		return;
	}
	const std::vector<Vec2> P(Points);
	float X0 = 1e9f, Y0 = 1e9f, X1 = -1e9f, Y1 = -1e9f;
	for (const Vec2& V : P)
	{
		X0 = MinF(X0, V.X);
		Y0 = MinF(Y0, V.Y);
		X1 = MaxF(X1, V.X);
		Y1 = MaxF(Y1, V.Y);
	}
	const float S = Scale();
	const float Pad = Grow / S;
	Fill(X0 - Pad, Y0 - Pad, X1 + Pad, Y1 + Pad, C, [&P, S, Grow](const Vec2& Q)
	{
		return PainterPolySdf(P, Q, false, 0.f) * S - Grow;
	});
}

void Painter::PolyStroke(std::initializer_list<Vec2> Points, float Width, const Rgba& C, float Grow)
{
	if (Points.size() < 2)
	{
		return;
	}
	const std::vector<Vec2> P(Points);
	float X0 = 1e9f, Y0 = 1e9f, X1 = -1e9f, Y1 = -1e9f;
	for (const Vec2& V : P)
	{
		X0 = MinF(X0, V.X);
		Y0 = MinF(Y0, V.Y);
		X1 = MaxF(X1, V.X);
		Y1 = MaxF(Y1, V.Y);
	}
	const float S = Scale();
	const float Pad = Grow / S + Width;
	Fill(X0 - Pad, Y0 - Pad, X1 + Pad, Y1 + Pad, C, [&P, S, Grow, Width](const Vec2& Q)
	{
		return PainterPolySdf(P, Q, true, Width * 0.5f) * S - Grow;
	});
}

void Painter::Arc(float Cx, float Cy, float R, float Width, float A0, float A1, const Rgba& C, float Grow)
{
	const float S = Scale();
	const float Pad = Grow / S + Width;
	const Vec2 Center(Cx, Cy);
	const Vec2 E0 = Center + Vec2::FromAngle(A0) * R;
	const Vec2 E1 = Center + Vec2::FromAngle(A1) * R;
	Fill(Cx - R - Pad, Cy - R - Pad, Cx + R + Pad, Cy + R + Pad, C, [=](const Vec2& Q)
	{
		const Vec2 D = Q - Center;
		float A = std::atan2(D.Y, D.X);
		while (A < A0)
		{
			A += TwoPi;
		}
		float Dist = 0.f;
		if (A <= A1)
		{
			Dist = AbsF(D.Length() - R);
		}
		else
		{
			Dist = MinF(Vec2::Dist(Q, E0), Vec2::Dist(Q, E1));
		}
		return (Dist - Width * 0.5f) * S - Grow;
	});
}

void Painter::RadialGradient(float Cx, float Cy, float R, const Rgba& Inner, const Rgba& Outer)
{
	const float S = Scale();
	for (int Y = 0; Y < Img.H; ++Y)
	{
		for (int X = 0; X < Img.W; ++X)
		{
			const Vec2 Q((static_cast<float>(X) + 0.5f) / S, (static_cast<float>(Y) + 0.5f) / S);
			const float T = Saturate(Vec2::Dist(Q, Vec2(Cx, Cy)) / R);
			const float Sm = SmoothStep(T);
			const Rgba C(LerpF(Inner.R, Outer.R, Sm), LerpF(Inner.G, Outer.G, Sm), LerpF(Inner.B, Outer.B, Sm), LerpF(Inner.A, Outer.A, Sm));
			Img.Blend(X, Y, C, 1.f);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Icons
// ---------------------------------------------------------------------------------------------

namespace
{
// Colours
const Rgba InkC = Rgba::Hex(0x1B1612, 0.9f);
const Rgba GoldC = Rgba::Hex(0xFFC83A);
const Rgba GoldLightC = Rgba::Hex(0xFFE7A0);
const Rgba GoldDeepC = Rgba::Hex(0xD9901F);
const Rgba BlueC = Rgba::Hex(0x4A7FE6);
const Rgba CreamC = Rgba::Hex(0xF1E6CC);
const Rgba StoneC = Rgba::Hex(0xD8CCB2);
const Rgba WoodC = Rgba::Hex(0xA8703F);
const Rgba WoodDarkC = Rgba::Hex(0x7A4E2A);
const Rgba MetalC = Rgba::Hex(0xCBD5DF);
const Rgba MetalDarkC = Rgba::Hex(0x7F8A96);
const Rgba RedC = Rgba::Hex(0xE0533D);
const Rgba GreenC = Rgba::Hex(0x6CCB5F);
const Rgba VioletC = Rgba::Hex(0x8A4CC0);
const Rgba VioletDarkC = Rgba::Hex(0x4A2566);
const Rgba BogGreenC = Rgba::Hex(0xA6EE63);
const Rgba WhiteC = Rgba::Hex(0xFFFFFF);
const Rgba LeafC = Rgba::Hex(0x3D8054);
const Rgba LeafLightC = Rgba::Hex(0x5BA66C);

// A pen draws either the dark outline pass (fattened shapes) or the colour pass.
struct IconPen
{
	Painter& P;
	bool bOutline = false;
	float Grow = 4.f;

	Rgba C(const Rgba& Fill) const { return bOutline ? InkC : Fill; }
	float G() const { return bOutline ? Grow : 0.f; }

	void Circle(float X, float Y, float R, const Rgba& Col) const { P.Circle(X, Y, R, C(Col), G()); }
	void Ring(float X, float Y, float R, float T, const Rgba& Col) const { P.Ring(X, Y, R, T, C(Col), G()); }
	void Rect(float X0, float Y0, float X1, float Y1, float Rad, const Rgba& Col) const { P.RoundRect(X0, Y0, X1, Y1, Rad, C(Col), G()); }
	void Line(float X0, float Y0, float X1, float Y1, float W, const Rgba& Col) const { P.Line(X0, Y0, X1, Y1, W, C(Col), G()); }
	void Poly(std::initializer_list<Vec2> Pts, const Rgba& Col) const { P.Poly(Pts, C(Col), G()); }
	void Stroke(std::initializer_list<Vec2> Pts, float W, const Rgba& Col) const { P.PolyStroke(Pts, W, C(Col), G()); }
	void Arc(float X, float Y, float R, float W, float A0, float A1, const Rgba& Col) const { P.Arc(X, Y, R, W, A0, A1, C(Col), G()); }
	// Details drawn only in the colour pass (no outline).
	void DCircle(float X, float Y, float R, const Rgba& Col) const
	{
		if (!bOutline)
		{
			P.Circle(X, Y, R, Col);
		}
	}
	void DLine(float X0, float Y0, float X1, float Y1, float W, const Rgba& Col) const
	{
		if (!bOutline)
		{
			P.Line(X0, Y0, X1, Y1, W, Col);
		}
	}
	void DPoly(std::initializer_list<Vec2> Pts, const Rgba& Col) const
	{
		if (!bOutline)
		{
			P.Poly(Pts, Col);
		}
	}
};

void IconSword(const IconPen& I, float Ox, float Oy, float S, const Rgba& Blade)
{
	// Diagonal sword from bottom-left to top-right, scaled around (Ox, Oy).
	auto Pt = [Ox, Oy, S](float X, float Y) { return Vec2(Ox + (X - 0.5f) * S, Oy + (Y - 0.5f) * S); };
	const Vec2 A = Pt(0.30f, 0.70f);
	const Vec2 B = Pt(0.82f, 0.18f);
	I.Line(A.X, A.Y, B.X, B.Y, 0.09f * S, Blade);
	const Vec2 G0 = Pt(0.20f, 0.58f);
	const Vec2 G1 = Pt(0.42f, 0.80f);
	I.Line(G0.X, G0.Y, G1.X, G1.Y, 0.07f * S, GoldC);
	const Vec2 H = Pt(0.16f, 0.84f);
	I.Line(A.X, A.Y, H.X, H.Y, 0.07f * S, WoodDarkC);
	I.DLine(A.X + 0.02f * S, A.Y - 0.02f * S, B.X - 0.04f * S, B.Y + 0.04f * S, 0.02f * S, WhiteC);
}

void IconShieldShape(const IconPen& I, float Cx, float Cy, float S, const Rgba& Col, const Rgba& Trim)
{
	auto Pt = [Cx, Cy, S](float X, float Y) { return Vec2(Cx + X * S, Cy + Y * S); };
	I.Poly({Pt(-0.3f, -0.36f), Pt(0.3f, -0.36f), Pt(0.3f, 0.02f), Pt(0.2f, 0.22f), Pt(0.f, 0.38f), Pt(-0.2f, 0.22f), Pt(-0.3f, 0.02f)}, Col);
	I.DPoly({Pt(-0.07f, -0.36f), Pt(0.07f, -0.36f), Pt(0.07f, 0.3f), Pt(0.f, 0.36f), Pt(-0.07f, 0.3f)}, Trim);
	I.DCircle(Cx, Cy - 0.04f * S, 0.09f * S, Trim);
}

void IconHouse(const IconPen& I, const Rgba& Roof, bool bChimney)
{
	if (bChimney)
	{
		I.Rect(0.62f, 0.14f, 0.74f, 0.4f, 0.02f, StoneC);
	}
	I.Rect(0.2f, 0.44f, 0.8f, 0.88f, 0.03f, CreamC);
	I.Poly({Vec2(0.5f, 0.1f), Vec2(0.92f, 0.48f), Vec2(0.08f, 0.48f)}, Roof);
	I.DPoly({Vec2(0.43f, 0.88f), Vec2(0.43f, 0.66f), Vec2(0.5f, 0.6f), Vec2(0.57f, 0.66f), Vec2(0.57f, 0.88f)}, WoodDarkC);
	I.DCircle(0.3f + 0.02f, 0.62f, 0.05f, GoldLightC);
}

void IconCastle(const IconPen& I)
{
	I.Rect(0.12f, 0.3f, 0.34f, 0.88f, 0.02f, StoneC);
	I.Rect(0.66f, 0.3f, 0.88f, 0.88f, 0.02f, StoneC);
	I.Rect(0.26f, 0.44f, 0.74f, 0.88f, 0.02f, CreamC);
	I.Poly({Vec2(0.23f, 0.08f), Vec2(0.38f, 0.32f), Vec2(0.08f, 0.32f)}, BlueC);
	I.Poly({Vec2(0.77f, 0.08f), Vec2(0.92f, 0.32f), Vec2(0.62f, 0.32f)}, BlueC);
	I.DPoly({Vec2(0.42f, 0.88f), Vec2(0.42f, 0.66f), Vec2(0.5f, 0.58f), Vec2(0.58f, 0.66f), Vec2(0.58f, 0.88f)}, WoodDarkC);
	for (int K = 0; K < 4; ++K)
	{
		const float X = 0.3f + static_cast<float>(K) * 0.12f;
		I.DPoly({Vec2(X, 0.44f), Vec2(X + 0.06f, 0.44f), Vec2(X + 0.06f, 0.38f), Vec2(X, 0.38f)}, StoneC);
	}
	I.DCircle(0.5f, 0.52f, 0.045f, GoldC);
}

void IconArrow(const IconPen& I, float X0, float Y0, float X1, float Y1, float W, const Rgba& Shaft)
{
	I.Line(X0, Y0, X1, Y1, W, Shaft);
	const Vec2 Dir = (Vec2(X1, Y1) - Vec2(X0, Y0)).Normalized();
	const Vec2 N(-Dir.Y, Dir.X);
	const Vec2 Tip = Vec2(X1, Y1) + Dir * (W * 2.2f);
	const Vec2 Base = Vec2(X1, Y1) - Dir * (W * 0.6f);
	I.Poly({Tip, Base + N * (W * 1.9f), Base - N * (W * 1.9f)}, MetalC);
	const Vec2 Tail = Vec2(X0, Y0);
	I.Poly({Tail + Dir * (W * 2.4f), Tail + N * (W * 1.6f) - Dir * (W * 0.2f), Tail - Dir * (W * 0.5f), Tail - N * (W * 1.6f) - Dir * (W * 0.2f)}, RedC);
}

void IconStar(const IconPen& I, float Cx, float Cy, float R, const Rgba& Col, bool bHollow)
{
	Vec2 Pts[10];
	for (int K = 0; K < 10; ++K)
	{
		const float A = -Pi * 0.5f + static_cast<float>(K) * Pi / 5.f;
		const float Rad = (K % 2 == 0) ? R : R * 0.45f;
		Pts[K] = Vec2(Cx + std::cos(A) * Rad, Cy + std::sin(A) * Rad);
	}
	if (bHollow)
	{
		I.Stroke({Pts[0], Pts[1], Pts[2], Pts[3], Pts[4], Pts[5], Pts[6], Pts[7], Pts[8], Pts[9]}, 0.06f, Col);
	}
	else
	{
		I.Poly({Pts[0], Pts[1], Pts[2], Pts[3], Pts[4], Pts[5], Pts[6], Pts[7], Pts[8], Pts[9]}, Col);
	}
}

void IconHeart(const IconPen& I, float Cx, float Cy, float S, const Rgba& Col)
{
	I.Circle(Cx - 0.17f * S, Cy - 0.1f * S, 0.2f * S, Col);
	I.Circle(Cx + 0.17f * S, Cy - 0.1f * S, 0.2f * S, Col);
	I.Poly({Vec2(Cx - 0.36f * S, Cy - 0.04f * S), Vec2(Cx + 0.36f * S, Cy - 0.04f * S), Vec2(Cx, Cy + 0.38f * S)}, Col);
}

void IconCrystal(const IconPen& I, float Cx, float Cy, float S)
{
	auto Pt = [Cx, Cy, S](float X, float Y) { return Vec2(Cx + X * S, Cy + Y * S); };
	I.Poly({Pt(0.f, -0.42f), Pt(0.3f, -0.1f), Pt(0.f, 0.42f), Pt(-0.3f, -0.1f)}, GoldC);
	I.DPoly({Pt(0.f, -0.42f), Pt(0.1f, -0.1f), Pt(0.f, 0.42f), Pt(-0.1f, -0.1f)}, GoldLightC);
	I.DPoly({Pt(-0.3f, -0.1f), Pt(-0.1f, -0.1f), Pt(0.f, 0.42f)}, GoldDeepC);
}

void IconAntlers(const IconPen& I, float Cy)
{
	I.Stroke({Vec2(0.44f, Cy + 0.2f), Vec2(0.3f, Cy - 0.05f), Vec2(0.18f, Cy - 0.25f)}, 0.055f, CreamC);
	I.Line(0.3f, Cy - 0.05f, 0.12f, Cy - 0.08f, 0.05f, CreamC);
	I.Line(0.24f, Cy - 0.15f, 0.3f, Cy - 0.3f, 0.05f, CreamC);
	I.Stroke({Vec2(0.56f, Cy + 0.2f), Vec2(0.7f, Cy - 0.05f), Vec2(0.82f, Cy - 0.25f)}, 0.055f, CreamC);
	I.Line(0.7f, Cy - 0.05f, 0.88f, Cy - 0.08f, 0.05f, CreamC);
	I.Line(0.76f, Cy - 0.15f, 0.7f, Cy - 0.3f, 0.05f, CreamC);
}

void IconLantern(const IconPen& I)
{
	I.Line(0.24f, 0.9f, 0.58f, 0.14f, 0.06f, WoodC);
	I.Line(0.58f, 0.14f, 0.7f, 0.2f, 0.04f, MetalDarkC);
	I.Rect(0.6f, 0.26f, 0.84f, 0.56f, 0.05f, MetalDarkC);
	I.Poly({Vec2(0.58f, 0.28f), Vec2(0.72f, 0.18f), Vec2(0.86f, 0.28f)}, BlueC);
	I.DCircle(0.72f, 0.42f, 0.085f, GoldLightC);
	I.DCircle(0.72f, 0.42f, 0.05f, WhiteC);
}

void IconFlame(const IconPen& I, float Cx, float Cy, float S)
{
	I.Circle(Cx, Cy + 0.1f * S, 0.22f * S, GoldC);
	I.Poly({Vec2(Cx - 0.21f * S, Cy + 0.06f * S), Vec2(Cx, Cy - 0.42f * S), Vec2(Cx + 0.21f * S, Cy + 0.06f * S)}, GoldC);
	I.DCircle(Cx, Cy + 0.14f * S, 0.12f * S, GoldLightC);
	I.DPoly({Vec2(Cx - 0.11f * S, Cy + 0.12f * S), Vec2(Cx, Cy - 0.18f * S), Vec2(Cx + 0.11f * S, Cy + 0.12f * S)}, GoldLightC);
}

void IconSun(const IconPen& I, float Cx, float Cy, float R)
{
	for (int K = 0; K < 8; ++K)
	{
		const float A = static_cast<float>(K) * Pi / 4.f;
		I.Line(Cx + std::cos(A) * R * 1.35f, Cy + std::sin(A) * R * 1.35f, Cx + std::cos(A) * R * 1.95f, Cy + std::sin(A) * R * 1.95f, 0.06f, GoldC);
	}
	I.Circle(Cx, Cy, R, GoldC);
	I.DCircle(Cx - R * 0.25f, Cy - R * 0.25f, R * 0.45f, GoldLightC);
}

void IconBow(const IconPen& I)
{
	I.Arc(0.22f, 0.5f, 0.42f, 0.07f, -1.1f, 1.1f, WoodC);
	I.Line(0.41f, 0.13f, 0.41f, 0.87f, 0.025f, CreamC);
	IconArrow(I, 0.2f, 0.5f, 0.78f, 0.5f, 0.045f, WoodDarkC);
}

void IconGear(const IconPen& I)
{
	for (int K = 0; K < 8; ++K)
	{
		const float A = static_cast<float>(K) * Pi / 4.f;
		I.Line(0.5f + std::cos(A) * 0.22f, 0.5f + std::sin(A) * 0.22f, 0.5f + std::cos(A) * 0.38f, 0.5f + std::sin(A) * 0.38f, 0.14f, MetalC);
	}
	I.Circle(0.5f, 0.5f, 0.27f, MetalC);
	I.DCircle(0.5f, 0.5f, 0.11f, MetalDarkC);
}

void DrawIcon(Icon Id, const IconPen& I)
{
	switch (Id)
	{
	case Icon::None:
		break;
	case Icon::Sunstone:
		IconCrystal(I, 0.5f, 0.5f, 1.f);
		break;
	case Icon::Timber:
		I.Rect(0.1f, 0.28f, 0.9f, 0.5f, 0.11f, WoodC);
		I.Rect(0.1f, 0.54f, 0.9f, 0.76f, 0.11f, WoodDarkC);
		I.Circle(0.21f, 0.39f, 0.09f, Rgba::Hex(0xE3B77E));
		I.Circle(0.21f, 0.65f, 0.09f, Rgba::Hex(0xD9A868));
		I.DCircle(0.21f, 0.39f, 0.035f, WoodC);
		I.DCircle(0.21f, 0.65f, 0.035f, WoodC);
		break;
	case Icon::Supply:
	case Icon::Cottage:
		IconHouse(I, BlueC, Id == Icon::Cottage);
		break;
	case Icon::Lamplighter:
	case Icon::Worker:
		IconLantern(I);
		break;
	case Icon::Shieldbearer:
	case Icon::Shield:
		IconShieldShape(I, 0.5f, 0.5f, 1.f, BlueC, GoldC);
		break;
	case Icon::Ranger:
	case Icon::Bow:
		IconBow(I);
		break;
	case Icon::StagRider:
	case Icon::StagLodge:
		IconAntlers(I, 0.46f);
		I.Poly({Vec2(0.38f, 0.58f), Vec2(0.62f, 0.58f), Vec2(0.56f, 0.9f), Vec2(0.44f, 0.9f)}, Rgba::Hex(0xA06A40));
		I.DCircle(0.45f, 0.68f, 0.025f, InkC);
		I.DCircle(0.55f, 0.68f, 0.025f, InkC);
		break;
	case Icon::Sage:
	case Icon::Wisdom:
		I.Line(0.3f, 0.9f, 0.52f, 0.38f, 0.06f, WoodC);
		IconSun(I, 0.6f, 0.28f, 0.12f);
		break;
	case Icon::Gloomling:
		I.Poly({Vec2(0.26f, 0.42f), Vec2(0.2f, 0.12f), Vec2(0.42f, 0.34f)}, VioletDarkC);
		I.Poly({Vec2(0.74f, 0.42f), Vec2(0.8f, 0.12f), Vec2(0.58f, 0.34f)}, VioletDarkC);
		I.Circle(0.5f, 0.58f, 0.32f, VioletDarkC);
		I.DCircle(0.4f, 0.54f, 0.06f, BogGreenC);
		I.DCircle(0.6f, 0.54f, 0.06f, BogGreenC);
		I.DPoly({Vec2(0.4f, 0.72f), Vec2(0.6f, 0.72f), Vec2(0.5f, 0.8f)}, BogGreenC);
		break;
	case Icon::Thornback:
		for (int K = 0; K < 4; ++K)
		{
			const float X = 0.24f + static_cast<float>(K) * 0.17f;
			I.Poly({Vec2(X - 0.07f, 0.5f), Vec2(X + 0.02f, 0.14f), Vec2(X + 0.09f, 0.5f)}, Rgba::Hex(0x6E8A40));
		}
		I.Rect(0.12f, 0.42f, 0.88f, 0.86f, 0.16f, Rgba::Hex(0x5A4150));
		I.DCircle(0.7f, 0.6f, 0.05f, BogGreenC);
		break;
	case Icon::Hexer:
		I.Poly({Vec2(0.5f, 0.08f), Vec2(0.86f, 0.9f), Vec2(0.14f, 0.9f)}, VioletC);
		I.Circle(0.5f, 0.52f, 0.17f, Rgba::Hex(0x2A1638));
		I.DCircle(0.44f, 0.52f, 0.04f, BogGreenC);
		I.DCircle(0.56f, 0.52f, 0.04f, BogGreenC);
		break;
	case Icon::BogTitan:
		I.Circle(0.5f, 0.55f, 0.36f, Rgba::Hex(0x4B5C3C));
		I.Poly({Vec2(0.2f, 0.36f), Vec2(0.1f, 0.08f), Vec2(0.36f, 0.26f)}, CreamC);
		I.Poly({Vec2(0.8f, 0.36f), Vec2(0.9f, 0.08f), Vec2(0.64f, 0.26f)}, CreamC);
		I.DCircle(0.38f, 0.5f, 0.07f, BogGreenC);
		I.DCircle(0.62f, 0.5f, 0.07f, BogGreenC);
		I.DLine(0.36f, 0.72f, 0.64f, 0.72f, 0.05f, Rgba::Hex(0x26301E));
		break;
	case Icon::Keep:
	case Icon::Home:
		IconCastle(I);
		break;
	case Icon::Storehouse:
		I.Rect(0.14f, 0.24f, 0.86f, 0.86f, 0.05f, WoodC);
		I.DLine(0.2f, 0.3f, 0.8f, 0.8f, 0.06f, WoodDarkC);
		I.DLine(0.8f, 0.3f, 0.2f, 0.8f, 0.06f, WoodDarkC);
		I.DLine(0.18f, 0.55f, 0.82f, 0.55f, 0.04f, WoodDarkC);
		break;
	case Icon::Barracks:
	case Icon::Army:
		IconSword(I, 0.5f, 0.5f, 0.95f, MetalC);
		I.Line(0.84f, 0.84f, 0.3f, 0.3f, 0.08f, MetalC);
		I.Line(0.82f, 0.58f, 0.58f, 0.82f, 0.065f, GoldC);
		I.Line(0.84f, 0.84f, 0.9f, 0.9f, 0.065f, WoodDarkC);
		if (Id == Icon::Army)
		{
			IconShieldShape(I, 0.5f, 0.56f, 0.55f, BlueC, GoldC);
		}
		break;
	case Icon::Forge:
		I.Poly({Vec2(0.12f, 0.38f), Vec2(0.88f, 0.38f), Vec2(0.88f, 0.5f), Vec2(0.66f, 0.56f), Vec2(0.66f, 0.72f), Vec2(0.78f, 0.86f), Vec2(0.22f, 0.86f), Vec2(0.34f, 0.72f), Vec2(0.34f, 0.56f), Vec2(0.12f, 0.46f)}, MetalDarkC);
		I.DLine(0.14f, 0.41f, 0.86f, 0.41f, 0.03f, MetalC);
		I.Circle(0.66f, 0.22f, 0.09f, Rgba::Hex(0xFF9B3D));
		I.DCircle(0.66f, 0.22f, 0.045f, GoldLightC);
		break;
	case Icon::Sanctum:
		I.Circle(0.5f, 0.42f, 0.28f, Rgba::Hex(0x9CBBF2));
		I.Rect(0.18f, 0.44f, 0.82f, 0.52f, 0.02f, GoldC);
		for (int K = 0; K < 3; ++K)
		{
			const float X = 0.28f + static_cast<float>(K) * 0.22f;
			I.Rect(X - 0.04f, 0.52f, X + 0.04f, 0.82f, 0.01f, CreamC);
		}
		I.Rect(0.16f, 0.8f, 0.84f, 0.88f, 0.02f, StoneC);
		I.DCircle(0.5f, 0.14f, 0.05f, GoldLightC);
		break;
	case Icon::Watchtower:
		I.Rect(0.34f, 0.36f, 0.66f, 0.9f, 0.03f, StoneC);
		I.Rect(0.28f, 0.34f, 0.72f, 0.42f, 0.02f, WoodC);
		I.Poly({Vec2(0.5f, 0.06f), Vec2(0.78f, 0.36f), Vec2(0.22f, 0.36f)}, BlueC);
		I.DCircle(0.5f, 0.56f, 0.05f, GoldLightC);
		break;
	case Icon::Beacon:
		I.Rect(0.4f, 0.5f, 0.6f, 0.86f, 0.02f, StoneC);
		I.Rect(0.3f, 0.44f, 0.7f, 0.52f, 0.03f, GoldC);
		I.Rect(0.26f, 0.84f, 0.74f, 0.92f, 0.02f, Rgba::Hex(0xB3A68C));
		IconFlame(I, 0.5f, 0.28f, 0.62f);
		break;
	case Icon::GloamHeart:
		IconHeart(I, 0.5f, 0.52f, 1.f, VioletC);
		I.DCircle(0.42f, 0.42f, 0.08f, Rgba::Hex(0xD99BFF));
		break;
	case Icon::Burrow:
		I.Poly({Vec2(0.08f, 0.84f), Vec2(0.22f, 0.44f), Vec2(0.5f, 0.3f), Vec2(0.78f, 0.44f), Vec2(0.92f, 0.84f)}, Rgba::Hex(0x5A4265));
		I.Circle(0.5f, 0.72f, 0.14f, Rgba::Hex(0x120A18));
		I.DCircle(0.45f, 0.7f, 0.025f, BogGreenC);
		I.DCircle(0.55f, 0.7f, 0.025f, BogGreenC);
		break;
	case Icon::Hexroot:
		I.Line(0.5f, 0.9f, 0.46f, 0.4f, 0.12f, Rgba::Hex(0x4E3A55));
		I.Circle(0.34f, 0.32f, 0.16f, VioletC);
		I.Circle(0.62f, 0.26f, 0.18f, VioletC);
		I.DCircle(0.3f, 0.56f, 0.05f, BogGreenC);
		I.DCircle(0.7f, 0.5f, 0.05f, BogGreenC);
		break;
	case Icon::ThornSpire:
		I.Poly({Vec2(0.5f, 0.06f), Vec2(0.68f, 0.9f), Vec2(0.32f, 0.9f)}, Rgba::Hex(0x5A4265));
		I.Poly({Vec2(0.4f, 0.5f), Vec2(0.2f, 0.36f), Vec2(0.42f, 0.42f)}, Rgba::Hex(0x6E8A40));
		I.Poly({Vec2(0.6f, 0.62f), Vec2(0.82f, 0.5f), Vec2(0.6f, 0.54f)}, Rgba::Hex(0x6E8A40));
		I.DCircle(0.5f, 0.2f, 0.06f, BogGreenC);
		break;
	case Icon::SunstoneNode:
		IconCrystal(I, 0.34f, 0.6f, 0.62f);
		IconCrystal(I, 0.64f, 0.52f, 0.8f);
		break;
	case Icon::Tree:
		I.Rect(0.45f, 0.72f, 0.55f, 0.92f, 0.02f, WoodC);
		I.Poly({Vec2(0.5f, 0.08f), Vec2(0.78f, 0.46f), Vec2(0.22f, 0.46f)}, LeafLightC);
		I.Poly({Vec2(0.5f, 0.3f), Vec2(0.84f, 0.76f), Vec2(0.16f, 0.76f)}, LeafC);
		break;
	case Icon::Build:
	case Icon::Repair:
		I.Line(0.24f, 0.86f, 0.62f, 0.38f, 0.08f, WoodC);
		I.Poly({Vec2(0.42f, 0.28f), Vec2(0.62f, 0.12f), Vec2(0.88f, 0.4f), Vec2(0.72f, 0.54f)}, MetalC);
		if (Id == Icon::Repair)
		{
			I.Line(0.18f, 0.2f, 0.18f, 0.44f, 0.07f, GreenC);
			I.Line(0.06f, 0.32f, 0.3f, 0.32f, 0.07f, GreenC);
		}
		break;
	case Icon::Stop:
		I.Rect(0.18f, 0.18f, 0.82f, 0.82f, 0.12f, RedC);
		I.DLine(0.32f, 0.5f, 0.68f, 0.5f, 0.1f, WhiteC);
		break;
	case Icon::Cancel:
		I.Line(0.24f, 0.24f, 0.76f, 0.76f, 0.13f, RedC);
		I.Line(0.76f, 0.24f, 0.24f, 0.76f, 0.13f, RedC);
		break;
	case Icon::Confirm:
		I.Stroke({Vec2(0.18f, 0.54f), Vec2(0.42f, 0.78f), Vec2(0.84f, 0.26f)}, 0.14f, GreenC);
		break;
	case Icon::Back:
		I.Poly({Vec2(0.14f, 0.5f), Vec2(0.5f, 0.18f), Vec2(0.5f, 0.82f)}, CreamC);
		I.Rect(0.44f, 0.4f, 0.86f, 0.6f, 0.04f, CreamC);
		break;
	case Icon::Pause:
		I.Rect(0.26f, 0.2f, 0.42f, 0.8f, 0.05f, CreamC);
		I.Rect(0.58f, 0.2f, 0.74f, 0.8f, 0.05f, CreamC);
		break;
	case Icon::Play:
		I.Poly({Vec2(0.28f, 0.16f), Vec2(0.82f, 0.5f), Vec2(0.28f, 0.84f)}, CreamC);
		break;
	case Icon::Speed:
		I.Poly({Vec2(0.12f, 0.2f), Vec2(0.5f, 0.5f), Vec2(0.12f, 0.8f)}, CreamC);
		I.Poly({Vec2(0.48f, 0.2f), Vec2(0.86f, 0.5f), Vec2(0.48f, 0.8f)}, CreamC);
		break;
	case Icon::Objectives:
		I.Rect(0.2f, 0.18f, 0.8f, 0.84f, 0.04f, CreamC);
		I.Rect(0.14f, 0.12f, 0.86f, 0.22f, 0.05f, Rgba::Hex(0xD8C49A));
		I.Rect(0.14f, 0.8f, 0.86f, 0.9f, 0.05f, Rgba::Hex(0xD8C49A));
		I.DLine(0.3f, 0.36f, 0.7f, 0.36f, 0.035f, Rgba::Hex(0x8A7A5A));
		I.DLine(0.3f, 0.5f, 0.7f, 0.5f, 0.035f, Rgba::Hex(0x8A7A5A));
		I.DLine(0.3f, 0.64f, 0.58f, 0.64f, 0.035f, Rgba::Hex(0x8A7A5A));
		break;
	case Icon::Settings:
		IconGear(I);
		break;
	case Icon::Star:
		IconStar(I, 0.5f, 0.53f, 0.42f, GoldC, false);
		break;
	case Icon::StarEmpty:
		IconStar(I, 0.5f, 0.53f, 0.4f, Rgba::Hex(0x8C8272), true);
		break;
	case Icon::Lock:
		I.Arc(0.5f, 0.42f, 0.17f, 0.08f, Pi, TwoPi, MetalC);
		I.Rect(0.24f, 0.42f, 0.76f, 0.86f, 0.06f, GoldC);
		I.DCircle(0.5f, 0.6f, 0.05f, InkC);
		I.DLine(0.5f, 0.6f, 0.5f, 0.74f, 0.04f, InkC);
		break;
	case Icon::Trophy:
		I.Ring(0.24f, 0.36f, 0.1f, 0.05f, GoldC);
		I.Ring(0.76f, 0.36f, 0.1f, 0.05f, GoldC);
		I.Poly({Vec2(0.26f, 0.14f), Vec2(0.74f, 0.14f), Vec2(0.66f, 0.52f), Vec2(0.34f, 0.52f)}, GoldC);
		I.Rect(0.46f, 0.5f, 0.54f, 0.72f, 0.01f, GoldDeepC);
		I.Rect(0.3f, 0.72f, 0.7f, 0.86f, 0.03f, GoldDeepC);
		I.DCircle(0.4f, 0.26f, 0.04f, GoldLightC);
		break;
	case Icon::Skull:
		I.Circle(0.5f, 0.44f, 0.3f, CreamC);
		I.Rect(0.34f, 0.6f, 0.66f, 0.84f, 0.05f, CreamC);
		I.DCircle(0.39f, 0.44f, 0.08f, InkC);
		I.DCircle(0.61f, 0.44f, 0.08f, InkC);
		I.DPoly({Vec2(0.5f, 0.56f), Vec2(0.46f, 0.64f), Vec2(0.54f, 0.64f)}, InkC);
		break;
	case Icon::Sword:
	case Icon::Blades:
		IconSword(I, 0.5f, 0.5f, 1.f, MetalC);
		if (Id == Icon::Blades)
		{
			I.DLine(0.18f, 0.2f, 0.18f, 0.36f, 0.035f, GoldLightC);
			I.DLine(0.1f, 0.28f, 0.26f, 0.28f, 0.035f, GoldLightC);
		}
		break;
	case Icon::Boot:
		I.Poly({Vec2(0.3f, 0.12f), Vec2(0.58f, 0.12f), Vec2(0.58f, 0.6f), Vec2(0.86f, 0.7f), Vec2(0.86f, 0.86f), Vec2(0.3f, 0.86f)}, WoodC);
		I.DLine(0.3f, 0.24f, 0.58f, 0.24f, 0.04f, WoodDarkC);
		break;
	case Icon::Brace:
		IconShieldShape(I, 0.46f, 0.52f, 0.9f, BlueC, GoldC);
		I.Line(0.84f, 0.2f, 0.92f, 0.12f, 0.05f, CreamC);
		I.Line(0.86f, 0.4f, 0.96f, 0.4f, 0.05f, CreamC);
		break;
	case Icon::Volley:
		IconArrow(I, 0.12f, 0.62f, 0.74f, 0.14f, 0.04f, WoodC);
		IconArrow(I, 0.2f, 0.78f, 0.82f, 0.3f, 0.04f, WoodC);
		IconArrow(I, 0.28f, 0.94f, 0.9f, 0.46f, 0.04f, WoodC);
		break;
	case Icon::Charge:
		I.Poly({Vec2(0.58f, 0.06f), Vec2(0.2f, 0.56f), Vec2(0.46f, 0.56f), Vec2(0.36f, 0.94f), Vec2(0.8f, 0.4f), Vec2(0.54f, 0.4f)}, GoldC);
		break;
	case Icon::Sunburst:
		IconSun(I, 0.5f, 0.5f, 0.2f);
		break;
	case Icon::Plate:
		I.Poly({Vec2(0.2f, 0.16f), Vec2(0.38f, 0.1f), Vec2(0.5f, 0.2f), Vec2(0.62f, 0.1f), Vec2(0.8f, 0.16f), Vec2(0.76f, 0.62f), Vec2(0.5f, 0.9f), Vec2(0.24f, 0.62f)}, MetalC);
		I.DLine(0.5f, 0.24f, 0.5f, 0.84f, 0.03f, MetalDarkC);
		I.DLine(0.3f, 0.42f, 0.7f, 0.42f, 0.03f, MetalDarkC);
		break;
	case Icon::Fletching:
		IconArrow(I, 0.14f, 0.86f, 0.8f, 0.2f, 0.06f, WoodC);
		break;
	case Icon::BoonHardy:
		IconHeart(I, 0.5f, 0.52f, 1.f, RedC);
		I.DCircle(0.4f, 0.4f, 0.07f, Rgba::Hex(0xFF9C8A));
		break;
	case Icon::BoonKeen:
		IconSword(I, 0.5f, 0.5f, 1.f, GoldLightC);
		break;
	case Icon::BoonSwift:
		I.Line(0.24f, 0.88f, 0.64f, 0.3f, 0.07f, WoodC);
		I.Arc(0.62f, 0.52f, 0.36f, 0.08f, -2.4f, -0.9f, MetalC);
		break;
	case Icon::BoonStonewright:
		for (int Row = 0; Row < 3; ++Row)
		{
			const float Y = 0.24f + static_cast<float>(Row) * 0.2f;
			const float Off = Row % 2 == 0 ? 0.f : 0.13f;
			for (int K = 0; K < 3; ++K)
			{
				const float X = 0.12f + Off + static_cast<float>(K) * 0.26f;
				if (X + 0.24f > 0.92f)
				{
					continue;
				}
				I.Rect(X, Y, X + 0.24f, Y + 0.17f, 0.03f, StoneC);
			}
		}
		break;
	case Icon::BoonCoffers:
		I.Rect(0.14f, 0.42f, 0.86f, 0.86f, 0.04f, WoodC);
		I.Rect(0.14f, 0.24f, 0.86f, 0.44f, 0.1f, WoodDarkC);
		I.Rect(0.44f, 0.4f, 0.56f, 0.56f, 0.02f, GoldC);
		I.DLine(0.16f, 0.62f, 0.84f, 0.62f, 0.03f, GoldC);
		break;
	case Icon::Count:
		break;
	}
}
} // namespace

void PaintIcon(Icon I, int Size, ImageRGBA& Out)
{
	Out.Init(Size, Size);
	Painter P(Out);
	IconPen Outline{P, true, MaxF(2.f, static_cast<float>(Size) * 0.035f)};
	DrawIcon(I, Outline);
	IconPen Colour{P, false, 0.f};
	DrawIcon(I, Colour);
}

void PaintDecal(DecalTex T, int Size, ImageRGBA& Out)
{
	Out.Init(Size, Size);
	Painter P(Out);
	const Rgba White(1.f, 1.f, 1.f, 1.f);
	switch (T)
	{
	case DecalTex::SelectionRing:
		P.Ring(0.5f, 0.5f, 0.42f, 0.07f, White);
		P.Ring(0.5f, 0.5f, 0.42f, 0.14f, Rgba(1.f, 1.f, 1.f, 0.25f));
		break;
	case DecalTex::BlobShadow:
		P.RadialGradient(0.5f, 0.5f, 0.5f, Rgba(0.f, 0.f, 0.f, 0.55f), Rgba(0.f, 0.f, 0.f, 0.f));
		break;
	case DecalTex::SoftGlow:
		P.RadialGradient(0.5f, 0.5f, 0.5f, Rgba(1.f, 1.f, 1.f, 0.9f), Rgba(1.f, 1.f, 1.f, 0.f));
		break;
	case DecalTex::RangeRing:
		P.Ring(0.5f, 0.5f, 0.485f, 0.012f, White);
		break;
	case DecalTex::PlacementCell:
		P.RoundRect(0.08f, 0.08f, 0.92f, 0.92f, 0.1f, Rgba(1.f, 1.f, 1.f, 0.3f));
		P.PolyStroke({Vec2(0.08f, 0.08f), Vec2(0.92f, 0.08f), Vec2(0.92f, 0.92f), Vec2(0.08f, 0.92f)}, 0.06f, White);
		break;
	case DecalTex::MoveMarker:
		P.Ring(0.5f, 0.5f, 0.38f, 0.06f, White);
		for (int K = 0; K < 4; ++K)
		{
			const float A = static_cast<float>(K) * Pi * 0.5f + Pi * 0.25f;
			const Vec2 Dir = Vec2::FromAngle(A);
			P.Line(0.5f + Dir.X * 0.12f, 0.5f + Dir.Y * 0.12f, 0.5f + Dir.X * 0.26f, 0.5f + Dir.Y * 0.26f, 0.05f, White);
		}
		break;
	case DecalTex::Count:
		break;
	}
}

void PaintMinimap(const GameMap& Map, int PixelsPerTile, ImageRGBA& Out)
{
	const int P = MaxI(1, PixelsPerTile);
	Out.Init(Map.GetWidth() * P, Map.GetHeight() * P);
	for (int Y = 0; Y < Map.GetHeight(); ++Y)
	{
		for (int X = 0; X < Map.GetWidth(); ++X)
		{
			const MapTile& T = Map.At(X, Y);
			const Rgb Base = GroundColor(T.G, X, Y);
			const Rgb C = MinimapColor(T, X, Y);
			for (int Py = 0; Py < P; ++Py)
			{
				for (int Px = 0; Px < P; ++Px)
				{
					Rgb Use = C;
					if ((T.Tree != TreeKind::None) && ((Px + Py) % 2 == 1))
					{
						Use = Base; // speckle forests so they read as trees
					}
					uint8_t* D = &Out.Px[static_cast<size_t>(((Y * P + Py) * Out.W + (X * P + Px)) * 4)];
					D[0] = Use.R;
					D[1] = Use.G;
					D[2] = Use.B;
					D[3] = 255;
				}
			}
		}
	}
}

} // namespace bh
