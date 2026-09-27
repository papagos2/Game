// Beaconhold — in-mission HUD.
#include "UI/SBhGameHud.h"

#include "Game/BhCommon.h"
#include "Game/BhPlayerController.h"
#include "UI/BhWidgets.h"

#include "BhData.h"
#include "BhRender.h"

#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
constexpr float HudPad = 18.f;
constexpr float SlotSize = 104.f;
constexpr float SlotGap = 8.f;
constexpr float QuickSize = 84.f;
constexpr int32 CardColumns = 4;
constexpr int32 CardRows = 3;
constexpr int32 MaxObjectives = 5;
constexpr int32 MaxToasts = 4;
constexpr float CardWidth = SlotSize * CardColumns + SlotGap * (CardColumns - 1) + 28.f;

bool HudIsTouchPlatform()
{
#if PLATFORM_ANDROID || PLATFORM_IOS
	return true;
#else
	return false;
#endif
}

FLinearColor HudSeverityColor(bh::NoticeSeverity S)
{
	switch (S)
	{
	case bh::NoticeSeverity::Good:
		return BhColors::Good;
	case bh::NoticeSeverity::Warning:
		return BhColors::Warning;
	case bh::NoticeSeverity::Danger:
		return BhColors::Danger;
	case bh::NoticeSeverity::Info:
		break;
	}
	return BhColors::Text;
}

float HudPulse(double Speed)
{
	return static_cast<float>(0.5 + 0.5 * FMath::Sin(FPlatformTime::Seconds() * Speed));
}

// The minimap: shows the director's minimap texture; pressing or dragging on it moves the camera.
class SBhMinimapView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBhMinimapView) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ABhPlayerController>, Owner)
		SLATE_ARGUMENT(const FSlateBrush*, Brush)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Owner = InArgs._Owner;
		ChildSlot
		[
			SNew(SImage).Image(InArgs._Brush)
		];
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		bDragging = true;
		Jump(MyGeometry, MouseEvent);
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (!bDragging)
		{
			return FReply::Unhandled();
		}
		Jump(MyGeometry, MouseEvent);
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override
	{
		return OnMouseButtonDown(MyGeometry, InTouchEvent);
	}

	virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override
	{
		return OnMouseMove(MyGeometry, InTouchEvent);
	}

	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override
	{
		return OnMouseButtonUp(MyGeometry, InTouchEvent);
	}

	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override
	{
		bDragging = false;
	}

private:
	void Jump(const FGeometry& MyGeometry, const FPointerEvent& Event)
	{
		ABhPlayerController* PC = Owner.Get();
		ABhDirector* D = PC != nullptr ? PC->GetDirector() : nullptr;
		const bh::Session* S = D != nullptr ? D->GetSession() : nullptr;
		const FVector2D Size = MyGeometry.GetLocalSize();
		if (S == nullptr || Size.X <= 0.0 || Size.Y <= 0.0)
		{
			return;
		}
		const FVector2D Local = MyGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
		D->JumpCamera(bh::MinimapToWorld(S->GetWorld().GetMap(), static_cast<float>(Local.X / Size.X), static_cast<float>(Local.Y / Size.Y)));
	}

	TWeakObjectPtr<ABhPlayerController> Owner;
	bool bDragging = false;
};
} // namespace

const bh::HudModel SBhGameHud::EmptyHud;

// ============================================================================ Construction

void SBhGameHud::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	Style = InArgs._Style;
	OnPause = InArgs._OnPause;
	MinimapBrush.DrawAs = ESlateBrushDrawType::Image;
	MinimapBrush.ImageSize = FVector2D(128.f, 128.f);

	// Full-screen containers never take input themselves, so taps between the panels reach the
	// input layer underneath (the world).
	ChildSlot
	[
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		[
			SNew(SSafeZone)
			.Visibility(EVisibility::SelfHitTestInvisible)
			.Padding(FMargin(HudPad))
			[
				SNew(SOverlay)
				.Visibility(EVisibility::SelfHitTestInvisible)
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
				[
					MakeTopLeft()
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top)
				[
					MakeTopCenter()
				]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top)
				[
					MakeTopRight()
				]
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)
				[
					MakeMinimap()
				]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom)
				[
					SAssignNew(SelectionHost, SBox)
					.Visibility(EVisibility::SelfHitTestInvisible)
				]
				+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
				[
					MakeCommandCard()
				]
			]
		]
		// Big warning banner (waves, destroyed buildings).
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 260.f))
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return Banner; })
			.Font(FBhStyle::Title(44.f))
			.Justification(ETextJustify::Center)
			.ShadowOffset(FVector2D(2.f, 2.f))
			.ShadowColorAndOpacity(BhColors::Shadow)
			.Visibility(EVisibility::HitTestInvisible)
			.ColorAndOpacity_Lambda([this]()
			{
				const float Age = static_cast<float>(FPlatformTime::Seconds() - BannerTime);
				const float Alpha = FMath::Clamp(Age < 0.25f ? Age / 0.25f : (3.2f - Age) / 0.6f, 0.f, 1.f);
				FLinearColor C = BhColors::Danger;
				C.A = Alpha;
				return FSlateColor(C);
			})
		]
	];
	SelectionHost->SetContent(BuildSelection());
}

void SBhGameHud::Reset()
{
	Toasts.Reset();
	Banner = FText::GetEmpty();
	BannerTime = -100.0;
	InfoIndex = -1;
	bInfoHover = false;
	LastSelectionSignature = ~0ull;
	if (ToastHost.IsValid())
	{
		ToastHost->SetContent(BuildToasts());
	}
}

ABhDirector* SBhGameHud::Director() const
{
	const ABhPlayerController* PC = Owner.Get();
	return PC != nullptr ? PC->GetDirector() : nullptr;
}

const bh::HudModel& SBhGameHud::Hud() const
{
	const ABhDirector* D = Director();
	return D != nullptr ? D->GetHud() : EmptyHud;
}

const bh::ActionButton* SBhGameHud::ActionAt(int32 Index) const
{
	const bh::HudModel& H = Hud();
	return Index >= 0 && Index < static_cast<int32>(H.Actions.size()) ? &H.Actions[static_cast<size_t>(Index)] : nullptr;
}

const bh::ActionButton* SBhGameHud::QuickAt(int32 Index) const
{
	const bh::HudModel& H = Hud();
	return Index >= 0 && Index < static_cast<int32>(H.QuickBar.size()) ? &H.QuickBar[static_cast<size_t>(Index)] : nullptr;
}

void SBhGameHud::Execute(const bh::ActionButton* A)
{
	ABhPlayerController* PC = Owner.Get();
	ABhDirector* D = Director();
	if (A == nullptr || PC == nullptr || D == nullptr)
	{
		return;
	}
	PC->PlayUi(A->bEnabled ? bh::Sfx::UiTap : bh::Sfx::UiError);
	D->ExecuteAction(A->Id);
}

// ============================================================================ Top of the screen

TSharedRef<SWidget> SBhGameHud::MakeTopLeft()
{
	const FBhStyle& S = *Style;
	TSharedRef<SWidget> Resources = BhUi::Panel(S,
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			BhUi::IconText(S, bh::Icon::Sunstone, TAttribute<FText>::CreateLambda([this]() { return FText::AsNumber(Hud().Sunstone); }), 38.f, 26.f)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(22.f, 0.f, 0.f, 0.f))
		[
			BhUi::IconText(S, bh::Icon::Timber, TAttribute<FText>::CreateLambda([this]() { return FText::AsNumber(Hud().Timber); }), 38.f, 26.f)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(22.f, 0.f, 0.f, 0.f))
		[
			BhUi::IconText(S, bh::Icon::Supply,
				TAttribute<FText>::CreateLambda([this]() { return FText::FromString(FString::Printf(TEXT("%d/%d"), Hud().SupplyUsed, Hud().SupplyCap)); }), 38.f, 26.f,
				TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Hud().bSupplyFull ? BhColors::Danger : BhColors::Text); }))
		],
		FMargin(18.f, 10.f));

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			Resources
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 0.f))
		[
			MakeTutorial()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 0.f))
		[
			MakeObjectives()
		];
}

FText SBhGameHud::WaveText() const
{
	const bh::HudModel& H = Hud();
	float Next = -1.f;
	if (H.WaveCountdown > 0.f)
	{
		Next = H.WaveCountdown;
	}
	if (H.EnemyAttackIn > 0.f && (Next < 0.f || H.EnemyAttackIn < Next))
	{
		Next = H.EnemyAttackIn;
	}
	if (Next < 0.f || Next > 600.f)
	{
		return FText::GetEmpty();
	}
	return FText::FromString(FString::Printf(TEXT("Gloam in %s"), *BhUE::TimeText(Next).ToString()));
}

TSharedRef<SWidget> SBhGameHud::MakeTopCenter()
{
	const FBhStyle& S = *Style;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			BhUi::Panel(S,
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					BhUi::Label(TAttribute<FText>::CreateLambda([this]() { return BhUE::TimeText(Hud().MissionTime); }), 24.f)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(18.f, 0.f, 0.f, 0.f))
				[
					SNew(SBox)
					.Visibility_Lambda([this]() { return WaveText().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
					[
						BhUi::Label(TAttribute<FText>::CreateLambda([this]() { return WaveText(); }), 22.f,
							TAttribute<FSlateColor>::CreateLambda([this]()
							{
								const bh::HudModel& H = Hud();
								const float Next = H.WaveCountdown > 0.f ? H.WaveCountdown : H.EnemyAttackIn;
								return FSlateColor(Next > 0.f && Next < 20.f ? FMath::Lerp(BhColors::Danger, BhColors::Warning, HudPulse(8.0)) : BhColors::Warning);
							}))
					]
				],
				FMargin(20.f, 8.f))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 10.f, 0.f, 0.f))
		[
			SAssignNew(ToastHost, SBox)
			.Visibility(EVisibility::HitTestInvisible)
			[
				BuildToasts()
			]
		];
}

TSharedRef<SWidget> SBhGameHud::MakeTopRight()
{
	const FBhStyle& S = *Style;
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[
			BhUi::TextButton(S, TAttribute<FText>::CreateLambda([this]()
				{
					const ABhDirector* D = Director();
					return FText::FromString(D != nullptr && D->GetSpeed() > 1.5f ? TEXT("2×") : TEXT("1×"));
				}),
				FOnClicked::CreateLambda([this]()
				{
					if (ABhDirector* D = Director())
					{
						D->SetSpeed(D->GetSpeed() > 1.5f ? 1.f : 2.f);
						if (ABhPlayerController* PC = Owner.Get())
						{
							PC->PlayUi(bh::Sfx::UiTap);
						}
					}
					return FReply::Handled();
				}),
				false, 84.f, 76.f)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[
			BhUi::IconButton(S, bh::Icon::Objectives, FOnClicked::CreateLambda([this]()
			{
				bObjectivesOpen = !bObjectivesOpen;
				if (ABhPlayerController* PC = Owner.Get())
				{
					PC->PlayUi(bh::Sfx::UiTap);
				}
				return FReply::Handled();
			}), 76.f)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			BhUi::IconButton(S, bh::Icon::Pause, FOnClicked::CreateLambda([this]()
			{
				OnPause.ExecuteIfBound();
				return FReply::Handled();
			}), 76.f)
		];
}

TSharedRef<SWidget> SBhGameHud::MakeTutorial()
{
	const FBhStyle& S = *Style;
	const FLinearColor InkSoft(0.25f, 0.18f, 0.1f, 1.f);
	return SNew(SBox)
		.WidthOverride(560.f)
		.Visibility_Lambda([this]() { return Hud().bTutorial ? EVisibility::Visible : EVisibility::Collapsed; })
		[
			BhUi::Panel(S,
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Font(FBhStyle::Font(16.f))
					.ColorAndOpacity(FSlateColor(InkSoft))
					.Text_Lambda([this]()
					{
						const bh::HudModel& H = Hud();
						return FText::FromString(FString::Printf(TEXT("TUTORIAL  ·  STEP %d OF %d"), H.TutorialStep + 1, FMath::Max(1, H.TutorialSteps)));
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))
				[
					SNew(STextBlock)
					.Font(FBhStyle::Font(21.f, false))
					.ColorAndOpacity(FSlateColor(BhColors::Ink))
					.WrapTextAt(520.f)
					.Text_Lambda([this]() { return BhUE::ToText(Hud().TutorialText); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 0.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox)
						.Visibility_Lambda([this]() { return Hud().bTutorialNeedsContinue ? EVisibility::Visible : EVisibility::Collapsed; })
						[
							BhUi::TextButton(S, FText::FromString(TEXT("Continue")), FOnClicked::CreateLambda([this]()
							{
								if (ABhDirector* D = Director())
								{
									D->ContinueTutorial();
								}
								if (ABhPlayerController* PC = Owner.Get())
								{
									PC->PlayUi(bh::Sfx::UiConfirm);
								}
								return FReply::Handled();
							}), true, 220.f, 64.f)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SSpacer)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(&S.FlatButton())
						.IsFocusable(false)
						.ContentPadding(FMargin(12.f, 8.f))
						.OnClicked_Lambda([this]()
						{
							if (ABhDirector* D = Director())
							{
								D->SkipTutorial();
							}
							return FReply::Handled();
						})
						[
							SNew(STextBlock)
							.Font(FBhStyle::Font(17.f))
							.ColorAndOpacity(FSlateColor(InkSoft))
							.Text(FText::FromString(TEXT("Skip tutorial")))
						]
					]
				],
				FMargin(22.f, 16.f), bh::UiTex::Parchment)
		];
}

TSharedRef<SWidget> SBhGameHud::MakeObjectives()
{
	const FBhStyle& S = *Style;
	TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
		[
			BhUi::Label(FText::FromString(TEXT("OBJECTIVES")), 17.f, FSlateColor(BhColors::Gold))
		];
	for (int32 I = 0; I < MaxObjectives; ++I)
	{
		auto Line = [this, I]() -> const bh::ObjectiveLine*
		{
			const bh::HudModel& H = Hud();
			return I < static_cast<int32>(H.Objectives.size()) ? &H.Objectives[static_cast<size_t>(I)] : nullptr;
		};
		Lines->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 2.f))
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([Line]() { return Line() != nullptr ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(24.f)
					.HeightOverride(24.f)
					[
						SNew(SImage).Image_Lambda([&S, Line]()
						{
							const bh::ObjectiveLine* L = Line();
							return S.Icon(L != nullptr && L->bDone ? bh::Icon::Confirm : (L != nullptr && L->bTimer ? bh::Icon::Speed : bh::Icon::StarEmpty));
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8.f, 0.f, 0.f, 0.f))
				[
					BhUi::Label(TAttribute<FText>::CreateLambda([Line]()
						{
							const bh::ObjectiveLine* L = Line();
							if (L == nullptr)
							{
								return FText::GetEmpty();
							}
							return BhUE::ToText(L->bOptional ? std::string("(Optional) ") + L->Text : L->Text);
						}),
						18.f,
						TAttribute<FSlateColor>::CreateLambda([Line]()
						{
							const bh::ObjectiveLine* L = Line();
							return FSlateColor(L != nullptr && L->bDone ? BhColors::Good : (L != nullptr && L->bOptional ? BhColors::TextDim : BhColors::Text));
						}),
						true, ETextJustify::Left, 380.f)
				]
			];
	}
	return SNew(SBox)
		.WidthOverride(430.f)
		.Visibility_Lambda([this]() { return bObjectivesOpen && !Hud().Objectives.empty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			BhUi::Panel(S, Lines, FMargin(18.f, 12.f))
		];
}

// ============================================================================ Bottom of the screen

TSharedRef<SWidget> SBhGameHud::MakeMinimap()
{
	const FBhStyle& S = *Style;
	return BhUi::Panel(S,
		SNew(SBox)
		.WidthOverride_Lambda([this]()
		{
			const ABhDirector* D = Director();
			const FIntPoint Size = D != nullptr ? D->GetMinimapSize() : FIntPoint(1, 1);
			return FOptionalSize(Size.X >= Size.Y ? 270.f : 270.f * static_cast<float>(Size.X) / static_cast<float>(FMath::Max(1, Size.Y)));
		})
		.HeightOverride_Lambda([this]()
		{
			const ABhDirector* D = Director();
			const FIntPoint Size = D != nullptr ? D->GetMinimapSize() : FIntPoint(1, 1);
			return FOptionalSize(Size.X >= Size.Y ? 270.f * static_cast<float>(Size.Y) / static_cast<float>(FMath::Max(1, Size.X)) : 270.f);
		})
		[
			SNew(SBhMinimapView)
			.Owner(Owner)
			.Brush(&MinimapBrush)
		],
		FMargin(8.f), bh::UiTex::PanelGold);
}

TSharedRef<SWidget> SBhGameHud::MakeActionSlot(int32 Index, bool bQuick)
{
	const FBhStyle& S = *Style;
	const float Size = bQuick ? QuickSize : SlotSize;
	auto Get = [this, Index, bQuick]() { return bQuick ? QuickAt(Index) : ActionAt(Index); };
	return SNew(SBox)
		.WidthOverride(Size)
		.HeightOverride(Size)
		.Visibility_Lambda([Get]() { return Get() != nullptr ? EVisibility::Visible : EVisibility::Hidden; })
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SButton)
				.ButtonStyle(&S.SlotButton())
				.IsFocusable(false)
				.ContentPadding(FMargin(Size * 0.13f))
				.OnClicked_Lambda([this, Get, Index, bQuick]()
				{
					if (!bQuick)
					{
						InfoIndex = Index;
						InfoTime = FPlatformTime::Seconds();
					}
					Execute(Get());
					return FReply::Handled();
				})
				.OnHovered_Lambda([this, Index, bQuick]()
				{
					if (!bQuick)
					{
						InfoIndex = Index;
						bInfoHover = true;
					}
				})
				.OnUnhovered_Lambda([this, bQuick]()
				{
					if (!bQuick)
					{
						bInfoHover = false;
					}
				})
				[
					SNew(SImage)
					.Image_Lambda([&S, Get]()
					{
						const bh::ActionButton* A = Get();
						return A != nullptr ? S.Icon(A->IconId) : S.None();
					})
					.ColorAndOpacity_Lambda([Get]()
					{
						const bh::ActionButton* A = Get();
						return FSlateColor(A != nullptr && !A->bEnabled ? FLinearColor(0.42f, 0.42f, 0.48f, 1.f) : FLinearColor::White);
					})
				]
			]
			// Cooldown veil, shrinking from the top.
			+ SOverlay::Slot().VAlign(VAlign_Top).Padding(FMargin(5.f))
			[
				SNew(SBox)
				.Visibility(EVisibility::HitTestInvisible)
				.HeightOverride_Lambda([Get, Size]()
				{
					const bh::ActionButton* A = Get();
					return FOptionalSize(A != nullptr ? (Size - 10.f) * FMath::Clamp(A->Cooldown, 0.f, 1.f) : 0.f);
				})
				[
					SNew(SImage)
					.Image(S.White())
					.ColorAndOpacity(FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.55f)))
				]
			]
			// Production / research progress.
			+ SOverlay::Slot().VAlign(VAlign_Bottom).Padding(FMargin(8.f, 0.f, 8.f, 8.f))
			[
				SNew(SBox)
				.Visibility_Lambda([Get]()
				{
					const bh::ActionButton* A = Get();
					return A != nullptr && A->Progress >= 0.f ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
				})
				[
					BhUi::Bar(S, TAttribute<float>::CreateLambda([Get]()
						{
							const bh::ActionButton* A = Get();
							return A != nullptr ? A->Progress : 0.f;
						}),
						FSlateColor(BhColors::Gold), Size - 16.f, 10.f)
				]
			]
			// Costs.
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(7.f, 0.f, 0.f, 4.f))
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([Get]()
				{
					const bh::ActionButton* A = Get();
					return A != nullptr && A->Progress < 0.f && (A->CostSunstone > 0 || A->CostTimber > 0) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
				})
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox)
					.Visibility_Lambda([Get]()
					{
						const bh::ActionButton* A = Get();
						return A != nullptr && A->CostSunstone > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
					})
					[
						BhUi::Label(TAttribute<FText>::CreateLambda([Get]()
							{
								const bh::ActionButton* A = Get();
								return A != nullptr ? FText::AsNumber(A->CostSunstone) : FText::GetEmpty();
							}),
							15.f, FSlateColor(BhColors::Sunstone))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f, 0.f, 0.f, 0.f))
				[
					SNew(SBox)
					.Visibility_Lambda([Get]()
					{
						const bh::ActionButton* A = Get();
						return A != nullptr && A->CostTimber > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
					})
					[
						BhUi::Label(TAttribute<FText>::CreateLambda([Get]()
							{
								const bh::ActionButton* A = Get();
								return A != nullptr ? FText::AsNumber(A->CostTimber) : FText::GetEmpty();
							}),
							15.f, FSlateColor(BhColors::Timber))
					]
				]
			]
			// Count badge.
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, 3.f, 7.f, 0.f))
			[
				SNew(SBox)
				.Visibility_Lambda([Get]()
				{
					const bh::ActionButton* A = Get();
					return A != nullptr && A->Badge > 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
				})
				[
					BhUi::Label(TAttribute<FText>::CreateLambda([Get]()
						{
							const bh::ActionButton* A = Get();
							return A != nullptr ? FText::AsNumber(A->Badge) : FText::GetEmpty();
						}),
						19.f, FSlateColor(BhColors::GoldPale))
				]
			]
			// Keyboard shortcut (desktop only).
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(7.f, 3.f, 0.f, 0.f))
			[
				SNew(SBox)
				.Visibility_Lambda([Get]()
				{
					const bh::ActionButton* A = Get();
					return !HudIsTouchPlatform() && A != nullptr && A->Hotkey != 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
				})
				[
					BhUi::Label(TAttribute<FText>::CreateLambda([Get]()
						{
							const bh::ActionButton* A = Get();
							const TCHAR Key[2] = {A != nullptr ? static_cast<TCHAR>(A->Hotkey) : TEXT(' '), 0};
							return FText::FromString(FString(Key));
						}),
						14.f, FSlateColor(BhColors::TextDim))
				]
			]
			// Tutorial highlight.
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image(S.Ui(bh::UiTex::Highlight))
				.Visibility_Lambda([Get]()
				{
					const bh::ActionButton* A = Get();
					return A != nullptr && A->bHighlight ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
				})
				.ColorAndOpacity_Lambda([]() { return FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.45f + 0.55f * HudPulse(6.0))); })
			]
		];
}

TSharedRef<SWidget> SBhGameHud::MakeInfoLine()
{
	const FBhStyle& S = *Style;
	auto Current = [this]() -> const bh::ActionButton*
	{
		const bool bRecent = FPlatformTime::Seconds() - InfoTime < 2.5;
		return (bInfoHover || bRecent) ? ActionAt(InfoIndex) : nullptr;
	};
	return SNew(SBox)
		.WidthOverride(CardWidth)
		.Visibility_Lambda([Current]() { return Current() != nullptr ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			BhUi::Panel(S,
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					BhUi::Label(TAttribute<FText>::CreateLambda([Current]()
						{
							const bh::ActionButton* A = Current();
							return A != nullptr ? BhUE::ToText(A->Label) : FText::GetEmpty();
						}),
						21.f, FSlateColor(BhColors::Gold))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))
				[
					BhUi::Label(TAttribute<FText>::CreateLambda([Current]()
						{
							const bh::ActionButton* A = Current();
							if (A == nullptr)
							{
								return FText::GetEmpty();
							}
							return BhUE::ToText(!A->bEnabled && !A->Reason.empty() ? A->Reason : A->Tooltip);
						}),
						17.f,
						TAttribute<FSlateColor>::CreateLambda([Current]()
						{
							const bh::ActionButton* A = Current();
							return FSlateColor(A != nullptr && !A->bEnabled ? BhColors::Danger : BhColors::Text);
						}),
						false, ETextJustify::Left, CardWidth - 40.f)
				],
				FMargin(18.f, 10.f))
		];
}

TSharedRef<SWidget> SBhGameHud::MakeCommandCard()
{
	const FBhStyle& S = *Style;
	TSharedRef<SVerticalBox> Grid = SNew(SVerticalBox);
	for (int32 Row = 0; Row < CardRows; ++Row)
	{
		TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox);
		for (int32 Col = 0; Col < CardColumns; ++Col)
		{
			Line->AddSlot()
				.AutoWidth()
				.Padding(FMargin(Col > 0 ? SlotGap : 0.f, 0.f, 0.f, 0.f))
				[
					MakeActionSlot(Row * CardColumns + Col, false)
				];
		}
		Grid->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, Row > 0 ? SlotGap : 0.f, 0.f, 0.f))
			[
				Line
			];
	}
	TSharedRef<SHorizontalBox> Quick = SNew(SHorizontalBox);
	for (int32 I = 0; I < 3; ++I)
	{
		Quick->AddSlot()
			.AutoWidth()
			.Padding(FMargin(I > 0 ? SlotGap : 0.f, 0.f, 0.f, 0.f))
			[
				MakeActionSlot(I, true)
			];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(FMargin(0.f, 0.f, 0.f, 10.f))
		[
			MakeInfoLine()
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(FMargin(0.f, 0.f, 0.f, 10.f))
		[
			Quick
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
		[
			BhUi::Panel(S, Grid, FMargin(14.f))
		];
}

// ============================================================================ Selection panel

uint64 SBhGameHud::SelectionSignature() const
{
	const bh::SelectionPanel& P = Hud().Selection;
	uint64 H = static_cast<uint64>(P.Kind);
	H = H * 1315423911ull + static_cast<uint64>(P.Count);
	H = H * 1315423911ull + static_cast<uint64>(P.Id);
	H = H * 1315423911ull + static_cast<uint64>(P.bConstructing ? 1 : 0);
	H = H * 1315423911ull + static_cast<uint64>(P.bShowCombat ? 1 : 0);
	for (const bh::SelectionGroup& G : P.Groups)
	{
		H = H * 1315423911ull + static_cast<uint64>(G.Type) * 131ull + static_cast<uint64>(G.Count);
	}
	return H;
}

TSharedRef<SWidget> SBhGameHud::BuildSelection()
{
	const FBhStyle& S = *Style;
	const bh::SelectionPanel& P = Hud().Selection;
	if (P.Kind == bh::SelectionKind::None || P.Count == 0)
	{
		return SNullWidget::NullWidget;
	}
	const auto Sel = [this]() -> const bh::SelectionPanel& { return Hud().Selection; };

	if (P.Count > 1 && !P.Groups.empty())
	{
		// Groups by type; tapping one keeps only that type selected.
		TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
		TSharedPtr<SHorizontalBox> Row;
		for (int32 I = 0; I < static_cast<int32>(P.Groups.size()) && I < 12; ++I)
		{
			if (I % 6 == 0)
			{
				Row = SNew(SHorizontalBox);
				Rows->AddSlot().AutoHeight().Padding(FMargin(0.f, I > 0 ? 8.f : 0.f, 0.f, 0.f))[Row.ToSharedRef()];
			}
			const bh::Archetype Type = P.Groups[static_cast<size_t>(I)].Type;
			const bh::Icon GroupIcon = P.Groups[static_cast<size_t>(I)].IconId;
			auto Group = [Sel, I]() -> const bh::SelectionGroup*
			{
				const bh::SelectionPanel& Cur = Sel();
				return I < static_cast<int32>(Cur.Groups.size()) ? &Cur.Groups[static_cast<size_t>(I)] : nullptr;
			};
			Row->AddSlot()
				.AutoWidth()
				.Padding(FMargin(I % 6 > 0 ? 8.f : 0.f, 0.f, 0.f, 0.f))
				[
					SNew(SBox)
					.WidthOverride(76.f)
					.HeightOverride(76.f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SButton)
							.ButtonStyle(&S.SlotButton())
							.IsFocusable(false)
							.ContentPadding(FMargin(9.f))
							.OnClicked_Lambda([this, Type]()
							{
								if (ABhDirector* D = Director())
								{
									D->SelectOnlyType(Type);
								}
								if (ABhPlayerController* PC = Owner.Get())
								{
									PC->PlayUi(bh::Sfx::Select);
								}
								return FReply::Handled();
							})
							[
								SNew(SImage).Image(S.Icon(GroupIcon))
							]
						]
						+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, 2.f, 6.f, 0.f))
						[
							BhUi::Label(TAttribute<FText>::CreateLambda([Group]()
								{
									const bh::SelectionGroup* G = Group();
									return G != nullptr && G->Count > 1 ? FText::AsNumber(G->Count) : FText::GetEmpty();
								}),
								18.f, FSlateColor(BhColors::GoldPale))
						]
						+ SOverlay::Slot().VAlign(VAlign_Bottom).Padding(FMargin(8.f, 0.f, 8.f, 7.f))
						[
							SNew(SBox)
							.Visibility(EVisibility::HitTestInvisible)
							[
								BhUi::Bar(S, TAttribute<float>::CreateLambda([Group]()
									{
										const bh::SelectionGroup* G = Group();
										return G != nullptr ? G->HpRatio : 0.f;
									}),
									FSlateColor(BhColors::Good), 60.f, 8.f)
							]
						]
					]
				];
		}
		return BhUi::Panel(S,
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
			[
				BhUi::Label(TAttribute<FText>::CreateLambda([Sel]() { return BhUE::ToText(Sel().Name); }), 22.f, FSlateColor(BhColors::GoldPale))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Rows
			],
			FMargin(16.f, 12.f));
	}

	// A single entity.
	const bool bBuilding = P.Kind == bh::SelectionKind::OwnBuilding;
	TSharedRef<SVerticalBox> Details = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
			[
				BhUi::Label(TAttribute<FText>::CreateLambda([Sel]() { return BhUE::ToText(Sel().Name); }), 25.f,
					TAttribute<FSlateColor>::CreateLambda([Sel]() { return FSlateColor(Sel().Owner == bh::Team::Enemy ? BhColors::Danger : BhColors::GoldPale); }))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(FMargin(12.f, 0.f, 0.f, 2.f))
			[
				BhUi::Label(TAttribute<FText>::CreateLambda([Sel]() { return BhUE::ToText(Sel().Status); }), 17.f, FSlateColor(BhColors::TextDim), false)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				BhUi::Bar(S, TAttribute<float>::CreateLambda([Sel]() { return Sel().MaxHp > 0.f ? Sel().Hp / Sel().MaxHp : 0.f; }),
					TAttribute<FSlateColor>::CreateLambda([Sel]()
					{
						const float R = Sel().MaxHp > 0.f ? Sel().Hp / Sel().MaxHp : 0.f;
						return FSlateColor(R > 0.6f ? BhColors::Good : (R > 0.3f ? BhColors::Warning : BhColors::Danger));
					}),
					300.f, 18.f)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f, 0.f, 0.f))
			[
				BhUi::Label(TAttribute<FText>::CreateLambda([Sel]()
					{
						return FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(Sel().Hp), FMath::RoundToInt(Sel().MaxHp)));
					}),
					18.f, FSlateColor(BhColors::Text), false)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([Sel]() { return Sel().bShowCombat ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().AutoWidth()
			[
				BhUi::IconText(S, bh::Icon::Sword, TAttribute<FText>::CreateLambda([Sel]() { return FText::AsNumber(Sel().Damage); }), 28.f, 19.f)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(18.f, 0.f, 0.f, 0.f))
			[
				BhUi::IconText(S, bh::Icon::Shield, TAttribute<FText>::CreateLambda([Sel]() { return FText::AsNumber(Sel().Armor); }), 28.f, 19.f)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(18.f, 0.f, 0.f, 0.f))
			[
				BhUi::IconText(S, bh::Icon::Bow, TAttribute<FText>::CreateLambda([Sel]()
					{
						return FText::FromString(FString::Printf(TEXT("%.1f"), Sel().Range));
					}),
					28.f, 19.f)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))
		[
			SNew(SBox)
			.Visibility_Lambda([Sel]() { return Sel().Extra.empty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				BhUi::Label(TAttribute<FText>::CreateLambda([Sel]() { return BhUE::ToText(Sel().Extra); }), 17.f, FSlateColor(BhColors::TextDim), false)
			]
		];

	if (bBuilding)
	{
		// Construction progress or the production queue.
		TSharedRef<SHorizontalBox> Queue = SNew(SHorizontalBox)
			.Visibility_Lambda([Sel]() { return !Sel().bConstructing && !Sel().Queue.empty() ? EVisibility::Visible : EVisibility::Collapsed; });
		for (int32 I = 0; I < bh::MaxQueueLength; ++I)
		{
			auto Entry = [Sel, I]() -> const bh::QueueEntry*
			{
				const bh::SelectionPanel& Cur = Sel();
				return I < static_cast<int32>(Cur.Queue.size()) ? &Cur.Queue[static_cast<size_t>(I)] : nullptr;
			};
			Queue->AddSlot()
				.AutoWidth()
				.Padding(FMargin(I > 0 ? 6.f : 0.f, 0.f, 0.f, 0.f))
				[
					SNew(SBox)
					.WidthOverride(58.f)
					.HeightOverride(58.f)
					.Visibility_Lambda([Entry]() { return Entry() != nullptr ? EVisibility::Visible : EVisibility::Hidden; })
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SButton)
							.ButtonStyle(&S.SlotButton())
							.IsFocusable(false)
							.ContentPadding(FMargin(7.f))
							.OnClicked_Lambda([this, Entry]()
							{
								const bh::QueueEntry* E = Entry();
								ABhDirector* D = Director();
								if (E != nullptr && D != nullptr)
								{
									D->ExecuteAction(bh::ActionId(bh::ActionKind::CancelQueue, E->Index));
									if (ABhPlayerController* PC = Owner.Get())
									{
										PC->PlayUi(bh::Sfx::UiTap);
									}
								}
								return FReply::Handled();
							})
							[
								SNew(SImage).Image_Lambda([&S, Entry]()
								{
									const bh::QueueEntry* E = Entry();
									return E != nullptr ? S.Icon(E->IconId) : S.None();
								})
							]
						]
						+ SOverlay::Slot().VAlign(VAlign_Bottom).Padding(FMargin(6.f, 0.f, 6.f, 5.f))
						[
							SNew(SBox)
							.Visibility_Lambda([I]() { return I == 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
							[
								BhUi::Bar(S, TAttribute<float>::CreateLambda([Entry]()
									{
										const bh::QueueEntry* E = Entry();
										return E != nullptr ? E->Progress : 0.f;
									}),
									FSlateColor(BhColors::Gold), 46.f, 7.f)
							]
						]
					]
				];
		}
		Details->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 10.f, 0.f, 0.f))
			[
				Queue
			];
		Details->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 10.f, 0.f, 0.f))
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([Sel]() { return Sel().bConstructing ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					BhUi::Label(FText::FromString(TEXT("Building")), 18.f, FSlateColor(BhColors::Gold))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f, 0.f, 0.f))
				[
					BhUi::Bar(S, TAttribute<float>::CreateLambda([Sel]() { return Sel().BuildProgress; }), FSlateColor(BhColors::Gold), 240.f, 16.f)
				]
			];
	}

	return BhUi::Panel(S,
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
		[
			SNew(SBox)
			.WidthOverride(104.f)
			.HeightOverride(104.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SImage).Image(S.Ui(bh::UiTex::Slot))
				]
				+ SOverlay::Slot().Padding(FMargin(12.f))
				[
					SNew(SImage).Image_Lambda([&S, Sel]() { return S.Icon(Sel().IconId); })
				]
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(18.f, 0.f, 0.f, 0.f))
		[
			SNew(SBox)
			.MinDesiredWidth(420.f)
			[
				Details
			]
		],
		FMargin(16.f, 14.f));
}

TSharedRef<SWidget> SBhGameHud::BuildToasts()
{
	const FBhStyle& S = *Style;
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	for (const FToast& T : Toasts)
	{
		const FLinearColor Color = HudSeverityColor(T.Entry.Severity);
		const bh::Icon ToastIcon = T.Entry.IconId;
		Box->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
			[
				BhUi::Panel(S,
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(34.f)
						.HeightOverride(34.f)
						.Visibility(ToastIcon != bh::Icon::None ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
						[
							SNew(SImage).Image(S.Icon(ToastIcon))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(ToastIcon != bh::Icon::None ? 10.f : 0.f, 0.f, 0.f, 0.f))
					[
						BhUi::Label(T.Entry.Text, 20.f, FSlateColor(Color))
					],
					FMargin(16.f, 8.f))
			];
	}
	return Box;
}

// ============================================================================ Per frame

void SBhGameHud::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	ABhDirector* D = Director();
	if (D == nullptr || !D->IsInMission())
	{
		return;
	}

	// Minimap texture (a new one per mission).
	UTexture2D* Minimap = D->GetMinimapTexture();
	if (Minimap != MinimapTexture.Get())
	{
		MinimapTexture = Minimap;
		MinimapBrush.SetResourceObject(Minimap);
		const FIntPoint Size = D->GetMinimapSize();
		MinimapBrush.ImageSize = FVector2D(static_cast<float>(FMath::Max(1, Size.X)), static_cast<float>(FMath::Max(1, Size.Y)));
	}

	// Selection details are rebuilt only when their shape changes.
	const uint64 Signature = SelectionSignature();
	if (Signature != LastSelectionSignature && SelectionHost.IsValid())
	{
		LastSelectionSignature = Signature;
		SelectionHost->SetContent(BuildSelection());
	}

	// Notices: new ones in, old ones out.
	const double Now = FPlatformTime::Seconds();
	bool bChanged = false;
	TArray<FBhNoticeEntry> Incoming;
	D->TakeNotices(Incoming);
	for (FBhNoticeEntry& N : Incoming)
	{
		if (N.Severity == bh::NoticeSeverity::Danger)
		{
			Banner = N.Text;
			BannerTime = Now;
		}
		FToast& T = Toasts.AddDefaulted_GetRef();
		T.Entry = N;
		T.Shown = Now;
		bChanged = true;
	}
	while (Toasts.Num() > MaxToasts)
	{
		Toasts.RemoveAt(0);
		bChanged = true;
	}
	for (int32 I = Toasts.Num() - 1; I >= 0; --I)
	{
		const double Life = Toasts[I].Entry.Severity == bh::NoticeSeverity::Danger ? 5.0 : 3.5;
		if (Now - Toasts[I].Shown > Life)
		{
			Toasts.RemoveAt(I);
			bChanged = true;
		}
	}
	if (bChanged && ToastHost.IsValid())
	{
		ToastHost->SetContent(BuildToasts());
	}
}
