// Beaconhold simulation core - mission session.
#include "BhSession.h"

namespace bh
{
bool Session::Start(const SessionConfig& InConfig, std::string& OutError)
{
	Config = InConfig;
	Config.MissionIndex = ClampI(Config.MissionIndex, 0, GetMissionCount() - 1);
	const MissionDef& M = bh::GetMission(Config.MissionIndex);
	if (!LoadMissionMap(M, TheWorld, OutError))
	{
		bStarted = false;
		return false;
	}
	if (Config.Seed != 0)
	{
		TheWorld.GetRng().SetState(M.Seed ^ (Config.Seed * 2654435761u));
	}
	ApplyModifiers();
	TheWorld.RecomputeSupply();
	Mission.Start(Config.MissionIndex, Config.Diff, Config.bTutorial, TheWorld);
	AI.Start(M.AI, Config.Diff, TheWorld);
	Control.Reset();
	Gestures.Reset();
	Camera.Init(TheWorld.GetMap().GetWidth(), TheWorld.GetMap().GetHeight(), GetKeepPos() + Vec2(0.f, -2.f));
	Pending.clear();
	Accumulator = 0.f;
	bPaused = false;
	Speed = 1.f;
	bContinuePressed = false;
	bDraggingPlacement = false;
	bStarted = true;
	return true;
}

void Session::ApplyModifiers()
{
	const MissionDef& M = bh::GetMission(Config.MissionIndex);
	const SessionModifiers Mods = ComputeModifiers(Config.BoonRanks);
	TeamState& Player = TheWorld.GetTeam(Team::Player);
	Player.HpMult = Mods.HpMult;
	Player.DamageMult = Mods.DamageMult;
	Player.GatherMult = Mods.GatherMult;
	Player.BuildTimeMult = Mods.BuildTimeMult;
	Player.BuildingHpMult = Mods.BuildingHpMult;
	Player.Res[0] = M.StartSunstone + Mods.BonusResources;
	Player.Res[1] = M.StartTimber + Mods.BonusResources;

	const DifficultyTuning Tuning = GetDifficultyTuning(Config.Diff);
	TeamState& Gloam = TheWorld.GetTeam(Team::Enemy);
	Gloam.HpMult = Tuning.EnemyHp;
	Gloam.DamageMult = Tuning.EnemyDamage;
	Gloam.BuildingHpMult = Tuning.EnemyHp;
	Gloam.bIgnoreSupply = true;

	// Entities placed by the map were spawned before the multipliers existed: rescale them.
	for (Entity& E : TheWorld.MutableEntities())
	{
		if (!E.bAlive || E.Owner == Team::Neutral)
		{
			continue;
		}
		const TeamState& T = TheWorld.GetTeam(E.Owner);
		const float Mult = E.IsBuilding() ? T.BuildingHpMult : T.HpMult;
		E.MaxHp = GetDef(E.Type).MaxHp * Mult;
		E.Hp = E.MaxHp;
	}
}

Vec2 Session::GetKeepPos() const
{
	for (const Entity& E : TheWorld.GetEntities())
	{
		if (E.bAlive && E.Owner == Team::Player && E.Type == Archetype::Keep)
		{
			return E.Pos;
		}
	}
	for (const Entity& E : TheWorld.GetEntities())
	{
		if (E.bAlive && E.Owner == Team::Player)
		{
			return E.Pos;
		}
	}
	return Vec2(static_cast<float>(TheWorld.GetMap().GetWidth()) * 0.5f, static_cast<float>(TheWorld.GetMap().GetHeight()) * 0.5f);
}

int Session::Update(float RealDt, const IViewProjector* View)
{
	const float Dt = ClampF(RealDt, 0.f, 0.25f);
	RealTime += static_cast<double>(Dt);
	if (!bStarted)
	{
		return 0;
	}
	if (View != nullptr)
	{
		const float H = View->GetScreenHeight();
		if (H > 0.f && H != LastScreenHeight)
		{
			LastScreenHeight = H;
			Gestures.Configure(H);
		}
		Gestures.Update(RealTime);
		for (size_t I = 0; I < Gestures.Events.size(); ++I)
		{
			RouteGesture(Gestures.Events[I], *View);
		}
	}
	Gestures.Events.clear();

	Camera.Update(Dt);
	if (!Control.bCameraMoved && Camera.PannedDistance > 2.5f)
	{
		Control.bCameraMoved = true;
	}

	int Ticks = 0;
	if (!bPaused)
	{
		Accumulator += Dt * Speed;
		while (Accumulator >= World::TickSeconds && Ticks < 8)
		{
			PlayerContext Ctx;
			Ctx.Selection = &Control.Selection;
			Ctx.SelectionLatch = Control.SelectionLatch;
			Ctx.ArmyLatch = Control.ArmyLatch;
			Ctx.bCameraMoved = Control.bCameraMoved;
			Ctx.bContinuePressed = bContinuePressed;
			const int StepBefore = Mission.TutorialIndex;
			TheWorld.Tick(World::TickSeconds);
			Mission.Tick(TheWorld, World::TickSeconds, Ctx);
			if (Mission.TutorialIndex != StepBefore)
			{
				Control.ResetLatches();
			}
			AI.Tick(TheWorld, World::TickSeconds);
			Control.Prune(TheWorld);
			Pending.insert(Pending.end(), TheWorld.Events.begin(), TheWorld.Events.end());
			TheWorld.Events.clear();
			bContinuePressed = false;
			Accumulator -= World::TickSeconds;
			++Ticks;
		}
		if (Ticks >= 8)
		{
			Accumulator = 0.f; // device too slow: drop time instead of spiralling
		}
	}
	const TutorialStep* Step = Mission.CurrentTutorialStep();
	Control.TutorialHighlight = Step != nullptr ? Step->Highlight : "";
	return Ticks;
}

void Session::TakeEvents(std::vector<GameEvent>& Out)
{
	Out.clear();
	Out.swap(Pending);
	Out.insert(Out.end(), TheWorld.Events.begin(), TheWorld.Events.end());
	TheWorld.Events.clear();
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------

void Session::PointerDown(int Id, float X, float Y, bool bMouse)
{
	Gestures.PointerDown(Id, X, Y, RealTime, bMouse);
}

void Session::PointerMove(int Id, float X, float Y)
{
	Gestures.PointerMove(Id, X, Y, RealTime);
}

void Session::PointerUp(int Id, float X, float Y)
{
	Gestures.PointerUp(Id, X, Y, RealTime);
}

bool Session::GetSelectionBox(float& X0, float& Y0, float& X1, float& Y1) const
{
	if (!Gestures.IsBoxActive())
	{
		return false;
	}
	Gestures.GetBox(X0, Y0, X1, Y1);
	return true;
}

float Session::TapTolerance(const IViewProjector& View, float X, float Y) const
{
	const float Pixels = 30.f * MaxF(0.5f, View.GetScreenHeight() / 1080.f);
	Vec2 A;
	Vec2 B;
	if (!View.ScreenToGround(X, Y, A) || !View.ScreenToGround(X + Pixels, Y, B))
	{
		return 0.6f;
	}
	return ClampF(Vec2::Dist(A, B), 0.35f, 1.4f);
}

void Session::MouseCommand(float X, float Y, const IViewProjector& View)
{
	Vec2 P;
	if (View.ScreenToGround(X, Y, P))
	{
		Control.CommandAt(TheWorld, P, TapTolerance(View, X, Y));
	}
}

void Session::MouseWheel(float Delta)
{
	if (Delta > 0.f)
	{
		Camera.ZoomBy(0.87f);
	}
	else if (Delta < 0.f)
	{
		Camera.ZoomBy(1.15f);
	}
}

void Session::PanByScreen(float Dx, float Dy, const IViewProjector& View)
{
	const float Cx = View.GetScreenWidth() * 0.5f;
	const float Cy = View.GetScreenHeight() * 0.5f;
	Vec2 A;
	Vec2 B;
	if (View.ScreenToGround(Cx, Cy, A) && View.ScreenToGround(Cx + Dx, Cy + Dy, B))
	{
		Camera.Pan(A - B);
	}
}

void Session::PanByWorld(const Vec2& Delta)
{
	Camera.Pan(Delta);
}

void Session::ExecuteAction(const ActionId& A)
{
	Control.Execute(TheWorld, Camera, A);
}

void Session::RouteGesture(const GestureEvent& E, const IViewProjector& View)
{
	switch (E.Type)
	{
	case GestureType::Tap:
	case GestureType::DoubleTap:
	{
		Vec2 P;
		if (View.ScreenToGround(E.X, E.Y, P))
		{
			Control.TapWorld(TheWorld, P, TapTolerance(View, E.X, E.Y), E.Type == GestureType::DoubleTap, &View);
		}
		break;
	}
	case GestureType::PanStart:
	{
		bDraggingPlacement = false;
		Camera.StopInertia();
		Vec2 Start;
		if (Control.bPlacing && View.ScreenToGround(E.PrevX, E.PrevY, Start))
		{
			const TileRect R = Control.PlacementRect().Expanded(1);
			bDraggingPlacement = R.DistanceTo(Start) <= 0.01f;
		}
		break;
	}
	case GestureType::Pan:
	{
		Vec2 Cur;
		if (bDraggingPlacement)
		{
			if (View.ScreenToGround(E.X, E.Y, Cur))
			{
				Control.MovePlacement(TheWorld, Cur);
			}
			break;
		}
		Vec2 Prev;
		if (View.ScreenToGround(E.PrevX, E.PrevY, Prev) && View.ScreenToGround(E.X, E.Y, Cur))
		{
			Camera.Pan(Prev - Cur);
		}
		break;
	}
	case GestureType::PanEnd:
	{
		if (bDraggingPlacement)
		{
			bDraggingPlacement = false;
			break;
		}
		const float Dt = 0.05f;
		Vec2 A;
		Vec2 B;
		if ((E.VelX != 0.f || E.VelY != 0.f) && View.ScreenToGround(E.X, E.Y, A) &&
			View.ScreenToGround(E.X - E.VelX * Dt, E.Y - E.VelY * Dt, B))
		{
			Camera.Fling((B - A) / Dt);
		}
		break;
	}
	case GestureType::BoxEnd:
		Control.BoxSelect(TheWorld, E.StartX, E.StartY, E.X, E.Y, View);
		break;
	case GestureType::PinchStart:
		Camera.StopInertia();
		break;
	case GestureType::Pinch:
	{
		if (E.Scale > 0.01f)
		{
			Camera.ZoomBy(1.f / E.Scale);
		}
		Vec2 Prev;
		Vec2 Cur;
		if (View.ScreenToGround(E.PrevX, E.PrevY, Prev) && View.ScreenToGround(E.X, E.Y, Cur))
		{
			Camera.Pan(Prev - Cur);
		}
		break;
	}
	case GestureType::LongPress:
	case GestureType::BoxStart:
	case GestureType::Box:
	case GestureType::BoxCancel:
	case GestureType::PinchEnd:
		break;
	}
}

} // namespace bh
