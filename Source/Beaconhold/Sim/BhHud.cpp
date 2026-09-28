// Beaconhold simulation core - HUD view-model.
#include "BhHud.h"

#include <cstdio>

namespace bh
{
std::string FormatTime(float Seconds)
{
	const int Total = MaxI(0, static_cast<int>(Seconds));
	char Buf[16];
	std::snprintf(Buf, sizeof(Buf), "%d:%02d", Total / 60, Total % 60);
	return Buf;
}

namespace
{
void HudFillSelection(const Session& S, SelectionPanel& P)
{
	const World& W = S.GetWorld();
	const PlayerControl& C = S.GetControl();
	P = SelectionPanel();
	P.Kind = C.GetSelectionKind(W);
	if (P.Kind == SelectionKind::None)
	{
		return;
	}
	std::vector<const Entity*> Sel;
	for (EntityId Id : C.Selection)
	{
		const Entity* E = W.Find(Id);
		if (E != nullptr && E->bAlive)
		{
			Sel.push_back(E);
		}
	}
	P.Count = static_cast<int>(Sel.size());
	if (Sel.empty())
	{
		P.Kind = SelectionKind::None;
		return;
	}
	if (Sel.size() > 1)
	{
		for (const Entity* E : Sel)
		{
			bool bFound = false;
			for (SelectionGroup& G : P.Groups)
			{
				if (G.Type == E->Type)
				{
					G.HpRatio = (G.HpRatio * static_cast<float>(G.Count) + E->HpRatio()) / static_cast<float>(G.Count + 1);
					++G.Count;
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				SelectionGroup G;
				G.Type = E->Type;
				G.IconId = GetDef(E->Type).IconId;
				G.Count = 1;
				G.HpRatio = E->HpRatio();
				P.Groups.push_back(G);
			}
		}
		P.Name = std::to_string(Sel.size()) + " selected";
		return;
	}

	const Entity& E = *Sel.front();
	const ArchetypeDef& D = GetDef(E.Type);
	P.Id = E.Id;
	P.Type = E.Type;
	P.IconId = D.IconId;
	P.Owner = E.Owner;
	P.Name = D.Name;
	P.Description = D.Description;
	P.Status = OrderText(E);
	P.Hp = E.Hp;
	P.MaxHp = E.MaxHp;
	if (E.IsResourceNode())
	{
		P.Extra = std::to_string(E.Amount) + " Sunstone left";
		return;
	}
	P.bShowCombat = D.Damage > 0.f;
	P.Damage = static_cast<int>(W.GetDamage(E) + 0.5f);
	P.Armor = static_cast<int>(W.GetArmor(E) + 0.5f);
	P.Range = W.GetRange(E);
	if (E.IsUnit() && E.CarryAmount > 0)
	{
		P.Extra = std::string("Carrying ") + std::to_string(E.CarryAmount) + " " + GetResourceName(E.CarryType);
	}
	if (E.IsBuilding())
	{
		P.bConstructing = !E.bConstructed;
		P.BuildProgress = E.BuildProgress;
		std::string Extra;
		auto Append = [&Extra](const std::string& Part)
		{
			Extra += Extra.empty() ? Part : ", " + Part;
		};
		if (D.SupplyProvided > 0 && E.Owner == Team::Player)
		{
			Append("+" + std::to_string(D.SupplyProvided) + " supply");
		}
		if (D.HealAmount > 0.f && D.HealRange > 0.f && !IsUnit(E.Type))
		{
			Append("heals allies nearby");
		}
		if (D.IncomePerSecond > 0.f)
		{
			Append("yields " + std::to_string(static_cast<int>(D.IncomePerSecond * 60.f + 0.5f)) + " Sunstone a minute");
		}
		if (!Extra.empty() && Extra[0] >= 'a' && Extra[0] <= 'z')
		{
			Extra[0] = static_cast<char>(Extra[0] - 'a' + 'A');
		}
		P.Extra = Extra;
		for (size_t I = 0; I < E.Queue.size(); ++I)
		{
			const ProductionItem& Item = E.Queue[I];
			QueueEntry Q;
			Q.IconId = Item.bResearch ? GetResearchDef(Item.Tech).IconId : GetDef(Item.Unit).IconId;
			Q.Progress = I == 0 ? Saturate(Item.Elapsed / MaxF(0.01f, Item.Total)) : 0.f;
			Q.Index = static_cast<int>(I);
			P.Queue.push_back(Q);
		}
	}
}
} // namespace

void BuildHudModel(const Session& S, HudModel& Out)
{
	const World& W = S.GetWorld();
	const MissionRuntime& M = S.GetMission();
	const MissionDef& Def = M.Def();
	const TeamState& T = W.GetTeam(Team::Player);

	Out.Sunstone = T.Res[0];
	Out.Timber = T.Res[1];
	Out.SupplyUsed = T.SupplyUsed;
	Out.SupplyCap = T.SupplyCap;
	Out.bSupplyFull = T.SupplyUsed >= T.SupplyCap && T.SupplyCap < SupplyHardCap;
	Out.MissionTime = M.Elapsed;
	Out.WaveCountdown = M.NextWaveIn();
	Out.EnemyAttackIn = S.GetAI().IsEnabled() ? S.GetAI().SecondsToNextAttack(W) : -1.f;
	Out.MissionTitle = Def.Title;
	Out.Outcome = M.Outcome;

	Out.Objectives.clear();
	for (size_t I = 0; I < Def.Objectives.size() && I < M.Objectives.size(); ++I)
	{
		const ObjectiveDef& O = Def.Objectives[I];
		const ObjectiveState& St = M.Objectives[I];
		ObjectiveLine L;
		L.Text = O.Text;
		L.Progress = St.Progress;
		L.Target = St.Target;
		L.bDone = St.bDone;
		L.bOptional = O.bOptional;
		L.bTimer = O.Kind == ObjectiveKind::Survive;
		if (L.bTimer && !L.bDone)
		{
			L.Text = std::string(O.Text) + " (" + FormatTime(O.Seconds - M.Elapsed) + ")";
		}
		else if (L.Target > 1 && !L.bTimer)
		{
			L.Text = std::string(O.Text) + " (" + std::to_string(L.Progress) + "/" + std::to_string(L.Target) + ")";
		}
		Out.Objectives.push_back(L);
	}

	HudFillSelection(S, Out.Selection);
	S.GetControl().BuildActions(W, Out.Actions);
	S.GetControl().BuildQuickBar(W, Out.QuickBar);

	const TutorialStep* Step = M.CurrentTutorialStep();
	Out.bTutorial = Step != nullptr;
	Out.TutorialText = Step != nullptr ? Step->Text : "";
	Out.TutorialStep = M.TutorialIndex;
	Out.TutorialSteps = static_cast<int>(Def.Tutorial.size());
	Out.bTutorialNeedsContinue = Step != nullptr && (Step->Cond == TutorialCond::Continue || Step->Cond == TutorialCond::CameraMoved);

	Out.bPlacing = S.GetControl().bPlacing;
	Out.bPlaceValid = S.GetControl().PlaceState == PlaceResult::Ok;
}

bool MakeNotice(const GameEvent& E, const Session& S, Notice& Out)
{
	Out = Notice();
	Out.Pos = E.Pos;
	const bool bMine = E.Owner == Team::Player;
	const char* ArchName = (E.Arch != Archetype::None && E.Arch < Archetype::Count) ? GetDef(E.Arch).Name : "";
	switch (E.Type)
	{
	case EventType::NotEnoughSunstone:
		Out.Text = "Not enough Sunstone";
		Out.Severity = NoticeSeverity::Warning;
		Out.IconId = Icon::Sunstone;
		return bMine;
	case EventType::NotEnoughTimber:
		Out.Text = "Not enough Timber";
		Out.Severity = NoticeSeverity::Warning;
		Out.IconId = Icon::Timber;
		return bMine;
	case EventType::SupplyBlocked:
		Out.Text = "Not enough supply - build a Cottage";
		Out.Severity = NoticeSeverity::Warning;
		Out.IconId = Icon::Supply;
		return bMine;
	case EventType::InvalidPlacement:
		Out.Text = "Can't build there";
		Out.Severity = NoticeSeverity::Warning;
		return bMine;
	case EventType::RequirementMissing:
		if (E.Arch != Archetype::None && E.Arch < Archetype::Count && GetDef(E.Arch).Requires != Archetype::None)
		{
			Out.Text = std::string("Requires ") + GetDef(GetDef(E.Arch).Requires).Name;
		}
		else
		{
			Out.Text = "Not available yet";
		}
		Out.Severity = NoticeSeverity::Warning;
		Out.IconId = Icon::Lock;
		return bMine;
	case EventType::QueueFull:
		Out.Text = "Queue is full";
		Out.Severity = NoticeSeverity::Warning;
		return bMine;
	case EventType::UnderAttack:
		Out.Text = std::string(ArchName) + " is under attack!";
		Out.Severity = NoticeSeverity::Danger;
		Out.bHasPos = true;
		Out.IconId = Icon::Sword;
		return bMine;
	case EventType::BuildingCompleted:
		if (!bMine)
		{
			return false;
		}
		Out.Text = E.Arch == Archetype::Beacon ? "A Beacon burns again!" : std::string(ArchName) + " complete";
		Out.Severity = NoticeSeverity::Good;
		Out.bHasPos = true;
		Out.IconId = E.Arch != Archetype::None ? GetDef(E.Arch).IconId : Icon::Build;
		return true;
	case EventType::UnitTrained:
	{
		// New units are seen and heard as they appear; what the player needs to know is that a
		// building has run out of orders (unless more were queued since).
		const Entity* Building = S.GetWorld().Find(E.B);
		if (!bMine || E.Sub != 1 || Building == nullptr || !Building->bAlive || !Building->Queue.empty())
		{
			return false;
		}
		Out.Text = std::string(GetDef(Building->Type).Name) + " finished training";
		Out.Severity = NoticeSeverity::Info;
		Out.bHasPos = true;
		Out.Pos = Building->Pos;
		Out.IconId = GetDef(E.Arch).IconId;
		return true;
	}
	case EventType::ResearchCompleted:
		if (!bMine)
		{
			return false;
		}
		Out.Text = std::string(GetResearchDef(static_cast<Research>(E.Sub)).Name) + " researched";
		Out.Severity = NoticeSeverity::Good;
		Out.IconId = GetResearchDef(static_cast<Research>(E.Sub)).IconId;
		return true;
	case EventType::NodeDepleted:
		Out.Text = "A Sunstone outcrop has run dry";
		Out.Severity = NoticeSeverity::Warning;
		Out.bHasPos = true;
		Out.IconId = Icon::SunstoneNode;
		return true;
	case EventType::WaveIncoming:
	{
		const MissionDef& Def = S.GetMission().Def();
		if (E.Sub >= 0 && E.Sub < static_cast<int>(Def.Waves.size()))
		{
			Out.Text = Def.Waves[static_cast<size_t>(E.Sub)].Announce;
		}
		else
		{
			Out.Text = "A Gloam war party is marching on you!";
		}
		Out.Severity = NoticeSeverity::Danger;
		Out.bHasPos = true;
		Out.IconId = Icon::Skull;
		return !Out.Text.empty();
	}
	case EventType::ObjectiveCompleted:
	{
		const MissionDef& Def = S.GetMission().Def();
		if (E.Sub < 0 || E.Sub >= static_cast<int>(Def.Objectives.size()))
		{
			return false;
		}
		Out.Text = std::string("Objective complete: ") + Def.Objectives[static_cast<size_t>(E.Sub)].Text;
		Out.Severity = NoticeSeverity::Good;
		Out.IconId = Icon::Trophy;
		return true;
	}
	case EventType::EntityDied:
		if (bMine && E.Arch != Archetype::None && IsBuilding(E.Arch))
		{
			Out.Text = std::string(ArchName) + " destroyed";
			Out.Severity = NoticeSeverity::Danger;
			Out.bHasPos = true;
			Out.IconId = Icon::Skull;
			return true;
		}
		return false;
	case EventType::Notice:
		switch (static_cast<NoticeCode>(E.Sub))
		{
		case NoticeCode::NoSoldiers:
			Out.Text = "No soldiers yet - train them at a Muster Hall";
			break;
		case NoticeCode::NoIdleWorkers:
			Out.Text = "All Lamplighters are busy";
			break;
		case NoticeCode::NeedWorker:
			Out.Text = "Select a Lamplighter to build";
			break;
		case NoticeCode::NoFreeBeaconSite:
			Out.Text = "No free Beacon site";
			break;
		}
		Out.Severity = NoticeSeverity::Info;
		return !Out.Text.empty();
	case EventType::EntitySpawned:
	case EventType::BuildingPlaced:
	case EventType::ConstructionCancelled:
	case EventType::AttackStarted:
	case EventType::ProjectileFired:
	case EventType::Hit:
	case EventType::Healed:
	case EventType::GatherStrike:
	case EventType::ResourceDelivered:
	case EventType::TreeFelled:
	case EventType::AbilityCast:
	case EventType::CommandMove:
	case EventType::CommandAttack:
	case EventType::CommandGather:
	case EventType::CommandBuild:
	case EventType::WaveSpawned:
	case EventType::ObjectiveFailed:
	case EventType::MissionWon:
	case EventType::MissionLost:
	case EventType::TutorialStep:
		break;
	}
	return false;
}

MissionSummary BuildSummary(const Session& S)
{
	MissionSummary Sum;
	const MissionRuntime& M = S.GetMission();
	const MissionDef& Def = M.Def();
	const TeamState& T = S.GetWorld().GetTeam(Team::Player);
	Sum.bWon = M.Outcome == MissionOutcome::Won;
	Sum.Stars = M.GetStarCount();
	for (int I = 0; I < 3; ++I)
	{
		Sum.StarEarned[I] = M.Stars[I];
	}
	for (int I = 0; I < 3; ++I)
	{
		Sum.StarText[I] = StarGoalText(Def, I);
	}
	Sum.Title = Sum.bWon ? "Victory" : "Defeat";
	Sum.Text = M.EndReason;
	Sum.Time = M.Elapsed;
	Sum.UnitsTrained = T.Stats.UnitsTrained;
	Sum.UnitsLost = T.Stats.UnitsLost;
	Sum.Kills = T.Stats.Kills;
	Sum.Gathered = T.Stats.Gathered[0] + T.Stats.Gathered[1];
	return Sum;
}

} // namespace bh
