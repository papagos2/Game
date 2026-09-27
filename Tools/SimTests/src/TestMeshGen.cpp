// Tests for low-poly mesh generation.
#include "TestFramework.h"

#include "BhMeshGen.h"
#include "BhMissions.h"
#include "BhWorld.h"

#include <cmath>

using namespace bh;

namespace
{
bool Near(const Vec3& A, const Vec3& B, float Eps = 1e-4f)
{
	return std::fabs(A.X - B.X) < Eps && std::fabs(A.Y - B.Y) < Eps && std::fabs(A.Z - B.Z) < Eps;
}
} // namespace

BH_TEST(Mesh_RotationFollowsUnrealConvention)
{
	BH_EXPECT(Near(Xform::Rotate(0.f, 90.f, 0.f, Vec3(1, 0, 0)), Vec3(0, 1, 0)));  // yaw turns X towards Y
	BH_EXPECT(Near(Xform::Rotate(90.f, 0.f, 0.f, Vec3(1, 0, 0)), Vec3(0, 0, 1)));  // pitch raises the nose
	BH_EXPECT(Near(Xform::Rotate(0.f, 0.f, 90.f, Vec3(0, 0, 1)), Vec3(0, 1, 0)));  // roll tips up towards right
	BH_EXPECT(Near(Xform::Rotate(0.f, 0.f, 90.f, Vec3(0, 1, 0)), Vec3(0, 0, -1)));
}

BH_TEST(Mesh_PrimitivesAreClosedAndOutward)
{
	const Shape Shapes[5] = {Shape::Cube, Shape::Cylinder, Shape::Sphere, Shape::Cone, Shape::Plane};
	const int Expected[2][5] = {{12, 48, 80, 24, 2}, {10, 18, 20, 8, 2}};
	for (int Pass = 0; Pass < 10; ++Pass)
	{
		const int I = Pass % 5;
		const MeshDetail Detail = Pass < 5 ? MeshDetail::Full : MeshDetail::Low;
		const MeshData M = BuildPrimitive(Shapes[I], Detail);
		BH_EXPECT(M.Sections.size() == 1);
		BH_EXPECT_MSG(M.TriangleCount() == Expected[Pass / 5][I], "shape %d (pass %d) has %d triangles", I, Pass, M.TriangleCount());
		const MeshSection& S = M.Sections[0];
		BH_EXPECT(S.UVs.size() == S.Positions.size() * 2);
		const Vec3 Centre = Shapes[I] == Shape::Cone ? Vec3(0, 0, -0.1f) : (Shapes[I] == Shape::Plane ? Vec3(0, 0, -1) : Vec3());
		int Inward = 0;
		for (size_t T = 0; T + 2 < S.Indices.size(); T += 3)
		{
			const Vec3 A = S.Positions[S.Indices[T]];
			const Vec3 B = S.Positions[S.Indices[T + 1]];
			const Vec3 C = S.Positions[S.Indices[T + 2]];
			const Vec3 G = Vec3::Cross(B - A, C - A);
			const Vec3 Mid = (A + B + C) * (1.f / 3.f);
			if (Vec3::Dot(G, Mid - Centre) <= 0.f)
			{
				++Inward;
			}
			for (const Vec3& P : {A, B, C})
			{
				BH_EXPECT(std::fabs(P.X) <= 0.5001f && std::fabs(P.Y) <= 0.5001f && std::fabs(P.Z) <= 0.5001f);
			}
		}
		BH_EXPECT_MSG(Inward == 0, "shape %d has %d inward triangles", I, Inward);
		for (const Vec3& N : S.Normals)
		{
			BH_EXPECT(std::fabs(N.Length() - 1.f) < 1e-3f);
		}
	}
}

BH_TEST(Mesh_ModelsMergeWithinBudget)
{
	for (int A = 0; A < NumArchetypes; ++A)
	{
		const ModelDef& Model = GetModel(static_cast<Archetype>(A));
		const MeshData Full = BuildModelMesh(Model, Team::Player, false);
		const MeshData Static = BuildModelMesh(Model, Team::Player, true);
		BH_EXPECT_MSG(Full.TriangleCount() > 0 && Full.TriangleCount() < 5000, "%s: %d tris", ArchetypeKey(static_cast<Archetype>(A)), Full.TriangleCount());
		BH_EXPECT(Static.TriangleCount() <= Full.TriangleCount());
		BH_EXPECT_MSG(Full.Sections.size() <= 12, "%s: %d sections", ArchetypeKey(static_cast<Archetype>(A)), static_cast<int>(Full.Sections.size()));
	}
	// Team colours resolve differently.
	const MeshData P = BuildModelMesh(GetModel(Archetype::Gloomling), Team::Player, false);
	const MeshData E = BuildModelMesh(GetModel(Archetype::Gloomling), Team::Enemy, false);
	bool bDiffers = false;
	for (const MeshSection& S : P.Sections)
	{
		bool bFound = false;
		for (const MeshSection& O : E.Sections)
		{
			bFound = bFound || O.Color == S.Color;
		}
		bDiffers = bDiffers || !bFound;
	}
	BH_EXPECT(bDiffers);
}

BH_TEST(Mesh_TerrainChunksCoverTheMap)
{
	World W;
	std::string Err;
	BH_EXPECT(LoadMissionMap(GetMission(2), W, Err));
	const GameMap& Map = W.GetMap();
	std::vector<GroundCell> Cells;
	std::vector<Rgb> Colors;
	BuildGround(Map, 2, Cells, Colors);
	BH_EXPECT(static_cast<int>(Cells.size()) == Map.GetWidth() * Map.GetHeight() * 4);
	int TopQuads = 0;
	int TotalTris = 0;
	for (int Y = 0; Y < Map.GetHeight(); Y += 16)
	{
		for (int X = 0; X < Map.GetWidth(); X += 16)
		{
			const TileRect R(X, Y, X + 16, Y + 16);
			const MeshData Ground = BuildGroundChunk(Cells, Colors, R, Map.GetWidth(), Map.GetHeight());
			TotalTris += Ground.TriangleCount();
			for (const MeshSection& S : Ground.Sections)
			{
				for (const Vec3& N : S.Normals)
				{
					TopQuads += (N.Z > 0.5f) ? 1 : 0;
				}
			}
		}
	}
	BH_EXPECT(TopQuads / 4 == static_cast<int>(Cells.size()));
	std::vector<TreeInstance> Trees;
	BuildTreeInstances(Map, Trees);
	int TreeTris = 0;
	for (int Y = -8; Y < Map.GetHeight() + 8; Y += 16)
	{
		for (int X = -8; X < Map.GetWidth() + 8; X += 16)
		{
			TreeTris += BuildTreeChunk(Trees, Map, TileRect(X, Y, X + 16, Y + 16)).TriangleCount();
		}
	}
	std::printf("  ground %d tris, trees %d tris (%d trees)\n", TotalTris, TreeTris, static_cast<int>(Trees.size()));
	// Scenery budget: the whole forest of the largest map, of which the camera sees a fraction.
	BH_EXPECT_MSG(TreeTris > 0 && TreeTris < 160000, "forest has %d triangles", TreeTris);
}
