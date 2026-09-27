// Beaconhold — UI style: brushes made from the runtime-painted textures, button styles, fonts
// and the colour set. Layout sizes are in Slate units on a 1080-high reference screen (the
// engine's DPI curve scales them to the device).
#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

#include "BhRender.h"
#include "BhTypes.h"

class UBhAssets;

namespace BhColors
{
const FLinearColor Ink(0.012f, 0.009f, 0.006f, 1.f);
const FLinearColor Text(0.96f, 0.93f, 0.86f, 1.f);
const FLinearColor TextDim(0.72f, 0.68f, 0.76f, 1.f);
const FLinearColor Gold(1.f, 0.74f, 0.25f, 1.f);
const FLinearColor GoldPale(1.f, 0.88f, 0.6f, 1.f);
const FLinearColor Good(0.45f, 0.92f, 0.5f, 1.f);
const FLinearColor Warning(1.f, 0.78f, 0.3f, 1.f);
const FLinearColor Danger(1.f, 0.38f, 0.3f, 1.f);
const FLinearColor Shadow(0.f, 0.f, 0.f, 0.75f);
const FLinearColor Sunstone(1.f, 0.78f, 0.25f, 1.f);
const FLinearColor Timber(0.88f, 0.62f, 0.38f, 1.f);
const FLinearColor Dim(0.f, 0.f, 0.f, 0.55f);
} // namespace BhColors

class FBhStyle
{
public:
	void Init(UBhAssets* Assets);
	bool IsReady() const { return bReady; }

	const FSlateBrush* Ui(bh::UiTex T) const;
	const FSlateBrush* Icon(bh::Icon I) const;
	const FSlateBrush* White() const { return &WhiteBrush; }
	const FSlateBrush* None() const { return &NoBrush; }

	const FButtonStyle& Button() const { return ButtonStyle; }
	const FButtonStyle& GoldButton() const { return GoldButtonStyle; }
	const FButtonStyle& SlotButton() const { return SlotButtonStyle; }
	const FButtonStyle& HotSlotButton() const { return HotSlotButtonStyle; }
	const FButtonStyle& FlatButton() const { return FlatButtonStyle; }

	static FSlateFontInfo Font(float Size, bool bBold = true);
	static FSlateFontInfo Title(float Size);

private:
	TArray<FSlateBrush> UiBrushes;
	TArray<FSlateBrush> IconBrushes;
	FSlateColorBrush WhiteBrush = FSlateColorBrush(FLinearColor::White);
	FSlateBrush NoBrush;
	FButtonStyle ButtonStyle;
	FButtonStyle GoldButtonStyle;
	FButtonStyle SlotButtonStyle;
	FButtonStyle HotSlotButtonStyle;
	FButtonStyle FlatButtonStyle;
	bool bReady = false;
};
