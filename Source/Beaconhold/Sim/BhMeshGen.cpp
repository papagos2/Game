// Beaconhold simulation core - low-poly mesh generation.
#include "BhMeshGen.h"

#include "BhMath.h"

#include <cmath>

namespace bh
{
namespace
{
constexpr float MeshDeg = 3.14159265358979f / 180.f;

int MeshRoundSides(MeshDetail Detail)
{
	return Detail == MeshDetail::Low ? 6 : 12;
}

// Low-detail scenery sits on the ground: an upright part never shows its underside.
bool MeshSkipsUnderside(MeshDetail Detail, const Xform& Inner)
{
	return Detail == MeshDetail::Low && AbsF(Inner.Pitch) < 0.01f && AbsF(Inner.Roll) < 0.01f;
}

void MeshPushVertex(MeshSection& S, const Vec3& P, const Vec3& N, float U, float V)
{
	S.Positions.push_back(P);
	S.Normals.push_back(N);
	S.UVs.push_back(U);
	S.UVs.push_back(V);
}

// Triangle in local space; winding is fixed so the geometric normal points away from Centre.
struct MeshLocalTri
{
	Vec3 P[3];
	Vec3 N[3];
	float UV[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
};

void MeshEmit(MeshSection& S, MeshLocalTri T, const Xform& Inner, const Xform* Outer, const Vec3& Centre)
{
	// Orient outwards (CCW seen from outside, right-handed maths) before transforming.
	const Vec3 G = Vec3::Cross(T.P[1] - T.P[0], T.P[2] - T.P[0]);
	const Vec3 C = (T.P[0] + T.P[1] + T.P[2]) * (1.f / 3.f);
	if (Vec3::Dot(G, C - Centre) < 0.f)
	{
		const Vec3 TP = T.P[1];
		T.P[1] = T.P[2];
		T.P[2] = TP;
		const Vec3 TN = T.N[1];
		T.N[1] = T.N[2];
		T.N[2] = TN;
		const float U = T.UV[2];
		const float V = T.UV[3];
		T.UV[2] = T.UV[4];
		T.UV[3] = T.UV[5];
		T.UV[4] = U;
		T.UV[5] = V;
	}
	const uint32_t Base = static_cast<uint32_t>(S.Positions.size());
	for (int K = 0; K < 3; ++K)
	{
		Vec3 P = Inner.Apply(T.P[K]);
		Vec3 N = Inner.ApplyNormal(T.N[K]);
		if (Outer != nullptr)
		{
			P = Outer->Apply(P);
			N = Outer->ApplyNormal(N);
		}
		MeshPushVertex(S, P, N, T.UV[K * 2], T.UV[K * 2 + 1]);
	}
	// Mirroring scales flip handedness: keep the transformed triangle outward-facing.
	const Vec3 WG = Vec3::Cross(S.Positions[Base + 1] - S.Positions[Base], S.Positions[Base + 2] - S.Positions[Base]);
	const Vec3 WN = S.Normals[Base] + S.Normals[Base + 1] + S.Normals[Base + 2];
	if (Vec3::Dot(WG, WN) < 0.f)
	{
		S.Indices.push_back(Base);
		S.Indices.push_back(Base + 2);
		S.Indices.push_back(Base + 1);
	}
	else
	{
		S.Indices.push_back(Base);
		S.Indices.push_back(Base + 1);
		S.Indices.push_back(Base + 2);
	}
}

void MeshFlatTri(MeshSection& S, const Vec3& A, const Vec3& B, const Vec3& C, const Xform& Inner, const Xform* Outer, const Vec3& Centre)
{
	MeshLocalTri T;
	T.P[0] = A;
	T.P[1] = B;
	T.P[2] = C;
	Vec3 N = Vec3::Cross(B - A, C - A).Normalized();
	const Vec3 Mid = (A + B + C) * (1.f / 3.f);
	if (Vec3::Dot(N, Mid - Centre) < 0.f)
	{
		N = N * -1.f;
	}
	T.N[0] = T.N[1] = T.N[2] = N;
	MeshEmit(S, T, Inner, Outer, Centre);
}

void MeshCube(MeshSection& S, const Xform& Inner, const Xform* Outer, MeshDetail Detail)
{
	const bool bSkipBottom = MeshSkipsUnderside(Detail, Inner);
	const Vec3 Normals[6] = {Vec3(1, 0, 0), Vec3(-1, 0, 0), Vec3(0, 1, 0), Vec3(0, -1, 0), Vec3(0, 0, 1), Vec3(0, 0, -1)};
	const Vec3 Us[6] = {Vec3(0, 1, 0), Vec3(0, 0, 1), Vec3(0, 0, 1), Vec3(1, 0, 0), Vec3(1, 0, 0), Vec3(0, 1, 0)};
	const Vec3 Vs[6] = {Vec3(0, 0, 1), Vec3(0, 1, 0), Vec3(1, 0, 0), Vec3(0, 0, 1), Vec3(0, 1, 0), Vec3(1, 0, 0)};
	const Vec3 Centre;
	for (int F = 0; F < 6; ++F)
	{
		if (F == 5 && bSkipBottom)
		{
			continue;
		}
		const Vec3 C = Normals[F] * 0.5f;
		const Vec3 U = Us[F] * 0.5f;
		const Vec3 V = Vs[F] * 0.5f;
		const Vec3 P0 = C - U - V;
		const Vec3 P1 = C + U - V;
		const Vec3 P2 = C + U + V;
		const Vec3 P3 = C - U + V;
		MeshFlatTri(S, P0, P1, P2, Inner, Outer, Centre);
		MeshFlatTri(S, P0, P2, P3, Inner, Outer, Centre);
	}
}

void MeshCylinder(MeshSection& S, const Xform& Inner, const Xform* Outer, MeshDetail Detail)
{
	const Vec3 Centre;
	const float R = 0.5f;
	const int Sides = MeshRoundSides(Detail);
	const bool bSkipBottom = MeshSkipsUnderside(Detail, Inner);
	for (int I = 0; I < Sides; ++I)
	{
		const float A0 = TwoPi * static_cast<float>(I) / static_cast<float>(Sides);
		const float A1 = TwoPi * static_cast<float>(I + 1) / static_cast<float>(Sides);
		const Vec3 B0(std::cos(A0) * R, std::sin(A0) * R, -0.5f);
		const Vec3 B1(std::cos(A1) * R, std::sin(A1) * R, -0.5f);
		const Vec3 T0(B0.X, B0.Y, 0.5f);
		const Vec3 T1(B1.X, B1.Y, 0.5f);
		MeshFlatTri(S, B0, B1, T1, Inner, Outer, Centre);
		MeshFlatTri(S, B0, T1, T0, Inner, Outer, Centre);
		MeshFlatTri(S, Vec3(0.f, 0.f, 0.5f), T0, T1, Inner, Outer, Centre);
		if (!bSkipBottom)
		{
			MeshFlatTri(S, Vec3(0.f, 0.f, -0.5f), B1, B0, Inner, Outer, Centre);
		}
	}
}

void MeshCone(MeshSection& S, const Xform& Inner, const Xform* Outer, MeshDetail Detail)
{
	const Vec3 Centre(0.f, 0.f, -0.1f); // inside the cone for orientation tests
	const float R = 0.5f;
	const Vec3 Apex(0.f, 0.f, 0.5f);
	const int Sides = Detail == MeshDetail::Low ? 8 : 12;
	const bool bSkipBase = MeshSkipsUnderside(Detail, Inner);
	for (int I = 0; I < Sides; ++I)
	{
		const float A0 = TwoPi * static_cast<float>(I) / static_cast<float>(Sides);
		const float A1 = TwoPi * static_cast<float>(I + 1) / static_cast<float>(Sides);
		const Vec3 B0(std::cos(A0) * R, std::sin(A0) * R, -0.5f);
		const Vec3 B1(std::cos(A1) * R, std::sin(A1) * R, -0.5f);
		MeshFlatTri(S, B0, B1, Apex, Inner, Outer, Centre);
		if (!bSkipBase)
		{
			MeshFlatTri(S, Vec3(0.f, 0.f, -0.5f), B1, B0, Inner, Outer, Centre);
		}
	}
}

void MeshSphere(MeshSection& S, const Xform& Inner, const Xform* Outer, MeshDetail Detail)
{
	// Icosphere with smooth normals: one subdivision (80 faces), or the bare icosahedron (20) for scenery.
	const float T = (1.f + std::sqrt(5.f)) * 0.5f;
	const Vec3 Ico[12] = {Vec3(-1, T, 0), Vec3(1, T, 0), Vec3(-1, -T, 0), Vec3(1, -T, 0), Vec3(0, -1, T), Vec3(0, 1, T),
		Vec3(0, -1, -T), Vec3(0, 1, -T), Vec3(T, 0, -1), Vec3(T, 0, 1), Vec3(-T, 0, -1), Vec3(-T, 0, 1)};
	const int Faces[20][3] = {{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
		{3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}};
	const Vec3 Centre;
	auto Norm = [](const Vec3& V) { return V.Normalized(); };
	for (const auto& F : Faces)
	{
		const Vec3 A = Norm(Ico[F[0]]);
		const Vec3 B = Norm(Ico[F[1]]);
		const Vec3 C = Norm(Ico[F[2]]);
		if (Detail == MeshDetail::Low)
		{
			MeshLocalTri L;
			const Vec3 Corners[3] = {A, B, C};
			for (int K = 0; K < 3; ++K)
			{
				L.P[K] = Corners[K] * 0.5f;
				L.N[K] = Corners[K];
			}
			MeshEmit(S, L, Inner, Outer, Centre);
			continue;
		}
		const Vec3 AB = Norm((A + B) * 0.5f);
		const Vec3 BC = Norm((B + C) * 0.5f);
		const Vec3 CA = Norm((C + A) * 0.5f);
		const Vec3 Tris[4][3] = {{A, AB, CA}, {B, BC, AB}, {C, CA, BC}, {AB, BC, CA}};
		for (const auto& Tr : Tris)
		{
			MeshLocalTri L;
			for (int K = 0; K < 3; ++K)
			{
				L.P[K] = Tr[K] * 0.5f;
				L.N[K] = Tr[K];
			}
			MeshEmit(S, L, Inner, Outer, Centre);
		}
	}
}

void MeshPlane(MeshSection& S, const Xform& Inner, const Xform* Outer)
{
	const Vec3 Centre(0.f, 0.f, -1.f);
	MeshLocalTri A;
	A.P[0] = Vec3(-0.5f, -0.5f, 0.f);
	A.P[1] = Vec3(0.5f, -0.5f, 0.f);
	A.P[2] = Vec3(0.5f, 0.5f, 0.f);
	A.N[0] = A.N[1] = A.N[2] = Vec3(0.f, 0.f, 1.f);
	const float UA[6] = {0.f, 0.f, 1.f, 0.f, 1.f, 1.f};
	for (int K = 0; K < 6; ++K)
	{
		A.UV[K] = UA[K];
	}
	MeshEmit(S, A, Inner, Outer, Centre);
	MeshLocalTri B;
	B.P[0] = Vec3(-0.5f, -0.5f, 0.f);
	B.P[1] = Vec3(0.5f, 0.5f, 0.f);
	B.P[2] = Vec3(-0.5f, 0.5f, 0.f);
	B.N[0] = B.N[1] = B.N[2] = Vec3(0.f, 0.f, 1.f);
	const float UB[6] = {0.f, 0.f, 1.f, 1.f, 0.f, 1.f};
	for (int K = 0; K < 6; ++K)
	{
		B.UV[K] = UB[K];
	}
	MeshEmit(S, B, Inner, Outer, Centre);
}

void MeshShape(MeshSection& S, Shape Kind, const Xform& Inner, const Xform* Outer, MeshDetail Detail)
{
	switch (Kind)
	{
	case Shape::Cube:
		MeshCube(S, Inner, Outer, Detail);
		break;
	case Shape::Cylinder:
		MeshCylinder(S, Inner, Outer, Detail);
		break;
	case Shape::Sphere:
		MeshSphere(S, Inner, Outer, Detail);
		break;
	case Shape::Cone:
		MeshCone(S, Inner, Outer, Detail);
		break;
	case Shape::Plane:
		MeshPlane(S, Inner, Outer);
		break;
	}
}

void MeshQuad(MeshSection& S, const Vec3& A, const Vec3& B, const Vec3& C, const Vec3& D, const Vec3& N)
{
	// Explicit quad (terrain): vertices given counter-clockwise around N.
	const uint32_t Base = static_cast<uint32_t>(S.Positions.size());
	MeshPushVertex(S, A, N, 0.f, 0.f);
	MeshPushVertex(S, B, N, 1.f, 0.f);
	MeshPushVertex(S, C, N, 1.f, 1.f);
	MeshPushVertex(S, D, N, 0.f, 1.f);
	const Vec3 G = Vec3::Cross(B - A, C - A);
	if (Vec3::Dot(G, N) >= 0.f)
	{
		S.Indices.insert(S.Indices.end(), {Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
	}
	else
	{
		S.Indices.insert(S.Indices.end(), {Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
	}
}

Rgb MeshDarken(Rgb C, float F)
{
	return Rgb(static_cast<uint8_t>(static_cast<float>(C.R) * F), static_cast<uint8_t>(static_cast<float>(C.G) * F), static_cast<uint8_t>(static_cast<float>(C.B) * F));
}
} // namespace

Vec3 Xform::Rotate(float InPitch, float InYaw, float InRoll, const Vec3& V)
{
	const float SP = std::sin(InPitch * MeshDeg);
	const float CP = std::cos(InPitch * MeshDeg);
	const float SY = std::sin(InYaw * MeshDeg);
	const float CY = std::cos(InYaw * MeshDeg);
	const float SR = std::sin(InRoll * MeshDeg);
	const float CR = std::cos(InRoll * MeshDeg);
	// Rows of Unreal's FRotationMatrix; row-vector convention (v * M).
	const Vec3 R0(CP * CY, CP * SY, SP);
	const Vec3 R1(SR * SP * CY - CR * SY, SR * SP * SY + CR * CY, -SR * CP);
	const Vec3 R2(-(CR * SP * CY + SR * SY), CY * SR - CR * SP * SY, CR * CP);
	return R0 * V.X + R1 * V.Y + R2 * V.Z;
}

Vec3 Xform::Apply(const Vec3& P) const
{
	const Vec3 Scaled(P.X * Scale.X, P.Y * Scale.Y, P.Z * Scale.Z);
	return Rotate(Pitch, Yaw, Roll, Scaled) + Translation;
}

Vec3 Xform::ApplyNormal(const Vec3& N) const
{
	const float SX = AbsF(Scale.X) > 1e-6f ? Scale.X : 1e-6f;
	const float SY = AbsF(Scale.Y) > 1e-6f ? Scale.Y : 1e-6f;
	const float SZ = AbsF(Scale.Z) > 1e-6f ? Scale.Z : 1e-6f;
	return Rotate(Pitch, Yaw, Roll, Vec3(N.X / SX, N.Y / SY, N.Z / SZ)).Normalized();
}

MeshSection& MeshData::SectionFor(Rgb Color, bool bUnlit)
{
	for (MeshSection& S : Sections)
	{
		if (S.Color == Color && S.bUnlit == bUnlit)
		{
			return S;
		}
	}
	Sections.emplace_back();
	Sections.back().Color = Color;
	Sections.back().bUnlit = bUnlit;
	return Sections.back();
}

int MeshData::TriangleCount() const
{
	int N = 0;
	for (const MeshSection& S : Sections)
	{
		N += static_cast<int>(S.Indices.size() / 3);
	}
	return N;
}

int MeshData::VertexCount() const
{
	int N = 0;
	for (const MeshSection& S : Sections)
	{
		N += static_cast<int>(S.Positions.size());
	}
	return N;
}

void AppendShape(MeshData& Out, Shape S, const Xform& T, Rgb Color, bool bUnlit, MeshDetail Detail)
{
	MeshShape(Out.SectionFor(Color, bUnlit), S, T, nullptr, Detail);
}

MeshData BuildPrimitive(Shape S, MeshDetail Detail)
{
	MeshData M;
	AppendShape(M, S, Xform(), Rgb(255, 255, 255), false, Detail);
	return M;
}

Xform PartXform(const PartDef& P)
{
	Xform X;
	X.Scale = Vec3(P.SX, P.SY, P.SZ);
	X.Pitch = P.Pitch;
	X.Yaw = P.Yaw;
	X.Roll = P.Roll;
	X.Translation = Vec3(P.X, P.Y, P.Z);
	return X;
}

MeshData BuildModelMesh(const ModelDef& M, Team T, bool bStaticOnly)
{
	MeshData Out;
	for (const PartDef& P : M.Parts)
	{
		if (bStaticOnly && P.Role != PartRole::Static)
		{
			continue;
		}
		MeshShape(Out.SectionFor(ResolveColor(P, T), P.bUnlit), P.S, PartXform(P), nullptr, MeshDetail::Full);
	}
	return Out;
}

MeshData BuildGroundChunk(const std::vector<GroundCell>& Cells, const std::vector<Rgb>& Colors, const TileRect& Region, int MapW, int MapH)
{
	MeshData Out;
	if (Cells.empty())
	{
		return Out;
	}
	const float Size = Cells.front().Size;
	const int Sub = MaxI(1, static_cast<int>(1.f / Size + 0.5f));
	const int CW = MapW * Sub;
	const int CH = MapH * Sub;
	auto CellZ = [&](int CX, int CY) -> float
	{
		if (CX < 0 || CY < 0 || CX >= CW || CY >= CH)
		{
			return -0.35f; // map edge drops to the surrounding forest floor
		}
		return Cells[static_cast<size_t>(CY * CW + CX)].Z;
	};
	for (int CY = MaxI(0, Region.Y0 * Sub); CY < MinI(CH, Region.Y1 * Sub); ++CY)
	{
		for (int CX = MaxI(0, Region.X0 * Sub); CX < MinI(CW, Region.X1 * Sub); ++CX)
		{
			const GroundCell& C = Cells[static_cast<size_t>(CY * CW + CX)];
			const Rgb Col = C.ColorIndex < Colors.size() ? Colors[C.ColorIndex] : palette::Grass;
			MeshSection& S = Out.SectionFor(Col, false);
			const float H = C.Size * 0.5f;
			const float X0 = C.X - H;
			const float X1 = C.X + H;
			const float Y0 = C.Y - H;
			const float Y1 = C.Y + H;
			MeshQuad(S, Vec3(X0, Y0, C.Z), Vec3(X1, Y0, C.Z), Vec3(X1, Y1, C.Z), Vec3(X0, Y1, C.Z), Vec3(0.f, 0.f, 1.f));
			// Banks where the neighbour is lower (water, map edge).
			const int NX[4] = {1, -1, 0, 0};
			const int NY[4] = {0, 0, 1, -1};
			for (int K = 0; K < 4; ++K)
			{
				const float NZ = CellZ(CX + NX[K], CY + NY[K]);
				if (NZ >= C.Z - 0.01f)
				{
					continue;
				}
				MeshSection& Bank = Out.SectionFor(MeshDarken(Col, 0.72f), false);
				const Vec3 N(static_cast<float>(NX[K]), static_cast<float>(NY[K]), 0.f);
				Vec3 A;
				Vec3 B;
				if (K == 0)
				{
					A = Vec3(X1, Y0, 0.f);
					B = Vec3(X1, Y1, 0.f);
				}
				else if (K == 1)
				{
					A = Vec3(X0, Y1, 0.f);
					B = Vec3(X0, Y0, 0.f);
				}
				else if (K == 2)
				{
					A = Vec3(X1, Y1, 0.f);
					B = Vec3(X0, Y1, 0.f);
				}
				else
				{
					A = Vec3(X0, Y0, 0.f);
					B = Vec3(X1, Y0, 0.f);
				}
				MeshQuad(Bank, Vec3(A.X, A.Y, NZ), Vec3(B.X, B.Y, NZ), Vec3(B.X, B.Y, C.Z), Vec3(A.X, A.Y, C.Z), N);
			}
		}
	}
	return Out;
}

MeshData BuildTreeChunk(const std::vector<TreeInstance>& Trees, const GameMap& Map, const TileRect& Region)
{
	MeshData Out;
	for (const TreeInstance& T : Trees)
	{
		const Tile At = Tile::FromPos(Vec2(T.X, T.Y));
		if (!Region.Contains(At))
		{
			continue;
		}
		const bool bOnMap = T.TileX >= 0;
		const bool bStanding = !bOnMap || Map.HasTree(T.TileX, T.TileY);
		Xform Outer;
		Outer.Scale = Vec3(T.Scale, T.Scale, T.Scale);
		Outer.Yaw = T.Yaw;
		Outer.Translation = Vec3(T.X, T.Y, 0.f);
		const ModelDef& Model = bStanding ? GetTreeModel(T.Kind) : GetStumpModel();
		for (const PartDef& P : Model.Parts)
		{
			MeshShape(Out.SectionFor(P.Color, P.bUnlit), P.S, PartXform(P), &Outer, MeshDetail::Low);
		}
	}
	return Out;
}

MeshData BuildPropChunk(const std::vector<PropInstance>& Props, const TileRect& Region)
{
	MeshData Out;
	for (const PropInstance& P : Props)
	{
		if (!Region.Contains(Tile::FromPos(Vec2(P.X, P.Y))))
		{
			continue;
		}
		Xform X;
		X.Scale = Vec3(P.SX, P.SY, P.SZ);
		X.Pitch = P.Pitch;
		X.Yaw = P.Yaw;
		X.Translation = Vec3(P.X, P.Y, P.Z);
		MeshShape(Out.SectionFor(P.Color, P.bUnlit), P.S, X, nullptr, MeshDetail::Low);
	}
	return Out;
}

} // namespace bh
