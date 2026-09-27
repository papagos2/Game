// Exports a mission scene (after the bot has played it for a while) as JSON for the
// browser preview in Tools/Preview. Usage:
//   bhexport <mission> <seconds> <out.json> [focusX focusY distance]
#include "Bot.h"

#include "BhSession.h"
#include "BhVisuals.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace bh;

namespace
{
const char* ShapeName(Shape S)
{
	switch (S)
	{
	case Shape::Cube:
		return "cube";
	case Shape::Cylinder:
		return "cyl";
	case Shape::Sphere:
		return "sphere";
	case Shape::Cone:
		return "cone";
	case Shape::Plane:
		return "plane";
	}
	return "cube";
}

void WriteColor(FILE* F, Rgb C)
{
	std::fprintf(F, "[%d,%d,%d]", C.R, C.G, C.B);
}

void WriteModel(FILE* F, const ModelDef& M, Team T)
{
	std::fprintf(F, "{\"h\":%.3f,\"shadow\":%.3f,\"scale\":%.3f,\"parts\":[", M.Height, M.ShadowRadius, M.Scale);
	for (size_t I = 0; I < M.Parts.size(); ++I)
	{
		const PartDef& P = M.Parts[I];
		std::fprintf(F, "%s{\"s\":\"%s\",\"x\":%.3f,\"y\":%.3f,\"z\":%.3f,\"sx\":%.3f,\"sy\":%.3f,\"sz\":%.3f,\"p\":%.2f,\"yw\":%.2f,\"r\":%.2f,\"u\":%d,\"role\":%d,\"c\":",
			I ? "," : "", ShapeName(P.S), P.X, P.Y, P.Z, P.SX, P.SY, P.SZ, P.Pitch, P.Yaw, P.Roll, P.bUnlit ? 1 : 0, static_cast<int>(P.Role));
		WriteColor(F, ResolveColor(P, T));
		std::fprintf(F, "}");
	}
	std::fprintf(F, "]}");
}
} // namespace

int main(int Argc, char** Argv)
{
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: bhexport <mission> <seconds> <out.json> [focusX focusY distance]\n");
		return 1;
	}
	const int Mission = std::atoi(Argv[1]);
	const float Seconds = static_cast<float>(std::atof(Argv[2]));
	Session S;
	SessionConfig C;
	C.MissionIndex = Mission;
	C.bTutorial = false;
	std::string Err;
	if (!S.Start(C, Err))
	{
		std::fprintf(stderr, "start failed: %s\n", Err.c_str());
		return 1;
	}
	if (Seconds > 0.f)
	{
		bht::BotConfig Cfg;
		bht::PlayMission(S, Cfg, Seconds);
	}
	const World& W = S.GetWorld();
	const GameMap& Map = W.GetMap();
	CameraRig Cam = S.GetCamera();
	if (Argc >= 7)
	{
		Cam.Focus = Vec2(static_cast<float>(std::atof(Argv[4])), static_cast<float>(std::atof(Argv[5])));
		Cam.Distance = static_cast<float>(std::atof(Argv[6]));
	}

	FILE* F = std::fopen(Argv[3], "w");
	if (F == nullptr)
	{
		return 1;
	}
	std::fprintf(F, "{\"w\":%d,\"h\":%d,\"camera\":{\"fx\":%.3f,\"fy\":%.3f,\"dist\":%.3f,\"pitch\":%.2f,\"yaw\":%.2f,\"fov\":%.2f},\n",
		Map.GetWidth(), Map.GetHeight(), Cam.Focus.X, Cam.Focus.Y, Cam.Distance, Cam.Pitch, Cam.Yaw, Cam.Fov);

	// Ground cells (2x2 per tile) with a shared colour table
	std::vector<GroundCell> Cells;
	std::vector<Rgb> Colors;
	BuildGround(Map, 2, Cells, Colors);
	std::fprintf(F, "\"groundColors\":[");
	for (size_t I = 0; I < Colors.size(); ++I)
	{
		std::fprintf(F, "%s", I ? "," : "");
		WriteColor(F, Colors[I]);
	}
	std::fprintf(F, "],\n\"cells\":[");
	for (size_t I = 0; I < Cells.size(); ++I)
	{
		const GroundCell& C2 = Cells[I];
		std::fprintf(F, "%s[%.3f,%.3f,%.3f,%.2f,%d]", I ? "," : "", C2.X, C2.Y, C2.Size, C2.Z, C2.ColorIndex);
	}
	std::fprintf(F, "],\n\"trees\":[");
	std::vector<TreeInstance> Trees;
	BuildTreeInstances(Map, Trees);
	for (size_t I = 0; I < Trees.size(); ++I)
	{
		const TreeInstance& T = Trees[I];
		std::fprintf(F, "%s[%d,%.3f,%.3f,%.1f,%.3f]", I ? "," : "", static_cast<int>(T.Kind), T.X, T.Y, T.Yaw, T.Scale);
	}
	bool bFirst = true;
	std::fprintf(F, "],\n\"ruins\":[");
	for (const TileRect& Site : Map.BeaconSites)
	{
		if (Map.At(Site.X0, Site.Y0).Occupant != NoEntity)
		{
			continue;
		}
		const Vec2 C2 = Site.Center();
		std::fprintf(F, "%s[%.2f,%.2f]", bFirst ? "" : ",", C2.X, C2.Y);
		bFirst = false;
	}
	std::fprintf(F, "],\n\"props\":[");
	std::vector<PropInstance> Props;
	GenerateProps(Map, Props);
	for (size_t I = 0; I < Props.size(); ++I)
	{
		const PropInstance& P = Props[I];
		std::fprintf(F, "%s{\"s\":\"%s\",\"x\":%.3f,\"y\":%.3f,\"z\":%.3f,\"sx\":%.3f,\"sy\":%.3f,\"sz\":%.3f,\"yw\":%.1f,\"p\":%.1f,\"u\":%d,\"c\":",
			I ? "," : "", ShapeName(P.S), P.X, P.Y, P.Z, P.SX, P.SY, P.SZ, P.Yaw, P.Pitch, P.bUnlit ? 1 : 0);
		WriteColor(F, P.Color);
		std::fprintf(F, "}");
	}
	std::fprintf(F, "],\n\"entities\":[");
	bFirst = true;
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive)
		{
			continue;
		}
		std::fprintf(F, "%s{\"a\":%d,\"team\":%d,\"x\":%.3f,\"y\":%.3f,\"f\":%.3f,\"built\":%d,\"prog\":%.2f,\"hp\":%.2f,\"carry\":%d}",
			bFirst ? "" : ",", ArchIndex(E.Type), TeamIndex(E.Owner), E.Pos.X, E.Pos.Y, E.Facing, E.bConstructed ? 1 : 0, E.BuildProgress, E.HpRatio(),
			E.CarryAmount > 0 ? static_cast<int>(E.CarryType) + 1 : 0);
		bFirst = false;
	}
	std::fprintf(F, "],\n\"models\":{");
	for (int A = 0; A < NumArchetypes; ++A)
	{
		std::fprintf(F, "%s\"%d\":{\"1\":", A ? "," : "", A);
		WriteModel(F, GetModel(static_cast<Archetype>(A)), Team::Player);
		std::fprintf(F, ",\"2\":");
		WriteModel(F, GetModel(static_cast<Archetype>(A)), Team::Enemy);
		std::fprintf(F, ",\"0\":");
		WriteModel(F, GetModel(static_cast<Archetype>(A)), Team::Neutral);
		std::fprintf(F, "}");
	}
	std::fprintf(F, "},\n\"treeModels\":{\"1\":");
	WriteModel(F, GetTreeModel(TreeKind::Pine), Team::Neutral);
	std::fprintf(F, ",\"2\":");
	WriteModel(F, GetTreeModel(TreeKind::Oak), Team::Neutral);
	std::fprintf(F, ",\"3\":");
	WriteModel(F, GetTreeModel(TreeKind::Dead), Team::Neutral);
	std::fprintf(F, "},\n\"ruinModel\":");
	WriteModel(F, GetBeaconRuinModel(), Team::Neutral);
	std::fprintf(F, "}\n");
	std::fclose(F);
	std::printf("exported %d entities, %d props to %s (t=%.0fs)\n", static_cast<int>(W.GetEntities().size()), static_cast<int>(Props.size()), Argv[3], W.GetTime());
	return 0;
}
