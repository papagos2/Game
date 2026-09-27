// Session-level tests: save/restore determinism, gestures, touch control flows, HUD model, progress.
#include "TestFramework.h"

#include "Bot.h"

#include "BhHud.h"
#include "BhProgress.h"
#include "BhSerialize.h"
#include "BhSession.h"

using namespace bh;

namespace
{
// Straight top-down projector: 40 pixels per tile, no camera motion (tests only).
class TopDownView : public IViewProjector
{
public:
	float Scale = 40.f;
	Vec2 Origin;
	bool ScreenToGround(float X, float Y, Vec2& Out) const override
	{
		Out = Origin + Vec2(X / Scale, Y / Scale);
		return true;
	}
	bool WorldToScreen(const Vec2& P, float, float& OutX, float& OutY) const override
	{
		OutX = (P.X - Origin.X) * Scale;
		OutY = (P.Y - Origin.Y) * Scale;
		return true;
	}
	float GetScreenWidth() const override { return 1920.f; }
	float GetScreenHeight() const override { return 1080.f; }
};

void Tap(Session& S, TopDownView& V, float X, float Y)
{
	S.PointerDown(0, X, Y, false);
	S.Update(0.03f, &V);
	S.PointerUp(0, X, Y);
	S.Update(0.4f, &V); // past the double-tap window
}

const Entity* FirstOwned(const World& W, Archetype A)
{
	for (const Entity& E : W.GetEntities())
	{
		if (E.bAlive && E.Owner == Team::Player && E.Type == A)
		{
			return &E;
		}
	}
	return nullptr;
}
} // namespace

BH_TEST(Session_SaveLoadIsDeterministic)
{
	Session A;
	SessionConfig C;
	C.MissionIndex = 2;
	C.bTutorial = false;
	std::string Err;
	BH_EXPECT(A.Start(C, Err));
	bht::BotConfig Cfg;
	bht::PlayMission(A, Cfg, 240.f);

	std::vector<uint8_t> Bytes;
	SaveSession(A, Bytes);
	BH_EXPECT(Bytes.size() > 1000);

	Session B;
	const bool bLoaded = LoadSession(B, Bytes, Err);
	BH_EXPECT_MSG(bLoaded, "load failed: %s", Err.c_str());
	BH_EXPECT(HashWorld(A.GetWorld()) == HashWorld(B.GetWorld()));
	for (int I = 0; I < 1200; ++I)
	{
		A.Update(World::TickSeconds, nullptr);
		B.Update(World::TickSeconds, nullptr);
		if (I % 200 == 0)
		{
			BH_EXPECT_MSG(HashWorld(A.GetWorld()) == HashWorld(B.GetWorld()), "diverged at tick %d", I);
		}
	}
	BH_EXPECT(HashWorld(A.GetWorld()) == HashWorld(B.GetWorld()));

	// Garbage and truncated data are rejected.
	std::vector<uint8_t> Bad(Bytes.begin(), Bytes.begin() + static_cast<long>(Bytes.size() / 2));
	Session D;
	BH_EXPECT(!LoadSession(D, Bad, Err));
	std::vector<uint8_t> Junk(64, 7);
	BH_EXPECT(!LoadSession(D, Junk, Err));
}

BH_TEST(Gestures_TapDoubleTapPanBoxPinch)
{
	GestureRecognizer G;
	G.Configure(1080.f);
	G.PointerDown(0, 100.f, 100.f, 0.0, false);
	G.PointerUp(0, 102.f, 101.f, 0.1);
	BH_EXPECT(G.Events.size() == 1 && G.Events[0].Type == GestureType::Tap);
	G.Events.clear();
	G.PointerDown(0, 101.f, 100.f, 0.25, false);
	G.PointerUp(0, 101.f, 100.f, 0.3);
	BH_EXPECT(G.Events.size() == 1 && G.Events[0].Type == GestureType::DoubleTap);
	G.Events.clear();

	// Pan
	G.PointerDown(0, 500.f, 500.f, 1.0, false);
	G.PointerMove(0, 540.f, 500.f, 1.05);
	G.PointerMove(0, 600.f, 500.f, 1.1);
	G.PointerUp(0, 600.f, 500.f, 1.12);
	BH_EXPECT(G.Events.front().Type == GestureType::PanStart);
	BH_EXPECT(G.Events.back().Type == GestureType::PanEnd && G.Events.back().VelX > 100.f);
	G.Events.clear();

	// Long press then drag = box
	G.PointerDown(0, 300.f, 300.f, 2.0, false);
	G.Update(2.5);
	BH_EXPECT(!G.Events.empty() && G.Events.back().Type == GestureType::LongPress);
	G.PointerMove(0, 400.f, 380.f, 2.6);
	BH_EXPECT(G.IsBoxActive());
	G.PointerUp(0, 400.f, 380.f, 2.7);
	BH_EXPECT(G.Events.back().Type == GestureType::BoxEnd && G.Events.back().StartX == 300.f);
	G.Events.clear();

	// Pinch
	G.PointerDown(0, 400.f, 400.f, 3.0, false);
	G.PointerDown(1, 600.f, 400.f, 3.01, false);
	G.PointerMove(1, 700.f, 400.f, 3.1);
	bool bPinch = false;
	for (const GestureEvent& E : G.Events)
	{
		if (E.Type == GestureType::Pinch)
		{
			bPinch = E.Scale > 1.4f;
		}
	}
	BH_EXPECT(bPinch);
	G.PointerUp(0, 400.f, 400.f, 3.2);
	G.PointerUp(1, 700.f, 400.f, 3.2);
	G.Events.clear();

	// Mouse drag = immediate box
	G.PointerDown(0, 100.f, 100.f, 4.0, true);
	G.PointerMove(0, 200.f, 200.f, 4.05);
	BH_EXPECT(G.IsBoxActive());
	G.PointerUp(0, 200.f, 200.f, 4.1);
}

BH_TEST(Session_TouchFlow_SelectGatherBuild)
{
	Session S;
	SessionConfig C;
	C.MissionIndex = 0;
	C.bTutorial = true;
	std::string Err;
	BH_EXPECT(S.Start(C, Err));
	World& W = S.GetWorld();
	TopDownView V;
	S.Update(0.05f, &V);

	// Tap a Lamplighter -> selected.
	const Entity* Worker = FirstOwned(W, Archetype::Lamplighter);
	BH_EXPECT(Worker != nullptr);
	const EntityId WorkerId = Worker->Id;
	Tap(S, V, Worker->Pos.X * V.Scale, Worker->Pos.Y * V.Scale);
	BH_EXPECT(S.GetControl().Selection.size() == 1 && S.GetControl().Selection[0] == WorkerId);

	// Tap the sunstone -> gathering.
	const EntityId Node = W.FindNearestNode(Worker->Pos, 50.f, NoEntity);
	const Entity* N = W.Find(Node);
	Tap(S, V, N->Pos.X * V.Scale, N->Pos.Y * V.Scale);
	BH_EXPECT(W.Find(WorkerId)->Order == OrderType::Gather);

	// Build menu -> Cottage -> placement -> confirm.
	HudModel Hud;
	BuildHudModel(S, Hud);
	BH_EXPECT(!Hud.Actions.empty() && Hud.Actions[0].Id.Kind == ActionKind::BuildMenu);
	S.ExecuteAction(ActionId(ActionKind::BuildMenu));
	BuildHudModel(S, Hud);
	BH_EXPECT(static_cast<int>(Hud.Actions.size()) == BuildMenuCount + 1);
	S.ExecuteAction(ActionId(ActionKind::PlaceBuilding, static_cast<int>(Archetype::Cottage)));
	BH_EXPECT(S.GetControl().bPlacing);
	BH_EXPECT(S.GetControl().PlaceState == PlaceResult::Ok);
	S.ExecuteAction(ActionId(ActionKind::ConfirmPlacement));
	BH_EXPECT(!S.GetControl().bPlacing);
	BH_EXPECT(W.CountOwned(Team::Player, Archetype::Cottage, true) == 1);

	// Tap the Keep -> train button available, tutorial highlight lives on it later.
	const Entity* Keep = FirstOwned(W, Archetype::Keep);
	Tap(S, V, Keep->Pos.X * V.Scale, Keep->Pos.Y * V.Scale);
	BuildHudModel(S, Hud);
	BH_EXPECT(Hud.Selection.Kind == SelectionKind::OwnBuilding);
	BH_EXPECT(!Hud.Actions.empty() && Hud.Actions[0].Id == ActionId(ActionKind::Train, static_cast<int>(Archetype::Lamplighter)));

	// Box select all Lamplighters on screen.
	S.PointerDown(0, 1.f, 1.f, true);
	S.Update(0.02f, &V);
	S.PointerMove(0, 1900.f, 1070.f);
	S.Update(0.02f, &V);
	S.PointerUp(0, 1900.f, 1070.f);
	S.Update(0.02f, &V);
	BH_EXPECT(S.GetControl().Selection.size() >= 3);
	BH_EXPECT(S.GetControl().GetSelectionKind(W) == SelectionKind::OwnUnits);
}

BH_TEST(Session_CameraPanAndPinch)
{
	Session S;
	SessionConfig C;
	C.MissionIndex = 2;
	C.bTutorial = false;
	std::string Err;
	BH_EXPECT(S.Start(C, Err));
	TopDownView V;
	const Vec2 Before = S.GetCamera().Focus;
	S.PointerDown(0, 800.f, 500.f, false);
	S.Update(0.02f, &V);
	S.PointerMove(0, 700.f, 450.f);
	S.Update(0.02f, &V);
	S.PointerMove(0, 600.f, 400.f);
	S.Update(0.02f, &V);
	S.PointerUp(0, 600.f, 400.f);
	S.Update(0.02f, &V);
	const Vec2 After = S.GetCamera().Focus;
	BH_EXPECT_MSG(After.X > Before.X + 3.f, "dragging left should move the view right (%.2f -> %.2f)", Before.X, After.X);
	BH_EXPECT(S.GetControl().bCameraMoved);

	const float Dist = S.GetCamera().TargetDistance;
	S.PointerDown(0, 800.f, 500.f, false);
	S.PointerDown(1, 900.f, 500.f, false);
	S.Update(0.02f, &V);
	S.PointerMove(1, 1100.f, 500.f);
	S.Update(0.02f, &V);
	S.PointerUp(0, 800.f, 500.f);
	S.PointerUp(1, 1100.f, 500.f);
	S.Update(0.02f, &V);
	BH_EXPECT(S.GetCamera().TargetDistance < Dist);
}

BH_TEST(Session_NoticesAndSummary)
{
	Session S;
	SessionConfig C;
	C.MissionIndex = 0;
	std::string Err;
	BH_EXPECT(S.Start(C, Err));
	World& W = S.GetWorld();
	const Entity* Keep = FirstOwned(W, Archetype::Keep);
	W.GetTeam(Team::Player).Res[0] = 0;
	W.CmdTrain(Keep->Id, Archetype::Lamplighter);
	std::vector<GameEvent> Events;
	S.TakeEvents(Events);
	bool bNotice = false;
	for (const GameEvent& E : Events)
	{
		Notice N;
		if (MakeNotice(E, S, N))
		{
			bNotice = N.Text == "Not enough Sunstone";
		}
	}
	BH_EXPECT(bNotice);
	const MissionSummary Sum = BuildSummary(S);
	BH_EXPECT(!Sum.bWon && Sum.StarText[1].find("Finish within") == 0);
}

BH_TEST(Progress_RenownAndBoons)
{
	CampaignProgress P;
	BH_EXPECT(P.IsUnlocked(0) && !P.IsUnlocked(1));
	BH_EXPECT(P.RecordVictory(0, 3, Difficulty::Normal));
	BH_EXPECT(P.IsUnlocked(1));
	BH_EXPECT(!P.RecordVictory(0, 2, Difficulty::Normal));
	BH_EXPECT(P.TotalRenown() == 3);
	BH_EXPECT(P.RaiseBoon(Boon::Hardy));  // costs 1
	BH_EXPECT(P.RaiseBoon(Boon::Hardy));  // costs 2
	BH_EXPECT(!P.RaiseBoon(Boon::Hardy)); // would cost 3, only 0 left
	BH_EXPECT(P.AvailableRenown() == 0);
	P.ResetBoons();
	BH_EXPECT(P.AvailableRenown() == 3);
	P.RecordVictory(1, 1, Difficulty::Hard);
	BH_EXPECT(P.TotalRenown() == 5);

	int Ranks[NumBoons] = {2, 0, 0, 0, 1};
	const SessionModifiers M = ComputeModifiers(Ranks);
	BH_EXPECT(M.HpMult > 1.15f && M.BonusResources == 75);

	Session S;
	SessionConfig C;
	C.MissionIndex = 1;
	C.BoonRanks[0] = 2;
	std::string Err;
	BH_EXPECT(S.Start(C, Err));
	const Entity* Worker = FirstOwned(S.GetWorld(), Archetype::Lamplighter);
	BH_EXPECT(Worker != nullptr && Worker->MaxHp > GetDef(Archetype::Lamplighter).MaxHp);
}
