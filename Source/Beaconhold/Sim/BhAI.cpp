// Beaconhold simulation core - Gloam commander AI.
#include "BhAI.h"

#include <algorithm>
#include <cstddef>

namespace bh
{
namespace
{
bool AIContains(const std::vector<EntityId>& List, EntityId Id)
{
	return std::find(List.begin(), List.end(), Id) != List.end();
}
} // namespace

void EnemyAI::Start(const AIConfig& InConfig, Difficulty InDiff, World& W)
{
	Config = InConfig;
	Diff = InDiff;
	Attackers.clear();
	GloomAccum = 0.f;
	ThinkTimer = 0.f;
	WavesLaunched = 0;
	LastDefenseTime = -100.f;
	WaveSize = Config.FirstWave;
	const DifficultyTuning Tuning = GetDifficultyTuning(Diff);
	NextAttack = Config.FirstAttack * Tuning.EnemyTiming;

	TeamState& Gloam = W.GetTeam(Team::Enemy);
	Gloam.bIgnoreSupply = true;
	if (Config.bEnabled)
	{
		Gloam.Res[0] += Config.StartGloom;
	}

	// Home is the Gloam Heart (or the centre of the Gloam buildings).
	Vec2 Sum;
	int Count = 0;
	bool bHeart = false;
	Vec2 PlayerBase(static_cast<float>(W.GetMap().GetWidth()) * 0.5f, static_cast<float>(W.GetMap().GetHeight()) * 0.5f);
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive)
		{
			continue;
		}
		if (E.Owner == Team::Enemy && E.IsBuilding())
		{
			if (E.Type == Archetype::GloamHeart)
			{
				Home = E.Pos;
				bHeart = true;
			}
			Sum += E.Pos;
			++Count;
		}
		if (E.Owner == Team::Player && E.Type == Archetype::Keep)
		{
			PlayerBase = E.Pos;
		}
	}
	if (!bHeart && Count > 0)
	{
		Home = Sum / static_cast<float>(Count);
	}
	Rally = Home + (PlayerBase - Home).Normalized() * 7.f;
	Tile Free;
	if (W.GetMap().FindNearestWalkable(Tile::FromPos(Rally), 6, Free))
	{
		Rally = Free.Center();
	}
	if (Config.bEnabled)
	{
		for (const Entity& E : W.GetEntities())
		{
			if (E.bAlive && E.Owner == Team::Enemy && E.IsBuilding() && GetDef(E.Type).Trains[0] != Archetype::None)
			{
				W.CmdSetRally(E.Id, Rally, NoEntity, nullptr);
			}
		}
	}
}

float EnemyAI::CurrentIncome(const World& W) const
{
	const float Minutes = W.GetTime() / 60.f;
	const float Base = MinF(Config.MaxIncome, Config.Income + Config.IncomeGrowth * Minutes);
	return Base * GetDifficultyTuning(Diff).EnemyIncome;
}

void EnemyAI::Tick(World& W, float Dt)
{
	if (!Config.bEnabled || W.bFrozen)
	{
		return;
	}
	GloomAccum += CurrentIncome(W) * Dt;
	if (GloomAccum >= 1.f)
	{
		const int Whole = static_cast<int>(GloomAccum);
		W.GetTeam(Team::Enemy).Res[0] += Whole;
		GloomAccum -= static_cast<float>(Whole);
	}
	ThinkTimer -= Dt;
	if (ThinkTimer > 0.f)
	{
		return;
	}
	ThinkTimer = 0.5f;
	MeasurePlayerArmy(W);
	Produce(W);
	Defend(W);
	ManageAttackers(W);
	if (W.GetTime() >= NextAttack)
	{
		LaunchWave(W);
	}
}

void EnemyAI::Produce(World& W)
{
	int Army = W.CountUnits(Team::Enemy, false);
	for (const Entity& E : W.GetEntities())
	{
		if (E.bAlive && E.Owner == Team::Enemy && E.IsBuilding())
		{
			for (const ProductionItem& Item : E.Queue)
			{
				if (!Item.bResearch)
				{
					++Army;
				}
			}
		}
	}
	const DifficultyTuning Tuning = GetDifficultyTuning(Diff);
	const bool bTitanTime = Config.TitanAfter >= 0.f && W.GetTime() >= Config.TitanAfter * Tuning.EnemyTiming;
	const bool bTitanExists = W.CountOwned(Team::Enemy, Archetype::BogTitan, true) > 0;
	bool bTitanQueued = false;
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive || E.Owner != Team::Enemy)
		{
			continue;
		}
		for (const ProductionItem& Item : E.Queue)
		{
			if (Item.Unit == Archetype::BogTitan)
			{
				bTitanQueued = true;
			}
		}
	}
	const bool bSaveForTitan = bTitanTime && !bTitanExists && !bTitanQueued;

	// Forget picks of buildings that are gone or busy; idle producers pick their next unit.
	Pending.erase(std::remove_if(Pending.begin(), Pending.end(), [&W](const AIPendingPick& P)
	{
		const Entity* B = W.Find(P.Building);
		return B == nullptr || !B->bAlive || !B->Queue.empty();
	}), Pending.end());
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive || E.Owner != Team::Enemy || !E.IsBuilding() || !E.bConstructed || !E.Queue.empty() || GetDef(E.Type).Trains[0] == Archetype::None)
		{
			continue;
		}
		const EntityId Id = E.Id;
		const bool bHasPick = std::any_of(Pending.begin(), Pending.end(), [Id](const AIPendingPick& P) { return P.Building == Id; });
		if (!bHasPick)
		{
			AIPendingPick P;
			P.Building = Id;
			P.Unit = PickCounter(W, E);
			P.Since = W.GetTime();
			Pending.push_back(P);
		}
	}
	// A Titan, when due, is the Heart's next unit whatever it picked before.
	if (bSaveForTitan)
	{
		for (AIPendingPick& P : Pending)
		{
			const Entity* B = W.Find(P.Building);
			if (B != nullptr && B->Type == Archetype::GloamHeart)
			{
				P.Unit = Archetype::BogTitan;
			}
		}
	}
	std::stable_sort(Pending.begin(), Pending.end(), [](const AIPendingPick& A, const AIPendingPick& B) { return A.Since < B.Since; });

	// Oldest first; an unaffordable pick keeps its gloom, so younger picks wait their turn.
	int Budget = W.GetTeam(Team::Enemy).Res[0];
	for (size_t I = 0; I < Pending.size() && Army < Config.ArmyCap;)
	{
		const AIPendingPick P = Pending[I];
		const int Cost = GetDef(P.Unit).CostSunstone;
		if (Budget < Cost)
		{
			break;
		}
		if (W.CmdTrain(P.Building, P.Unit))
		{
			Budget -= Cost;
			++Army;
			Pending.erase(Pending.begin() + static_cast<std::ptrdiff_t>(I));
			continue;
		}
		++I;
	}
}

void EnemyAI::MeasurePlayerArmy(const World& W)
{
	for (float& S : ArmyShare)
	{
		S = 0.f;
	}
	float Total = 0.f;
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive || E.Owner != Team::Player || !W.IsCombatUnit(E))
		{
			continue;
		}
		const float Supply = static_cast<float>(MaxI(1, GetDef(E.Type).SupplyCost));
		Total += Supply;
		switch (E.Type)
		{
		case Archetype::Shieldbearer:
			ArmyShare[0] += Supply;
			break;
		case Archetype::Ranger:
			ArmyShare[1] += Supply;
			break;
		case Archetype::StagRider:
			ArmyShare[2] += Supply;
			break;
		case Archetype::Sage:
			ArmyShare[3] += Supply;
			break;
		default:
			break;
		}
	}
	if (Total > 0.f)
	{
		for (float& S : ArmyShare)
		{
			S /= Total;
		}
	}
}

Archetype EnemyAI::PickCounter(World& W, const Entity& Building) const
{
	const ArchetypeDef& D = GetDef(Building.Type);
	if (W.GetTime() < Config.EliteAfter * GetDifficultyTuning(Diff).EnemyTiming)
	{
		return D.Trains[0];
	}
	Archetype Options[MaxTrainOptions];
	float Weights[MaxTrainOptions];
	int Count = 0;
	float Sum = 0.f;
	for (Archetype A : D.Trains)
	{
		if (A == Archetype::None || A == Archetype::BogTitan)
		{
			continue;
		}
		const GloamPickWeights& P = GetGloamPickWeights(A);
		const float Weight = MaxF(0.1f, P.Base + ArmyShare[0] * P.VsShieldbearer + ArmyShare[1] * P.VsRanger + ArmyShare[2] * P.VsStagRider +
			ArmyShare[3] * P.VsSage);
		Options[Count] = A;
		Weights[Count] = Weight;
		Sum += Weight;
		++Count;
	}
	if (Count == 0)
	{
		return D.Trains[0];
	}
	float Roll = W.GetRng().Range(0.f, Sum);
	for (int I = 0; I < Count; ++I)
	{
		Roll -= Weights[I];
		if (Roll <= 0.f)
		{
			return Options[I];
		}
	}
	return Options[Count - 1];
}

void EnemyAI::Defend(World& W)
{
	const float Now = W.GetTime();
	if (Now - LastDefenseTime < 3.f)
	{
		return;
	}
	Vec2 ThreatPos;
	bool bThreat = false;
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive || E.Owner != Team::Enemy || !E.IsBuilding() || Now - E.LastDamagedTime > 2.5f)
		{
			continue;
		}
		const Entity* Attacker = W.Find(E.LastAttacker);
		if (Attacker != nullptr && Attacker->bAlive)
		{
			ThreatPos = Attacker->Pos;
			bThreat = true;
			break;
		}
	}
	if (!bThreat)
	{
		return;
	}
	LastDefenseTime = Now;
	std::vector<EntityId> Defenders;
	for (const Entity& E : W.GetEntities())
	{
		if (E.bAlive && E.Owner == Team::Enemy && E.IsUnit() && !AIContains(Attackers, E.Id) &&
			Vec2::Dist(E.Pos, Home) < 20.f && E.EngageTarget == NoEntity)
		{
			Defenders.push_back(E.Id);
		}
	}
	if (!Defenders.empty())
	{
		W.CmdMove(Defenders, ThreatPos, true);
	}
}

void EnemyAI::ManageAttackers(World& W)
{
	std::vector<EntityId> Alive;
	for (EntityId Id : Attackers)
	{
		const Entity* E = W.Find(Id);
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
	Attackers.swap(Alive);

	// Home units that wandered off (after defending) drift back to the rally point.
	std::vector<EntityId> Stragglers;
	for (const Entity& E : W.GetEntities())
	{
		if (E.bAlive && E.Owner == Team::Enemy && E.IsUnit() && E.Order == OrderType::Idle && E.EngageTarget == NoEntity &&
			!AIContains(Attackers, E.Id) && Vec2::Dist(E.Pos, Rally) > 9.f)
		{
			Stragglers.push_back(E.Id);
		}
	}
	if (!Stragglers.empty())
	{
		W.CmdMove(Stragglers, Rally, true);
	}
}

void EnemyAI::LaunchWave(World& W)
{
	std::vector<const Entity*> Home_;
	for (const Entity& E : W.GetEntities())
	{
		if (E.bAlive && E.Owner == Team::Enemy && E.IsUnit() && !AIContains(Attackers, E.Id) && Vec2::Dist(E.Pos, Rally) < 22.f)
		{
			Home_.push_back(&E);
		}
	}
	// The oldest units stay home as guards.
	std::sort(Home_.begin(), Home_.end(), [](const Entity* A, const Entity* B) { return A->Id < B->Id; });
	const int Guards = MinI(Config.HomeGuard, static_cast<int>(Home_.size()));
	const int Available = static_cast<int>(Home_.size()) - Guards;
	const bool bOverdue = W.GetTime() > NextAttack + 60.f;
	const int Needed = MaxI(2, (WaveSize * 3) / 4);
	if (Available < Needed && !(bOverdue && Available >= 2))
	{
		return;
	}
	std::vector<EntityId> Wave;
	for (int I = Guards; I < static_cast<int>(Home_.size()) && static_cast<int>(Wave.size()) < WaveSize; ++I)
	{
		Wave.push_back(Home_[static_cast<size_t>(I)]->Id);
	}
	Vec2 Target;
	if (Wave.empty() || !FindAttackTarget(W, Rally, Team::Enemy, Target, (WavesLaunched % 2) == 1))
	{
		return;
	}
	Vec2 Center;
	for (EntityId Id : Wave)
	{
		if (const Entity* E = W.Find(Id))
		{
			Center += E->Pos;
		}
	}
	Center = Center / static_cast<float>(Wave.size());
	W.CmdMove(Wave, Target, true);
	Attackers.insert(Attackers.end(), Wave.begin(), Wave.end());
	++WavesLaunched;
	WaveSize = MinI(Config.MaxWave, WaveSize + Config.WaveGrowth);
	NextAttack = W.GetTime() + Config.AttackInterval * GetDifficultyTuning(Diff).EnemyTiming;
	W.EmitSimple(EventType::WaveIncoming, Team::Enemy, Center, NoEntity, Archetype::None, 0.f, 1000 + WavesLaunched);
}

} // namespace bh
