// Beaconhold simulation core - a playable mission session.
//
// Owns the world, mission script, enemy AI, player control, camera and gesture recognizer.
// The presentation layer feeds it frame time and raw pointer input, and renders its state.
#pragma once

#include "BhAI.h"
#include "BhCamera.h"
#include "BhControl.h"
#include "BhGestures.h"
#include "BhMissions.h"
#include "BhProgress.h"
#include "BhWorld.h"

#include <string>
#include <vector>

namespace bh
{
struct SessionConfig
{
	int MissionIndex = 0;
	Difficulty Diff = Difficulty::Normal;
	int BoonRanks[NumBoons] = {};
	bool bTutorial = true;
	uint32_t Seed = 0; // 0: the mission's own seed (the game); others vary a playtest
};

class Session
{
public:
	bool Start(const SessionConfig& InConfig, std::string& OutError);
	bool IsStarted() const { return bStarted; }

	// Advances input, camera and simulation. Returns the number of simulation ticks run.
	int Update(float RealDt, const IViewProjector* View);
	// 0..1 interpolation factor between Entity::PrevPos and Entity::Pos for rendering.
	float GetAlpha() const { return Accumulator / World::TickSeconds; }

	// Raw input (screen pixels).
	void PointerDown(int Id, float X, float Y, bool bMouse);
	void PointerMove(int Id, float X, float Y);
	void PointerUp(int Id, float X, float Y);
	void MouseCommand(float X, float Y, const IViewProjector& View);
	void MouseWheel(float Delta);
	void PanByScreen(float Dx, float Dy, const IViewProjector& View);
	void PanByWorld(const Vec2& Delta);
	void ExecuteAction(const ActionId& A);
	void ContinueTutorial() { bContinuePressed = true; }
	void SkipTutorial() { Mission.SkipTutorial(); }
	void JumpCamera(const Vec2& P) { Camera.JumpTo(P); }
	bool GetSelectionBox(float& X0, float& Y0, float& X1, float& Y1) const;

	// Events produced since the last call (for sound, effects and notices).
	void TakeEvents(std::vector<GameEvent>& Out);

	World& GetWorld() { return TheWorld; }
	const World& GetWorld() const { return TheWorld; }
	MissionRuntime& GetMission() { return Mission; }
	const MissionRuntime& GetMission() const { return Mission; }
	EnemyAI& GetAI() { return AI; }
	const EnemyAI& GetAI() const { return AI; }
	PlayerControl& GetControl() { return Control; }
	const PlayerControl& GetControl() const { return Control; }
	CameraRig& GetCamera() { return Camera; }
	const CameraRig& GetCamera() const { return Camera; }
	GestureRecognizer& GetGestures() { return Gestures; }
	const SessionConfig& GetConfig() const { return Config; }
	double GetRealTime() const { return RealTime; }
	Vec2 GetKeepPos() const;

	bool bPaused = false;
	float Speed = 1.f;

	// Used by the save system to restore a suspended session.
	void RestoreConfig(const SessionConfig& InConfig) { Config = InConfig; bStarted = true; }
	void SetAccumulator(float A) { Accumulator = A; }

private:
	void RouteGesture(const GestureEvent& E, const IViewProjector& View);
	float TapTolerance(const IViewProjector& View, float X, float Y) const;
	void ApplyModifiers();

	SessionConfig Config;
	World TheWorld;
	MissionRuntime Mission;
	EnemyAI AI;
	PlayerControl Control;
	CameraRig Camera;
	GestureRecognizer Gestures;
	std::vector<GameEvent> Pending;
	float Accumulator = 0.f;
	double RealTime = 0.0;
	bool bStarted = false;
	bool bContinuePressed = false;
	bool bDraggingPlacement = false;
	float LastScreenHeight = 0.f;
};

} // namespace bh
