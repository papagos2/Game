// Beaconhold — reusable Slate building blocks.
#include "UI/BhWidgets.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

namespace BhUi
{
TSharedRef<SWidget> Label(const TAttribute<FText>& Text, float Size, const TAttribute<FSlateColor>& Color, bool bBold, ETextJustify::Type Justify, float WrapAt)
{
	return SNew(STextBlock)
		.Text(Text)
		.Font(FBhStyle::Font(Size, bBold))
		.ColorAndOpacity(Color)
		.ShadowOffset(FVector2D(1.5f, 1.5f))
		.ShadowColorAndOpacity(BhColors::Shadow)
		.Justification(Justify)
		.WrapTextAt(WrapAt);
}

TSharedRef<SWidget> Panel(const FBhStyle& Style, const TSharedRef<SWidget>& Content, const FMargin& Padding, bh::UiTex Texture)
{
	return SNew(SBorder)
		.BorderImage(Style.Ui(Texture))
		.Padding(Padding)
		[
			Content
		];
}

TSharedRef<SWidget> TextButton(const FBhStyle& Style, const TAttribute<FText>& Text, FOnClicked OnClicked, bool bPrimary, float Width, float Height, const TAttribute<bool>& Enabled)
{
	return SNew(SBox)
		.WidthOverride(Width)
		.HeightOverride(Height)
		[
			SNew(SButton)
			.ButtonStyle(bPrimary ? &Style.GoldButton() : &Style.Button())
			.IsFocusable(false)
			.IsEnabled(Enabled)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked(OnClicked)
			[
				SNew(STextBlock)
				.Text(Text)
				.Font(FBhStyle::Font(FMath::Clamp(Height * 0.34f, 16.f, 34.f)))
				.ColorAndOpacity(FSlateColor(bPrimary ? BhColors::Ink : BhColors::Text))
				.ShadowOffset(bPrimary ? FVector2D(0.f, 1.f) : FVector2D(1.5f, 1.5f))
				.ShadowColorAndOpacity(bPrimary ? FLinearColor(1.f, 0.95f, 0.8f, 0.5f) : BhColors::Shadow)
				.Justification(ETextJustify::Center)
			]
		];
}

TSharedRef<SWidget> IconButton(const FBhStyle& Style, bh::Icon Icon, FOnClicked OnClicked, float Size, const TAttribute<bool>& Enabled)
{
	return SNew(SBox)
		.WidthOverride(Size)
		.HeightOverride(Size)
		[
			SNew(SButton)
			.ButtonStyle(&Style.Button())
			.IsFocusable(false)
			.IsEnabled(Enabled)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.ContentPadding(FMargin(Size * 0.14f))
			.OnClicked(OnClicked)
			[
				SNew(SImage).Image(Style.Icon(Icon))
			]
		];
}

TSharedRef<SWidget> Bar(const FBhStyle& Style, const TAttribute<float>& Fraction, const TAttribute<FSlateColor>& Fill, float Width, float Height)
{
	const float Inset = FMath::Max(1.f, Height * 0.18f);
	const float Inner = Width - Inset * 2.f;
	return SNew(SBox)
		.WidthOverride(Width)
		.HeightOverride(Height)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SImage).Image(Style.Ui(bh::UiTex::BarBack))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(Inset))
			[
				SNew(SBox)
				.WidthOverride_Lambda([Fraction, Inner]() { return FOptionalSize(FMath::Max(0.f, Inner * FMath::Clamp(Fraction.Get(), 0.f, 1.f))); })
				[
					SNew(SImage)
					.Image(Style.White())
					.ColorAndOpacity(Fill)
				]
			]
		];
}

TSharedRef<SWidget> IconText(const FBhStyle& Style, bh::Icon Icon, const TAttribute<FText>& Text, float IconSize, float FontSize, const TAttribute<FSlateColor>& Color)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(IconSize)
			.HeightOverride(IconSize)
			[
				SNew(SImage).Image(Style.Icon(Icon))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(IconSize * 0.15f, 0.f, 0.f, 0.f))
		[
			Label(Text, FontSize, Color)
		];
}

TSharedRef<SWidget> Stars(const FBhStyle& Style, const TAttribute<int32>& Count, float Size)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (int32 I = 0; I < 3; ++I)
	{
		Row->AddSlot()
			.AutoWidth()
			.Padding(FMargin(Size * 0.06f, 0.f))
			[
				SNew(SBox)
				.WidthOverride(Size)
				.HeightOverride(Size)
				[
					SNew(SImage)
					.Image_Lambda([&Style, Count, I]() { return Style.Icon(Count.Get() > I ? bh::Icon::Star : bh::Icon::StarEmpty); })
				]
			];
	}
	return Row;
}

TSharedRef<SWidget> Dimmer(const FBhStyle& Style, float Opacity)
{
	return SNew(SImage)
		.Image(Style.White())
		.ColorAndOpacity(FSlateColor(FLinearColor(0.02f, 0.015f, 0.03f, Opacity)));
}
} // namespace BhUi
