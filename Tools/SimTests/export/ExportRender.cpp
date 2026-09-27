// Exports the exact render buffers the Unreal layer builds (palette geometry, blob shadows,
// palette texture) for a mission scene, so the look can be reviewed in the browser without the
// engine. Usage:
//   bhrender <mission> <seconds> <outdir> [focusX focusY distance]
// Writes <outdir>/render.json (header), opaque.bin, shadow.bin, palette.rgba, shadow.rgba.
#include "Bot.h"

#include "BhPainter.h"
#include "BhRender.h"
#include "BhSession.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace bh;

namespace
{
struct Placement
{
	float Scale = 1.f;
	float ScaleZ = 1.f;
	float Yaw = 0.f;
	Vec3 Translation;
};

// Component transform as Unreal applies it: scale, rotate (yaw), translate.
Vec3 ExportPlace(const Vec3& P, const Placement& T)
{
	const Vec3 Scaled(P.X * T.Scale, P.Y * T.Scale, P.Z * T.Scale * T.ScaleZ);
	return Xform::Rotate(0.f, T.Yaw, 0.f, Scaled) + T.Translation;
}

void ExportAppend(RenderSection& Dst, const RenderSection& Src, const Placement& T, const Vec3& PreOffset)
{
	const uint32_t Base = static_cast<uint32_t>(Dst.Positions.size());
	for (size_t I = 0; I < Src.Positions.size(); ++I)
	{
		Dst.Positions.push_back(ExportPlace(Src.Positions[I] + PreOffset, T));
		Dst.Normals.push_back(Src.Normals[I]);
		Dst.UVs.push_back(Src.UVs[I * 2]);
		Dst.UVs.push_back(Src.UVs[I * 2 + 1]);
	}
	for (uint32_t Index : Src.Indices)
	{
		Dst.Indices.push_back(Base + Index);
	}
}

void ExportAppendMesh(RenderMesh& Dst, const RenderMesh& Src, const Placement& T, const Vec3& PreOffset = Vec3())
{
	ExportAppend(Dst.Opaque, Src.Opaque, T, PreOffset);
	ExportAppend(Dst.Shadow, Src.Shadow, T, PreOffset);
}

bool ExportWriteSection(const std::string& Path, const RenderSection& S)
{
	FILE* F = std::fopen(Path.c_str(), "wb");
	if (F == nullptr)
	{
		return false;
	}
	// Interleaved X Y Z U V (float32), then uint32 indices.
	for (size_t I = 0; I < S.Positions.size(); ++I)
	{
		const float V[5] = {S.Positions[I].X, S.Positions[I].Y, S.Positions[I].Z, S.UVs[I * 2], S.UVs[I * 2 + 1]};
		std::fwrite(V, sizeof(float), 5, F);
	}
	std::fwrite(S.Indices.data(), sizeof(uint32_t), S.Indices.size(), F);
	std::fclose(F);
	return true;
}
} // namespace

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: bhrender <mission> <seconds> <outdir> [focusX focusY distance]\n");
		return 1;
	}
	Session S;
	SessionConfig C;
	C.MissionIndex = std::atoi(Argv[1]);
	C.bTutorial = false;
	std::string Err;
	if (!S.Start(C, Err))
	{
		std::fprintf(stderr, "start failed: %s\n", Err.c_str());
		return 1;
	}
	const float Seconds = static_cast<float>(std::atof(Argv[2]));
	if (Seconds > 0.f)
	{
		bht::BotConfig Cfg;
		bht::PlayMission(S, Cfg, Seconds);
	}
	const std::string Dir = Argv[3];
	const World& W = S.GetWorld();
	const GameMap& Map = W.GetMap();
	CameraRig Cam = S.GetCamera();
	if (Argc >= 7)
	{
		Cam.Focus = Vec2(static_cast<float>(std::atof(Argv[4])), static_cast<float>(std::atof(Argv[5])));
		Cam.Distance = static_cast<float>(std::atof(Argv[6]));
	}

	ColorPalette Palette;
	RenderMesh Scene;
	const Placement Identity;

	// World: terrain, trees and props in 16-tile chunks, plus the forest floor.
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
			ExportAppendMesh(Scene, Chunk, Identity);
		}
	}
	RenderMesh Floor;
	BuildFloorMesh(Map.GetWidth(), Map.GetHeight(), 48.f, Palette, Floor);
	ExportAppendMesh(Scene, Floor, Identity);

	// Ruins on free beacon sites.
	RenderMesh Ruin;
	BuildRuinMesh(Palette, Ruin);
	for (const TileRect& Site : Map.BeaconSites)
	{
		if (Map.At(Site.X0, Site.Y0).Occupant != NoEntity)
		{
			continue;
		}
		Placement P;
		P.Yaw = BuildingYaw;
		P.Translation = Vec3(Site.Center().X, Site.Center().Y, 0.f) * UnrealUnitsPerTile;
		ExportAppendMesh(Scene, Ruin, P);
	}

	// Entities at rest, posed as the engine layer poses them.
	int Count = 0;
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive)
		{
			continue;
		}
		const ModelDef& Model = GetModel(E.Type);
		RenderMesh Body;
		BuildEntityBodyMesh(E.Type, E.Owner, Palette, Body);
		AnimInput In;
		In.Type = E.Type;
		In.Owner = E.Owner;
		In.Act = E.Act;
		In.Carry = E.CarryAmount > 0 ? E.CarryType : Resource::None;
		In.bConstructed = E.bConstructed;
		In.BuildProgress = E.BuildProgress;
		In.Seed = E.Id;
		const EntityPose Pose = EvaluateEntityPose(In);
		Placement P;
		P.Scale = Model.Scale * Pose.ScaleXY;
		P.ScaleZ = Pose.ScaleZ / Pose.ScaleXY;
		P.Yaw = E.IsUnit() ? E.Facing * 180.f / Pi : BuildingYaw;
		P.Translation = (Vec3(E.Pos.X, E.Pos.Y, 0.f) + Pose.Offset) * UnrealUnitsPerTile;
		ExportAppendMesh(Scene, Body, P);
		for (int Role = 1; Role < NumPartRoles; ++Role)
		{
			RenderMesh Part;
			Vec3 Pivot;
			if (!BuildRoleMesh(Model, static_cast<PartRole>(Role), E.Owner, E.IsUnit() ? UnitBakeYaw : BuildingYaw, Palette, Part, Pivot))
			{
				continue;
			}
			if (!EvaluateRolePose(static_cast<PartRole>(Role), In).bVisible)
			{
				continue;
			}
			ExportAppendMesh(Scene, Part, P, Pivot * UnrealUnitsPerTile);
		}
		if (E.IsBuilding() && !E.bConstructed)
		{
			RenderMesh Scaffold;
			BuildScaffoldMesh(GetDef(E.Type).Footprint, Palette, Scaffold);
			Placement SP;
			SP.Yaw = BuildingYaw;
			SP.Translation = Vec3(E.Pos.X, E.Pos.Y, 0.f) * UnrealUnitsPerTile;
			ExportAppendMesh(Scene, Scaffold, SP);
		}
		++Count;
	}

	if (!ExportWriteSection(Dir + "/opaque.bin", Scene.Opaque) || !ExportWriteSection(Dir + "/shadow.bin", Scene.Shadow))
	{
		std::fprintf(stderr, "cannot write to %s\n", Dir.c_str());
		return 1;
	}
	FILE* F = std::fopen((Dir + "/palette.rgba").c_str(), "wb");
	std::fwrite(Palette.GetPixels().data(), 1, Palette.GetPixels().size(), F);
	std::fclose(F);
	ImageRGBA Shadow;
	PaintDecal(DecalTex::BlobShadow, 128, Shadow);
	F = std::fopen((Dir + "/shadow.rgba").c_str(), "wb");
	std::fwrite(Shadow.Px.data(), 1, Shadow.Px.size(), F);
	std::fclose(F);
	F = std::fopen((Dir + "/render.json").c_str(), "w");
	std::fprintf(F,
		"{\"opaqueVerts\":%d,\"opaqueIndices\":%d,\"shadowVerts\":%d,\"shadowIndices\":%d,\"paletteW\":%d,\"paletteH\":%d,\"shadowSize\":128,"
		"\"camera\":{\"fx\":%.3f,\"fy\":%.3f,\"dist\":%.3f,\"pitch\":%.2f,\"yaw\":%.2f,\"fov\":%.2f}}\n",
		static_cast<int>(Scene.Opaque.Positions.size()), static_cast<int>(Scene.Opaque.Indices.size()), static_cast<int>(Scene.Shadow.Positions.size()),
		static_cast<int>(Scene.Shadow.Indices.size()), PaletteColumns, PaletteRows, Cam.Focus.X, Cam.Focus.Y, Cam.Distance, Cam.Pitch, Cam.Yaw, Cam.Fov);
	std::fclose(F);
	std::printf("exported %d entities, %d opaque + %d shadow triangles, %d palette colours (t=%.0fs)\n", Count, Scene.Opaque.TriangleCount(),
		Scene.Shadow.TriangleCount(), Palette.Count(), W.GetTime());
	return 0;
}
