// Playtest telemetry.
#include "Playtest.h"

#include "BhHud.h"
#include "BhSerialize.h"

#include <cmath>
#include <cstdio>

using namespace bh;

namespace bht
{
namespace
{
bool Finite(float V)
{
	return std::isfinite(V);
}

bool IsWardenArch(int Arch)
{
	return Arch >= 0 && Arch < NumArchetypes && GetDef(static_cast<Archetype>(Arch)).Faction == Team::Player;
}

std::string Clock(float Seconds)
{
	if (Seconds < 0.f)
	{
		return "never";
	}
	return FormatTime(Seconds);
}
} // namespace

void Telemetry::Begin(const Session& S)
{
	const World& W = S.GetWorld();
	for (const Entity& E : W.GetEntities())
	{
		TypeOf[E.Id] = E.Type;
	}
	const TeamState& P = W.GetTeam(Team::Player);
	for (int R = 0; R < NumResources; ++R)
	{
		PrevRes[R] = P.Res[R];
	}
	NextSample = W.GetTime();
	LastSampleTime = W.GetTime();
	NextSaveCheck = W.GetTime() + SaveCheckInterval;
}

void Telemetry::OnTick(const Session& S, const std::vector<GameEvent>& Events)
{
	const World& W = S.GetWorld();
	const float Now = W.GetTime();
	PeakEventsPerTick = MaxI(PeakEventsPerTick, static_cast<int>(Events.size()));
	for (const GameEvent& E : Events)
	{
		switch (E.Type)
		{
		case EventType::EntitySpawned:
			TypeOf[E.A] = E.Arch;
			break;
		case EventType::UnitTrained:
			if (E.Arch != Archetype::None)
			{
				++Types[ArchIndex(E.Arch)].Trained;
				if (E.Owner == Team::Player && !GetDef(E.Arch).IsWorker && FirstSoldier < 0.f)
				{
					FirstSoldier = Now;
				}
			}
			break;
		case EventType::ResourceDelivered:
			if (E.Owner == Team::Player && E.Sub >= 0 && E.Sub < NumResources)
			{
				const int Amount = static_cast<int>(E.Value);
				if (IsBuilding(E.Arch))
				{
					PassiveIncome[E.Sub] += Amount; // Beacons and other income buildings
				}
				else
				{
					WorkerIncome[E.Sub] += Amount;
				}
				GatheredThisSample[E.Sub] += Amount;
			}
			break;
		case EventType::Hit:
		{
			if (E.Sub >= 0 && E.Sub < NumArchetypes)
			{
				Types[E.Sub].DamageDealt += E.Value;
			}
			if (E.Arch != Archetype::None)
			{
				Types[ArchIndex(E.Arch)].DamageTaken += E.Value;
			}
			const bool bPlayerInvolved = E.Owner == Team::Player || IsWardenArch(E.Sub);
			if (bPlayerInvolved)
			{
				if (FirstCombat < 0.f)
				{
					FirstCombat = Now;
				}
				if (LastCombat >= 0.f && Now - LastCombat > LullThreshold)
				{
					Lulls.push_back({LastCombat, Now});
					LongestLull = MaxF(LongestLull, Now - LastCombat);
				}
				LastCombat = Now;
				const float Second = std::floor(Now);
				if (Second != LastCombatSecond)
				{
					LastCombatSecond = Second;
					CombatSeconds += 1.f;
				}
			}
			break;
		}
		case EventType::EntityDied:
		{
			if (E.Arch != Archetype::None)
			{
				++Types[ArchIndex(E.Arch)].Lost;
			}
			const auto Killer = TypeOf.find(E.B);
			if (E.B != NoEntity && Killer != TypeOf.end() && Killer->second != Archetype::None)
			{
				++Types[ArchIndex(Killer->second)].Kills;
			}
			if (E.Owner == Team::Player)
			{
				if (IsUnit(E.Arch) && FirstLoss < 0.f)
				{
					FirstLoss = Now;
				}
				if (IsBuilding(E.Arch) && FirstBuildingLost < 0.f)
				{
					FirstBuildingLost = Now;
				}
			}
			break;
		}
		case EventType::WaveIncoming:
			if (E.Sub >= 1000)
			{
				WaveTimes.push_back(Now); // AI wave launched (scripted waves announce ahead of time)
			}
			break;
		case EventType::WaveSpawned:
			WaveTimes.push_back(Now);
			break;
		default:
			break;
		}
	}
	if (Now >= NextSample)
	{
		Sample(S, Now - LastSampleTime);
		LastSampleTime = Now;
		NextSample = Now + 1.f;
	}
}

void Telemetry::Sample(const Session& S, float Dt)
{
	const World& W = S.GetWorld();
	const float Now = W.GetTime();
	++Samples;
	const TeamState& P = W.GetTeam(Team::Player);
	for (int R = 0; R < NumResources; ++R)
	{
		FloatIntegral[R] += static_cast<float>(P.Res[R]) * Dt;
		const int Spend = PrevRes[R] + GatheredThisSample[R] - P.Res[R];
		if (Spend > 0)
		{
			Spent[R] += Spend;
		}
		PrevRes[R] = P.Res[R];
		GatheredThisSample[R] = 0;
	}
	if (P.SupplyCap < SupplyHardCap && P.SupplyUsed >= P.SupplyCap)
	{
		SupplyBlockedSeconds += Dt;
	}
	const int Gloom = W.GetTeam(Team::Enemy).Res[0];
	PeakGloom = MaxI(PeakGloom, Gloom);
	GloomFloatIntegral += static_cast<float>(Gloom) * Dt;

	int PlayerUnits = 0;
	int EnemyUnits = 0;
	int Alive = 0;
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive)
		{
			continue;
		}
		++Alive;
		if (!E.IsUnit())
		{
			continue;
		}
		if (E.Owner == Team::Player)
		{
			++PlayerUnits;
		}
		else if (E.Owner == Team::Enemy)
		{
			++EnemyUnits;
		}
		if (E.Owner == Team::Player && GetDef(E.Type).IsWorker)
		{
			if (E.Order == OrderType::Idle && E.EngageTarget == NoEntity)
			{
				IdleWorkerSeconds += Dt;
			}
			else if ((E.Order == OrderType::Gather || E.Order == OrderType::Return) && E.GatherType != Resource::None)
			{
				WorkerSeconds[static_cast<int>(E.GatherType)] += Dt;
				if (E.Order == OrderType::Gather && E.Act == Activity::Idle)
				{
					WaitingWorkerSeconds += Dt;
				}
			}
		}

		// Stuck: trying to walk but not getting anywhere.
		const bool bTrying = E.Act == Activity::Walking || E.Act == Activity::Carrying;
		auto It = Trackers.find(E.Id);
		if (!bTrying)
		{
			if (It != Trackers.end())
			{
				Trackers.erase(It);
			}
			continue;
		}
		if (It == Trackers.end())
		{
			Trackers[E.Id] = Tracker{E.Pos, Now, false, E.AttackSerial};
			continue;
		}
		Tracker& T = It->second;
		if (Vec2::Dist(E.Pos, T.Anchor) > StuckRadius || E.AttackSerial != T.Swings)
		{
			T = Tracker{E.Pos, Now, false, E.AttackSerial};
		}
		else if (!T.bReported && Now - T.Since >= StuckSeconds)
		{
			T.bReported = true;
			StuckIncident I;
			I.Id = E.Id;
			I.Type = E.Type;
			I.Owner = E.Owner;
			I.Pos = E.Pos;
			I.Time = Now;
			I.Order = E.Order;
			const EntityId TargetId = E.EngageTarget != NoEntity ? E.EngageTarget : E.OrderTarget;
			if (const Entity* Target = W.Find(TargetId))
			{
				I.TargetType = Target->Type;
				I.TargetPos = Target->Pos;
			}
			else
			{
				I.TargetPos = E.OrderPoint;
			}
			I.bHasPath = E.bHasPath;
			I.PathLeft = E.Path.size() > E.PathIndex ? E.Path.size() - E.PathIndex : 0;
			I.EngageTimer = E.EngageTimer;
			I.StuckTimer = E.StuckTimer;
			Stuck.push_back(I);
		}
	}
	PeakPlayerUnits = MaxI(PeakPlayerUnits, PlayerUnits);
	PeakEnemyUnits = MaxI(PeakEnemyUnits, EnemyUnits);
	PeakEntities = MaxI(PeakEntities, Alive);
	PeakProjectiles = MaxI(PeakProjectiles, static_cast<int>(W.GetProjectiles().size()));
	CheckInvariants(S);
	if (Now >= NextSaveCheck && S.GetMission().Outcome == MissionOutcome::InProgress)
	{
		NextSaveCheck = Now + SaveCheckInterval;
		CheckSaveRoundTrip(S);
	}
}

void Telemetry::CheckSaveRoundTrip(const Session& S)
{
	// The app can be suspended at any moment: the save must load and hold the very same world.
	std::vector<uint8_t> Bytes;
	SaveSession(S, Bytes);
	Session Copy;
	std::string Err;
	++SaveChecks;
	if (!LoadSession(Copy, Bytes, Err))
	{
		Violation("suspend save at " + FormatTime(S.GetWorld().GetTime()) + " does not load: " + Err);
	}
	else if (HashWorld(Copy.GetWorld()) != HashWorld(S.GetWorld()))
	{
		Violation("suspend save at " + FormatTime(S.GetWorld().GetTime()) + " restores a different world");
	}
}

void Telemetry::Violation(const std::string& Text)
{
	if (Violations.size() < 40)
	{
		Violations.push_back(Text);
	}
	else if (Violations.size() == 40)
	{
		Violations.push_back("... (more)");
	}
}

void Telemetry::CheckInvariants(const Session& S)
{
	const World& W = S.GetWorld();
	const GameMap& Map = W.GetMap();
	const float Now = W.GetTime();
	char Buf[200];
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive)
		{
			continue;
		}
		const char* Name = GetDef(E.Type).Name;
		if (!Finite(E.Pos.X) || !Finite(E.Pos.Y) || !Finite(E.Hp))
		{
			std::snprintf(Buf, sizeof(Buf), "t=%.0f %s #%u has a non-finite position or health", Now, Name, E.Id);
			Violation(Buf);
			continue;
		}
		if (E.Pos.X < 0.f || E.Pos.Y < 0.f || E.Pos.X > static_cast<float>(Map.GetWidth()) || E.Pos.Y > static_cast<float>(Map.GetHeight()))
		{
			std::snprintf(Buf, sizeof(Buf), "t=%.0f %s #%u is off the map at %.1f,%.1f", Now, Name, E.Id, E.Pos.X, E.Pos.Y);
			Violation(Buf);
		}
		if (W.Find(E.Id) != &E)
		{
			std::snprintf(Buf, sizeof(Buf), "t=%.0f %s #%u is missing from the id index", Now, Name, E.Id);
			Violation(Buf);
		}
		if (!E.IsResourceNode() && (E.Hp <= 0.f || E.Hp > E.MaxHp + 0.01f))
		{
			std::snprintf(Buf, sizeof(Buf), "t=%.0f %s #%u alive with hp %.1f of %.1f", Now, Name, E.Id, E.Hp, E.MaxHp);
			Violation(Buf);
		}
		if (E.IsUnit())
		{
			if (!Map.IsWalkablePos(E.Pos))
			{
				std::snprintf(Buf, sizeof(Buf), "t=%.0f %s #%u stands in a blocked tile at %.1f,%.1f", Now, Name, E.Id, E.Pos.X, E.Pos.Y);
				Violation(Buf);
			}
			if (E.CarryAmount < 0 || E.CarryAmount > GatherTuning::TimberPerTrip)
			{
				std::snprintf(Buf, sizeof(Buf), "t=%.0f %s #%u carries %d", Now, Name, E.Id, E.CarryAmount);
				Violation(Buf);
			}
		}
		if (static_cast<int>(E.Queue.size()) > MaxQueueLength)
		{
			std::snprintf(Buf, sizeof(Buf), "t=%.0f %s #%u queue of %zu", Now, Name, E.Id, E.Queue.size());
			Violation(Buf);
		}
		if (E.IsResourceNode() && E.Amount < 0)
		{
			std::snprintf(Buf, sizeof(Buf), "t=%.0f outcrop #%u amount %d", Now, E.Id, E.Amount);
			Violation(Buf);
		}
	}
	for (int T = 0; T < NumTeams; ++T)
	{
		const TeamState& State = W.GetTeam(static_cast<Team>(T));
		if (State.Res[0] < 0 || State.Res[1] < 0 || State.SupplyUsed < 0)
		{
			std::snprintf(Buf, sizeof(Buf), "t=%.0f team %d resources %d/%d supply %d", Now, T, State.Res[0], State.Res[1], State.SupplyUsed);
			Violation(Buf);
		}
	}
	for (const Projectile& Pr : W.GetProjectiles())
	{
		if (!Finite(Pr.Pos.X) || !Finite(Pr.Pos.Y))
		{
			Violation("non-finite projectile");
		}
	}
	if (static_cast<int>(W.GetEntities().size()) > World::MaxEntities)
	{
		Violation("entity limit exceeded");
	}
}

void Telemetry::End(const Session& S)
{
	const World& W = S.GetWorld();
	Duration = W.GetTime();
	Outcome = S.GetMission().Outcome;
	if (LastCombat >= 0.f && Duration - LastCombat > LullThreshold && Outcome == MissionOutcome::InProgress)
	{
		Lulls.push_back({LastCombat, Duration});
		LongestLull = MaxF(LongestLull, Duration - LastCombat);
	}
}

float Telemetry::IncomePerWorkerMinute(Resource R) const
{
	const int I = static_cast<int>(R);
	return WorkerSeconds[I] > 1.f ? static_cast<float>(WorkerIncome[I]) / (WorkerSeconds[I] / 60.f) : 0.f;
}

float Telemetry::AverageFloat(Resource R) const
{
	return Duration > 0.f ? FloatIntegral[static_cast<int>(R)] / Duration : 0.f;
}

std::string Telemetry::Summary() const
{
	std::string Out;
	char Buf[400];
	const char* Result = Outcome == MissionOutcome::Won ? "WON" : (Outcome == MissionOutcome::Lost ? "LOST" : "UNFINISHED");
	std::snprintf(Buf, sizeof(Buf), "    %s at %s | first soldier %s, first combat %s, first loss %s, first building lost %s\n", Result,
		Clock(Duration).c_str(), Clock(FirstSoldier).c_str(), Clock(FirstCombat).c_str(), Clock(FirstLoss).c_str(), Clock(FirstBuildingLost).c_str());
	Out += Buf;
	std::snprintf(Buf, sizeof(Buf),
		"    economy: sunstone %d (+%d passive) timber %d | per worker-minute: sunstone %.1f timber %.1f | spent %d/%d\n"
		"             avg banked %.0f/%.0f | idle worker-s %.0f, waiting at outcrop %.0f | supply-blocked %.0fs\n",
		WorkerIncome[0], PassiveIncome[0], WorkerIncome[1], IncomePerWorkerMinute(Resource::Sunstone), IncomePerWorkerMinute(Resource::Timber), Spent[0],
		Spent[1], AverageFloat(Resource::Sunstone), AverageFloat(Resource::Timber), IdleWorkerSeconds, WaitingWorkerSeconds, SupplyBlockedSeconds);
	Out += Buf;
	std::snprintf(Buf, sizeof(Buf), "    pacing: combat %.0fs (%.0f%% of the mission), lulls over %.0fs: %zu, longest %s", CombatSeconds,
		Duration > 0.f ? 100.f * CombatSeconds / Duration : 0.f, LullThreshold, Lulls.size(), Clock(LongestLull).c_str());
	Out += Buf;
	for (const Lull& L : Lulls)
	{
		std::snprintf(Buf, sizeof(Buf), " [%s-%s]", Clock(L.Start).c_str(), Clock(L.End).c_str());
		Out += Buf;
	}
	Out += "\n    waves:";
	for (float T : WaveTimes)
	{
		Out += " " + Clock(T);
	}
	std::snprintf(Buf, sizeof(Buf), " | peak gloom %d, avg %.0f\n", PeakGloom, Duration > 0.f ? GloomFloatIntegral / Duration : 0.f);
	Out += Buf;
	Out += "    units (trained/lost/kills, damage dealt/taken):\n";
	for (int A = 0; A < NumArchetypes; ++A)
	{
		const TypeStats& T = Types[A];
		if (!IsUnit(static_cast<Archetype>(A)) && T.Kills == 0 && T.DamageDealt <= 0.f)
		{
			continue;
		}
		if (T.Trained == 0 && T.Lost == 0 && T.Kills == 0 && T.DamageDealt <= 0.f && T.DamageTaken <= 0.f)
		{
			continue;
		}
		std::snprintf(Buf, sizeof(Buf), "      %-13s %3d/%3d/%3d  %7.0f/%7.0f\n", GetDef(static_cast<Archetype>(A)).Name, T.Trained, T.Lost, T.Kills,
			T.DamageDealt, T.DamageTaken);
		Out += Buf;
	}
	std::snprintf(Buf, sizeof(Buf),
		"    health: stuck %zu, invariant violations %zu, save round trips %d | peaks: units %d vs %d, entities %d, projectiles %d, events/tick %d\n", Stuck.size(),
		Violations.size(), SaveChecks, PeakPlayerUnits, PeakEnemyUnits, PeakEntities, PeakProjectiles, PeakEventsPerTick);
	Out += Buf;
	for (size_t I = 0; I < Stuck.size() && I < 6; ++I)
	{
		const StuckIncident& K = Stuck[I];
		std::snprintf(Buf, sizeof(Buf), "      stuck: %s %s #%u at %.1f,%.1f (t=%s, order %d) -> %s at %.1f,%.1f path=%d/%zu engage=%.1f\n",
			K.Owner == Team::Player ? "Warden" : "Gloam", GetDef(K.Type).Name, K.Id, K.Pos.X, K.Pos.Y, Clock(K.Time).c_str(), static_cast<int>(K.Order),
			K.TargetType != Archetype::None ? GetDef(K.TargetType).Name : "point", K.TargetPos.X, K.TargetPos.Y, K.bHasPath ? 1 : 0, K.PathLeft,
			K.EngageTimer);
		Out += Buf;
	}
	for (size_t I = 0; I < Violations.size() && I < 6; ++I)
	{
		Out += "      violation: " + Violations[I] + "\n";
	}
	return Out;
}

} // namespace bht
