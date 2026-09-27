// Beaconhold — root of the UI: the input layer, the in-mission HUD and every menu and dialog
// (main menu, campaign, boons, settings, about, briefing, pause, results, confirmations).
// Keyboard shortcuts for desktop are handled here too.
#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "Widgets/SCompoundWidget.h"

#include "UI/BhStyle.h"

#include "BhHud.h"
#include "BhSynth.h"

class ABhDirector;
class ABhPlayerController;
class SBhGameHud;
class SBhInputLayer;
class SBox;
class UBhGameInstance;

class SBhRoot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBhRoot) : _Owner(nullptr) {}
		SLATE_ARGUMENT(ABhPlayerController*, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	// Pauses the mission and shows the pause menu (also used when the app goes to background).
	void ShowPauseMenu();

private:
	enum class EScreen : uint8
	{
		MainMenu,
		Campaign,
		Boons,
		Settings,
		About,
		Briefing,
		InGame,
		Paused,
		Results,
		Confirm,
	};

	void SetScreen(EScreen NewScreen);
	TSharedRef<SWidget> BuildScreen(EScreen S);
	TSharedRef<SWidget> BuildMainMenu();
	TSharedRef<SWidget> BuildCampaign();
	TSharedRef<SWidget> BuildBoons();
	TSharedRef<SWidget> BuildSettings();
	TSharedRef<SWidget> BuildAbout();
	TSharedRef<SWidget> BuildBriefing();
	TSharedRef<SWidget> BuildPause();
	TSharedRef<SWidget> BuildResults();
	TSharedRef<SWidget> BuildConfirm();
	TSharedRef<SWidget> MenuPage(const FText& Title, const TSharedRef<SWidget>& Body, const TSharedRef<SWidget>& Footer);
	TSharedRef<SWidget> Modal(const TSharedRef<SWidget>& Content, float Width);
	TSharedRef<SWidget> VolumeRow(const FText& Name, bool bMusic);

	FOnClicked Click(TFunction<void()> Action, bh::Sfx Cue = bh::Sfx::UiTap);
	void Confirm(const FText& Text, TFunction<void()> Action);
	void Back();
	void BeginMission(int32 MissionIndex);
	bool IsMissionScreen(EScreen S) const;

	ABhDirector* Director() const;
	UBhGameInstance* GameInstance() const;

	TWeakObjectPtr<ABhPlayerController> Owner;
	FBhStyle Style;
	TSharedPtr<SBox> ScreenHost;
	TSharedPtr<SBhGameHud> GameHud;
	TSharedPtr<SBhInputLayer> InputLayer;
	EScreen Screen = EScreen::MainMenu;
	EScreen SettingsReturn = EScreen::MainMenu;
	EScreen ConfirmReturn = EScreen::MainMenu;
	FText ConfirmText;
	TFunction<void()> ConfirmAction;
	int32 SelectedMission = 0;
	bh::Difficulty SelectedDifficulty = bh::Difficulty::Normal;
	bool bTutorialChoice = true;
	bh::MissionSummary Summary;
	double ResultsTime = 0.0;
	bool bPanLeft = false;
	bool bPanRight = false;
	bool bPanUp = false;
	bool bPanDown = false;
};
