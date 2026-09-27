// Beaconhold simulation core — player control: selection, smart commands, placement, actions.
#pragma once

#include "BhCamera.h"
#include "BhWorld.h"

#include <string>
#include <vector>

namespace bh
{
// Implemented by the presentation layer (UE): converts between screen pixels and the ground plane.
class IViewProjector
{
public:
	virtual ~IViewProjector() = default;
	virtual bool ScreenToGround(float X, float Y, Vec2& Out) const = 0;
	virtual bool WorldToScreen(const Vec2& P, float Height, float& OutX, float& OutY) const = 0;
	virtual float GetScreenWidth() const = 0;
	virtual float GetScreenHeight() const = 0;
};

enum class ActionKind : uint8_t
{
	None,
	Train,
	Research,
	BuildMenu,
	PlaceBuilding,
	Ability,
	Stop,
	ReturnCargo,
	CancelConstruction,
	CancelQueue,
	ConfirmPlacement,
	CancelPlacement,
	Back,
	SelectArmy,
	SelectIdleWorker,
	Home,
	Deselect,
};

struct ActionId
{
	ActionKind Kind = ActionKind::None;
	int Param = 0;

	ActionId() = default;
	ActionId(ActionKind InKind, int InParam = 0) : Kind(InKind), Param(InParam) {}
	bool operator==(const ActionId& O) const { return Kind == O.Kind && Param == O.Param; }
	bool operator!=(const ActionId& O) const { return !(*this == O); }
};

// Does this action match a tutorial highlight id such as "train:Lamplighter" or "army"?
bool ActionMatchesHighlight(const ActionId& A, const char* Highlight);

struct ActionButton
{
	ActionId Id;
	Icon IconId = Icon::None;
	std::string Label;
	std::string Tooltip;
	int CostSunstone = 0;
	int CostTimber = 0;
	int CostSupply = 0;
	bool bEnabled = true;
	std::string Reason;    // why it's disabled
	float Cooldown = 0.f;  // 0..1 remaining (abilities, research progress shown elsewhere)
	float Progress = -1.f; // 0..1 when something is in progress
	int Badge = 0;         // queued count / ready count
	bool bHighlight = false;
	char Hotkey = 0;
};

enum class SelectionKind : uint8_t
{
	None,
	OwnUnits,
	OwnBuilding,
	Enemy,
	Resource,
};

enum class PickKind : uint8_t
{
	None,
	Entity,
	Tree,
	BeaconSite,
};

struct PickResult
{
	PickKind Kind = PickKind::None;
	EntityId Id = NoEntity;
	Tile T;
};

// Notice codes carried by EventType::Notice (Sub).
enum class NoticeCode : int
{
	NoSoldiers = 1,
	NoIdleWorkers = 2,
	NeedWorker = 3,
	NoFreeBeaconSite = 4,
};

class PlayerControl
{
public:
	void Reset();

	std::vector<EntityId> Selection;
	bool bBuildMenu = false;
	bool bPlacing = false;
	Archetype PlaceType = Archetype::None;
	Tile PlaceTile;
	PlaceResult PlaceState = PlaceResult::Ok;
	bool bCameraMoved = false;
	std::string TutorialHighlight;

	// Remembered since the current tutorial step began, so a quick select-then-act still counts.
	uint64_t SelectionLatch = 0; // bit per Archetype
	int ArmyLatch = 0;           // most combat units selected at once
	void ResetLatches();

	SelectionKind GetSelectionKind(const World& W) const;
	void SelectOne(const World& W, EntityId Id);
	void SelectMany(const World& W, const std::vector<EntityId>& Ids);
	void ClearSelection();
	void Prune(const World& W);
	bool HasWorkers(const World& W) const;
	bool HasCombat(const World& W) const;
	bool HasOwnUnits(const World& W) const;
	EntityId GetSingleBuilding(const World& W) const;
	std::vector<EntityId> SelectedWorkers(const World& W) const;
	std::vector<EntityId> SelectedNonWorkers(const World& W) const;

	PickResult PickAt(const World& W, const Vec2& P, float Tolerance) const;

	// Touch/mouse intents.
	void TapWorld(World& W, const Vec2& P, float Tolerance, bool bDouble, const IViewProjector* View);
	void CommandAt(World& W, const Vec2& P, float Tolerance);
	void BoxSelect(World& W, float X0, float Y0, float X1, float Y1, const IViewProjector& View);
	void Execute(World& W, CameraRig& Camera, const ActionId& A);

	// Placement.
	void BeginPlacement(World& W, Archetype Type, const Vec2& Near);
	void MovePlacement(World& W, const Vec2& P);
	bool ConfirmPlacement(World& W);
	void CancelPlacement();
	TileRect PlacementRect() const;

	// UI data.
	void BuildActions(const World& W, std::vector<ActionButton>& Out) const;
	void BuildQuickBar(const World& W, std::vector<ActionButton>& Out) const;
	int CountIdleWorkers(const World& W) const;

private:
	void Latch(const World& W);
	void SmartCommand(World& W, const PickResult& Pick, const Vec2& P);
	bool SnapBeacon(const World& W, const Vec2& Near, Tile& Out) const;
	int IdleCycle = 0;
};

} // namespace bh
