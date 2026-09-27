// Beaconhold — root of the UI.
#include "UI/SBhRoot.h"

#include "Game/BhCommon.h"
#include "Game/BhDirector.h"
#include "Game/BhGameInstance.h"
#include "Game/BhPlayerController.h"
#include "UI/BhWidgets.h"
#include "UI/SBhGameHud.h"
#include "UI/SBhInputLayer.h"

#include "BhData.h"
#include "BhMissions.h"
#include "BhProgress.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
constexpr float MenuButtonWidth = 460.f;
constexpr float MenuButtonHeight = 92.f;

bool RootCanQuit()
{
#if PLATFORM_IOS
	return false; // iOS apps never quit themselves
#else
	return true;
#endif
}

FText RootText(const TCHAR* S)
{
	return FText::FromString(FString(S));
}

const TCHAR* RootRoman(int32 I)
{
	static const TCHAR* Numerals[8] = {TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V"), TEXT("VI"), TEXT("VII"), TEXT("VIII")};
	return Numerals[FMath::Clamp(I, 0, 7)];
}
} // namespace

// ============================================================================ Construction

void SBhRoot::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	if (ABhPlayerController* PC = Owner.Get())
	{
		Style.Init(PC->GetAssets());
	}
	if (const UBhGameInstance* GI = GameInstance())
	{
		SelectedDifficulty = static_cast<bh::Difficulty>(FMath::Clamp(GI->GetUserSettings().LastDifficulty, 0, 2));
		bTutorialChoice = !GI->GetUserSettings().bTutorialDone;
	}

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SAssignNew(InputLayer, SBhInputLayer)
			.Owner(Owner)
			.Visibility_Lambda([this]() { return Screen == EScreen::InGame ? EVisibility::Visible : EVisibility::Collapsed; })
		]
		+ SOverlay::Slot()
		[
			SAssignNew(GameHud, SBhGameHud)
			.Owner(Owner)
			.Style(&Style)
			.OnPause(FSimpleDelegate::CreateLambda([this]() { ShowPauseMenu(); }))
			.Visibility_Lambda([this]()
			{
				if (Screen == EScreen::InGame)
				{
					return EVisibility::SelfHitTestInvisible;
				}
				return IsMissionScreen(Screen) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
		]
		+ SOverlay::Slot()
		[
			// Never blocks by itself: only the menus and dialogs placed in it take input.
			SAssignNew(ScreenHost, SBox)
			.Visibility(EVisibility::SelfHitTestInvisible)
		]
	];
	SetScreen(EScreen::MainMenu);
}

ABhDirector* SBhRoot::Director() const
{
	const ABhPlayerController* PC = Owner.Get();
	return PC != nullptr ? PC->GetDirector() : nullptr;
}

UBhGameInstance* SBhRoot::GameInstance() const
{
	const ABhPlayerController* PC = Owner.Get();
	return PC != nullptr ? PC->GetBhGameInstance() : nullptr;
}

bool SBhRoot::IsMissionScreen(EScreen S) const
{
	switch (S)
	{
	case EScreen::Briefing:
	case EScreen::InGame:
	case EScreen::Paused:
	case EScreen::Results:
		return true;
	case EScreen::Settings:
	case EScreen::Confirm:
	{
		const ABhDirector* D = Director();
		return D != nullptr && D->IsInMission();
	}
	default:
		return false;
	}
}

FOnClicked SBhRoot::Click(TFunction<void()> Action, bh::Sfx Cue)
{
	return FOnClicked::CreateLambda([this, Action, Cue]()
	{
		if (ABhPlayerController* PC = Owner.Get())
		{
			PC->PlayUi(Cue);
		}
		if (Action)
		{
			Action();
		}
		return FReply::Handled();
	});
}

void SBhRoot::SetScreen(EScreen NewScreen)
{
	Screen = NewScreen;
	if (ScreenHost.IsValid())
	{
		ScreenHost->SetContent(BuildScreen(NewScreen));
	}
	if (NewScreen == EScreen::Results)
	{
		ResultsTime = FPlatformTime::Seconds();
	}
	// Keep keyboard focus here so shortcuts keep working after clicks.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
	}
}

TSharedRef<SWidget> SBhRoot::BuildScreen(EScreen S)
{
	switch (S)
	{
	case EScreen::MainMenu:
		return BuildMainMenu();
	case EScreen::Campaign:
		return BuildCampaign();
	case EScreen::Boons:
		return BuildBoons();
	case EScreen::Settings:
		return BuildSettings();
	case EScreen::About:
		return BuildAbout();
	case EScreen::Briefing:
		return BuildBriefing();
	case EScreen::Paused:
		return BuildPause();
	case EScreen::Results:
		return BuildResults();
	case EScreen::Confirm:
		return BuildConfirm();
	case EScreen::InGame:
		break;
	}
	return SNullWidget::NullWidget;
}

// ============================================================================ Layout helpers

TSharedRef<SWidget> SBhRoot::MenuPage(const FText& Title, const TSharedRef<SWidget>& Body, const TSharedRef<SWidget>& Footer)
{
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage).Image(Style.Ui(bh::UiTex::Vignette))
		]
		+ SOverlay::Slot()
		[
			BhUi::Dimmer(Style, 0.35f)
		]
		+ SOverlay::Slot()
		[
			SNew(SSafeZone)
			.Padding(FMargin(40.f, 30.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(Title)
					.Font(FBhStyle::Title(60.f))
					.ColorAndOpacity(FSlateColor(BhColors::Gold))
					.ShadowOffset(FVector2D(2.f, 3.f))
					.ShadowColorAndOpacity(BhColors::Shadow)
				]
				+ SVerticalBox::Slot().FillHeight(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.f, 20.f))
				[
					Body
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					Footer
				]
			]
		];
}

TSharedRef<SWidget> SBhRoot::Modal(const TSharedRef<SWidget>& Content, float Width)
{
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			BhUi::Dimmer(Style, 0.6f)
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(Width)
			[
				BhUi::Panel(Style, Content, FMargin(40.f, 32.f), bh::UiTex::PanelGold)
			]
		];
}

// ============================================================================ Main menu

TSharedRef<SWidget> SBhRoot::BuildMainMenu()
{
	UBhGameInstance* GI = GameInstance();
	const bool bSuspended = GI != nullptr && GI->HasSuspended();
	TSharedRef<SVerticalBox> Buttons = SNew(SVerticalBox);
	if (bSuspended)
	{
		Buttons->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
		[
			BhUi::TextButton(Style, RootText(TEXT("Continue")), Click([this]()
			{
				ABhPlayerController* PC = Owner.Get();
				if (PC != nullptr && PC->ContinueSuspended())
				{
					if (GameHud.IsValid())
					{
						GameHud->Reset();
					}
					if (ABhDirector* D = Director())
					{
						D->SetPaused(true);
					}
					SetScreen(EScreen::Paused);
				}
				else
				{
					SetScreen(EScreen::MainMenu);
				}
			}, bh::Sfx::UiConfirm), true, MenuButtonWidth, MenuButtonHeight)
		];
		Buttons->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 16.f))
		[
			BhUi::Label(FText::FromString(GI->GetSuspendedSummary()), 18.f, FSlateColor(BhColors::TextDim), false)
		];
	}
	Buttons->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
	[
		BhUi::TextButton(Style, RootText(TEXT("Campaign")), Click([this]() { SetScreen(EScreen::Campaign); }), !bSuspended, MenuButtonWidth, MenuButtonHeight)
	];
	Buttons->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
	[
		BhUi::TextButton(Style, RootText(TEXT("Warden Boons")), Click([this]() { SetScreen(EScreen::Boons); }), false, MenuButtonWidth, MenuButtonHeight)
	];
	Buttons->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
	[
		BhUi::TextButton(Style, RootText(TEXT("Settings")), Click([this]()
		{
			SettingsReturn = EScreen::MainMenu;
			SetScreen(EScreen::Settings);
		}), false, MenuButtonWidth, MenuButtonHeight)
	];
	if (RootCanQuit())
	{
		Buttons->AddSlot().AutoHeight()
		[
			BhUi::TextButton(Style, RootText(TEXT("Quit")), Click([this]()
			{
				if (ABhPlayerController* PC = Owner.Get())
				{
					PC->QuitGame();
				}
			}), false, MenuButtonWidth, MenuButtonHeight)
		];
	}

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage).Image(Style.Ui(bh::UiTex::Vignette))
		]
		+ SOverlay::Slot()
		[
			SNew(SSafeZone)
			.Padding(FMargin(40.f, 30.f))
			[
				SNew(SOverlay)
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.f, 70.f, 0.f, 0.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(RootText(TEXT("BEACONHOLD")))
						.Font(FBhStyle::Title(110.f))
						.ColorAndOpacity(FSlateColor(BhColors::Gold))
						.ShadowOffset(FVector2D(3.f, 4.f))
						.ShadowColorAndOpacity(BhColors::Shadow)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 6.f, 0.f, 0.f))
					[
						BhUi::Label(RootText(TEXT("Rekindle the beacons. Hold back the Gloam.")), 28.f, FSlateColor(BhColors::GoldPale), false)
					]
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.f, 200.f, 0.f, 0.f))
				[
					Buttons
				]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
				[
					SNew(SButton)
					.ButtonStyle(&Style.FlatButton())
					.IsFocusable(false)
					.ContentPadding(FMargin(12.f, 8.f))
					.OnClicked(Click([this]() { SetScreen(EScreen::About); }))
					[
						BhUi::Label(RootText(TEXT("v0.1  ·  About")), 17.f, FSlateColor(BhColors::TextDim), false)
					]
				]
			]
		];
}

// ============================================================================ Campaign

TSharedRef<SWidget> SBhRoot::BuildCampaign()
{
	UBhGameInstance* GI = GameInstance();
	const bh::CampaignProgress Progress = GI != nullptr ? GI->GetProgress() : bh::CampaignProgress();
	TSharedRef<SHorizontalBox> Cards = SNew(SHorizontalBox);
	for (int32 I = 0; I < bh::GetMissionCount(); ++I)
	{
		const bh::MissionDef& M = bh::GetMission(I);
		const bool bUnlocked = Progress.IsUnlocked(I);
		const bool bSelected = I == SelectedMission;
		const int32 Stars = Progress.BestStars[FMath::Clamp(I, 0, bh::MaxCampaignMissions - 1)];
		Cards->AddSlot()
			.AutoWidth()
			.Padding(FMargin(I > 0 ? 22.f : 0.f, 0.f, 0.f, 0.f))
			[
				SNew(SBox)
				.WidthOverride(400.f)
				.HeightOverride(330.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SButton)
						.ButtonStyle(&Style.SlotButton())
						.IsFocusable(false)
						.ContentPadding(FMargin(24.f, 20.f))
						.OnClicked(Click([this, I]()
						{
							SelectedMission = I;
							SetScreen(EScreen::Campaign);
						}, bh::Sfx::Select))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								BhUi::Label(FText::FromString(FString::Printf(TEXT("MISSION %s"), RootRoman(I))), 18.f, FSlateColor(BhColors::Gold))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))
							[
								BhUi::Label(BhUE::ToText(M.Title), 30.f, FSlateColor(bUnlocked ? BhColors::Text : BhColors::TextDim), true, ETextJustify::Left, 350.f)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))
							[
								BhUi::Label(BhUE::ToText(bUnlocked ? M.Tagline : "Win the previous mission to unlock."), 19.f, FSlateColor(BhColors::TextDim), false, ETextJustify::Left, 350.f)
							]
							+ SVerticalBox::Slot().FillHeight(1.f)
							[
								SNew(SSpacer)
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								BhUi::Stars(Style, Stars, 40.f)
							]
						]
					]
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, 18.f, 18.f, 0.f))
					[
						SNew(SBox)
						.WidthOverride(48.f)
						.HeightOverride(48.f)
						.Visibility(bUnlocked ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
						[
							SNew(SImage).Image(Style.Icon(bh::Icon::Lock))
						]
					]
					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Image(Style.Ui(bh::UiTex::Highlight))
						.Visibility(bSelected ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
					]
				]
			];
	}

	TSharedRef<SHorizontalBox> Difficulty = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 16.f, 0.f))
		[
			BhUi::Label(RootText(TEXT("Difficulty")), 22.f, FSlateColor(BhColors::GoldPale))
		];
	for (int32 D = 0; D < 3; ++D)
	{
		const bh::Difficulty Diff = static_cast<bh::Difficulty>(D);
		Difficulty->AddSlot()
			.AutoWidth()
			.Padding(FMargin(D > 0 ? 10.f : 0.f, 0.f, 0.f, 0.f))
			[
				BhUi::TextButton(Style, BhUE::ToText(bh::DifficultyName(Diff)), Click([this, Diff]()
				{
					SelectedDifficulty = Diff;
					SetScreen(EScreen::Campaign);
				}), Diff == SelectedDifficulty, 170.f, 66.f)
			];
	}
	if (SelectedMission == 0)
	{
		Difficulty->AddSlot()
			.AutoWidth()
			.Padding(FMargin(34.f, 0.f, 0.f, 0.f))
			[
				BhUi::TextButton(Style, bTutorialChoice ? RootText(TEXT("Tutorial: On")) : RootText(TEXT("Tutorial: Off")), Click([this]()
				{
					bTutorialChoice = !bTutorialChoice;
					SetScreen(EScreen::Campaign);
				}), bTutorialChoice, 230.f, 66.f)
			];
	}

	const bool bCanStart = Progress.IsUnlocked(SelectedMission);
	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			Cards
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 30.f, 0.f, 0.f))
		[
			Difficulty
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 18.f, 0.f, 0.f))
		[
			BhUi::Label(FText::FromString(FString::Printf(TEXT("Stars and Hard victories earn Renown for Warden Boons  (Renown: %d)"), Progress.TotalRenown())), 18.f,
				FSlateColor(BhColors::TextDim), false)
		];
	TSharedRef<SWidget> Footer = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 20.f, 0.f))
		[
			BhUi::TextButton(Style, RootText(TEXT("Back")), Click([this]() { SetScreen(EScreen::MainMenu); }), false, 300.f, 84.f)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			BhUi::TextButton(Style, RootText(TEXT("Begin")), Click([this]() { BeginMission(SelectedMission); }, bh::Sfx::UiConfirm), true, 300.f, 84.f, bCanStart)
		];
	return MenuPage(RootText(TEXT("CAMPAIGN")), Body, Footer);
}

void SBhRoot::BeginMission(int32 MissionIndex)
{
	ABhPlayerController* PC = Owner.Get();
	if (PC == nullptr)
	{
		return;
	}
	const bool bTutorial = MissionIndex == 0 && bTutorialChoice;
	if (!PC->StartMission(MissionIndex, SelectedDifficulty, bTutorial))
	{
		return;
	}
	if (GameHud.IsValid())
	{
		GameHud->Reset();
	}
	if (ABhDirector* D = Director())
	{
		D->SetPaused(true); // until the briefing is dismissed
	}
	SetScreen(EScreen::Briefing);
}

// ============================================================================ Boons

TSharedRef<SWidget> SBhRoot::BuildBoons()
{
	UBhGameInstance* GI = GameInstance();
	const bh::CampaignProgress Progress = GI != nullptr ? GI->GetProgress() : bh::CampaignProgress();
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	for (int32 B = 0; B < bh::NumBoons; ++B)
	{
		const bh::Boon Boon = static_cast<bh::Boon>(B);
		const bh::BoonDef& Def = bh::GetBoonDef(Boon);
		const int32 Rank = Progress.BoonRanks[B];
		const bool bMax = Rank >= bh::MaxBoonRank;
		TSharedRef<SHorizontalBox> Pips = SNew(SHorizontalBox);
		for (int32 P = 0; P < bh::MaxBoonRank; ++P)
		{
			Pips->AddSlot()
				.AutoWidth()
				.Padding(FMargin(4.f, 0.f))
				[
					SNew(SBox)
					.WidthOverride(22.f)
					.HeightOverride(22.f)
					[
						SNew(SImage)
						.Image(Style.Ui(bh::UiTex::Circle))
						.ColorAndOpacity(FSlateColor(P < Rank ? BhColors::Gold : FLinearColor(0.25f, 0.22f, 0.3f, 1.f)))
					]
				];
		}
		const FText ButtonText = bMax ? RootText(TEXT("Mastered")) : FText::FromString(FString::Printf(TEXT("Raise  (%d)"), bh::BoonRankCost(Rank)));
		Rows->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, B > 0 ? 12.f : 0.f, 0.f, 0.f))
			[
				SNew(SBox)
				.WidthOverride(1100.f)
				[
					BhUi::Panel(Style,
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(72.f)
							.HeightOverride(72.f)
							[
								SNew(SImage).Image(Style.Icon(Def.IconId))
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(20.f, 0.f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								BhUi::Label(BhUE::ToText(Def.Name), 26.f, FSlateColor(BhColors::GoldPale))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))
							[
								BhUi::Label(BhUE::ToText(Def.Description), 18.f, FSlateColor(BhColors::TextDim), false, ETextJustify::Left, 560.f)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 20.f, 0.f))
						[
							Pips
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							BhUi::TextButton(Style, ButtonText, Click([this, Boon]()
							{
								if (UBhGameInstance* G = GameInstance())
								{
									G->RaiseBoon(Boon);
								}
								SetScreen(EScreen::Boons);
							}, bh::Sfx::UiConfirm), !bMax && Progress.CanRaiseBoon(Boon), 230.f, 70.f, !bMax && Progress.CanRaiseBoon(Boon))
						],
						FMargin(20.f, 14.f))
				]
			];
	}
	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 18.f))
		[
			BhUi::Label(FText::FromString(FString::Printf(TEXT("Renown available: %d of %d"), Progress.AvailableRenown(), Progress.TotalRenown())), 24.f,
				FSlateColor(BhColors::Gold))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Rows
		];
	TSharedRef<SWidget> Footer = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 20.f, 0.f))
		[
			BhUi::TextButton(Style, RootText(TEXT("Back")), Click([this]() { SetScreen(EScreen::MainMenu); }), false, 300.f, 84.f)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			BhUi::TextButton(Style, RootText(TEXT("Refund all")), Click([this]()
			{
				if (UBhGameInstance* G = GameInstance())
				{
					G->ResetBoons();
				}
				SetScreen(EScreen::Boons);
			}), false, 300.f, 84.f, Progress.SpentRenown() > 0)
		];
	return MenuPage(RootText(TEXT("WARDEN BOONS")), Body, Footer);
}

// ============================================================================ Settings & about

TSharedRef<SWidget> SBhRoot::VolumeRow(const FText& Name, bool bMusic)
{
	auto Current = [this, bMusic]()
	{
		const UBhGameInstance* GI = GameInstance();
		if (GI == nullptr)
		{
			return 0.f;
		}
		return bMusic ? GI->GetUserSettings().MusicVolume : GI->GetUserSettings().SfxVolume;
	};
	auto Change = [this, bMusic](float Delta)
	{
		UBhGameInstance* GI = GameInstance();
		ABhPlayerController* PC = Owner.Get();
		if (GI == nullptr || PC == nullptr)
		{
			return;
		}
		FBhUserSettings S = GI->GetUserSettings();
		float& V = bMusic ? S.MusicVolume : S.SfxVolume;
		V = FMath::Clamp(FMath::RoundToFloat((V + Delta) * 10.f) / 10.f, 0.f, 1.f);
		PC->ApplySettings(S);
	};
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(260.f)
			[
				BhUi::Label(Name, 26.f, FSlateColor(BhColors::GoldPale))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			BhUi::TextButton(Style, RootText(TEXT("-")), Click([Change]() { Change(-0.1f); }), false, 76.f, 76.f)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(16.f, 0.f))
		[
			BhUi::Bar(Style, TAttribute<float>::CreateLambda(Current), FSlateColor(BhColors::Gold), 360.f, 26.f)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			BhUi::TextButton(Style, RootText(TEXT("+")), Click([Change]() { Change(0.1f); }), false, 76.f, 76.f)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(16.f, 0.f, 0.f, 0.f))
		[
			SNew(SBox)
			.WidthOverride(90.f)
			[
				BhUi::Label(TAttribute<FText>::CreateLambda([Current]() { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Current() * 100.f))); }), 22.f)
			]
		];
}

TSharedRef<SWidget> SBhRoot::BuildSettings()
{
	const UBhGameInstance* GI = GameInstance();
	const bool bAlways = GI != nullptr && GI->GetUserSettings().bAlwaysShowHealth;
	auto SetHealth = [this](bool bValue)
	{
		UBhGameInstance* G = GameInstance();
		ABhPlayerController* PC = Owner.Get();
		if (G != nullptr && PC != nullptr)
		{
			FBhUserSettings S = G->GetUserSettings();
			S.bAlwaysShowHealth = bValue;
			PC->ApplySettings(S);
		}
		SetScreen(EScreen::Settings);
	};
	const bool bInMission = SettingsReturn != EScreen::MainMenu;
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			VolumeRow(RootText(TEXT("Music")), true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 18.f, 0.f, 0.f))
		[
			VolumeRow(RootText(TEXT("Effects")), false)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 30.f, 0.f, 0.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(260.f)
				[
					BhUi::Label(RootText(TEXT("Health bars")), 26.f, FSlateColor(BhColors::GoldPale))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				BhUi::TextButton(Style, RootText(TEXT("When damaged")), Click([SetHealth]() { SetHealth(false); }), !bAlways, 250.f, 76.f)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(12.f, 0.f, 0.f, 0.f))
			[
				BhUi::TextButton(Style, RootText(TEXT("Always")), Click([SetHealth]() { SetHealth(true); }), bAlways, 250.f, 76.f)
			]
		];
	if (!bInMission)
	{
		Body->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 50.f, 0.f, 0.f))
			[
				BhUi::TextButton(Style, RootText(TEXT("Reset campaign progress")), Click([this]()
				{
					Confirm(RootText(TEXT("Erase all stars, boons and saved missions? This cannot be undone.")), [this]()
					{
						if (UBhGameInstance* G = GameInstance())
						{
							G->ResetProgress();
						}
						SetScreen(EScreen::Settings);
					});
				}), false, 460.f, 76.f)
			];
	}
	TSharedRef<SWidget> Footer = BhUi::TextButton(Style, RootText(TEXT("Back")), Click([this]() { SetScreen(SettingsReturn); }), true, 300.f, 84.f);
	return MenuPage(RootText(TEXT("SETTINGS")), Body, Footer);
}

TSharedRef<SWidget> SBhRoot::BuildAbout()
{
	const FText Text = RootText(TEXT(
		"Beaconhold is an original real-time strategy game. The Gloam has smothered the old beacons; "
		"rebuild the Wardens' holdfast, gather Sunstone and Timber, raise an army and light the beacons again.\n\n"
		"Every model, texture, icon, sound effect and melody is generated by the game's own code; "
		"no third-party art or audio is used.\n\n"
		"Touch: tap to select, tap the ground to move or act, drag to pan, pinch to zoom, "
		"hold then drag to select a group, double-tap a unit to select all of its kind.\n"
		"Mouse: left-drag to select, right-click to command, right-drag to pan, wheel to zoom, "
		"WASD or arrows to scroll, Esc to pause."));
	TSharedRef<SWidget> Body = SNew(SBox)
		.WidthOverride(1100.f)
		[
			BhUi::Panel(Style, BhUi::Label(Text, 22.f, FSlateColor(BhColors::Text), false, ETextJustify::Left, 1020.f), FMargin(36.f, 28.f))
		];
	TSharedRef<SWidget> Footer = BhUi::TextButton(Style, RootText(TEXT("Back")), Click([this]() { SetScreen(EScreen::MainMenu); }), true, 300.f, 84.f);
	return MenuPage(RootText(TEXT("ABOUT")), Body, Footer);
}

// ============================================================================ Mission dialogs

TSharedRef<SWidget> SBhRoot::BuildBriefing()
{
	const ABhDirector* D = Director();
	const bh::Session* S = D != nullptr ? D->GetSession() : nullptr;
	if (S == nullptr)
	{
		return SNullWidget::NullWidget;
	}
	const bh::MissionDef& M = S->GetMission().Def();
	TSharedRef<SVerticalBox> Objectives = SNew(SVerticalBox);
	for (const bh::ObjectiveDef& O : M.Objectives)
	{
		Objectives->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 3.f))
			[
				BhUi::Label(FText::FromString(FString::Printf(TEXT("•  %s%s"), O.bOptional ? TEXT("(Optional) ") : TEXT(""), *BhUE::ToFString(O.Text))), 20.f,
					FSlateColor(O.bOptional ? BhColors::TextDim : BhColors::Text), false, ETextJustify::Left, 900.f)
			];
	}
	TSharedRef<SWidget> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			BhUi::Label(FText::FromString(FString::Printf(TEXT("MISSION %s  ·  %s"), RootRoman(S->GetConfig().MissionIndex), *BhUE::ToFString(bh::DifficultyName(S->GetConfig().Diff)))),
				18.f, FSlateColor(BhColors::Gold))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Text(BhUE::ToText(M.Title))
			.Font(FBhStyle::Title(46.f))
			.ColorAndOpacity(FSlateColor(BhColors::GoldPale))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 14.f, 0.f, 0.f))
		[
			BhUi::Label(BhUE::ToText(M.Briefing), 21.f, FSlateColor(BhColors::Text), false, ETextJustify::Left, 900.f)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 18.f, 0.f, 6.f))
		[
			BhUi::Label(RootText(TEXT("OBJECTIVES")), 18.f, FSlateColor(BhColors::Gold))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Objectives
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 26.f, 0.f, 0.f))
		[
			BhUi::TextButton(Style, RootText(TEXT("Begin")), Click([this]()
			{
				if (ABhDirector* Dir = Director())
				{
					Dir->SetPaused(false);
				}
				SetScreen(EScreen::InGame);
			}, bh::Sfx::UiConfirm), true, 320.f, 88.f)
		];
	return Modal(Content, 1000.f);
}

void SBhRoot::ShowPauseMenu()
{
	ABhDirector* D = Director();
	if (D == nullptr || !D->IsInMission() || Screen == EScreen::Results)
	{
		return;
	}
	D->SetPaused(true);
	SetScreen(EScreen::Paused);
}

TSharedRef<SWidget> SBhRoot::BuildPause()
{
	TSharedRef<SWidget> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 24.f))
		[
			SNew(STextBlock)
			.Text(RootText(TEXT("PAUSED")))
			.Font(FBhStyle::Title(52.f))
			.ColorAndOpacity(FSlateColor(BhColors::Gold))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 14.f))
		[
			BhUi::TextButton(Style, RootText(TEXT("Resume")), Click([this]()
			{
				if (ABhDirector* D = Director())
				{
					D->SetPaused(false);
				}
				SetScreen(EScreen::InGame);
			}, bh::Sfx::UiConfirm), true, MenuButtonWidth, MenuButtonHeight)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 14.f))
		[
			BhUi::TextButton(Style, RootText(TEXT("Restart mission")), Click([this]()
			{
				Confirm(RootText(TEXT("Restart this mission from the beginning?")), [this]()
				{
					ABhPlayerController* PC = Owner.Get();
					if (PC != nullptr && PC->RestartMission())
					{
						if (GameHud.IsValid())
						{
							GameHud->Reset();
						}
						if (ABhDirector* D = Director())
						{
							D->SetPaused(true);
						}
						SetScreen(EScreen::Briefing);
					}
					else
					{
						SetScreen(EScreen::Paused);
					}
				});
			}), false, MenuButtonWidth, MenuButtonHeight)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 14.f))
		[
			BhUi::TextButton(Style, RootText(TEXT("Settings")), Click([this]()
			{
				SettingsReturn = EScreen::Paused;
				SetScreen(EScreen::Settings);
			}), false, MenuButtonWidth, MenuButtonHeight)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			BhUi::TextButton(Style, RootText(TEXT("Save & main menu")), Click([this]()
			{
				if (ABhPlayerController* PC = Owner.Get())
				{
					PC->LeaveMission();
				}
				SetScreen(EScreen::MainMenu);
			}), false, MenuButtonWidth, MenuButtonHeight)
		];
	return Modal(Content, 600.f);
}

TSharedRef<SWidget> SBhRoot::BuildResults()
{
	const bool bWon = Summary.bWon;
	const ABhDirector* D = Director();
	const int32 Mission = D != nullptr ? D->GetMissionIndex() : 0;
	const bool bHasNext = bWon && Mission + 1 < bh::GetMissionCount();

	// Stars are revealed one after another.
	TSharedRef<SHorizontalBox> Stars = SNew(SHorizontalBox);
	for (int32 I = 0; I < 3; ++I)
	{
		const bool bEarned = Summary.StarEarned[I];
		Stars->AddSlot()
			.AutoWidth()
			.Padding(FMargin(10.f, 0.f))
			[
				SNew(SBox)
				.WidthOverride(92.f)
				.HeightOverride(92.f)
				[
					SNew(SImage)
					.Image_Lambda([this, I, bEarned]()
					{
						const double Age = FPlatformTime::Seconds() - ResultsTime;
						const bool bShown = bEarned && Age > 0.5 + 0.45 * static_cast<double>(I);
						return Style.Icon(bShown ? bh::Icon::Star : bh::Icon::StarEmpty);
					})
				]
			];
	}
	TSharedRef<SVerticalBox> Criteria = SNew(SVerticalBox);
	for (int32 I = 0; I < 3; ++I)
	{
		Criteria->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 3.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(28.f)
					.HeightOverride(28.f)
					[
						SNew(SImage).Image(Style.Icon(Summary.StarEarned[I] ? bh::Icon::Confirm : bh::Icon::Cancel))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f, 0.f, 0.f, 0.f))
				[
					BhUi::Label(BhUE::ToText(Summary.StarText[I]), 20.f, FSlateColor(Summary.StarEarned[I] ? BhColors::Text : BhColors::TextDim), false)
				]
			];
	}
	const FString Stats = FString::Printf(TEXT("Time %s    ·    Trained %d    ·    Lost %d    ·    Gloam defeated %d    ·    Gathered %d"),
		*BhUE::TimeText(Summary.Time).ToString(), Summary.UnitsTrained, Summary.UnitsLost, Summary.Kills, Summary.Gathered);

	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	if (bHasNext)
	{
		Buttons->AddSlot()
			.AutoWidth()
			.Padding(FMargin(0.f, 0.f, 16.f, 0.f))
			[
				BhUi::TextButton(Style, RootText(TEXT("Next mission")), Click([this, Mission]()
				{
					SelectedMission = Mission + 1;
					BeginMission(SelectedMission);
				}, bh::Sfx::UiConfirm), true, 300.f, 84.f)
			];
	}
	Buttons->AddSlot()
		.AutoWidth()
		.Padding(FMargin(0.f, 0.f, 16.f, 0.f))
		[
			BhUi::TextButton(Style, RootText(bWon ? TEXT("Play again") : TEXT("Try again")), Click([this]()
			{
				ABhPlayerController* PC = Owner.Get();
				if (PC != nullptr && PC->RestartMission())
				{
					if (GameHud.IsValid())
					{
						GameHud->Reset();
					}
					if (ABhDirector* Dir = Director())
					{
						Dir->SetPaused(true);
					}
					SetScreen(EScreen::Briefing);
				}
			}), !bHasNext, 300.f, 84.f)
		];
	Buttons->AddSlot()
		.AutoWidth()
		[
			BhUi::TextButton(Style, RootText(TEXT("Main menu")), Click([this]()
			{
				if (ABhPlayerController* PC = Owner.Get())
				{
					PC->LeaveMission();
				}
				SetScreen(EScreen::MainMenu);
			}), false, 300.f, 84.f)
		];

	TSharedRef<SWidget> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(RootText(bWon ? TEXT("VICTORY") : TEXT("DEFEAT")))
			.Font(FBhStyle::Title(72.f))
			.ColorAndOpacity(FSlateColor(bWon ? BhColors::Gold : BhColors::Danger))
			.ShadowOffset(FVector2D(2.f, 3.f))
			.ShadowColorAndOpacity(BhColors::Shadow)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 8.f, 0.f, 0.f))
		[
			BhUi::Label(BhUE::ToText(Summary.Text), 21.f, FSlateColor(BhColors::Text), false, ETextJustify::Center, 900.f)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 20.f, 0.f, 0.f))
		[
			Stars
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 16.f, 0.f, 0.f))
		[
			Criteria
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 18.f, 0.f, 0.f))
		[
			BhUi::Label(FText::FromString(Stats), 18.f, FSlateColor(BhColors::TextDim), false)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 28.f, 0.f, 0.f))
		[
			Buttons
		];
	return Modal(Content, 1080.f);
}

void SBhRoot::Confirm(const FText& Text, TFunction<void()> Action)
{
	ConfirmText = Text;
	ConfirmAction = MoveTemp(Action);
	ConfirmReturn = Screen;
	SetScreen(EScreen::Confirm);
}

TSharedRef<SWidget> SBhRoot::BuildConfirm()
{
	TSharedRef<SWidget> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			BhUi::Label(ConfirmText, 24.f, FSlateColor(BhColors::Text), false, ETextJustify::Center, 640.f)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 30.f, 0.f, 0.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 16.f, 0.f))
			[
				BhUi::TextButton(Style, RootText(TEXT("Cancel")), Click([this]() { SetScreen(ConfirmReturn); }), false, 260.f, 80.f)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				BhUi::TextButton(Style, RootText(TEXT("Yes")), Click([this]()
				{
					TFunction<void()> Action = MoveTemp(ConfirmAction);
					ConfirmAction = nullptr;
					if (Action)
					{
						Action();
					}
				}, bh::Sfx::UiConfirm), true, 260.f, 80.f)
			]
		];
	return Modal(Content, 760.f);
}

// ============================================================================ Navigation & input

void SBhRoot::Back()
{
	switch (Screen)
	{
	case EScreen::InGame:
		ShowPauseMenu();
		break;
	case EScreen::Paused:
		if (ABhDirector* D = Director())
		{
			D->SetPaused(false);
		}
		SetScreen(EScreen::InGame);
		break;
	case EScreen::Campaign:
	case EScreen::Boons:
	case EScreen::About:
		SetScreen(EScreen::MainMenu);
		break;
	case EScreen::Settings:
		SetScreen(SettingsReturn);
		break;
	case EScreen::Confirm:
		SetScreen(ConfirmReturn);
		break;
	case EScreen::MainMenu:
	case EScreen::Briefing:
	case EScreen::Results:
		break;
	}
}

FReply SBhRoot::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Android_Back)
	{
		Back();
		return FReply::Handled();
	}
	if (Screen != EScreen::InGame)
	{
		return FReply::Unhandled();
	}
	if (Key == EKeys::Left || Key == EKeys::A)
	{
		bPanLeft = true;
	}
	else if (Key == EKeys::Right || Key == EKeys::D)
	{
		bPanRight = true;
	}
	else if (Key == EKeys::Up || Key == EKeys::W)
	{
		bPanUp = true;
	}
	else if (Key == EKeys::Down || Key == EKeys::S)
	{
		bPanDown = true;
	}
	else if (Key == EKeys::SpaceBar)
	{
		ShowPauseMenu();
	}
	else if (ABhDirector* D = Director())
	{
		// Command card shortcuts (letters shown on the buttons).
		const TCHAR Char = FChar::ToUpper(static_cast<TCHAR>(InKeyEvent.GetCharacter()));
		for (const bh::ActionButton& A : D->GetHud().Actions)
		{
			if (A.Hotkey != 0 && static_cast<TCHAR>(FChar::ToUpper(static_cast<TCHAR>(A.Hotkey))) == Char)
			{
				if (ABhPlayerController* PC = Owner.Get())
				{
					PC->PlayUi(A.bEnabled ? bh::Sfx::UiTap : bh::Sfx::UiError);
				}
				D->ExecuteAction(A.Id);
				return FReply::Handled();
			}
		}
	}
	return FReply::Handled();
}

FReply SBhRoot::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Left || Key == EKeys::A)
	{
		bPanLeft = false;
	}
	else if (Key == EKeys::Right || Key == EKeys::D)
	{
		bPanRight = false;
	}
	else if (Key == EKeys::Up || Key == EKeys::W)
	{
		bPanUp = false;
	}
	else if (Key == EKeys::Down || Key == EKeys::S)
	{
		bPanDown = false;
	}
	return FReply::Handled();
}

void SBhRoot::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	ABhDirector* D = Director();
	if (D == nullptr)
	{
		return;
	}
	if (D->IsInMission())
	{
		if (D->TakeSummary(Summary))
		{
			SetScreen(EScreen::Results);
		}
		if (Screen == EScreen::InGame)
		{
			const float Right = (bPanRight ? 1.f : 0.f) - (bPanLeft ? 1.f : 0.f);
			const float Up = (bPanUp ? 1.f : 0.f) - (bPanDown ? 1.f : 0.f);
			D->KeyboardPan(Right, Up, InDeltaTime);
		}
	}
	else if (IsMissionScreen(Screen) && Screen != EScreen::Settings && Screen != EScreen::Confirm)
	{
		// The mission is gone (e.g. it could not be restored): back to the menu.
		SetScreen(EScreen::MainMenu);
	}
}
