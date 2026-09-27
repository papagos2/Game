// Beaconhold simulation core - HUD view-model, notices and mission summary.
// Everything the UI shows is computed here so the presentation layer only draws it.
#pragma once

#include "BhSession.h"

#include <string>
#include <vector>

namespace bh
{
struct ObjectiveLine
{
	std::string Text;
	int Progress = 0;
	int Target = 0;
	bool bDone = false;
	bool bOptional = false;
	bool bTimer = false;
};

struct QueueEntry
{
	Icon IconId = Icon::None;
	float Progress = 0.f; // front item only
	int Index = 0;
};

struct SelectionGroup
{
	Archetype Type = Archetype::None;
	Icon IconId = Icon::None;
	int Count = 0;
	float HpRatio = 1.f;
};

struct SelectionPanel
{
	SelectionKind Kind = SelectionKind::None;
	int Count = 0;
	EntityId Id = NoEntity;
	Archetype Type = Archetype::None;
	Icon IconId = Icon::None;
	Team Owner = Team::Neutral;
	std::string Name;
	std::string Status;
	std::string Description;
	float Hp = 0.f;
	float MaxHp = 0.f;
	bool bShowCombat = false;
	int Damage = 0;
	int Armor = 0;
	float Range = 0.f;
	std::string Extra; // carrying / resources left / supply
	bool bConstructing = false;
	float BuildProgress = 0.f;
	std::vector<QueueEntry> Queue;
	std::vector<SelectionGroup> Groups;
};

enum class NoticeSeverity : uint8_t
{
	Info,
	Good,
	Warning,
	Danger,
};

struct Notice
{
	std::string Text;
	NoticeSeverity Severity = NoticeSeverity::Info;
	bool bHasPos = false;
	Vec2 Pos;
	Icon IconId = Icon::None;
};

struct HudModel
{
	int Sunstone = 0;
	int Timber = 0;
	int SupplyUsed = 0;
	int SupplyCap = 0;
	bool bSupplyFull = false;
	float MissionTime = 0.f;
	float WaveCountdown = -1.f;
	float EnemyAttackIn = -1.f;
	std::string MissionTitle;
	std::vector<ObjectiveLine> Objectives;
	SelectionPanel Selection;
	std::vector<ActionButton> Actions;
	std::vector<ActionButton> QuickBar;
	bool bTutorial = false;
	std::string TutorialText;
	int TutorialStep = 0;
	int TutorialSteps = 0;
	bool bTutorialNeedsContinue = false;
	bool bPlacing = false;
	bool bPlaceValid = false;
	MissionOutcome Outcome = MissionOutcome::InProgress;
};

struct MissionSummary
{
	bool bWon = false;
	int Stars = 0;
	bool StarEarned[3] = {false, false, false};
	std::string StarText[3];
	std::string Title;
	std::string Text;
	float Time = 0.f;
	int UnitsTrained = 0;
	int UnitsLost = 0;
	int Kills = 0;
	int Gathered = 0;
};

void BuildHudModel(const Session& S, HudModel& Out);
bool MakeNotice(const GameEvent& E, const Session& S, Notice& Out);
MissionSummary BuildSummary(const Session& S);
std::string FormatTime(float Seconds);

} // namespace bh
