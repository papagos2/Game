// Beaconhold simulation core - campaign missions, map loader and mission runtime.
//
// Map legend:
//   .  grass        ,  meadow        :  dirt path     ;  sand        ~  water
//   #  rock         %  blight        T  pine          Y  oak         D  dead tree
//   B  beacon site (2x2)             S  sunstone outcrop (2x2)
//   K  keep (4x4)   C  cottage (2x2) M  muster hall (3x3)  W  watchtower (2x2)  R  storehouse (2x2)
//   H  gloam heart (4x4)  U  burrow (3x3)  X  hexroot (3x3)  F  thorn spire (2x2)
//   w lamplighter  s shieldbearer  r ranger  k stag rider  e sage
//   g gloomling    h thornback     x hexer   o bog titan
//   1-9 wave spawn points
#include "BhMissions.h"

#include "BhMissionMaps.h"

#include <cstring>

namespace bh
{
namespace
{
std::vector<MissionDef> BuildMissions()
{
	std::vector<MissionDef> List;

	// ------------------------------------------------------------------ 1. Kindling (tutorial)
	{
		MissionDef M;
		M.Id = "kindling";
		M.Title = "Kindling";
		M.Tagline = "Tutorial - learn to lead the Wardens";
		M.Briefing = "A Gloam Burrow has sprouted across the stream. Gather Sunstone and Timber, "
					 "raise a Muster Hall, train soldiers and burn the Burrow out.";
		M.VictoryText = "The Burrow is ash. The vale breathes easier tonight.";
		M.DefeatText = "The Keep has fallen. Rally the Wardens and try again.";
		M.Rows = maps::Kindling;
		M.RowCount = maps::KindlingRows;
		M.StartSunstone = 150;
		M.StartTimber = 100;
		M.NodeAmount = 1200;
		M.TaggedArch = Archetype::Burrow;
		M.ParTime = 600.f;
		M.Seed = 101;

		ObjectiveDef O;
		O.Kind = ObjectiveKind::DestroyTagged;
		O.Text = "Destroy the Gloam Burrow";
		M.Objectives.push_back(O);

		auto Step = [&M](const char* Text, TutorialCond Cond, Archetype Arch, int Count, Resource Res, MarkerKind Marker, Archetype MarkerArch, const char* Highlight)
		{
			TutorialStep S;
			S.Text = Text;
			S.Cond = Cond;
			S.Arch = Arch;
			S.Count = Count;
			S.Res = Res;
			S.Marker = Marker;
			S.MarkerArch = MarkerArch;
			S.Highlight = Highlight;
			M.Tutorial.push_back(S);
		};
		Step("Welcome, Warden. The Gloam has crept into the vale. Drag with one finger to look around.",
			TutorialCond::CameraMoved, Archetype::None, 1, Resource::None, MarkerKind::None, Archetype::None, "");
		Step("Tap a Lamplighter to select it.",
			TutorialCond::SelectArch, Archetype::Lamplighter, 1, Resource::None, MarkerKind::OwnArch, Archetype::Lamplighter, "");
		Step("Now tap the golden Sunstone. Lamplighters mine it and carry it to the Keep.",
			TutorialCond::GatherRes, Archetype::None, 1, Resource::Sunstone, MarkerKind::SunstoneNode, Archetype::None, "");
		Step("Select another Lamplighter and tap a tree to cut Timber.",
			TutorialCond::GatherRes, Archetype::None, 1, Resource::Timber, MarkerKind::Tree, Archetype::None, "");
		Step("Tap the Beacon Keep, then train a new Lamplighter.",
			TutorialCond::HaveCount, Archetype::Lamplighter, 4, Resource::None, MarkerKind::OwnArch, Archetype::Keep, "train:Lamplighter");
		Step("Every soldier needs a home. Select a Lamplighter, tap Build, then Cottage. Drag the outline to a clear spot and tap Place.",
			TutorialCond::HaveCount, Archetype::Cottage, 1, Resource::None, MarkerKind::None, Archetype::None, "place:Cottage");
		Step("Now build a Muster Hall the same way - it trains soldiers.",
			TutorialCond::HaveCount, Archetype::Barracks, 1, Resource::None, MarkerKind::None, Archetype::None, "place:Barracks");
		Step("When the Muster Hall is finished, tap it and train 4 Shieldbearers or Rangers.",
			TutorialCond::CombatUnits, Archetype::None, 4, Resource::None, MarkerKind::OwnArch, Archetype::Barracks, "train:Shieldbearer");
		Step("Tap the Army button to select all your soldiers at once.",
			TutorialCond::SelectArmy, Archetype::None, 3, Resource::None, MarkerKind::None, Archetype::None, "army");
		Step("Tap the Gloam Burrow to attack it. Burn it down!",
			TutorialCond::ObjectiveDone, Archetype::None, 0, Resource::None, MarkerKind::EnemyArch, Archetype::Burrow, "");
		List.push_back(M);
	}

	// ------------------------------------------------------------------ 2. The Long Dusk (defense)
	{
		MissionDef M;
		M.Id = "long_dusk";
		M.Title = "The Long Dusk";
		M.Tagline = "Relight the Beacons and hold until dawn";
		M.Briefing = "Night falls early in the north. Relight the two ancient Beacons and hold the Keep "
					 "until dawn. The Gloam will come across the river - build Watchtowers at the fords.";
		M.VictoryText = "Dawn breaks over two burning Beacons. The Gloam slinks back into the fog.";
		M.DefeatText = "The Keep is overrun. Night wins this time.";
		M.Rows = maps::LongDusk;
		M.RowCount = maps::LongDuskRows;
		M.StartSunstone = 250;
		M.StartTimber = 200;
		M.NodeAmount = 1500;
		// Dawn comes at the same time for everyone, so the second star asks for a clean defence.
		M.SecondStar = StarGoal::NoBuildingLost;
		M.Seed = 202;

		ObjectiveDef Beacons;
		Beacons.Kind = ObjectiveKind::BuildCount;
		Beacons.Text = "Relight both Beacons";
		Beacons.Arch = Archetype::Beacon;
		Beacons.Count = 2;
		M.Objectives.push_back(Beacons);
		ObjectiveDef Dawn;
		Dawn.Kind = ObjectiveKind::Survive;
		Dawn.Text = "Hold until dawn";
		Dawn.Seconds = 600.f;
		M.Objectives.push_back(Dawn);

		auto Wave = [&M](float Time, int Spawn, std::vector<WaveUnit> Units, const char* Announce)
		{
			WaveDef W;
			W.Time = Time;
			W.SpawnPoint = Spawn;
			W.Units = Units;
			W.Announce = Announce;
			M.Waves.push_back(W);
		};
		Wave(100.f, 2, {{Archetype::Gloomling, 3}}, "Gloomlings are coming across the middle ford!");
		Wave(160.f, 1, {{Archetype::Gloomling, 4}}, "The Gloam stirs in the north-west.");
		Wave(220.f, 3, {{Archetype::Gloomling, 4}, {Archetype::Thornback, 1}}, "A Thornback leads a raid from the north-east!");
		Wave(280.f, 2, {{Archetype::Gloomling, 6}}, "Gloomlings swarm the middle ford!");
		Wave(340.f, 1, {{Archetype::Gloomling, 4}, {Archetype::Hexer, 2}}, "Hexers approach from the north-west.");
		Wave(400.f, 3, {{Archetype::Thornback, 2}, {Archetype::Gloomling, 5}}, "Thornbacks from the north-east!");
		Wave(460.f, 2, {{Archetype::Gloomling, 5}, {Archetype::Hexer, 3}, {Archetype::Thornback, 2}}, "A large host crosses the middle ford!");
		Wave(520.f, 1, {{Archetype::Thornback, 3}, {Archetype::Hexer, 3}}, "The Gloam presses from the north-west!");
		Wave(520.f, 3, {{Archetype::Gloomling, 6}}, "");
		Wave(575.f, 2, {{Archetype::BogTitan, 1}, {Archetype::Gloomling, 6}}, "A Bog Titan rises! Hold the line until dawn!");
		List.push_back(M);
	}

	// ------------------------------------------------------------------ 3. Heart of the Gloam (conquest)
	{
		MissionDef M;
		M.Id = "heart";
		M.Title = "Heart of the Gloam";
		M.Tagline = "Destroy the Gloam Heart";
		M.Briefing = "The Gloam Heart beats in the north-east. Expand, relight the Beacons for Sunstone, "
					 "and march across the river to end the Gloam for good.";
		M.VictoryText = "The Heart shatters. Light returns to Hollowmere.";
		M.DefeatText = "The Keep has fallen and the vale goes dark.";
		M.Rows = maps::Heart;
		M.RowCount = maps::HeartRows;
		M.StartSunstone = 250;
		M.StartTimber = 200;
		M.NodeAmount = 1800;
		M.TaggedArch = Archetype::GloamHeart;
		M.ParTime = 1080.f;
		M.Seed = 303;

		ObjectiveDef Heart;
		Heart.Kind = ObjectiveKind::DestroyTagged;
		Heart.Text = "Destroy the Gloam Heart";
		M.Objectives.push_back(Heart);
		ObjectiveDef Beacons;
		Beacons.Kind = ObjectiveKind::BuildCount;
		Beacons.Text = "Relight two Beacons (optional)";
		Beacons.Arch = Archetype::Beacon;
		Beacons.Count = 2;
		Beacons.bOptional = true;
		M.Objectives.push_back(Beacons);

		M.AI.bEnabled = true;
		M.AI.StartGloom = 120;
		M.AI.Income = 2.4f;
		M.AI.IncomeGrowth = 0.55f;
		M.AI.MaxIncome = 7.f;
		M.AI.FirstAttack = 210.f;
		M.AI.AttackInterval = 110.f;
		M.AI.FirstWave = 4;
		M.AI.WaveGrowth = 2;
		M.AI.MaxWave = 18;
		M.AI.HomeGuard = 4;
		M.AI.ArmyCap = 36;
		M.AI.TitanAfter = 780.f;
		List.push_back(M);
	}
	return List;
}

const std::vector<MissionDef>& MissionList()
{
	static const std::vector<MissionDef> Missions = BuildMissions();
	return Missions;
}

struct MissionBlockLetter
{
	char Letter;
	Archetype Arch;
	Team Owner;
};

const MissionBlockLetter MissionBlocks[] = {
	{'K', Archetype::Keep, Team::Player},
	{'C', Archetype::Cottage, Team::Player},
	{'M', Archetype::Barracks, Team::Player},
	{'W', Archetype::Watchtower, Team::Player},
	{'R', Archetype::Storehouse, Team::Player},
	{'H', Archetype::GloamHeart, Team::Enemy},
	{'U', Archetype::Burrow, Team::Enemy},
	{'X', Archetype::Hexroot, Team::Enemy},
	{'F', Archetype::ThornSpire, Team::Enemy},
	{'S', Archetype::SunstoneNode, Team::Neutral},
	{'B', Archetype::None, Team::Neutral}, // beacon site
};

const MissionBlockLetter MissionUnits[] = {
	{'w', Archetype::Lamplighter, Team::Player},
	{'s', Archetype::Shieldbearer, Team::Player},
	{'r', Archetype::Ranger, Team::Player},
	{'k', Archetype::StagRider, Team::Player},
	{'e', Archetype::Sage, Team::Player},
	{'g', Archetype::Gloomling, Team::Enemy},
	{'h', Archetype::Thornback, Team::Enemy},
	{'x', Archetype::Hexer, Team::Enemy},
	{'o', Archetype::BogTitan, Team::Enemy},
};

const MissionBlockLetter* FindMissionLetter(const MissionBlockLetter* Table, size_t Count, char C)
{
	for (size_t I = 0; I < Count; ++I)
	{
		if (Table[I].Letter == C)
		{
			return &Table[I];
		}
	}
	return nullptr;
}

int ScaledCount(int Count, float Scale)
{
	const int N = static_cast<int>(static_cast<float>(Count) * Scale + 0.5f);
	return MaxI(1, N);
}
} // namespace

int GetMissionCount()
{
	return static_cast<int>(MissionList().size());
}

const MissionDef& GetMission(int Index)
{
	const std::vector<MissionDef>& List = MissionList();
	return List[static_cast<size_t>(ClampI(Index, 0, static_cast<int>(List.size()) - 1))];
}

DifficultyTuning GetDifficultyTuning(Difficulty D)
{
	DifficultyTuning T;
	switch (D)
	{
	case Difficulty::Easy:
		T.EnemyIncome = 0.7f;
		T.EnemyTiming = 1.3f;
		T.WaveSize = 0.7f;
		T.EnemyHp = 0.85f;
		T.EnemyDamage = 0.85f;
		break;
	case Difficulty::Normal:
		break;
	case Difficulty::Hard:
		T.EnemyIncome = 1.35f;
		T.EnemyTiming = 0.8f;
		T.WaveSize = 1.35f;
		T.EnemyHp = 1.1f;
		T.EnemyDamage = 1.1f;
		break;
	}
	return T;
}

const char* DifficultyName(Difficulty D)
{
	switch (D)
	{
	case Difficulty::Easy:
		return "Easy";
	case Difficulty::Normal:
		return "Normal";
	case Difficulty::Hard:
		return "Hard";
	}
	return "";
}

bool LoadMissionMap(const MissionDef& M, World& W, std::string& OutError)
{
	if (M.Rows == nullptr || M.RowCount <= 0)
	{
		OutError = "mission has no map";
		return false;
	}
	const int Height = M.RowCount;
	const int Width = static_cast<int>(std::strlen(M.Rows[0]));
	for (int Y = 0; Y < Height; ++Y)
	{
		if (static_cast<int>(std::strlen(M.Rows[Y])) != Width)
		{
			OutError = "map row " + std::to_string(Y) + " has the wrong width";
			return false;
		}
	}
	W.Reset(Width, Height, M.Seed);
	GameMap& Map = W.GetMap();

	const size_t NumBlocks = sizeof(MissionBlocks) / sizeof(MissionBlocks[0]);
	const size_t NumUnits = sizeof(MissionUnits) / sizeof(MissionUnits[0]);

	// Pass 1: terrain.
	for (int Y = 0; Y < Height; ++Y)
	{
		for (int X = 0; X < Width; ++X)
		{
			const char C = M.Rows[Y][X];
			MapTile& T = Map.At(X, Y);
			T = MapTile();
			switch (C)
			{
			case '.':
				T.G = Ground::Grass;
				break;
			case ',':
				T.G = Ground::Meadow;
				break;
			case ':':
				T.G = Ground::Dirt;
				break;
			case ';':
				T.G = Ground::Sand;
				break;
			case '~':
				T.G = Ground::Water;
				break;
			case '#':
				T.G = Ground::Rock;
				break;
			case '%':
				T.G = Ground::Blight;
				break;
			case 'T':
				T.G = Ground::Grass;
				T.Tree = TreeKind::Pine;
				T.Wood = GatherTuning::WoodPerTree;
				break;
			case 'Y':
				T.G = Ground::Grass;
				T.Tree = TreeKind::Oak;
				T.Wood = GatherTuning::WoodPerTree;
				break;
			case 'D':
				T.G = Ground::Blight;
				T.Tree = TreeKind::Dead;
				T.Wood = GatherTuning::WoodPerTree / 2;
				break;
			case 'B':
			case 'S':
				T.G = Ground::Dirt;
				break;
			default:
				if (C >= '1' && C <= '9')
				{
					T.G = Ground::Dirt;
					Map.SpawnPoints[C - '0'].push_back(Tile(X, Y));
				}
				else if (const MissionBlockLetter* L = FindMissionLetter(MissionBlocks, NumBlocks, C))
				{
					T.G = L->Owner == Team::Enemy ? Ground::Blight : Ground::Grass;
				}
				else if (const MissionBlockLetter* U = FindMissionLetter(MissionUnits, NumUnits, C))
				{
					T.G = U->Owner == Team::Enemy ? Ground::Blight : Ground::Grass;
				}
				else
				{
					OutError = std::string("unknown map character '") + C + "' at " + std::to_string(X) + "," + std::to_string(Y);
					return false;
				}
				break;
			}
		}
	}

	// Pass 2: multi-tile blocks (buildings, resource nodes, beacon sites).
	std::vector<uint8_t> Claimed(static_cast<size_t>(Width * Height), 0);
	for (int Y = 0; Y < Height; ++Y)
	{
		for (int X = 0; X < Width; ++X)
		{
			const char C = M.Rows[Y][X];
			const MissionBlockLetter* L = FindMissionLetter(MissionBlocks, NumBlocks, C);
			if (L == nullptr || Claimed[static_cast<size_t>(Y * Width + X)] != 0)
			{
				continue;
			}
			const int Size = L->Arch == Archetype::None ? 2 : MaxI(GetDef(L->Arch).Footprint, 1);
			for (int BY = Y; BY < Y + Size; ++BY)
			{
				for (int BX = X; BX < X + Size; ++BX)
				{
					if (BX >= Width || BY >= Height || M.Rows[BY][BX] != C || Claimed[static_cast<size_t>(BY * Width + BX)] != 0)
					{
						OutError = std::string("incomplete block '") + C + "' at " + std::to_string(X) + "," + std::to_string(Y);
						return false;
					}
					Claimed[static_cast<size_t>(BY * Width + BX)] = 1;
				}
			}
			if (L->Arch == Archetype::None)
			{
				const int SiteIndex = static_cast<int>(Map.BeaconSites.size());
				Map.BeaconSites.push_back(TileRect(X, Y, X + 2, Y + 2));
				for (int BY = Y; BY < Y + 2; ++BY)
				{
					for (int BX = X; BX < X + 2; ++BX)
					{
						Map.At(BX, BY).BeaconSite = static_cast<int8_t>(SiteIndex);
					}
				}
			}
			else if (L->Arch == Archetype::SunstoneNode)
			{
				W.SpawnResourceNode(Archetype::SunstoneNode, Tile(X, Y), M.NodeAmount);
			}
			else
			{
				const EntityId Id = W.SpawnBuilding(L->Arch, L->Owner, Tile(X, Y), true);
				if (Entity* E = W.Find(Id))
				{
					if (L->Owner == Team::Enemy && L->Arch == M.TaggedArch)
					{
						E->Tag = 1;
					}
				}
			}
		}
	}

	// Pass 3: units.
	for (int Y = 0; Y < Height; ++Y)
	{
		for (int X = 0; X < Width; ++X)
		{
			if (const MissionBlockLetter* U = FindMissionLetter(MissionUnits, NumUnits, M.Rows[Y][X]))
			{
				W.SpawnUnit(U->Arch, U->Owner, Tile(X, Y).Center());
			}
		}
	}
	W.Events.clear();
	return true;
}

std::string StarGoalText(const MissionDef& M, int Index)
{
	switch (Index)
	{
	case 0:
		return "Victory";
	case 1:
		if (M.SecondStar == StarGoal::NoBuildingLost)
		{
			return "Lose no buildings";
		}
		return std::string("Finish within ") + std::to_string(static_cast<int>(M.ParTime) / 60) + ":" +
			(static_cast<int>(M.ParTime) % 60 < 10 ? "0" : "") + std::to_string(static_cast<int>(M.ParTime) % 60);
	default:
		return "Keep never below half health";
	}
}

bool FindAttackTarget(const World& W, const Vec2& From, Team Attacker, Vec2& OutPos, bool bPreferBeacons)
{
	float BestD = 1e9f;
	bool bFound = false;
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive || !AreEnemies(Attacker, E.Owner) || E.IsResourceNode())
		{
			continue;
		}
		float D = Vec2::Dist(E.Pos, From);
		if (E.IsUnit())
		{
			D += 30.f; // buildings first
		}
		if (bPreferBeacons && E.Type == Archetype::Beacon)
		{
			D -= 20.f;
		}
		if (D < BestD)
		{
			BestD = D;
			OutPos = E.Pos;
			bFound = true;
		}
	}
	return bFound;
}

// ---------------------------------------------------------------------------------------------
// Mission runtime
// ---------------------------------------------------------------------------------------------

void MissionRuntime::Start(int InMissionIndex, Difficulty InDiff, bool bWithTutorial, World& W)
{
	MissionIndex = ClampI(InMissionIndex, 0, GetMissionCount() - 1);
	Diff = InDiff;
	Outcome = MissionOutcome::InProgress;
	Elapsed = 0.f;
	const MissionDef& M = Def();
	Objectives.assign(M.Objectives.size(), ObjectiveState());
	for (size_t I = 0; I < M.Objectives.size(); ++I)
	{
		const ObjectiveDef& O = M.Objectives[I];
		ObjectiveState& S = Objectives[I];
		switch (O.Kind)
		{
		case ObjectiveKind::DestroyTagged:
		{
			int Count = 0;
			for (const Entity& E : W.GetEntities())
			{
				if (E.bAlive && E.Tag == 1)
				{
					++Count;
				}
			}
			S.Target = MaxI(Count, 1);
			break;
		}
		case ObjectiveKind::BuildCount:
		case ObjectiveKind::TrainCount:
		case ObjectiveKind::GatherAmount:
			S.Target = O.Count;
			break;
		case ObjectiveKind::Survive:
			S.Target = static_cast<int>(O.Seconds);
			break;
		}
	}
	NextWave = 0;
	WaveAnnounced.assign(M.Waves.size(), 0);
	WaveUnits.clear();
	RetargetTimer = 0.f;
	bTutorialActive = bWithTutorial && !M.Tutorial.empty();
	TutorialIndex = 0;
	TutorialStepTime = 0.f;
	KeepId = NoEntity;
	for (const Entity& E : W.GetEntities())
	{
		if (E.bAlive && E.Owner == Team::Player && E.Type == Archetype::Keep)
		{
			KeepId = E.Id;
			break;
		}
	}
	KeepMinRatio = 1.f;
	Stars[0] = Stars[1] = Stars[2] = false;
	EndReason.clear();
	if (bTutorialActive)
	{
		W.EmitSimple(EventType::TutorialStep, Team::Player, Vec2(), NoEntity, Archetype::None, 0.f, 0);
	}
}

const TutorialStep* MissionRuntime::CurrentTutorialStep() const
{
	const MissionDef& M = Def();
	if (!bTutorialActive || TutorialIndex < 0 || TutorialIndex >= static_cast<int>(M.Tutorial.size()))
	{
		return nullptr;
	}
	return &M.Tutorial[static_cast<size_t>(TutorialIndex)];
}

float MissionRuntime::NextWaveIn() const
{
	const MissionDef& M = Def();
	if (NextWave >= M.Waves.size())
	{
		return -1.f;
	}
	return M.Waves[NextWave].Time * GetDifficultyTuning(Diff).EnemyTiming - Elapsed;
}

int MissionRuntime::GetStarCount() const
{
	return (Stars[0] ? 1 : 0) + (Stars[1] ? 1 : 0) + (Stars[2] ? 1 : 0);
}

void MissionRuntime::Tick(World& W, float Dt, const PlayerContext& Ctx)
{
	if (Outcome != MissionOutcome::InProgress)
	{
		return;
	}
	Elapsed += Dt;

	// The Keep is the heart of every mission.
	if (KeepId != NoEntity)
	{
		const Entity* Keep = W.Find(KeepId);
		if (Keep == nullptr || !Keep->bAlive)
		{
			Finish(W, MissionOutcome::Lost, Def().DefeatText);
			return;
		}
		KeepMinRatio = MinF(KeepMinRatio, Keep->HpRatio());
	}
	bool bHasAnything = false;
	for (const Entity& E : W.GetEntities())
	{
		if (E.bAlive && E.Owner == Team::Player)
		{
			bHasAnything = true;
			break;
		}
	}
	if (!bHasAnything)
	{
		Finish(W, MissionOutcome::Lost, Def().DefeatText);
		return;
	}

	UpdateWaves(W, Dt);
	UpdateObjectives(W);
	if (Outcome != MissionOutcome::InProgress)
	{
		return;
	}
	UpdateTutorial(W, Dt, Ctx);
}

void MissionRuntime::UpdateWaves(World& W, float Dt)
{
	const MissionDef& M = Def();
	const DifficultyTuning Tuning = GetDifficultyTuning(Diff);
	const GameMap& Map = W.GetMap();

	for (size_t I = 0; I < M.Waves.size(); ++I)
	{
		const WaveDef& Wave = M.Waves[I];
		const float At = Wave.Time * Tuning.EnemyTiming;
		if (WaveAnnounced[I] == 0 && Elapsed >= At - 20.f && Wave.Announce[0] != '\0')
		{
			WaveAnnounced[I] = 1;
			Vec2 Pos;
			const int SP = ClampI(Wave.SpawnPoint, 0, 9);
			if (!Map.SpawnPoints[SP].empty())
			{
				Pos = Map.SpawnPoints[SP].front().Center();
			}
			W.EmitSimple(EventType::WaveIncoming, Team::Enemy, Pos, NoEntity, Archetype::None, At - Elapsed, static_cast<int>(I));
		}
	}

	while (NextWave < M.Waves.size() && Elapsed >= M.Waves[NextWave].Time * Tuning.EnemyTiming)
	{
		const WaveDef& Wave = M.Waves[NextWave];
		const int SP = ClampI(Wave.SpawnPoint, 0, 9);
		Vec2 Origin(static_cast<float>(Map.GetWidth()) * 0.5f, 1.5f);
		if (!Map.SpawnPoints[SP].empty())
		{
			Origin = Map.SpawnPoints[SP].front().Center();
		}
		std::vector<EntityId> Spawned;
		for (const WaveUnit& Group : Wave.Units)
		{
			const int Count = Group.Arch == Archetype::BogTitan ? Group.Count : ScaledCount(Group.Count, Tuning.WaveSize);
			for (int K = 0; K < Count; ++K)
			{
				const Vec2 Offset(W.GetRng().Range(-1.8f, 1.8f), W.GetRng().Range(-0.5f, 1.8f));
				const EntityId Id = W.SpawnUnit(Group.Arch, Team::Enemy, Origin + Offset);
				if (Id != NoEntity)
				{
					Spawned.push_back(Id);
				}
			}
		}
		Vec2 Target;
		if (!Spawned.empty() && FindAttackTarget(W, Origin, Team::Enemy, Target, true))
		{
			W.CmdMove(Spawned, Target, true);
		}
		WaveUnits.insert(WaveUnits.end(), Spawned.begin(), Spawned.end());
		W.EmitSimple(EventType::WaveSpawned, Team::Enemy, Origin, NoEntity, Archetype::None, static_cast<float>(Spawned.size()), static_cast<int>(NextWave));
		++NextWave;
	}

	// Keep wave units pushing towards the next target once they run out of things to do.
	RetargetTimer -= Dt;
	if (RetargetTimer <= 0.f && !WaveUnits.empty())
	{
		RetargetTimer = 2.f;
		std::vector<EntityId> Alive;
		for (EntityId Id : WaveUnits)
		{
			Entity* E = W.Find(Id);
			if (E == nullptr || !E->bAlive)
			{
				continue;
			}
			Alive.push_back(Id);
			if (E->Order == OrderType::Idle && E->EngageTarget == NoEntity)
			{
				Vec2 Target;
				if (FindAttackTarget(W, E->Pos, Team::Enemy, Target, false))
				{
					W.CmdMove(std::vector<EntityId>(1, Id), Target, true);
				}
			}
		}
		WaveUnits.swap(Alive);
	}
}

void MissionRuntime::UpdateObjectives(World& W)
{
	const MissionDef& M = Def();
	bool bAllPrimaryDone = true;
	for (size_t I = 0; I < M.Objectives.size(); ++I)
	{
		const ObjectiveDef& O = M.Objectives[I];
		ObjectiveState& S = Objectives[I];
		if (!S.bDone)
		{
			switch (O.Kind)
			{
			case ObjectiveKind::DestroyTagged:
			{
				int Alive = 0;
				for (const Entity& E : W.GetEntities())
				{
					if (E.bAlive && E.Tag == 1)
					{
						++Alive;
					}
				}
				S.Progress = S.Target - Alive;
				S.bDone = Alive == 0;
				break;
			}
			case ObjectiveKind::BuildCount:
				S.Progress = W.CountOwned(Team::Player, O.Arch, false);
				S.bDone = S.Progress >= O.Count;
				break;
			case ObjectiveKind::TrainCount:
				S.Progress = W.CountOwned(Team::Player, O.Arch, false);
				S.bDone = S.Progress >= O.Count;
				break;
			case ObjectiveKind::Survive:
				S.Progress = static_cast<int>(Elapsed);
				S.bDone = Elapsed >= O.Seconds;
				break;
			case ObjectiveKind::GatherAmount:
			{
				const int R = static_cast<int>(O.Res);
				S.Progress = (R >= 0 && R < NumResources) ? W.GetTeam(Team::Player).Stats.Gathered[R] : 0;
				S.bDone = S.Progress >= O.Count;
				break;
			}
			}
			S.Progress = MinI(S.Progress, S.Target);
			if (S.bDone)
			{
				W.EmitSimple(EventType::ObjectiveCompleted, Team::Player, Vec2(), NoEntity, Archetype::None, 0.f, static_cast<int>(I));
			}
		}
		if (!O.bOptional && !S.bDone)
		{
			bAllPrimaryDone = false;
		}
	}
	if (bAllPrimaryDone && !M.Objectives.empty())
	{
		Finish(W, MissionOutcome::Won, M.VictoryText);
	}
}

bool MissionRuntime::TutorialConditionMet(const TutorialStep& Step, World& W, const PlayerContext& Ctx) const
{
	switch (Step.Cond)
	{
	case TutorialCond::Continue:
		return Ctx.bContinuePressed;
	case TutorialCond::CameraMoved:
		return Ctx.bCameraMoved || Ctx.bContinuePressed;
	case TutorialCond::SelectArch:
		if (Step.Arch != Archetype::None && ((Ctx.SelectionLatch >> ArchIndex(Step.Arch)) & 1u) != 0)
		{
			return true;
		}
		if (Ctx.Selection != nullptr)
		{
			for (EntityId Id : *Ctx.Selection)
			{
				const Entity* E = W.Find(Id);
				if (E != nullptr && E->bAlive && E->Owner == Team::Player && E->Type == Step.Arch)
				{
					return true;
				}
			}
		}
		return false;
	case TutorialCond::GatherRes:
	{
		int Count = 0;
		for (const Entity& E : W.GetEntities())
		{
			if (E.bAlive && E.Owner == Team::Player && E.IsUnit() && E.GatherType == Step.Res &&
				(E.Order == OrderType::Gather || E.Order == OrderType::Return))
			{
				++Count;
			}
		}
		return Count >= Step.Count;
	}
	case TutorialCond::HaveCount:
	{
		int Count = W.CountOwned(Team::Player, Step.Arch, true);
		if (IsUnit(Step.Arch))
		{
			for (const Entity& E : W.GetEntities())
			{
				if (!E.bAlive || E.Owner != Team::Player)
				{
					continue;
				}
				for (const ProductionItem& Item : E.Queue)
				{
					if (!Item.bResearch && Item.Unit == Step.Arch)
					{
						++Count;
					}
				}
			}
		}
		return Count >= Step.Count;
	}
	case TutorialCond::CompleteCount:
		return W.CountOwned(Team::Player, Step.Arch, false) >= Step.Count;
	case TutorialCond::CombatUnits:
		return W.CountUnits(Team::Player, true) >= Step.Count;
	case TutorialCond::SelectArmy:
		if (Ctx.ArmyLatch >= Step.Count)
		{
			return true;
		}
		if (Ctx.Selection != nullptr)
		{
			int Count = 0;
			for (EntityId Id : *Ctx.Selection)
			{
				const Entity* E = W.Find(Id);
				if (E != nullptr && E->bAlive && W.IsCombatUnit(*E))
				{
					++Count;
				}
			}
			return Count >= Step.Count;
		}
		return false;
	case TutorialCond::ObjectiveDone:
		return Step.Count >= 0 && Step.Count < static_cast<int>(Objectives.size()) && Objectives[static_cast<size_t>(Step.Count)].bDone;
	}
	return false;
}

void MissionRuntime::UpdateTutorial(World& W, float Dt, const PlayerContext& Ctx)
{
	if (!bTutorialActive)
	{
		return;
	}
	const MissionDef& M = Def();
	TutorialStepTime += Dt;
	// Advance through every step whose condition already holds (players may run ahead).
	int Guard = 0;
	while (TutorialIndex < static_cast<int>(M.Tutorial.size()) && Guard++ < 16)
	{
		const TutorialStep& Step = M.Tutorial[static_cast<size_t>(TutorialIndex)];
		if (TutorialStepTime < 0.75f || !TutorialConditionMet(Step, W, Ctx))
		{
			break;
		}
		++TutorialIndex;
		TutorialStepTime = 0.f;
		W.EmitSimple(EventType::TutorialStep, Team::Player, Vec2(), NoEntity, Archetype::None, 0.f, TutorialIndex);
	}
	if (TutorialIndex >= static_cast<int>(M.Tutorial.size()))
	{
		bTutorialActive = false;
	}
}

void MissionRuntime::Finish(World& W, MissionOutcome Result, const std::string& Reason)
{
	if (Outcome != MissionOutcome::InProgress)
	{
		return;
	}
	Outcome = Result;
	EndReason = Reason;
	bTutorialActive = false;
	if (Result == MissionOutcome::Won)
	{
		Stars[0] = true;
		Stars[1] = Def().SecondStar == StarGoal::NoBuildingLost ? W.GetTeam(Team::Player).Stats.BuildingsLost == 0 : Elapsed <= Def().ParTime;
		Stars[2] = KeepMinRatio >= 0.5f;
		W.EmitSimple(EventType::MissionWon, Team::Player, Vec2(), NoEntity, Archetype::None, Elapsed, GetStarCount());
	}
	else
	{
		W.EmitSimple(EventType::MissionLost, Team::Player, Vec2(), NoEntity, Archetype::None, Elapsed, 0);
	}
	W.bFrozen = true;
}

} // namespace bh
