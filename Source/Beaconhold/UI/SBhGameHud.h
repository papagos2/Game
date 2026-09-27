// Beaconhold — in-mission HUD: resources, timer and wave warnings, notices, objectives, the
// tutorial card, the minimap, the selection panel, the command card and the quick bar.
// Built once; values update through attribute lambdas, and the few variable-size parts
// (selection details, notices) are rebuilt only when their shape changes.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "Game/BhDirector.h"
#include "UI/BhStyle.h"

#include "BhHud.h"

class ABhPlayerController;
class SBox;
class SVerticalBox;

class SBhGameHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBhGameHud) : _Style(nullptr) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ABhPlayerController>, Owner)
		SLATE_ARGUMENT(const FBhStyle*, Style)
		SLATE_EVENT(FSimpleDelegate, OnPause)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	// Forget transient state (a new mission started).
	void Reset();

private:
	struct FToast
	{
		FBhNoticeEntry Entry;
		double Shown = 0.0;
	};

	ABhDirector* Director() const;
	const bh::HudModel& Hud() const;
	const bh::ActionButton* ActionAt(int32 Index) const;
	const bh::ActionButton* QuickAt(int32 Index) const;
	void Execute(const bh::ActionButton* A);

	TSharedRef<SWidget> MakeTopLeft();
	TSharedRef<SWidget> MakeTopCenter();
	TSharedRef<SWidget> MakeTopRight();
	TSharedRef<SWidget> MakeMinimap();
	TSharedRef<SWidget> MakeCommandCard();
	TSharedRef<SWidget> MakeActionSlot(int32 Index, bool bQuick);
	TSharedRef<SWidget> MakeInfoLine();
	TSharedRef<SWidget> MakeTutorial();
	TSharedRef<SWidget> MakeObjectives();
	TSharedRef<SWidget> BuildSelection();
	TSharedRef<SWidget> BuildToasts();
	FText WaveText() const;
	uint64 SelectionSignature() const;

	TWeakObjectPtr<ABhPlayerController> Owner;
	const FBhStyle* Style = nullptr;
	FSimpleDelegate OnPause;

	TSharedPtr<SBox> SelectionHost;
	TSharedPtr<SBox> ToastHost;
	uint64 LastSelectionSignature = ~0ull;
	TArray<FToast> Toasts;
	FText Banner;
	double BannerTime = -100.0;
	int32 InfoIndex = -1;       // action whose description the info line shows
	double InfoTime = -100.0;
	bool bInfoHover = false;
	bool bObjectivesOpen = true;
	FSlateBrush MinimapBrush;
	TWeakObjectPtr<UTexture2D> MinimapTexture;
	static const bh::HudModel EmptyHud;
};
