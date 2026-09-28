// Session-level tests: save/restore determinism, gestures, touch control flows, HUD model, progress.
#include "TestFramework.h"

#include "Bot.h"

#include "BhHud.h"
#include "BhProgress.h"
#include "BhSerialize.h"
#include "BhSession.h"
#include "BhVisuals.h"

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

// Like the game camera: tilted, so height lifts a point up the screen (tests of screen picking).
class ObliqueView : public TopDownView
{
public:
	bool WorldToScreen(const Vec2& P, float Height, float& OutX, float& OutY) const override
	{
		OutX = (P.X - Origin.X) * Scale;
		OutY = (P.Y - Origin.Y) * Scale - Height * Scale * 0.8f;
		return true;
	}
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
	BH_EXPECT(static_cast<int>(Hud.Actions.size()) == BuildableStructureCount + 1);
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

BH_TEST(Tutorial_PlaceStepLightsThePlaceButton)
{
	// The "build a Cottage" step lights the way through every tap: Build, Cottage, then Place
	// once the outline is on the map (a first-time player did not know how to finish).
	Session S;
	SessionConfig C;
	C.MissionIndex = 0;
	C.bTutorial = true;
	std::string Err;
	BH_EXPECT(S.Start(C, Err));
	World& W = S.GetWorld();
	TopDownView V;
	const MissionDef& Def = GetMission(0);
	int StepIndex = -1;
	for (size_t I = 0; I < Def.Tutorial.size(); ++I)
	{
		StepIndex = std::string(Def.Tutorial[I].Highlight) == "place:Cottage" ? static_cast<int>(I) : StepIndex;
	}
	BH_EXPECT(StepIndex >= 0);
	S.GetMission().TutorialIndex = StepIndex;
	S.Update(0.01f, &V);
	const Entity* Worker = FirstOwned(W, Archetype::Lamplighter);
	BH_EXPECT(Worker != nullptr);
	if (Worker == nullptr || StepIndex < 0)
	{
		return;
	}
	S.GetControl().SelectOne(W, Worker->Id);
	auto Lit = [&S](ActionId Id)
	{
		HudModel Hud;
		BuildHudModel(S, Hud);
		for (const ActionButton& B : Hud.Actions)
		{
			if (B.Id == Id)
			{
				return B.bHighlight;
			}
		}
		return false;
	};
	BH_EXPECT_MSG(Lit(ActionId(ActionKind::BuildMenu)), "Build is not lit");
	S.ExecuteAction(ActionId(ActionKind::BuildMenu));
	BH_EXPECT_MSG(Lit(ActionId(ActionKind::PlaceBuilding, static_cast<int>(Archetype::Cottage))), "Cottage is not lit");
	S.ExecuteAction(ActionId(ActionKind::PlaceBuilding, static_cast<int>(Archetype::Cottage)));
	BH_EXPECT(S.GetControl().bPlacing);
	BH_EXPECT_MSG(Lit(ActionId(ActionKind::ConfirmPlacement)), "Place is not lit while placing the Cottage");
}

BH_TEST(Hud_TellsWhenTrainingStops)
{
	// Units appearing are seen and heard; the message worth sending is that a building has run
	// out of orders. Per-unit "X ready" messages were four in ten of all messages in playtests.
	Session S;
	SessionConfig C;
	C.MissionIndex = 1;
	C.bTutorial = false;
	std::string Err;
	BH_EXPECT(S.Start(C, Err));
	World& W = S.GetWorld();
	const Entity* Keep = FirstOwned(W, Archetype::Keep);
	BH_EXPECT(Keep != nullptr);
	if (Keep == nullptr)
	{
		return;
	}
	W.GetTeam(Team::Player).Res[0] = 1000;
	W.GetTeam(Team::Player).Res[1] = 1000;
	W.GetTeam(Team::Player).bIgnoreSupply = true;
	S.GetControl().SelectOne(W, Keep->Id);
	S.ExecuteAction(ActionId(ActionKind::Train, static_cast<int>(Archetype::Lamplighter)));
	S.ExecuteAction(ActionId(ActionKind::Train, static_cast<int>(Archetype::Lamplighter)));
	std::vector<std::string> Texts;
	int Trained = 0;
	std::vector<GameEvent> Events;
	for (int I = 0; I < 1200 && Trained < 2; ++I)
	{
		S.Update(World::TickSeconds, nullptr);
		S.TakeEvents(Events);
		for (const GameEvent& E : Events)
		{
			Trained += E.Type == EventType::UnitTrained ? 1 : 0;
			Notice N;
			if (MakeNotice(E, S, N))
			{
				Texts.push_back(N.Text);
			}
		}
	}
	BH_EXPECT_MSG(Trained == 2, "trained %d", Trained);
	int Done = 0;
	for (const std::string& T : Texts)
	{
		Done += T.find("finished training") != std::string::npos ? 1 : 0;
		BH_EXPECT_MSG(T.find(" ready") == std::string::npos, "per-unit message: %s", T.c_str());
	}
	BH_EXPECT_MSG(Done == 1, "%d 'finished training' messages for one emptied queue", Done);
}

BH_TEST(Hud_HotkeysAvoidCameraKeys)
{
	// On desktop W, A, S and D pan the camera, so no command may use them as its shortcut, and
	// no two commands on one card may share a key.
	Session S;
	SessionConfig C;
	C.MissionIndex = 0;
	std::string Err;
	BH_EXPECT(S.Start(C, Err));
	World& W = S.GetWorld();
	TopDownView V;
	S.Update(0.05f, &V);
	const Entity* Keep = FirstOwned(W, Archetype::Keep);
	BH_EXPECT(Keep != nullptr);
	if (Keep == nullptr)
	{
		return;
	}
	std::vector<EntityId> Army;
	const Archetype Soldiers[] = {Archetype::Shieldbearer, Archetype::Ranger, Archetype::StagRider, Archetype::Sage};
	for (Archetype A : Soldiers)
	{
		Army.push_back(W.SpawnUnit(A, Team::Player, Keep->Pos + Vec2(4.f, 0.f)));
	}
	int Checked = 0;
	auto CheckActions = [&]()
	{
		HudModel Hud;
		BuildHudModel(S, Hud);
		std::string Used;
		for (const ActionButton& B : Hud.Actions)
		{
			const char K = B.Hotkey;
			BH_EXPECT_MSG(K != 'W' && K != 'A' && K != 'S' && K != 'D', "'%s' uses %c", B.Label.c_str(), K);
			// One key, one command: a shared key would only ever reach the first button.
			BH_EXPECT_MSG(K == 0 || Used.find(K) == std::string::npos, "'%s' repeats %c", B.Label.c_str(), K);
			Used.push_back(K);
			++Checked;
		}
	};
	S.GetControl().SelectMany(W, Army); // soldiers: abilities and stop
	CheckActions();
	S.GetControl().SelectMany(W, {Keep->Id}); // keep: training
	CheckActions();
	const Entity* Worker = FirstOwned(W, Archetype::Lamplighter);
	BH_EXPECT(Worker != nullptr);
	if (Worker != nullptr)
	{
		S.GetControl().SelectMany(W, {Worker->Id}); // worker: build, gather
		CheckActions();
		S.ExecuteAction(ActionId(ActionKind::BuildMenu));
		CheckActions();
		S.ExecuteAction(ActionId(ActionKind::PlaceBuilding, static_cast<int>(Archetype::Cottage)));
		CheckActions(); // placement: confirm, cancel
	}
	BH_EXPECT(Checked >= 15);
}

BH_TEST(Session_TapsPickWhatIsDrawn)
{
	// Players tap the drawn body: a tower's top, a soldier's head. The ground under such a tap
	// lies behind the object, so picking by the ground point alone missed tall buildings.
	Session S;
	SessionConfig C;
	C.MissionIndex = 1;
	C.bTutorial = false;
	std::string Err;
	BH_EXPECT(S.Start(C, Err));
	World& W = S.GetWorld();
	ObliqueView V;
	V.Origin = Vec2(10.f, 10.f);
	S.Update(0.05f, &V);
	const EntityId Tower = W.SpawnBuilding(Archetype::Watchtower, Team::Player, Tile(30, 26), true);
	const Entity* T = W.Find(Tower);
	BH_EXPECT(T != nullptr);
	if (T == nullptr)
	{
		return;
	}
	const ModelDef& M = GetModel(Archetype::Watchtower);
	float X = 0.f;
	float Y = 0.f;
	V.WorldToScreen(T->Pos, M.Height * M.Scale * 0.9f, X, Y);
	Tap(S, V, X, Y);
	BH_EXPECT_MSG(S.GetControl().Selection.size() == 1 && S.GetControl().Selection[0] == Tower, "tapping the top of a Watchtower did not select it");

	// A soldier standing in front of the Keep: its head selects it; the Keep's top selects the Keep.
	const Entity* Keep = FirstOwned(W, Archetype::Keep);
	BH_EXPECT(Keep != nullptr);
	if (Keep == nullptr)
	{
		return;
	}
	const EntityId Guard = W.SpawnUnit(Archetype::Shieldbearer, Team::Player, Vec2(Keep->Pos.X, static_cast<float>(Keep->Rect.Y1) + 0.6f));
	S.Update(0.05f, &V);
	const Entity* G = W.Find(Guard);
	const ModelDef& GM = GetModel(Archetype::Shieldbearer);
	V.WorldToScreen(G->Pos, GM.Height * GM.Scale * 0.8f, X, Y);
	Tap(S, V, X + 6.f, Y);
	BH_EXPECT_MSG(S.GetControl().Selection.size() == 1 && S.GetControl().Selection[0] == Guard, "tapping a soldier's head did not select it");
	const ModelDef& KM = GetModel(Archetype::Keep);
	V.WorldToScreen(Keep->Pos, KM.Height * KM.Scale * 0.85f, X, Y);
	Tap(S, V, X, Y);
	BH_EXPECT_MSG(S.GetControl().Selection.size() == 1 && S.GetControl().Selection[0] == Keep->Id, "tapping the Keep's top did not select it (selected %zu, first %s)",
		S.GetControl().Selection.size(), S.GetControl().Selection.empty() ? "-" : GetDef(W.Find(S.GetControl().Selection[0])->Type).Name);
}

BH_TEST(Session_TapBesideOwnUnitKeepsTheArmy)
{
	// With soldiers selected, a tap next to (not on) one of our own units is a move there. The
	// finger-sized tolerance used to turn such taps into selecting that unit, dropping the army.
	Session S;
	SessionConfig C;
	C.MissionIndex = 1;
	C.bTutorial = false;
	std::string Err;
	BH_EXPECT(S.Start(C, Err));
	World& W = S.GetWorld();
	const GameMap& Map = W.GetMap();
	const Entity* Keep = FirstOwned(W, Archetype::Keep);
	BH_EXPECT(Keep != nullptr);
	if (Keep == nullptr)
	{
		return;
	}
	// An open 9x9 patch of ground near the Keep.
	Tile Open(-1, -1);
	for (int R = 3; R < 20 && Open.X < 0; ++R)
	{
		for (int Dy = -R; Dy <= R && Open.X < 0; ++Dy)
		{
			for (int Dx = -R; Dx <= R && Open.X < 0; ++Dx)
			{
				const Tile T(Tile::FromPos(Keep->Pos).X + Dx, Tile::FromPos(Keep->Pos).Y + Dy);
				bool bClear = true;
				for (int Y = T.Y - 4; Y <= T.Y + 4 && bClear; ++Y)
				{
					for (int X = T.X - 4; X <= T.X + 4 && bClear; ++X)
					{
						bClear = Map.IsBuildableGround(X, Y);
					}
				}
				Open = bClear ? T : Open;
			}
		}
	}
	BH_EXPECT(Open.X >= 0);
	if (Open.X < 0)
	{
		return;
	}
	const Vec2 Spot = Open.Center();
	const EntityId Worker = W.SpawnUnit(Archetype::Lamplighter, Team::Player, Spot);
	const std::vector<EntityId> Army = {W.SpawnUnit(Archetype::Shieldbearer, Team::Player, Spot + Vec2(-3.f, 3.f)),
		W.SpawnUnit(Archetype::Shieldbearer, Team::Player, Spot + Vec2(-2.f, 3.f))};
	TopDownView V;
	S.Update(0.05f, &V);
	const Entity* Wk = W.Find(Worker);
	const ModelDef& M = GetModel(Archetype::Lamplighter);
	const float BodyPx = MaxF(Wk->Radius, M.ShadowRadius) * M.Scale * V.Scale;
	const float X = Wk->Pos.X * V.Scale + BodyPx + 0.7f * PickRadiusScreen * V.GetScreenHeight(); // beside, not on it
	const float Y = Wk->Pos.Y * V.Scale;
	PlayerControl& Ctl = S.GetControl();
	Ctl.SelectMany(W, Army);
	Tap(S, V, X, Y);
	BH_EXPECT_MSG(Ctl.Selection.size() == 2, "the army was dropped: %zu selected", Ctl.Selection.size());
	for (EntityId Id : Army)
	{
		const Entity* E = W.Find(Id);
		BH_EXPECT_MSG(E != nullptr && (E->Order == OrderType::Move || E->Order == OrderType::AttackMove), "a soldier was not ordered to move");
	}
	// Nothing selected: the same tap is generous and picks the worker.
	Ctl.ClearSelection();
	Tap(S, V, Wk->Pos.X * V.Scale + BodyPx + 0.7f * PickRadiusScreen * V.GetScreenHeight(), Wk->Pos.Y * V.Scale);
	BH_EXPECT_MSG(Ctl.Selection.size() == 1 && Ctl.Selection[0] == Worker, "a tap beside a lone unit did not select it");
	// With the army selected, a tap on the worker itself still selects it.
	Ctl.SelectMany(W, Army);
	Tap(S, V, Wk->Pos.X * V.Scale, Wk->Pos.Y * V.Scale);
	BH_EXPECT_MSG(Ctl.Selection.size() == 1 && Ctl.Selection[0] == Worker, "a tap on our own unit did not select it");
}

BH_TEST(Session_PlacementDragKeepsTheGrab)
{
	// Dragging a building ghost moves it with the finger from wherever it was grabbed, instead of
	// jumping its centre under the fingertip (where the finger would hide it).
	Session S;
	SessionConfig C;
	C.MissionIndex = 1;
	C.bTutorial = false;
	std::string Err;
	BH_EXPECT(S.Start(C, Err));
	World& W = S.GetWorld();
	TopDownView V;
	S.Update(0.05f, &V);
	const Entity* Worker = FirstOwned(W, Archetype::Lamplighter);
	BH_EXPECT(Worker != nullptr);
	if (Worker == nullptr)
	{
		return;
	}
	S.GetControl().SelectOne(W, Worker->Id);
	S.GetControl().BeginPlacement(W, Archetype::Cottage, Worker->Pos + Vec2(3.f, 0.f));
	BH_EXPECT(S.GetControl().bPlacing);
	const TileRect Before = S.GetControl().PlacementRect();
	const float X = (static_cast<float>(Before.X0) + 0.3f) * V.Scale;
	const float Y = (static_cast<float>(Before.Y0) + 0.3f) * V.Scale;
	S.PointerDown(0, X, Y, false);
	S.Update(0.02f, &V);
	for (int I = 1; I <= 10; ++I)
	{
		S.PointerMove(0, X + 16.f * static_cast<float>(I), Y + 8.f * static_cast<float>(I)); // 4 tiles right, 2 down
		S.Update(0.02f, &V);
	}
	S.PointerUp(0, X + 160.f, Y + 80.f);
	S.Update(0.02f, &V);
	const TileRect After = S.GetControl().PlacementRect();
	BH_EXPECT_MSG(S.GetControl().bPlacing && After.X0 == Before.X0 + 4 && After.Y0 == Before.Y0 + 2, "moved by %d,%d instead of 4,2", After.X0 - Before.X0,
		After.Y0 - Before.Y0);
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

BH_TEST(Session_PinchZoomsTowardsTheFingers)
{
	// Like a map app: the ground between the fingers stays under them while zooming, so zooming
	// into a corner of the screen needs no extra drag. It used to zoom about the screen centre.
	Session S;
	SessionConfig C;
	C.MissionIndex = 2;
	C.bTutorial = false;
	std::string Err;
	BH_EXPECT(S.Start(C, Err));
	// A projector that follows the live camera: tiles shrink on screen as the camera backs off.
	class RigView : public IViewProjector
	{
	public:
		const CameraRig* Rig = nullptr;
		float Scale() const { return 800.f / Rig->Distance; }
		bool ScreenToGround(float X, float Y, Vec2& Out) const override
		{
			Out = Rig->Focus + Vec2((X - 960.f) / Scale(), (Y - 540.f) / Scale());
			return true;
		}
		bool WorldToScreen(const Vec2& P, float, float& OutX, float& OutY) const override
		{
			OutX = 960.f + (P.X - Rig->Focus.X) * Scale();
			OutY = 540.f + (P.Y - Rig->Focus.Y) * Scale();
			return true;
		}
		float GetScreenWidth() const override { return 1920.f; }
		float GetScreenHeight() const override { return 1080.f; }
	};
	RigView V;
	V.Rig = &S.GetCamera();
	S.GetCamera().Focus = Vec2(32.f, 28.f);
	S.GetCamera().Distance = S.GetCamera().TargetDistance = 30.f;
	S.Update(0.02f, &V);
	const float CX = 1250.f;
	const float CY = 700.f;
	Vec2 Before;
	V.ScreenToGround(CX, CY, Before);
	S.PointerDown(0, CX - 60.f, CY, false);
	S.PointerDown(1, CX + 60.f, CY, false);
	S.Update(0.02f, &V);
	for (int I = 1; I <= 8; ++I)
	{
		const float Half = 60.f + 15.f * static_cast<float>(I); // fingers spread: zoom in
		S.PointerMove(0, CX - Half, CY);
		S.PointerMove(1, CX + Half, CY);
		S.Update(0.02f, &V);
	}
	S.PointerUp(0, CX - 180.f, CY);
	S.PointerUp(1, CX + 180.f, CY);
	for (int I = 0; I < 30; ++I)
	{
		S.Update(0.02f, &V); // let the zoom settle
	}
	BH_EXPECT(S.GetCamera().Distance < 20.f);
	Vec2 After;
	V.ScreenToGround(CX, CY, After);
	BH_EXPECT_MSG(Vec2::Dist(Before, After) < 0.5f, "the ground under the fingers drifted %.2f tiles", Vec2::Dist(Before, After));
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
