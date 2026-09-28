// Beaconhold simulation core - session save/restore.
//
// One visitor (VisitEntity etc.) describes every field once and is used for both writing and
// reading, so the two can never drift apart. All targets are little-endian.
#include "BhSerialize.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>

namespace bh
{
namespace
{
constexpr uint32_t SaveMagic = 0x31534842u; // "BHS1"
constexpr uint32_t MaxSavedVector = 200000u;

template <typename Ar>
void SaveVisitEntity(Ar& A, Entity& E);

uint64_t Checksum(const uint8_t* Data, size_t Size)
{
	uint64_t H = 1469598103934665603ull; // FNV-1a
	for (size_t I = 0; I < Size; ++I)
	{
		H ^= Data[I];
		H *= 1099511628211ull;
	}
	return H;
}

class SaveWriter
{
public:
	explicit SaveWriter(std::vector<uint8_t>& InBuf) : Buf(InBuf) {}

	template <typename T>
	void Pod(const T& V)
	{
		static_assert(std::is_trivially_copyable<T>::value, "only trivially copyable types can be written raw");
		const size_t Old = Buf.size();
		Buf.resize(Old + sizeof(T));
		std::memcpy(Buf.data() + Old, &V, sizeof(T));
	}
	template <typename T>
	void operator()(T& V)
	{
		Pod(V);
	}
	void operator()(bool& V)
	{
		const uint8_t Raw = V ? 1 : 0;
		Pod(Raw);
	}
	template <typename T>
	void Array(std::vector<T>& V, uint32_t /*MaxCount*/)
	{
		uint32_t N = static_cast<uint32_t>(V.size());
		Pod(N);
		for (T& Item : V)
		{
			(*this)(Item);
		}
	}
	template <typename T>
	void operator()(std::vector<T>& V)
	{
		Array(V, MaxSavedVector);
	}
	void operator()(std::string& V)
	{
		uint32_t N = static_cast<uint32_t>(V.size());
		Pod(N);
		Buf.insert(Buf.end(), V.begin(), V.end());
	}
	void operator()(Entity& E) { SaveVisitEntity(*this, E); }
	void Size(size_t& V)
	{
		uint32_t N = static_cast<uint32_t>(V);
		Pod(N);
	}
	bool Failed() const { return false; }

private:
	std::vector<uint8_t>& Buf;
};

class SaveReader
{
public:
	explicit SaveReader(const std::vector<uint8_t>& InBuf) : Buf(InBuf) {}

	template <typename T>
	void Pod(T& V)
	{
		static_assert(std::is_trivially_copyable<T>::value, "only trivially copyable types can be read raw");
		if (bError || Pos + sizeof(T) > Buf.size())
		{
			bError = true;
			return;
		}
		std::memcpy(&V, Buf.data() + Pos, sizeof(T));
		Pos += sizeof(T);
	}
	template <typename T>
	void operator()(T& V)
	{
		Pod(V);
	}
	void operator()(bool& V)
	{
		uint8_t Raw = 0;
		Pod(Raw);
		bError = bError || Raw > 1;
		V = Raw == 1;
	}
	template <typename T>
	void Array(std::vector<T>& V, uint32_t MaxCount)
	{
		uint32_t N = 0;
		Pod(N);
		// Each element takes at least a byte (a plain struct its full size), so a damaged count
		// can never make us allocate more than the save itself could hold.
		const size_t MinBytes = std::is_trivially_copyable<T>::value ? sizeof(T) : 1;
		if (bError || N > MaxCount || static_cast<size_t>(N) * MinBytes > Buf.size() - Pos)
		{
			bError = true;
			return;
		}
		V = std::vector<T>(N);
		for (T& Item : V)
		{
			(*this)(Item);
		}
	}
	template <typename T>
	void operator()(std::vector<T>& V)
	{
		Array(V, MaxSavedVector);
	}
	void operator()(std::string& V)
	{
		uint32_t N = 0;
		Pod(N);
		if (bError || Pos + N > Buf.size())
		{
			bError = true;
			return;
		}
		V.assign(reinterpret_cast<const char*>(Buf.data() + Pos), N);
		Pos += N;
	}
	void operator()(Entity& E) { SaveVisitEntity(*this, E); }
	void Size(size_t& V)
	{
		uint32_t N = 0;
		Pod(N);
		V = N;
	}
	bool Failed() const { return bError; }
	bool AtEnd() const { return Pos == Buf.size(); }

private:
	const std::vector<uint8_t>& Buf;
	size_t Pos = 0;
	bool bError = false;
};

template <typename Ar>
void SaveVisitEntity(Ar& A, Entity& E)
{
	A(E.Id);
	A(E.Type);
	A(E.Kind);
	A(E.Owner);
	A(E.bAlive);
	A(E.Tag);
	A(E.Pos);
	A(E.PrevPos);
	A(E.Facing);
	A(E.Radius);
	A(E.Hp);
	A(E.MaxHp);
	A(E.Rect);
	A(E.Order);
	A(E.OrderPoint);
	A(E.OrderTarget);
	A(E.EngageTarget);
	A(E.EngageTimer);
	A(E.LeashPoint);
	A(E.Path);
	A.Size(E.PathIndex);
	A(E.PathGoal);
	A(E.bHasPath);
	A(E.RepathTimer);
	A(E.StuckTimer);
	A(E.BestWaypointDist);
	A(E.AttackCooldown);
	A(E.WindupTimer);
	A(E.WindupTarget);
	A(E.ScanTimer);
	A(E.LastDamagedTime);
	A(E.LastAttacker);
	A(E.AttackSerial);
	A(E.Kills);
	A(E.CarryType);
	A(E.CarryAmount);
	A(E.GatherTimer);
	A(E.GatherFxTimer);
	A(E.GatherType);
	A(E.GatherTile);
	A(E.GatherNode);
	A(E.BuildTarget);
	A(E.bResumeGather);
	A(E.AbilityCooldown);
	A(E.Buff);
	A(E.BuffTimer);
	A(E.bChargeReady);
	A(E.HealTimer);
	A(E.bConstructed);
	A(E.BuildProgress);
	A(E.Builders);
	A.Array(E.Queue, static_cast<uint32_t>(MaxQueueLength));
	A(E.bHasRally);
	A(E.RallyPoint);
	A(E.RallyNode);
	A(E.bRallyTree);
	A(E.RallyTree);
	A(E.IncomeAccum);
	A(E.Amount);
	A(E.Miners);
	A(E.Act);
	A(E.SpawnTime);
}

// Everything except the entity list lives in these plain structs so both directions share code.
struct SaveBlob
{
	SessionConfig Config;
	int MapW = 0;
	int MapH = 0;
	std::vector<MapTile> Tiles;
	std::vector<TileRect> BeaconSites;
	std::vector<Tile> SpawnPoints[10];
	std::vector<Entity> Entities;
	std::vector<Projectile> Projectiles;
	TeamState Teams[NumTeams];
	float Time = 0.f;
	uint32_t NextId = 1;
	uint32_t NextProjectileId = 1;
	uint32_t RngState = 1;
	bool bFrozen = false;
	// Mission
	int MissionIndex = 0;
	Difficulty Diff = Difficulty::Normal;
	MissionOutcome Outcome = MissionOutcome::InProgress;
	float Elapsed = 0.f;
	std::vector<ObjectiveState> Objectives;
	size_t NextWave = 0;
	std::vector<uint8_t> WaveAnnounced;
	std::vector<EntityId> WaveUnits;
	float RetargetTimer = 0.f;
	bool bTutorialActive = false;
	int TutorialIndex = 0;
	float TutorialStepTime = 0.f;
	EntityId KeepId = NoEntity;
	float KeepMinRatio = 1.f;
	bool Stars[3] = {false, false, false};
	std::string EndReason;
	// AI (its configuration is the mission's, never changed in play, so it is not saved)
	Difficulty AIDiff = Difficulty::Normal;
	float GloomAccum = 0.f;
	float ThinkTimer = 0.f;
	float NextAttack = 0.f;
	int WaveSize = 0;
	int WavesLaunched = 0;
	Vec2 Home;
	Vec2 Rally;
	std::vector<EntityId> Attackers;
	float LastDefenseTime = 0.f;
	std::vector<AIPendingPick> AIPending;
	// Camera & clock
	Vec2 Focus;
	float Distance = 20.f;
	float Accumulator = 0.f;
};

template <typename Ar>
void SaveVisitBlob(Ar& A, SaveBlob& B)
{
	A(B.Config);
	A(B.MapW);
	A(B.MapH);
	A(B.Tiles);
	A(B.BeaconSites);
	for (std::vector<Tile>& Points : B.SpawnPoints)
	{
		A(Points);
	}
	A.Array(B.Entities, static_cast<uint32_t>(World::MaxEntities));
	A(B.Projectiles);
	for (TeamState& T : B.Teams)
	{
		A(T);
	}
	A(B.Time);
	A(B.NextId);
	A(B.NextProjectileId);
	A(B.RngState);
	A(B.bFrozen);
	A(B.MissionIndex);
	A(B.Diff);
	A(B.Outcome);
	A(B.Elapsed);
	A(B.Objectives);
	A.Size(B.NextWave);
	A(B.WaveAnnounced);
	A(B.WaveUnits);
	A(B.RetargetTimer);
	A(B.bTutorialActive);
	A(B.TutorialIndex);
	A(B.TutorialStepTime);
	A(B.KeepId);
	A(B.KeepMinRatio);
	for (bool& Star : B.Stars)
	{
		A(Star);
	}
	A(B.EndReason);
	A(B.AIDiff);
	A(B.GloomAccum);
	A(B.ThinkTimer);
	A(B.NextAttack);
	A(B.WaveSize);
	A(B.WavesLaunched);
	A(B.Home);
	A(B.Rally);
	A(B.Attackers);
	A(B.LastDefenseTime);
	A(B.AIPending);
	A(B.Focus);
	A(B.Distance);
	A(B.Accumulator);
}

// ---------------------------------------------------------------------------------------------
// Validation: everything a loaded save feeds into array indices, loops, allocations, positions
// and arithmetic is checked before any of it reaches the game. Mere implausibility (hit points
// above the maximum and the like) is left alone: it cannot crash anything.
// ---------------------------------------------------------------------------------------------

// A bool inside a struct read as raw bytes: look at the byte, never at the (maybe invalid) bool.
bool BoolByte(const bool& V)
{
	uint8_t Raw = 0;
	std::memcpy(&Raw, &V, 1);
	return Raw <= 1;
}

bool Finite(float V)
{
	return std::isfinite(V);
}

bool InRange(float V, float Lo, float Hi)
{
	return std::isfinite(V) && V >= Lo && V <= Hi;
}

bool InRange(int V, int Lo, int Hi)
{
	return V >= Lo && V <= Hi;
}

struct SaveCheck
{
	float MapW = 0.f;
	float MapH = 0.f;
	int TilesW = 0;
	int TilesH = 0;

	bool OnMap(const Vec2& P) const { return InRange(P.X, 0.f, MapW) && InRange(P.Y, 0.f, MapH); }
	bool OnMap(const Tile& T) const { return InRange(T.X, 0, TilesW - 1) && InRange(T.Y, 0, TilesH - 1); }
	bool OnMap(const TileRect& R) const { return R.X0 >= 0 && R.Y0 >= 0 && R.X0 < R.X1 && R.Y0 < R.Y1 && R.X1 <= TilesW && R.Y1 <= TilesH; }
};

bool ValidTeam(Team T)
{
	return static_cast<int>(T) < NumTeams;
}

bool ValidArchetype(Archetype A)
{
	return A < Archetype::Count;
}

bool ValidResource(Resource R)
{
	return R == Resource::Sunstone || R == Resource::Timber || R == Resource::None;
}

bool ValidDifficulty(Difficulty D)
{
	return D == Difficulty::Easy || D == Difficulty::Normal || D == Difficulty::Hard;
}

const char* CheckEntity(const Entity& E, const SaveCheck& C, uint32_t NextId)
{
	if (E.Id == NoEntity || E.Id >= NextId || !ValidArchetype(E.Type) || E.Kind != GetDef(E.Type).Kind || !ValidTeam(E.Owner))
	{
		return "entity identity";
	}
	if (E.Order > OrderType::Build || !ValidResource(E.CarryType) || !ValidResource(E.GatherType) || E.Buff > BuffType::Charge ||
		E.Act > Activity::Carrying)
	{
		return "entity state";
	}
	const float Timers[] = {E.Facing, E.EngageTimer, E.RepathTimer, E.StuckTimer, E.BestWaypointDist, E.AttackCooldown, E.WindupTimer, E.ScanTimer,
		E.LastDamagedTime, E.GatherTimer, E.GatherFxTimer, E.AbilityCooldown, E.BuffTimer, E.HealTimer, E.IncomeAccum, E.SpawnTime};
	for (float T : Timers)
	{
		if (!Finite(T))
		{
			return "entity timer";
		}
	}
	if (!InRange(E.Radius, 0.f, E.IsUnit() ? World::MaxUnitRadius : 4.f) || !InRange(E.MaxHp, 0.001f, 1e6f) || !InRange(E.Hp, -1e6f, 1e6f) || !InRange(E.BuildProgress, 0.f, 1.f))
	{
		return "entity health";
	}
	if (!C.OnMap(E.Pos) || !C.OnMap(E.PrevPos) || !C.OnMap(E.OrderPoint) || !C.OnMap(E.LeashPoint) || !C.OnMap(E.PathGoal) || !C.OnMap(E.RallyPoint) ||
		!C.OnMap(E.GatherTile) || !C.OnMap(E.RallyTree) || (!E.IsUnit() && !C.OnMap(E.Rect)))
	{
		return "entity position";
	}
	if (E.PathIndex > E.Path.size() || E.Path.size() > static_cast<size_t>(C.TilesW * C.TilesH))
	{
		return "entity path";
	}
	for (const Vec2& P : E.Path)
	{
		if (!C.OnMap(P))
		{
			return "entity path";
		}
	}
	if (!InRange(E.CarryAmount, 0, 1000) || !InRange(E.Amount, 0, 1000000) || !InRange(E.Kills, 0, 1000000) || !InRange(E.Builders, 0, 1000) ||
		!InRange(E.Miners, 0, 1000))
	{
		return "entity counters";
	}
	for (const ProductionItem& Item : E.Queue)
	{
		if (!BoolByte(Item.bResearch) || (Item.bResearch ? Item.Tech >= Research::Count : !ValidArchetype(Item.Unit)) || !InRange(Item.Total, 0.001f, 1e5f) ||
			!InRange(Item.Elapsed, 0.f, 1e5f) || !InRange(Item.PaidSunstone, 0, 100000) || !InRange(Item.PaidTimber, 0, 100000))
		{
			return "production queue";
		}
	}
	return nullptr;
}

const char* CheckBlob(const SaveBlob& B)
{
	// Session and map.
	if (B.MissionIndex < 0 || B.MissionIndex >= GetMissionCount() || B.Config.MissionIndex != B.MissionIndex)
	{
		return "unknown mission";
	}
	if (!ValidDifficulty(B.Config.Diff) || !ValidDifficulty(B.Diff) || !ValidDifficulty(B.AIDiff) || !BoolByte(B.Config.bTutorial))
	{
		return "settings";
	}
	for (int Rank : B.Config.BoonRanks)
	{
		if (!InRange(Rank, 0, MaxBoonRank))
		{
			return "settings";
		}
	}
	if (!InRange(B.MapW, 1, 1024) || !InRange(B.MapH, 1, 1024) || B.Tiles.size() != static_cast<size_t>(B.MapW) * static_cast<size_t>(B.MapH) ||
		B.BeaconSites.size() > 127u)
	{
		return "map size";
	}
	SaveCheck C;
	C.MapW = static_cast<float>(B.MapW);
	C.MapH = static_cast<float>(B.MapH);
	C.TilesW = B.MapW;
	C.TilesH = B.MapH;
	for (const MapTile& T : B.Tiles)
	{
		if (T.G >= Ground::Count || T.Tree > TreeKind::Dead || T.BeaconSite < -1 || T.BeaconSite >= static_cast<int>(B.BeaconSites.size()))
		{
			return "map tile";
		}
	}
	for (const TileRect& Site : B.BeaconSites)
	{
		if (!C.OnMap(Site))
		{
			return "beacon site";
		}
	}
	for (const std::vector<Tile>& Points : B.SpawnPoints)
	{
		for (const Tile& P : Points)
		{
			if (!C.OnMap(P))
			{
				return "spawn point";
			}
		}
	}

	// World.
	if (!InRange(B.Time, 0.f, 1e6f) || B.NextId == NoEntity || B.NextId > 0x7FFFFFFFu)
	{
		return "world clock";
	}
	std::vector<EntityId> Ids;
	Ids.reserve(B.Entities.size());
	for (const Entity& E : B.Entities)
	{
		if (const char* Bad = CheckEntity(E, C, B.NextId))
		{
			return Bad;
		}
		Ids.push_back(E.Id);
	}
	std::sort(Ids.begin(), Ids.end());
	if (std::adjacent_find(Ids.begin(), Ids.end()) != Ids.end())
	{
		return "duplicate entity";
	}
	for (const Projectile& P : B.Projectiles)
	{
		if (!BoolByte(P.bAlive) || !ValidTeam(P.Owner) || !(ValidArchetype(P.SourceType) || P.SourceType == Archetype::None) || !C.OnMap(P.Pos) ||
			!C.OnMap(P.PrevPos) || !C.OnMap(P.Start) || !C.OnMap(P.TargetPos) || !InRange(P.Speed, 0.01f, 1000.f) || !InRange(P.Damage, 0.f, 1e6f) ||
			!InRange(P.Splash, 0.f, 100.f) || !InRange(P.BuildingMult, 0.f, 100.f) || !InRange(P.Travelled, 0.f, 1e4f) || !InRange(P.TotalDist, 0.f, 1e4f))
		{
			return "projectile";
		}
	}
	for (const TeamState& T : B.Teams)
	{
		bool bFlags = BoolByte(T.bIgnoreSupply);
		for (const bool& R : T.Researched)
		{
			bFlags = bFlags && BoolByte(R);
		}
		const TeamStats& St = T.Stats;
		if (!bFlags || !InRange(T.Res[0], 0, 10000000) || !InRange(T.Res[1], 0, 10000000) || !InRange(T.SupplyUsed, 0, 100000) ||
			!InRange(T.SupplyCap, 0, 100000) || !InRange(T.HpMult, 0.01f, 100.f) || !InRange(T.DamageMult, 0.01f, 100.f) ||
			!InRange(T.GatherMult, 0.01f, 100.f) || !InRange(T.BuildTimeMult, 0.01f, 100.f) || !InRange(T.BuildingHpMult, 0.01f, 100.f) ||
			!Finite(T.LastAlertTime) || !InRange(St.UnitsTrained, 0, 10000000) || !InRange(St.UnitsLost, 0, 10000000) || !InRange(St.Kills, 0, 10000000) ||
			!InRange(St.BuildingsBuilt, 0, 10000000) || !InRange(St.BuildingsLost, 0, 10000000) || !InRange(St.Gathered[0], 0, 1000000000) ||
			!InRange(St.Gathered[1], 0, 1000000000))
		{
			return "team";
		}
	}

	// Mission, enemy commander, camera.
	const MissionDef& Def = GetMission(B.MissionIndex);
	if (B.Outcome > MissionOutcome::Lost || !InRange(B.Elapsed, 0.f, 1e6f) || B.NextWave > Def.Waves.size() ||
		!InRange(B.TutorialIndex, 0, static_cast<int>(Def.Tutorial.size())) || !Finite(B.TutorialStepTime) || !Finite(B.RetargetTimer) ||
		!Finite(B.KeepMinRatio))
	{
		return "mission state";
	}
	for (const ObjectiveState& O : B.Objectives)
	{
		if (!BoolByte(O.bDone))
		{
			return "objective";
		}
	}
	if (!Finite(B.GloomAccum) || !Finite(B.ThinkTimer) || !Finite(B.NextAttack) || !Finite(B.LastDefenseTime) || !InRange(B.WaveSize, 0, 1000) ||
		!InRange(B.WavesLaunched, 0, 1000000) || !C.OnMap(B.Home) || !C.OnMap(B.Rally))
	{
		return "enemy commander";
	}
	for (const AIPendingPick& P : B.AIPending)
	{
		if (!ValidArchetype(P.Unit) || !Finite(P.Since))
		{
			return "enemy commander";
		}
	}
	if (!Finite(B.Focus.X) || !Finite(B.Focus.Y) || !Finite(B.Distance) || !Finite(B.Accumulator))
	{
		return "camera";
	}
	return nullptr;
}
} // namespace

void SaveSession(const Session& S, std::vector<uint8_t>& OutBytes)
{
	SaveBlob B;
	const World& W = S.GetWorld();
	const GameMap& Map = W.GetMap();
	const MissionRuntime& M = S.GetMission();
	const EnemyAI& AI = S.GetAI();
	B.Config = S.GetConfig();
	B.MapW = Map.GetWidth();
	B.MapH = Map.GetHeight();
	B.Tiles = Map.GetTiles();
	B.BeaconSites = Map.BeaconSites;
	for (int I = 0; I < 10; ++I)
	{
		B.SpawnPoints[I] = Map.SpawnPoints[I];
	}
	B.Entities = W.GetEntities();
	B.Projectiles = W.GetProjectiles();
	for (int I = 0; I < NumTeams; ++I)
	{
		B.Teams[I] = W.GetTeam(static_cast<Team>(I));
	}
	B.Time = W.GetTime();
	B.NextId = W.GetNextId();
	B.NextProjectileId = W.GetNextProjectileId();
	B.RngState = W.GetRngState();
	B.bFrozen = W.bFrozen;
	B.MissionIndex = M.MissionIndex;
	B.Diff = M.Diff;
	B.Outcome = M.Outcome;
	B.Elapsed = M.Elapsed;
	B.Objectives = M.Objectives;
	B.NextWave = M.NextWave;
	B.WaveAnnounced = M.WaveAnnounced;
	B.WaveUnits = M.WaveUnits;
	B.RetargetTimer = M.RetargetTimer;
	B.bTutorialActive = M.bTutorialActive;
	B.TutorialIndex = M.TutorialIndex;
	B.TutorialStepTime = M.TutorialStepTime;
	B.KeepId = M.KeepId;
	B.KeepMinRatio = M.KeepMinRatio;
	for (int I = 0; I < 3; ++I)
	{
		B.Stars[I] = M.Stars[I];
	}
	B.EndReason = M.EndReason;
	B.AIDiff = AI.Diff;
	B.GloomAccum = AI.GloomAccum;
	B.ThinkTimer = AI.ThinkTimer;
	B.NextAttack = AI.NextAttack;
	B.WaveSize = AI.WaveSize;
	B.WavesLaunched = AI.WavesLaunched;
	B.Home = AI.Home;
	B.Rally = AI.Rally;
	B.Attackers = AI.Attackers;
	B.LastDefenseTime = AI.LastDefenseTime;
	B.AIPending = AI.Pending;
	B.Focus = S.GetCamera().Focus;
	B.Distance = S.GetCamera().TargetDistance;
	B.Accumulator = S.GetAlpha() * World::TickSeconds;

	OutBytes.clear();
	SaveWriter Writer(OutBytes);
	uint32_t Magic = SaveMagic;
	uint32_t Version = SessionSaveVersion;
	uint64_t Sum = 0;
	Writer(Magic);
	Writer(Version);
	Writer(Sum); // filled in below
	SaveVisitBlob(Writer, B);
	ResealSave(OutBytes);
}

void ResealSave(std::vector<uint8_t>& Bytes)
{
	if (Bytes.size() >= SaveHeaderBytes)
	{
		const uint64_t Sum = Checksum(Bytes.data() + SaveHeaderBytes, Bytes.size() - SaveHeaderBytes);
		std::memcpy(Bytes.data() + 8, &Sum, sizeof(Sum));
	}
}

bool IsSaveIntact(const uint8_t* Data, size_t Size, std::string& OutError)
{
	uint32_t Magic = 0;
	uint32_t Version = 0;
	uint64_t Sum = 0;
	if (Data == nullptr || Size < SaveHeaderBytes)
	{
		OutError = "not a Beaconhold save";
		return false;
	}
	std::memcpy(&Magic, Data, sizeof(Magic));
	std::memcpy(&Version, Data + 4, sizeof(Version));
	std::memcpy(&Sum, Data + 8, sizeof(Sum));
	if (Magic != SaveMagic)
	{
		OutError = "not a Beaconhold save";
		return false;
	}
	if (Version != SessionSaveVersion)
	{
		OutError = "save from a different game version";
		return false;
	}
	if (Sum != Checksum(Data + SaveHeaderBytes, Size - SaveHeaderBytes))
	{
		OutError = "save data is damaged";
		return false;
	}
	return true;
}

bool LoadSession(Session& S, const std::vector<uint8_t>& Bytes, std::string& OutError)
{
	if (!IsSaveIntact(Bytes.data(), Bytes.size(), OutError))
	{
		return false;
	}
	SaveReader Reader(Bytes);
	uint32_t Magic = 0;
	uint32_t Version = 0;
	uint64_t Sum = 0;
	Reader(Magic);
	Reader(Version);
	Reader(Sum);
	SaveBlob B;
	SaveVisitBlob(Reader, B);
	if (Reader.Failed() || !Reader.AtEnd())
	{
		OutError = "save data is malformed";
		return false;
	}
	if (const char* Bad = CheckBlob(B))
	{
		OutError = std::string("invalid ") + Bad + " in save";
		return false;
	}

	World& W = S.GetWorld();
	W.Reset(B.MapW, B.MapH, B.RngState);
	GameMap& Map = W.GetMap();
	if (!Map.Assign(B.MapW, B.MapH, B.Tiles))
	{
		OutError = "corrupt map in save";
		return false;
	}
	Map.BeaconSites = B.BeaconSites;
	for (int I = 0; I < 10; ++I)
	{
		Map.SpawnPoints[I] = B.SpawnPoints[I];
	}
	W.MutableEntities() = B.Entities;
	W.MutableProjectiles() = B.Projectiles;
	for (int I = 0; I < NumTeams; ++I)
	{
		W.GetTeam(static_cast<Team>(I)) = B.Teams[I];
	}
	W.RestoreCounters(B.NextId, B.NextProjectileId, B.Time, B.RngState);
	W.bFrozen = B.bFrozen;
	W.RebuildIndex();
	W.RecomputeSupply();

	MissionRuntime& M = S.GetMission();
	M.MissionIndex = B.MissionIndex;
	M.Diff = B.Diff;
	M.Outcome = B.Outcome;
	M.Elapsed = B.Elapsed;
	M.Objectives = B.Objectives;
	M.NextWave = B.NextWave;
	M.WaveAnnounced = B.WaveAnnounced;
	M.WaveAnnounced.resize(M.Def().Waves.size(), 0);
	M.WaveUnits = B.WaveUnits;
	M.RetargetTimer = B.RetargetTimer;
	M.bTutorialActive = B.bTutorialActive;
	M.TutorialIndex = B.TutorialIndex;
	M.TutorialStepTime = B.TutorialStepTime;
	M.KeepId = B.KeepId;
	M.KeepMinRatio = B.KeepMinRatio;
	for (int I = 0; I < 3; ++I)
	{
		M.Stars[I] = B.Stars[I];
	}
	M.EndReason = B.EndReason;
	M.Objectives.resize(M.Def().Objectives.size());

	EnemyAI& AI = S.GetAI();
	AI.Config = M.Def().AI;
	AI.Diff = B.AIDiff;
	AI.GloomAccum = B.GloomAccum;
	AI.ThinkTimer = B.ThinkTimer;
	AI.NextAttack = B.NextAttack;
	AI.WaveSize = B.WaveSize;
	AI.WavesLaunched = B.WavesLaunched;
	AI.Home = B.Home;
	AI.Rally = B.Rally;
	AI.Attackers = B.Attackers;
	AI.LastDefenseTime = B.LastDefenseTime;
	AI.Pending = B.AIPending;

	S.GetCamera().Init(B.MapW, B.MapH, B.Focus);
	S.GetCamera().Distance = S.GetCamera().TargetDistance = B.Distance;
	S.GetCamera().Clamp();
	S.GetControl().Reset();
	S.GetGestures().Reset();
	S.RestoreConfig(B.Config);
	S.SetAccumulator(ClampF(B.Accumulator, 0.f, World::TickSeconds));
	S.bPaused = false;
	S.Speed = 1.f;
	return true;
}

uint64_t HashWorld(const World& W)
{
	uint64_t H = 1469598103934665603ull;
	auto Mix = [&H](uint64_t V)
	{
		for (int I = 0; I < 8; ++I)
		{
			H ^= (V >> (I * 8)) & 0xFFu;
			H *= 1099511628211ull;
		}
	};
	for (const Entity& E : W.GetEntities())
	{
		Mix(E.Id);
		Mix(static_cast<uint64_t>(E.Type));
		Mix(static_cast<uint64_t>(static_cast<int64_t>(E.Pos.X * 1000.f)));
		Mix(static_cast<uint64_t>(static_cast<int64_t>(E.Pos.Y * 1000.f)));
		Mix(static_cast<uint64_t>(static_cast<int64_t>(E.Hp * 100.f)));
		Mix(static_cast<uint64_t>(E.Order));
	}
	for (int T = 0; T < NumTeams; ++T)
	{
		Mix(static_cast<uint64_t>(W.GetTeam(static_cast<Team>(T)).Res[0]));
		Mix(static_cast<uint64_t>(W.GetTeam(static_cast<Team>(T)).Res[1]));
	}
	Mix(static_cast<uint64_t>(W.GetTime() * 1000.f));
	return H;
}

} // namespace bh
