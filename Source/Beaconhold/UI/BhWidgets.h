// Beaconhold — small reusable Slate building blocks shared by the HUD and the menus.
#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

#include "UI/BhStyle.h"

namespace BhUi
{
// Text with a soft shadow so it stays readable over the 3D world.
TSharedRef<SWidget> Label(const TAttribute<FText>& Text, float Size, const TAttribute<FSlateColor>& Color = FSlateColor(BhColors::Text), bool bBold = true,
	ETextJustify::Type Justify = ETextJustify::Left, float WrapAt = 0.f);

// Content on a nine-slice panel.
TSharedRef<SWidget> Panel(const FBhStyle& Style, const TSharedRef<SWidget>& Content, const FMargin& Padding = FMargin(18.f), bh::UiTex Texture = bh::UiTex::Panel);

// Text button of a fixed size. Primary buttons are gold.
TSharedRef<SWidget> TextButton(const FBhStyle& Style, const TAttribute<FText>& Text, FOnClicked OnClicked, bool bPrimary = false, float Width = 380.f, float Height = 88.f,
	const TAttribute<bool>& Enabled = true);

// Square icon button.
TSharedRef<SWidget> IconButton(const FBhStyle& Style, bh::Icon Icon, FOnClicked OnClicked, float Size = 76.f, const TAttribute<bool>& Enabled = true);

// Horizontal fill bar (0..1).
TSharedRef<SWidget> Bar(const FBhStyle& Style, const TAttribute<float>& Fraction, const TAttribute<FSlateColor>& Fill, float Width, float Height);

// Icon followed by text (resource counters, stats).
TSharedRef<SWidget> IconText(const FBhStyle& Style, bh::Icon Icon, const TAttribute<FText>& Text, float IconSize, float FontSize,
	const TAttribute<FSlateColor>& Color = FSlateColor(BhColors::Text));

// Row of three stars, the first Count lit.
TSharedRef<SWidget> Stars(const FBhStyle& Style, const TAttribute<int32>& Count, float Size);

// A full-screen dimming layer that swallows clicks (behind modal dialogs).
TSharedRef<SWidget> Dimmer(const FBhStyle& Style, float Opacity = 0.6f);
} // namespace BhUi
