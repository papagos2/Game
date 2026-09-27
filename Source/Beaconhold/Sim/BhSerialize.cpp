// Beaconhold simulation core — session save/restore.
//
// One visitor (VisitEntity etc.) describes every field once and is used for both writing and
// reading, so the two can never drift apart. All targets are little-endian.
#include "BhSerialize.h"

#include <cstring>
#include <type_traits>

namespace bh
{
namespace
{
constexpr uint32_t SaveMagic = 0x31534842u; // "BHS1"

template <typename Ar>
void SaveVisitEntity(Ar& A, Entity& E);

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
	template <typename T>
	void operator()(std::vector<T>& V)
	{
		uint32_t N = static_cast<uint32_t>(V.size());
		Pod(N);
		for (T& Item : V)
		{
			(*this)(Item);
		}
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
	template <typename T>
	void operator()(std::vector<T>& V)
	{
		uint32_t N = 0;
		Pod(N);
		if (bError || N > 200000u)
		{
			bError = true;
			return;
		}
		V.clear();
		V.resize(N);
		for (T& Item : V)
		{
			(*this)(Item);
		}
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
	A(E.Queue);
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
	// AI
	AIConfig AICfg;
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
	A(B.Entities);
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
	A(B.AICfg);
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
	A(B.Focus);
	A(B.Distance);
	A(B.Accumulator);
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
	B.AICfg = AI.Config;
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
	B.Focus = S.GetCamera().Focus;
	B.Distance = S.GetCamera().TargetDistance;
	B.Accumulator = S.GetAlpha() * World::TickSeconds;

	OutBytes.clear();
	SaveWriter Writer(OutBytes);
	uint32_t Magic = SaveMagic;
	uint32_t Version = SessionSaveVersion;
	Writer(Magic);
	Writer(Version);
	SaveVisitBlob(Writer, B);
}

bool LoadSession(Session& S, const std::vector<uint8_t>& Bytes, std::string& OutError)
{
	SaveReader Reader(Bytes);
	uint32_t Magic = 0;
	uint32_t Version = 0;
	Reader(Magic);
	Reader(Version);
	if (Reader.Failed() || Magic != SaveMagic)
	{
		OutError = "not a Beaconhold save";
		return false;
	}
	if (Version != SessionSaveVersion)
	{
		OutError = "save from a different game version";
		return false;
	}
	SaveBlob B;
	SaveVisitBlob(Reader, B);
	if (Reader.Failed())
	{
		OutError = "save data is truncated";
		return false;
	}
	if (B.MissionIndex < 0 || B.MissionIndex >= GetMissionCount() || B.Config.MissionIndex != B.MissionIndex)
	{
		OutError = "unknown mission in save";
		return false;
	}
	for (const Entity& E : B.Entities)
	{
		if (E.Type == Archetype::None || E.Type >= Archetype::Count || static_cast<int>(E.Owner) >= NumTeams)
		{
			OutError = "corrupt entity in save";
			return false;
		}
	}
	if (static_cast<int>(B.Entities.size()) > World::MaxEntities)
	{
		OutError = "too many entities in save";
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
	AI.Config = B.AICfg;
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

	S.GetCamera().Init(B.MapW, B.MapH, B.Focus);
	S.GetCamera().Distance = S.GetCamera().TargetDistance = B.Distance;
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
