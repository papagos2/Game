// Beaconhold simulation core — player control.
#include "BhControl.h"

#include <algorithm>
#include <cstring>

namespace bh
{
namespace
{
std::string ControlRequiresText(Archetype Requires)
{
	return std::string("Requires ") + GetDef(Requires).Name;
}
} // namespace

bool ActionMatchesHighlight(const ActionId& A, const char* Highlight)
{
	if (Highlight == nullptr || Highlight[0] == '\0')
	{
		return false;
	}
	std::string Key;
	switch (A.Kind)
	{
	case ActionKind::Train:
		Key = std::string("train:") + ArchetypeKey(static_cast<Archetype>(A.Param));
		break;
	case ActionKind::Research:
		Key = std::string("research:") + ResearchKey(static_cast<Research>(A.Param));
		break;
	case ActionKind::BuildMenu:
		Key = "build";
		break;
	case ActionKind::PlaceBuilding:
		Key = std::string("place:") + ArchetypeKey(static_cast<Archetype>(A.Param));
		break;
	case ActionKind::Ability:
		Key = std::string("ability:") + AbilityKey(static_cast<Ability>(A.Param));
		break;
	case ActionKind::SelectArmy:
		Key = "army";
		break;
	case ActionKind::SelectIdleWorker:
		Key = "idle";
		break;
	case ActionKind::ConfirmPlacement:
		Key = "confirm";
		break;
	case ActionKind::Home:
		Key = "home";
		break;
	case ActionKind::None:
	case ActionKind::Stop:
	case ActionKind::ReturnCargo:
	case ActionKind::CancelConstruction:
	case ActionKind::CancelQueue:
	case ActionKind::CancelPlacement:
	case ActionKind::Back:
	case ActionKind::Deselect:
		return false;
	}
	if (Key == Highlight)
	{
		return true;
	}
	// "place:X" also lights up the Build button that leads to it.
	return A.Kind == ActionKind::BuildMenu && std::strncmp(Highlight, "place:", 6) == 0;
}

void PlayerControl::Reset()
{
	Selection.clear();
	bBuildMenu = false;
	bPlacing = false;
	PlaceType = Archetype::None;
	PlaceState = PlaceResult::Ok;
	bCameraMoved = false;
	TutorialHighlight.clear();
	IdleCycle = 0;
	ResetLatches();
}

// ---------------------------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------------------------

SelectionKind PlayerControl::GetSelectionKind(const World& W) const
{
	if (Selection.empty())
	{
		return SelectionKind::None;
	}
	const Entity* E = W.Find(Selection.front());
	if (E == nullptr || !E->bAlive)
	{
		return SelectionKind::None;
	}
	if (E->IsResourceNode())
	{
		return SelectionKind::Resource;
	}
	if (E->Owner != Team::Player)
	{
		return SelectionKind::Enemy;
	}
	return E->IsBuilding() ? SelectionKind::OwnBuilding : SelectionKind::OwnUnits;
}

void PlayerControl::SelectOne(const World& W, EntityId Id)
{
	Selection.assign(1, Id);
	bBuildMenu = false;
	Latch(W);
}

void PlayerControl::SelectMany(const World& W, const std::vector<EntityId>& Ids)
{
	Selection = Ids;
	bBuildMenu = false;
	Latch(W);
}

void PlayerControl::ResetLatches()
{
	SelectionLatch = 0;
	ArmyLatch = 0;
}

void PlayerControl::Latch(const World& W)
{
	int Army = 0;
	for (EntityId Id : Selection)
	{
		const Entity* E = W.Find(Id);
		if (E == nullptr || !E->bAlive || E->Owner != Team::Player)
		{
			continue;
		}
		SelectionLatch |= (uint64_t(1) << ArchIndex(E->Type));
		Army += W.IsCombatUnit(*E) ? 1 : 0;
	}
	ArmyLatch = MaxI(ArmyLatch, Army);
}

void PlayerControl::ClearSelection()
{
	Selection.clear();
	bBuildMenu = false;
}

void PlayerControl::Prune(const World& W)
{
	Selection.erase(std::remove_if(Selection.begin(), Selection.end(), [&W](EntityId Id)
	{
		const Entity* E = W.Find(Id);
		return E == nullptr || !E->bAlive;
	}), Selection.end());
	if (Selection.empty())
	{
		bBuildMenu = false;
	}
	if (bPlacing)
	{
		PlaceState = W.CanPlace(PlaceType, PlaceTile);
		if (!HasWorkers(W))
		{
			CancelPlacement();
		}
	}
}

bool PlayerControl::HasWorkers(const World& W) const
{
	for (EntityId Id : Selection)
	{
		const Entity* E = W.Find(Id);
		if (E != nullptr && E->bAlive && E->Owner == Team::Player && E->IsUnit() && GetDef(E->Type).IsWorker)
		{
			return true;
		}
	}
	return false;
}

bool PlayerControl::HasCombat(const World& W) const
{
	for (EntityId Id : Selection)
	{
		const Entity* E = W.Find(Id);
		if (E != nullptr && E->bAlive && E->Owner == Team::Player && W.IsCombatUnit(*E))
		{
			return true;
		}
	}
	return false;
}

bool PlayerControl::HasOwnUnits(const World& W) const
{
	for (EntityId Id : Selection)
	{
		const Entity* E = W.Find(Id);
		if (E != nullptr && E->bAlive && E->Owner == Team::Player && E->IsUnit())
		{
			return true;
		}
	}
	return false;
}

EntityId PlayerControl::GetSingleBuilding(const World& W) const
{
	if (Selection.size() != 1)
	{
		return NoEntity;
	}
	const Entity* E = W.Find(Selection.front());
	return (E != nullptr && E->bAlive && E->IsBuilding() && E->Owner == Team::Player) ? E->Id : NoEntity;
}

std::vector<EntityId> PlayerControl::SelectedWorkers(const World& W) const
{
	std::vector<EntityId> Out;
	for (EntityId Id : Selection)
	{
		const Entity* E = W.Find(Id);
		if (E != nullptr && E->bAlive && E->Owner == Team::Player && E->IsUnit() && GetDef(E->Type).IsWorker)
		{
			Out.push_back(Id);
		}
	}
	return Out;
}

std::vector<EntityId> PlayerControl::SelectedNonWorkers(const World& W) const
{
	std::vector<EntityId> Out;
	for (EntityId Id : Selection)
	{
		const Entity* E = W.Find(Id);
		if (E != nullptr && E->bAlive && E->Owner == Team::Player && E->IsUnit() && !GetDef(E->Type).IsWorker)
		{
			Out.push_back(Id);
		}
	}
	return Out;
}

int PlayerControl::CountIdleWorkers(const World& W) const
{
	int Count = 0;
	for (const Entity& E : W.GetEntities())
	{
		if (E.bAlive && E.Owner == Team::Player && E.IsUnit() && GetDef(E.Type).IsWorker && E.Order == OrderType::Idle)
		{
			++Count;
		}
	}
	return Count;
}

// ---------------------------------------------------------------------------------------------
// Picking & commands
// ---------------------------------------------------------------------------------------------

PickResult PlayerControl::PickAt(const World& W, const Vec2& P, float Tolerance) const
{
	PickResult R;
	float BestScore = 1e9f;
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive)
		{
			continue;
		}
		float Score = 1e9f;
		if (E.IsUnit())
		{
			const float D = Vec2::Dist(P, E.Pos) - E.Radius;
			if (D <= Tolerance)
			{
				Score = D - 0.3f; // units win over buildings behind them
			}
		}
		else
		{
			const float D = E.Rect.DistanceTo(P);
			if (D <= MinF(Tolerance * 0.5f, 0.3f))
			{
				Score = D;
			}
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			R.Kind = PickKind::Entity;
			R.Id = E.Id;
		}
	}
	if (R.Kind == PickKind::Entity)
	{
		return R;
	}
	const Tile T = Tile::FromPos(P);
	const GameMap& Map = W.GetMap();
	if (Map.InBounds(T))
	{
		if (Map.HasTree(T.X, T.Y))
		{
			R.Kind = PickKind::Tree;
			R.T = T;
		}
		else if (Map.At(T).BeaconSite >= 0 && Map.At(T).Occupant == NoEntity)
		{
			R.Kind = PickKind::BeaconSite;
			R.T = T;
		}
	}
	return R;
}

void PlayerControl::SmartCommand(World& W, const PickResult& Pick, const Vec2& P)
{
	const std::vector<EntityId> Workers = SelectedWorkers(W);
	const std::vector<EntityId> Others = SelectedNonWorkers(W);
	const std::vector<EntityId> All = Selection;

	if (Pick.Kind == PickKind::Entity)
	{
		const Entity* T = W.Find(Pick.Id);
		if (T == nullptr)
		{
			return;
		}
		if (AreEnemies(Team::Player, T->Owner))
		{
			W.CmdAttack(All, Pick.Id);
			return;
		}
		if (T->IsResourceNode())
		{
			if (!Workers.empty())
			{
				W.CmdGatherNode(Workers, Pick.Id);
			}
			if (!Others.empty())
			{
				W.CmdMove(Others, P, true);
			}
			return;
		}
		if (T->Owner == Team::Player && T->IsBuilding() && !Workers.empty() && (!T->bConstructed || T->Hp < T->MaxHp))
		{
			W.CmdHelpBuild(Workers, Pick.Id);
			if (!Others.empty())
			{
				W.CmdMove(Others, P, true);
			}
			return;
		}
		if (T->Owner == Team::Player && T->IsBuilding() && GetDef(T->Type).IsDropOff && !Workers.empty())
		{
			W.CmdReturnCargo(Workers);
		}
	}
	else if (Pick.Kind == PickKind::Tree)
	{
		if (!Workers.empty())
		{
			W.CmdGatherTree(Workers, Pick.T);
		}
		if (!Others.empty())
		{
			W.CmdMove(Others, P, true);
		}
		return;
	}
	W.CmdMove(All, P, true);
}

void PlayerControl::TapWorld(World& W, const Vec2& P, float Tolerance, bool bDouble, const IViewProjector* View)
{
	if (bPlacing)
	{
		MovePlacement(W, P);
		return;
	}
	const PickResult Pick = PickAt(W, P, Tolerance);
	const bool bOwnUnits = HasOwnUnits(W);
	const EntityId SelectedBuilding = GetSingleBuilding(W);

	if (Pick.Kind == PickKind::Entity)
	{
		const Entity* T = W.Find(Pick.Id);
		if (T == nullptr)
		{
			return;
		}
		if (T->Owner == Team::Player && T->IsUnit())
		{
			if (bDouble && View != nullptr)
			{
				std::vector<EntityId> Same;
				for (const Entity& E : W.GetEntities())
				{
					if (!E.bAlive || E.Owner != Team::Player || E.Type != T->Type)
					{
						continue;
					}
					float SX = 0.f;
					float SY = 0.f;
					if (View->WorldToScreen(E.Pos, 0.5f, SX, SY) && SX >= 0.f && SY >= 0.f && SX <= View->GetScreenWidth() && SY <= View->GetScreenHeight())
					{
						Same.push_back(E.Id);
					}
				}
				if (!Same.empty())
				{
					SelectMany(W, Same);
					return;
				}
			}
			SelectOne(W, Pick.Id);
			return;
		}
		if (T->Owner == Team::Player && T->IsBuilding())
		{
			if (bOwnUnits && HasWorkers(W) && (!T->bConstructed || T->Hp < T->MaxHp))
			{
				SmartCommand(W, Pick, P);
				return;
			}
			SelectOne(W, Pick.Id);
			return;
		}
		if (T->IsResourceNode())
		{
			if (bOwnUnits)
			{
				SmartCommand(W, Pick, P);
			}
			else if (SelectedBuilding != NoEntity)
			{
				W.CmdSetRally(SelectedBuilding, T->Pos, T->Id, nullptr);
			}
			else
			{
				SelectOne(W, Pick.Id);
			}
			return;
		}
		// Enemy.
		if (bOwnUnits)
		{
			SmartCommand(W, Pick, P);
		}
		else
		{
			SelectOne(W, Pick.Id);
		}
		return;
	}

	if (Pick.Kind == PickKind::Tree)
	{
		if (bOwnUnits)
		{
			SmartCommand(W, Pick, P);
		}
		else if (SelectedBuilding != NoEntity)
		{
			W.CmdSetRally(SelectedBuilding, Pick.T.Center(), NoEntity, &Pick.T);
		}
		return;
	}

	if (Pick.Kind == PickKind::BeaconSite && HasWorkers(W))
	{
		BeginPlacement(W, Archetype::Beacon, P);
		return;
	}

	// Plain ground.
	if (bOwnUnits)
	{
		SmartCommand(W, Pick, P);
	}
	else if (SelectedBuilding != NoEntity && GetDef(W.Find(SelectedBuilding)->Type).Trains[0] != Archetype::None)
	{
		W.CmdSetRally(SelectedBuilding, P, NoEntity, nullptr);
	}
	else
	{
		ClearSelection();
	}
}

void PlayerControl::CommandAt(World& W, const Vec2& P, float Tolerance)
{
	if (bPlacing)
	{
		CancelPlacement();
		return;
	}
	const PickResult Pick = PickAt(W, P, Tolerance);
	if (HasOwnUnits(W))
	{
		SmartCommand(W, Pick, P);
		return;
	}
	const EntityId B = GetSingleBuilding(W);
	if (B != NoEntity)
	{
		const Entity* NodeE = Pick.Kind == PickKind::Entity ? W.Find(Pick.Id) : nullptr;
		if (NodeE != nullptr && NodeE->IsResourceNode())
		{
			W.CmdSetRally(B, NodeE->Pos, NodeE->Id, nullptr);
		}
		else if (Pick.Kind == PickKind::Tree)
		{
			W.CmdSetRally(B, Pick.T.Center(), NoEntity, &Pick.T);
		}
		else
		{
			W.CmdSetRally(B, P, NoEntity, nullptr);
		}
	}
}

void PlayerControl::BoxSelect(World& W, float X0, float Y0, float X1, float Y1, const IViewProjector& View)
{
	const float MinX = MinF(X0, X1);
	const float MaxX = MaxF(X0, X1);
	const float MinY = MinF(Y0, Y1);
	const float MaxY = MaxF(Y0, Y1);
	std::vector<EntityId> Combat;
	std::vector<EntityId> Workers;
	for (const Entity& E : W.GetEntities())
	{
		if (!E.bAlive || E.Owner != Team::Player || !E.IsUnit())
		{
			continue;
		}
		float SX = 0.f;
		float SY = 0.f;
		if (!View.WorldToScreen(E.Pos, 0.4f, SX, SY))
		{
			continue;
		}
		if (SX >= MinX && SX <= MaxX && SY >= MinY && SY <= MaxY)
		{
			(W.IsCombatUnit(E) ? Combat : Workers).push_back(E.Id);
		}
	}
	if (!Combat.empty())
	{
		SelectMany(W, Combat);
	}
	else if (!Workers.empty())
	{
		SelectMany(W, Workers);
	}
}

// ---------------------------------------------------------------------------------------------
// Placement
// ---------------------------------------------------------------------------------------------

TileRect PlayerControl::PlacementRect() const
{
	const int F = MaxI(GetDef(PlaceType).Footprint, 1);
	return TileRect(PlaceTile.X, PlaceTile.Y, PlaceTile.X + F, PlaceTile.Y + F);
}

bool PlayerControl::SnapBeacon(const World& W, const Vec2& Near, Tile& Out) const
{
	float BestD = 1e9f;
	bool bFound = false;
	for (const TileRect& Site : W.GetMap().BeaconSites)
	{
		if (W.GetMap().At(Site.X0, Site.Y0).Occupant != NoEntity)
		{
			continue;
		}
		const float D = Vec2::Dist(Site.Center(), Near);
		if (D < BestD)
		{
			BestD = D;
			Out = Tile(Site.X0, Site.Y0);
			bFound = true;
		}
	}
	return bFound;
}

void PlayerControl::BeginPlacement(World& W, Archetype Type, const Vec2& Near)
{
	if (!HasWorkers(W))
	{
		W.EmitSimple(EventType::Notice, Team::Player, Near, NoEntity, Type, 0.f, static_cast<int>(NoticeCode::NeedWorker));
		return;
	}
	const Availability Avail = W.CheckBuild(Team::Player, Type);
	if (Avail != Availability::Ok)
	{
		const EventType Feedback = Avail == Availability::NoSunstone ? EventType::NotEnoughSunstone
			: Avail == Availability::NoTimber ? EventType::NotEnoughTimber
			: EventType::RequirementMissing;
		W.EmitSimple(Feedback, Team::Player, Near, NoEntity, Type);
		return;
	}
	PlaceType = Type;
	bBuildMenu = false;
	if (GetDef(Type).NeedsBeaconSite)
	{
		Tile Site;
		if (!SnapBeacon(W, Near, Site))
		{
			W.EmitSimple(EventType::Notice, Team::Player, Near, NoEntity, Type, 0.f, static_cast<int>(NoticeCode::NoFreeBeaconSite));
			return;
		}
		bPlacing = true;
		PlaceTile = Site;
		PlaceState = W.CanPlace(PlaceType, PlaceTile);
		return;
	}
	bPlacing = true;
	const int F = MaxI(GetDef(Type).Footprint, 1);
	const Tile Start = Tile::FromPos(Near - Vec2(static_cast<float>(F) * 0.5f - 0.5f, static_cast<float>(F) * 0.5f - 0.5f));
	PlaceTile = Start;
	// Start on the closest valid spot so the first confirm usually just works.
	for (int R = 0; R <= 6; ++R)
	{
		bool bFound = false;
		for (int Dy = -R; Dy <= R && !bFound; ++Dy)
		{
			for (int Dx = -R; Dx <= R && !bFound; ++Dx)
			{
				if (MaxI(AbsI(Dx), AbsI(Dy)) != R)
				{
					continue;
				}
				const Tile T(Start.X + Dx, Start.Y + Dy);
				if (W.CanPlace(Type, T) == PlaceResult::Ok)
				{
					PlaceTile = T;
					bFound = true;
				}
			}
		}
		if (bFound)
		{
			break;
		}
	}
	PlaceState = W.CanPlace(PlaceType, PlaceTile);
}

void PlayerControl::MovePlacement(World& W, const Vec2& P)
{
	if (!bPlacing)
	{
		return;
	}
	if (GetDef(PlaceType).NeedsBeaconSite)
	{
		Tile Site;
		if (SnapBeacon(W, P, Site))
		{
			PlaceTile = Site;
		}
	}
	else
	{
		const int F = MaxI(GetDef(PlaceType).Footprint, 1);
		PlaceTile = Tile::FromPos(P - Vec2(static_cast<float>(F) * 0.5f - 0.5f, static_cast<float>(F) * 0.5f - 0.5f));
	}
	PlaceState = W.CanPlace(PlaceType, PlaceTile);
}

bool PlayerControl::ConfirmPlacement(World& W)
{
	if (!bPlacing)
	{
		return false;
	}
	PlaceState = W.CanPlace(PlaceType, PlaceTile);
	if (PlaceState != PlaceResult::Ok)
	{
		W.EmitSimple(EventType::InvalidPlacement, Team::Player, PlacementRect().Center(), NoEntity, PlaceType);
		return false;
	}
	const std::vector<EntityId> Workers = SelectedWorkers(W);
	if (Workers.empty())
	{
		CancelPlacement();
		return false;
	}
	const EntityId Id = W.CmdBuild(Workers, PlaceType, PlaceTile);
	bPlacing = false;
	return Id != NoEntity;
}

void PlayerControl::CancelPlacement()
{
	bPlacing = false;
	PlaceType = Archetype::None;
}

// ---------------------------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------------------------

void PlayerControl::Execute(World& W, CameraRig& Camera, const ActionId& A)
{
	switch (A.Kind)
	{
	case ActionKind::Train:
	{
		const EntityId B = GetSingleBuilding(W);
		if (B != NoEntity)
		{
			W.CmdTrain(B, static_cast<Archetype>(A.Param));
		}
		break;
	}
	case ActionKind::Research:
	{
		const EntityId B = GetSingleBuilding(W);
		if (B != NoEntity)
		{
			W.CmdResearch(B, static_cast<Research>(A.Param));
		}
		break;
	}
	case ActionKind::BuildMenu:
		bBuildMenu = true;
		break;
	case ActionKind::Back:
		bBuildMenu = false;
		break;
	case ActionKind::PlaceBuilding:
		BeginPlacement(W, static_cast<Archetype>(A.Param), Camera.Focus);
		break;
	case ActionKind::Ability:
		W.CmdAbility(Selection, static_cast<Ability>(A.Param));
		break;
	case ActionKind::Stop:
		W.CmdStop(Selection);
		break;
	case ActionKind::ReturnCargo:
		W.CmdReturnCargo(Selection);
		break;
	case ActionKind::CancelConstruction:
	{
		const EntityId B = GetSingleBuilding(W);
		if (B != NoEntity)
		{
			W.CmdCancelConstruction(B);
			ClearSelection();
		}
		break;
	}
	case ActionKind::CancelQueue:
	{
		const EntityId B = GetSingleBuilding(W);
		if (B != NoEntity)
		{
			W.CmdCancelQueueItem(B, A.Param);
		}
		break;
	}
	case ActionKind::ConfirmPlacement:
		ConfirmPlacement(W);
		break;
	case ActionKind::CancelPlacement:
		CancelPlacement();
		break;
	case ActionKind::SelectArmy:
	{
		std::vector<EntityId> Army;
		for (const Entity& E : W.GetEntities())
		{
			if (E.bAlive && E.Owner == Team::Player && W.IsCombatUnit(E))
			{
				Army.push_back(E.Id);
			}
		}
		if (Army.empty())
		{
			W.EmitSimple(EventType::Notice, Team::Player, Camera.Focus, NoEntity, Archetype::None, 0.f, static_cast<int>(NoticeCode::NoSoldiers));
		}
		else
		{
			CancelPlacement();
			SelectMany(W, Army);
		}
		break;
	}
	case ActionKind::SelectIdleWorker:
	{
		std::vector<const Entity*> Idle;
		for (const Entity& E : W.GetEntities())
		{
			if (E.bAlive && E.Owner == Team::Player && E.IsUnit() && GetDef(E.Type).IsWorker && E.Order == OrderType::Idle)
			{
				Idle.push_back(&E);
			}
		}
		if (Idle.empty())
		{
			W.EmitSimple(EventType::Notice, Team::Player, Camera.Focus, NoEntity, Archetype::None, 0.f, static_cast<int>(NoticeCode::NoIdleWorkers));
			break;
		}
		std::sort(Idle.begin(), Idle.end(), [](const Entity* L, const Entity* R) { return L->Id < R->Id; });
		IdleCycle = IdleCycle % static_cast<int>(Idle.size());
		const Entity* Pick = Idle[static_cast<size_t>(IdleCycle)];
		++IdleCycle;
		CancelPlacement();
		SelectOne(W, Pick->Id);
		Camera.JumpTo(Pick->Pos);
		break;
	}
	case ActionKind::Home:
		for (const Entity& E : W.GetEntities())
		{
			if (E.bAlive && E.Owner == Team::Player && E.Type == Archetype::Keep)
			{
				Camera.JumpTo(E.Pos);
				break;
			}
		}
		break;
	case ActionKind::Deselect:
		CancelPlacement();
		ClearSelection();
		break;
	case ActionKind::None:
		break;
	}
}

void PlayerControl::BuildQuickBar(const World& W, std::vector<ActionButton>& Out) const
{
	Out.clear();
	{
		ActionButton B;
		B.Id = ActionId(ActionKind::SelectArmy);
		B.IconId = Icon::Army;
		B.Label = "Army";
		B.Tooltip = "Select all soldiers";
		B.Badge = W.CountUnits(Team::Player, true);
		B.bEnabled = B.Badge > 0;
		B.Hotkey = 'Q';
		Out.push_back(B);
	}
	{
		ActionButton B;
		B.Id = ActionId(ActionKind::SelectIdleWorker);
		B.IconId = Icon::Worker;
		B.Label = "Idle";
		B.Tooltip = "Select an idle Lamplighter";
		B.Badge = CountIdleWorkers(W);
		B.bEnabled = B.Badge > 0;
		B.Hotkey = 'E';
		Out.push_back(B);
	}
	{
		ActionButton B;
		B.Id = ActionId(ActionKind::Home);
		B.IconId = Icon::Home;
		B.Label = "Keep";
		B.Tooltip = "Centre the view on your Keep";
		B.Hotkey = 'H';
		Out.push_back(B);
	}
	for (ActionButton& B : Out)
	{
		B.bHighlight = ActionMatchesHighlight(B.Id, TutorialHighlight.c_str());
	}
}

void PlayerControl::BuildActions(const World& W, std::vector<ActionButton>& Out) const
{
	Out.clear();
	if (bPlacing)
	{
		ActionButton Ok;
		Ok.Id = ActionId(ActionKind::ConfirmPlacement);
		Ok.IconId = Icon::Confirm;
		Ok.Label = "Place";
		Ok.bEnabled = PlaceState == PlaceResult::Ok;
		Ok.Reason = PlaceState == PlaceResult::NeedsBeaconSite ? "Must be on a Beacon site"
			: PlaceState == PlaceResult::Blight ? "Cannot build on blight"
			: PlaceState == PlaceResult::OutOfBounds ? "Out of bounds"
			: PlaceState == PlaceResult::Blocked ? "Blocked"
			: "";
		Ok.Hotkey = 'F';
		Out.push_back(Ok);
		ActionButton Cancel;
		Cancel.Id = ActionId(ActionKind::CancelPlacement);
		Cancel.IconId = Icon::Cancel;
		Cancel.Label = "Cancel";
		Cancel.Hotkey = 'X';
		Out.push_back(Cancel);
	}
	else
	{
		switch (GetSelectionKind(W))
		{
		case SelectionKind::OwnUnits:
		{
			const bool bWorkers = HasWorkers(W);
			if (bWorkers && bBuildMenu)
			{
				for (int I = 0; I < BuildMenuCount; ++I)
				{
					const Archetype A = BuildMenu[I];
					const ArchetypeDef& D = GetDef(A);
					ActionButton B;
					B.Id = ActionId(ActionKind::PlaceBuilding, static_cast<int>(A));
					B.IconId = D.IconId;
					B.Label = D.Name;
					B.Tooltip = D.Description;
					B.CostSunstone = D.CostSunstone;
					B.CostTimber = D.CostTimber;
					const Availability Avail = W.CheckBuild(Team::Player, A);
					B.bEnabled = Avail == Availability::Ok;
					B.Reason = Avail == Availability::Locked && D.Requires != Archetype::None ? ControlRequiresText(D.Requires) : AvailabilityText(Avail);
					B.Hotkey = static_cast<char>('1' + I);
					Out.push_back(B);
				}
				ActionButton Back;
				Back.Id = ActionId(ActionKind::Back);
				Back.IconId = Icon::Back;
				Back.Label = "Back";
				Back.Hotkey = 'X';
				Out.push_back(Back);
				break;
			}
			if (bWorkers)
			{
				ActionButton Build;
				Build.Id = ActionId(ActionKind::BuildMenu);
				Build.IconId = Icon::Build;
				Build.Label = "Build";
				Build.Tooltip = "Choose a building to place";
				Build.Hotkey = 'B';
				Out.push_back(Build);
				bool bCarrying = false;
				for (EntityId Id : Selection)
				{
					const Entity* E = W.Find(Id);
					if (E != nullptr && E->CarryAmount > 0)
					{
						bCarrying = true;
					}
				}
				if (bCarrying)
				{
					ActionButton Ret;
					Ret.Id = ActionId(ActionKind::ReturnCargo);
					Ret.IconId = Icon::Keep;
					Ret.Label = "Return";
					Ret.Tooltip = "Bring cargo to the nearest drop-off";
					Ret.Hotkey = 'R';
					Out.push_back(Ret);
				}
			}
			// One button per ability present in the selection.
			bool bSeen[NumAbilities] = {};
			const char AbilityHotkeys[] = {'Z', 'X', 'C', 'V'};
			int AbilitySlot = 0;
			for (EntityId Id : Selection)
			{
				const Entity* E = W.Find(Id);
				if (E == nullptr || !E->bAlive)
				{
					continue;
				}
				const Ability Ab = GetDef(E->Type).AbilityId;
				if (Ab == Ability::None || bSeen[static_cast<int>(Ab)])
				{
					continue;
				}
				bSeen[static_cast<int>(Ab)] = true;
				int Ready = 0;
				float MinRemaining = 1e9f;
				float MaxCooldown = 1.f;
				for (EntityId Other : Selection)
				{
					const Entity* O = W.Find(Other);
					if (O == nullptr || !O->bAlive || GetDef(O->Type).AbilityId != Ab)
					{
						continue;
					}
					if (O->AbilityCooldown <= 0.f)
					{
						++Ready;
					}
					MinRemaining = MinF(MinRemaining, O->AbilityCooldown);
					MaxCooldown = MaxF(0.01f, W.GetAbilityCooldownMax(*O));
				}
				const AbilityDef& AD = GetAbilityDef(Ab);
				ActionButton B;
				B.Id = ActionId(ActionKind::Ability, static_cast<int>(Ab));
				B.IconId = AD.IconId;
				B.Label = AD.Name;
				B.Tooltip = AD.Description;
				B.Badge = Ready;
				B.bEnabled = Ready > 0;
				B.Cooldown = Ready > 0 ? 0.f : Saturate(MinRemaining / MaxCooldown);
				B.Reason = Ready > 0 ? "" : "Recharging";
				B.Hotkey = AbilityHotkeys[AbilitySlot++ % 4];
				Out.push_back(B);
			}
			ActionButton Stop;
			Stop.Id = ActionId(ActionKind::Stop);
			Stop.IconId = Icon::Stop;
			Stop.Label = "Stop";
			Stop.Tooltip = "Stop and hold here";
			Stop.Hotkey = 'S';
			Out.push_back(Stop);
			break;
		}
		case SelectionKind::OwnBuilding:
		{
			const Entity* B = W.Find(Selection.front());
			if (B == nullptr)
			{
				break;
			}
			const ArchetypeDef& BD = GetDef(B->Type);
			if (!B->bConstructed)
			{
				ActionButton Cancel;
				Cancel.Id = ActionId(ActionKind::CancelConstruction);
				Cancel.IconId = Icon::Cancel;
				Cancel.Label = "Cancel";
				Cancel.Tooltip = "Cancel construction (75% refund)";
				Cancel.Hotkey = 'X';
				Out.push_back(Cancel);
				break;
			}
			int HotkeyIndex = 0;
			for (Archetype U : BD.Trains)
			{
				if (U == Archetype::None)
				{
					continue;
				}
				const ArchetypeDef& UD = GetDef(U);
				ActionButton Btn;
				Btn.Id = ActionId(ActionKind::Train, static_cast<int>(U));
				Btn.IconId = UD.IconId;
				Btn.Label = UD.Name;
				Btn.Tooltip = UD.Description;
				Btn.CostSunstone = UD.CostSunstone;
				Btn.CostTimber = UD.CostTimber;
				Btn.CostSupply = UD.SupplyCost;
				const Availability Avail = W.CheckTrain(*B, U);
				Btn.bEnabled = Avail == Availability::Ok;
				Btn.Reason = AvailabilityText(Avail);
				for (const ProductionItem& Item : B->Queue)
				{
					if (!Item.bResearch && Item.Unit == U)
					{
						++Btn.Badge;
					}
				}
				Btn.Hotkey = static_cast<char>('1' + HotkeyIndex++);
				Out.push_back(Btn);
			}
			for (Research Line : BD.Researches)
			{
				if (Line == Research::None)
				{
					continue;
				}
				const Research R = W.NextResearchLevel(Team::Player, Line);
				const ResearchDef& RD = GetResearchDef(R);
				const Availability Avail = W.CheckResearch(*B, R);
				ActionButton Btn;
				Btn.Id = ActionId(ActionKind::Research, static_cast<int>(R));
				Btn.IconId = RD.IconId;
				Btn.Label = RD.Name;
				Btn.Tooltip = RD.Description;
				Btn.CostSunstone = RD.CostSunstone;
				Btn.CostTimber = RD.CostTimber;
				Btn.bEnabled = Avail == Availability::Ok;
				Btn.Reason = AvailabilityText(Avail);
				if (Avail == Availability::Done)
				{
					Btn.CostSunstone = Btn.CostTimber = 0;
				}
				Btn.Hotkey = static_cast<char>('1' + HotkeyIndex++);
				Out.push_back(Btn);
			}
			break;
		}
		case SelectionKind::None:
		case SelectionKind::Enemy:
		case SelectionKind::Resource:
			break;
		}
	}
	for (ActionButton& B : Out)
	{
		B.bHighlight = ActionMatchesHighlight(B.Id, TutorialHighlight.c_str());
	}
}

} // namespace bh
