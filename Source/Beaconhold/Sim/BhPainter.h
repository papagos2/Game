// Beaconhold simulation core — procedural 2D painter for icons and UI/decal textures.
//
// Icons are drawn from anti-aliased signed-distance shapes into RGBA8 images at runtime, so
// the game needs no image assets. The same code produces preview PNGs in the test harness.
#pragma once

#include "BhMap.h"
#include "BhMath.h"
#include "BhTypes.h"

#include <cstdint>
#include <initializer_list>
#include <vector>

namespace bh
{
struct Rgba
{
	float R = 1.f;
	float G = 1.f;
	float B = 1.f;
	float A = 1.f;
	constexpr Rgba() = default;
	constexpr Rgba(float InR, float InG, float InB, float InA = 1.f) : R(InR), G(InG), B(InB), A(InA) {}
	static constexpr Rgba Hex(uint32_t V, float Alpha = 1.f)
	{
		return Rgba(static_cast<float>((V >> 16) & 0xFF) / 255.f, static_cast<float>((V >> 8) & 0xFF) / 255.f, static_cast<float>(V & 0xFF) / 255.f, Alpha);
	}
};

struct ImageRGBA
{
	int W = 0;
	int H = 0;
	std::vector<uint8_t> Px; // RGBA8, straight alpha, row-major, top row first

	void Init(int InW, int InH);
	void Blend(int X, int Y, const Rgba& C, float Coverage);
};

// Draws shapes in normalized coordinates ([0,1] x [0,1], y down) with 1-pixel anti-aliasing.
// Every call takes Grow (pixels) so the same shape can be drawn fatter as an outline pass.
class Painter
{
public:
	explicit Painter(ImageRGBA& InImage) : Img(InImage) {}

	void Circle(float Cx, float Cy, float R, const Rgba& C, float Grow = 0.f);
	void Ring(float Cx, float Cy, float R, float Thickness, const Rgba& C, float Grow = 0.f);
	void RoundRect(float X0, float Y0, float X1, float Y1, float Radius, const Rgba& C, float Grow = 0.f);
	void Line(float X0, float Y0, float X1, float Y1, float Width, const Rgba& C, float Grow = 0.f);
	void Poly(std::initializer_list<Vec2> Points, const Rgba& C, float Grow = 0.f);
	void PolyStroke(std::initializer_list<Vec2> Points, float Width, const Rgba& C, float Grow = 0.f);
	void Arc(float Cx, float Cy, float R, float Width, float A0, float A1, const Rgba& C, float Grow = 0.f);
	void RadialGradient(float Cx, float Cy, float R, const Rgba& Inner, const Rgba& Outer);

private:
	template <typename Fn>
	void Fill(float BX0, float BY0, float BX1, float BY1, const Rgba& C, Fn&& SignedDistancePx);
	float Scale() const { return static_cast<float>(Img.W); }

	ImageRGBA& Img;
};

// Paints a square icon with a transparent background.
void PaintIcon(Icon I, int Size, ImageRGBA& Out);

enum class DecalTex : uint8_t
{
	SelectionRing,
	BlobShadow,
	SoftGlow,
	RangeRing,
	PlacementCell,
	MoveMarker,
	Count,
};
void PaintDecal(DecalTex T, int Size, ImageRGBA& Out);

// Minimap base image: PixelsPerTile pixels per map tile.
void PaintMinimap(const GameMap& Map, int PixelsPerTile, ImageRGBA& Out);

} // namespace bh
