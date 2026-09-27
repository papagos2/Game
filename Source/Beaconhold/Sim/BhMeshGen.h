// Beaconhold simulation core - low-poly mesh generation.
//
// Builds the actual triangle meshes the game renders: flat-shaded primitives, whole models
// merged into one mesh per archetype (sections grouped by colour), and chunked terrain, tree
// and prop meshes. Units are tiles; the presentation layer scales by 100 for Unreal units.
// Transforms follow Unreal conventions (X forward, Y right, Z up; FRotator Pitch/Yaw/Roll).
#pragma once

#include "BhMap.h"
#include "BhVisuals.h"

#include <cstdint>
#include <vector>

namespace bh
{
struct Vec3
{
	float X = 0.f;
	float Y = 0.f;
	float Z = 0.f;
	constexpr Vec3() = default;
	constexpr Vec3(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}
	Vec3 operator+(const Vec3& O) const { return Vec3(X + O.X, Y + O.Y, Z + O.Z); }
	Vec3 operator-(const Vec3& O) const { return Vec3(X - O.X, Y - O.Y, Z - O.Z); }
	Vec3 operator*(float S) const { return Vec3(X * S, Y * S, Z * S); }
	static float Dot(const Vec3& A, const Vec3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	static Vec3 Cross(const Vec3& A, const Vec3& B) { return Vec3(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X); }
	float Length() const { return std::sqrt(X * X + Y * Y + Z * Z); }
	Vec3 Normalized() const
	{
		const float L = Length();
		return L > 1e-8f ? Vec3(X / L, Y / L, Z / L) : Vec3(0.f, 0.f, 1.f);
	}
};

// Scale, then rotate (Unreal FRotator order), then translate.
struct Xform
{
	Vec3 Scale = Vec3(1.f, 1.f, 1.f);
	float Pitch = 0.f;
	float Yaw = 0.f;
	float Roll = 0.f;
	Vec3 Translation;

	Vec3 Apply(const Vec3& P) const;
	Vec3 ApplyNormal(const Vec3& N) const; // inverse-transpose for non-uniform scale
	static Vec3 Rotate(float Pitch, float Yaw, float Roll, const Vec3& V);
};

struct MeshSection
{
	Rgb Color;
	bool bUnlit = false;
	std::vector<Vec3> Positions;
	std::vector<Vec3> Normals;
	std::vector<float> UVs; // 2 floats per vertex
	std::vector<uint32_t> Indices;
};

struct MeshData
{
	std::vector<MeshSection> Sections;

	MeshSection& SectionFor(Rgb Color, bool bUnlit);
	int TriangleCount() const;
	int VertexCount() const;
	bool Empty() const { return Sections.empty(); }
};

// Full detail for models close to the eye; Low for the thousands of scenery pieces (fewer
// sides, bare icosahedron spheres, no undersides on upright parts resting on the ground).
enum class MeshDetail : uint8_t
{
	Full,
	Low
};

// Adds a unit-sized primitive (centred, size 1) with the given transform and colour.
void AppendShape(MeshData& Out, Shape S, const Xform& T, Rgb Color, bool bUnlit, MeshDetail Detail = MeshDetail::Full);

// A single unit primitive in one white section (used for animated parts and decal planes).
MeshData BuildPrimitive(Shape S, MeshDetail Detail = MeshDetail::Full);

// All parts of a model merged into one mesh. When bStaticOnly, animated parts are left out.
MeshData BuildModelMesh(const ModelDef& M, Team T, bool bStaticOnly);

// Local transform of one model part (before the owner's transform).
Xform PartXform(const PartDef& P);

// Chunked world meshes. Region is in tiles [X0, X1) x [Y0, Y1); may extend outside the map.
// Trees and props use MeshDetail::Low.
MeshData BuildGroundChunk(const std::vector<GroundCell>& Cells, const std::vector<Rgb>& Colors, const TileRect& Region, int MapW, int MapH);
MeshData BuildTreeChunk(const std::vector<TreeInstance>& Trees, const GameMap& Map, const TileRect& Region);
MeshData BuildPropChunk(const std::vector<PropInstance>& Props, const TileRect& Region);

} // namespace bh
