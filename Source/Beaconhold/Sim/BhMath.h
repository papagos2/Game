// Beaconhold simulation core — pure C++17, no Unreal includes.
// Basic 2D math used by the simulation. World units are tiles (1 tile = 1 m = 100 UE units).
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace bh
{
constexpr float Pi = 3.14159265358979f;
constexpr float TwoPi = 6.28318530717959f;

inline float MinF(float A, float B) { return A < B ? A : B; }
inline float MaxF(float A, float B) { return A > B ? A : B; }
inline float ClampF(float V, float Lo, float Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
inline int MinI(int A, int B) { return A < B ? A : B; }
inline int MaxI(int A, int B) { return A > B ? A : B; }
inline int ClampI(int V, int Lo, int Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
inline float LerpF(float A, float B, float T) { return A + (B - A) * T; }
inline float AbsF(float V) { return V < 0.f ? -V : V; }
inline int AbsI(int V) { return V < 0 ? -V : V; }
inline float Saturate(float V) { return ClampF(V, 0.f, 1.f); }
inline float SmoothStep(float T)
{
	const float C = Saturate(T);
	return C * C * (3.f - 2.f * C);
}

struct Vec2
{
	float X = 0.f;
	float Y = 0.f;

	constexpr Vec2() = default;
	constexpr Vec2(float InX, float InY) : X(InX), Y(InY) {}

	constexpr Vec2 operator+(const Vec2& O) const { return Vec2(X + O.X, Y + O.Y); }
	constexpr Vec2 operator-(const Vec2& O) const { return Vec2(X - O.X, Y - O.Y); }
	constexpr Vec2 operator*(float S) const { return Vec2(X * S, Y * S); }
	constexpr Vec2 operator/(float S) const { return Vec2(X / S, Y / S); }
	constexpr Vec2 operator-() const { return Vec2(-X, -Y); }
	Vec2& operator+=(const Vec2& O)
	{
		X += O.X;
		Y += O.Y;
		return *this;
	}
	Vec2& operator-=(const Vec2& O)
	{
		X -= O.X;
		Y -= O.Y;
		return *this;
	}
	Vec2& operator*=(float S)
	{
		X *= S;
		Y *= S;
		return *this;
	}

	float LengthSq() const { return X * X + Y * Y; }
	float Length() const { return std::sqrt(LengthSq()); }
	Vec2 Normalized() const
	{
		const float L = Length();
		return L > 1e-6f ? Vec2(X / L, Y / L) : Vec2();
	}
	static float Dot(const Vec2& A, const Vec2& B) { return A.X * B.X + A.Y * B.Y; }
	static float DistSq(const Vec2& A, const Vec2& B) { return (A - B).LengthSq(); }
	static float Dist(const Vec2& A, const Vec2& B) { return (A - B).Length(); }
	static Vec2 Lerp(const Vec2& A, const Vec2& B, float T) { return A + (B - A) * T; }
	static Vec2 FromAngle(float Radians) { return Vec2(std::cos(Radians), std::sin(Radians)); }
	float Angle() const { return std::atan2(Y, X); }
};

inline Vec2 operator*(float S, const Vec2& V) { return V * S; }

// Shortest signed difference between two angles (radians), in [-Pi, Pi].
inline float AngleDelta(float From, float To)
{
	float D = std::fmod(To - From + Pi, TwoPi);
	if (D < 0.f)
	{
		D += TwoPi;
	}
	return D - Pi;
}

// Rotate an angle towards a target by at most MaxStep radians.
inline float TurnTowards(float Current, float Target, float MaxStep)
{
	const float D = AngleDelta(Current, Target);
	if (AbsF(D) <= MaxStep)
	{
		return Target;
	}
	return Current + (D > 0.f ? MaxStep : -MaxStep);
}

// Integer tile coordinate.
struct Tile
{
	int X = 0;
	int Y = 0;

	constexpr Tile() = default;
	constexpr Tile(int InX, int InY) : X(InX), Y(InY) {}
	bool operator==(const Tile& O) const { return X == O.X && Y == O.Y; }
	bool operator!=(const Tile& O) const { return !(*this == O); }
	Vec2 Center() const { return Vec2(static_cast<float>(X) + 0.5f, static_cast<float>(Y) + 0.5f); }
	static Tile FromPos(const Vec2& P) { return Tile(static_cast<int>(std::floor(P.X)), static_cast<int>(std::floor(P.Y))); }
};

// Axis-aligned tile rectangle (inclusive min, exclusive max).
struct TileRect
{
	int X0 = 0;
	int Y0 = 0;
	int X1 = 0;
	int Y1 = 0;

	constexpr TileRect() = default;
	constexpr TileRect(int InX0, int InY0, int InX1, int InY1) : X0(InX0), Y0(InY0), X1(InX1), Y1(InY1) {}
	bool Contains(const Tile& T) const { return T.X >= X0 && T.X < X1 && T.Y >= Y0 && T.Y < Y1; }
	TileRect Expanded(int N) const { return TileRect(X0 - N, Y0 - N, X1 + N, Y1 + N); }
	Vec2 Center() const { return Vec2((static_cast<float>(X0) + static_cast<float>(X1)) * 0.5f, (static_cast<float>(Y0) + static_cast<float>(Y1)) * 0.5f); }
	int Width() const { return X1 - X0; }
	int Height() const { return Y1 - Y0; }

	// Distance from a point to the rectangle (0 when inside).
	float DistanceTo(const Vec2& P) const
	{
		const float Dx = MaxF(MaxF(static_cast<float>(X0) - P.X, 0.f), P.X - static_cast<float>(X1));
		const float Dy = MaxF(MaxF(static_cast<float>(Y0) - P.Y, 0.f), P.Y - static_cast<float>(Y1));
		return std::sqrt(Dx * Dx + Dy * Dy);
	}
	// Closest point of the rectangle to P.
	Vec2 ClosestPoint(const Vec2& P) const
	{
		return Vec2(ClampF(P.X, static_cast<float>(X0), static_cast<float>(X1)), ClampF(P.Y, static_cast<float>(Y0), static_cast<float>(Y1)));
	}
};

// Small deterministic RNG (xorshift32).
class Rng
{
public:
	explicit Rng(uint32_t Seed = 0x9E3779B9u) : State(Seed != 0 ? Seed : 0x9E3779B9u) {}

	uint32_t Next()
	{
		uint32_t S = State;
		S ^= S << 13;
		S ^= S >> 17;
		S ^= S << 5;
		State = S;
		return S;
	}
	float Float01() { return static_cast<float>(Next() >> 8) * (1.0f / 16777216.0f); }
	float Range(float Lo, float Hi) { return Lo + (Hi - Lo) * Float01(); }
	int RangeInt(int Lo, int HiInclusive)
	{
		const uint32_t Span = static_cast<uint32_t>(HiInclusive - Lo + 1);
		return Lo + static_cast<int>(Next() % (Span == 0 ? 1u : Span));
	}
	bool Chance(float P) { return Float01() < P; }
	uint32_t GetState() const { return State; }
	void SetState(uint32_t S) { State = S != 0 ? S : 0x9E3779B9u; }

private:
	uint32_t State;
};

// Stable hash for procedural variation (tile shades, prop placement).
inline uint32_t HashXY(int X, int Y, uint32_t Seed)
{
	uint32_t H = Seed ^ (static_cast<uint32_t>(X) * 0x8DA6B343u) ^ (static_cast<uint32_t>(Y) * 0xD8163841u);
	H ^= H >> 13;
	H *= 0x5BD1E995u;
	H ^= H >> 15;
	return H;
}

inline float Hash01(int X, int Y, uint32_t Seed) { return static_cast<float>(HashXY(X, Y, Seed) >> 8) * (1.0f / 16777216.0f); }

} // namespace bh
