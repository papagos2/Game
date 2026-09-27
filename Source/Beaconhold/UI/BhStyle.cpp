// Beaconhold - UI style.
#include "UI/BhStyle.h"

#include "Game/BhAssets.h"

#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

namespace
{
FSlateBrush StyleTinted(const FSlateBrush* Source, const FLinearColor& Tint)
{
	FSlateBrush Copy = Source != nullptr ? *Source : FSlateBrush();
	Copy.TintColor = FSlateColor(Tint);
	return Copy;
}
} // namespace

void FBhStyle::Init(UBhAssets* Assets)
{
	if (bReady || Assets == nullptr)
	{
		return;
	}
	UiBrushes.SetNum(bh::NumUiTex);
	for (int32 T = 0; T < bh::NumUiTex; ++T)
	{
		FSlateBrush& B = UiBrushes[T];
		const float Margin = bh::UiTexMargin(static_cast<bh::UiTex>(T));
		B.SetResourceObject(Assets->GetUiTexture(static_cast<bh::UiTex>(T)));
		// Painted at 96 px, drawn at a 48-unit natural size so corners stay crisp.
		B.ImageSize = FVector2D(48.f, 48.f);
		B.DrawAs = Margin > 0.f ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
		B.Margin = FMargin(Margin);
		B.TintColor = FSlateColor(FLinearColor::White);
	}
	IconBrushes.SetNum(bh::NumIcons);
	for (int32 I = 0; I < bh::NumIcons; ++I)
	{
		FSlateBrush& B = IconBrushes[I];
		B.SetResourceObject(Assets->GetIcon(static_cast<bh::Icon>(I)));
		B.ImageSize = FVector2D(64.f, 64.f);
		B.DrawAs = ESlateBrushDrawType::Image;
		B.TintColor = FSlateColor(FLinearColor::White);
	}
	NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;

	const FLinearColor DisabledTint(0.55f, 0.55f, 0.6f, 0.85f);
	ButtonStyle
		.SetNormal(*Ui(bh::UiTex::Button))
		.SetHovered(*Ui(bh::UiTex::ButtonHover))
		.SetPressed(*Ui(bh::UiTex::ButtonPressed))
		.SetDisabled(StyleTinted(Ui(bh::UiTex::Button), DisabledTint))
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	GoldButtonStyle
		.SetNormal(*Ui(bh::UiTex::ButtonGold))
		.SetHovered(*Ui(bh::UiTex::ButtonGoldHover))
		.SetPressed(*Ui(bh::UiTex::ButtonGoldPressed))
		.SetDisabled(StyleTinted(Ui(bh::UiTex::ButtonGold), DisabledTint))
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	SlotButtonStyle
		.SetNormal(*Ui(bh::UiTex::Slot))
		.SetHovered(StyleTinted(Ui(bh::UiTex::Slot), FLinearColor(1.25f, 1.2f, 1.1f, 1.f)))
		.SetPressed(StyleTinted(Ui(bh::UiTex::Slot), FLinearColor(0.75f, 0.72f, 0.7f, 1.f)))
		.SetDisabled(StyleTinted(Ui(bh::UiTex::Slot), FLinearColor(0.6f, 0.6f, 0.65f, 0.9f)))
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	HotSlotButtonStyle
		.SetNormal(*Ui(bh::UiTex::SlotHot))
		.SetHovered(StyleTinted(Ui(bh::UiTex::SlotHot), FLinearColor(1.2f, 1.15f, 1.05f, 1.f)))
		.SetPressed(StyleTinted(Ui(bh::UiTex::SlotHot), FLinearColor(0.8f, 0.78f, 0.72f, 1.f)))
		.SetDisabled(StyleTinted(Ui(bh::UiTex::SlotHot), FLinearColor(0.6f, 0.6f, 0.65f, 0.9f)))
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	FlatButtonStyle
		.SetNormal(NoBrush)
		.SetHovered(StyleTinted(White(), FLinearColor(1.f, 1.f, 1.f, 0.08f)))
		.SetPressed(StyleTinted(White(), FLinearColor(1.f, 1.f, 1.f, 0.15f)))
		.SetDisabled(NoBrush)
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f));
	bReady = true;
}

const FSlateBrush* FBhStyle::Ui(bh::UiTex T) const
{
	const int32 Index = static_cast<int32>(T);
	return UiBrushes.IsValidIndex(Index) ? &UiBrushes[Index] : &NoBrush;
}

const FSlateBrush* FBhStyle::Icon(bh::Icon I) const
{
	const int32 Index = static_cast<int32>(I);
	return IconBrushes.IsValidIndex(Index) && I != bh::Icon::None ? &IconBrushes[Index] : &NoBrush;
}

FSlateFontInfo FBhStyle::Font(float Size, bool bBold)
{
	return FCoreStyle::GetDefaultFontStyle(bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")), Size);
}

FSlateFontInfo FBhStyle::Title(float Size)
{
	FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle(FName(TEXT("Bold")), Size);
	F.LetterSpacing = 160;
	F.OutlineSettings.OutlineSize = FMath::Max(1, FMath::RoundToInt(Size / 16.f));
	F.OutlineSettings.OutlineColor = FLinearColor(0.1f, 0.06f, 0.02f, 0.9f);
	return F;
}
