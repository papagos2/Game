// Beaconhold — world-space overlays.
#include "Game/BhHUD.h"

#include "Game/BhCommon.h"
#include "Game/BhDirector.h"
#include "Game/BhGameInstance.h"
#include "Game/BhPlayerController.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

namespace
{
FLinearColor HudHealthColor(float Ratio, bh::Team Owner)
{
	if (Owner == bh::Team::Enemy)
	{
		return FLinearColor(0.95f, 0.28f, 0.32f);
	}
	if (Ratio > 0.6f)
	{
		return FLinearColor(0.35f, 0.9f, 0.4f);
	}
	if (Ratio > 0.3f)
	{
		return FLinearColor(1.f, 0.78f, 0.25f);
	}
	return FLinearColor(1.f, 0.35f, 0.25f);
}
} // namespace

ABhDirector* ABhHUD::FindDirector() const
{
	const ABhPlayerController* PC = Cast<ABhPlayerController>(PlayerOwner);
	return PC != nullptr ? PC->GetDirector() : nullptr;
}

void ABhHUD::DrawHUD()
{
	Super::DrawHUD();
	ABhDirector* D = FindDirector();
	if (D == nullptr || Canvas == nullptr || !D->IsInMission())
	{
		return;
	}
	const float Scale = FMath::Max(0.5f, Canvas->ClipY / 1080.f);
	DrawBars(*D, Scale);
	DrawFloatTexts(*D, Scale);
	DrawTutorialPointer(*D, Scale);
	DrawSelectionBox(*D, Scale);
}

void ABhHUD::DrawBars(ABhDirector& D, float Scale)
{
	bool bShowAll = false;
	if (const UBhGameInstance* GI = GetGameInstance<UBhGameInstance>())
	{
		bShowAll = GI->GetUserSettings().bAlwaysShowHealth;
	}
	TArray<FBhBarInfo> Bars;
	D.GetBars(bShowAll, Bars);
	const float Height = FMath::RoundToFloat(6.f * Scale);
	for (const FBhBarInfo& Bar : Bars)
	{
		const FVector A = Project(Bar.Top);
		const FVector B = Project(Bar.Top + FVector(BhUE::TileSize * Bar.Width * 0.5f, 0.f, 0.f));
		if (A.Z <= 0.f || A.X < -50.f || A.Y < -50.f || A.X > Canvas->ClipX + 50.f || A.Y > Canvas->ClipY + 50.f)
		{
			continue;
		}
		const float HalfWidth = FMath::Clamp(static_cast<float>(B.X - A.X), 14.f * Scale, 70.f * Scale);
		const float X = static_cast<float>(A.X) - HalfWidth;
		const float Y = static_cast<float>(A.Y);
		const float W = HalfWidth * 2.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), X - 1.f, Y - 1.f, W + 2.f, Height + 2.f);
		DrawRect(HudHealthColor(Bar.HpRatio, Bar.Owner), X, Y, W * FMath::Clamp(Bar.HpRatio, 0.f, 1.f), Height);
		if (Bar.bSelected)
		{
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.35f), X, Y, W * FMath::Clamp(Bar.HpRatio, 0.f, 1.f), FMath::Max(1.f, Height * 0.35f));
		}
		if (Bar.Progress >= 0.f)
		{
			const float PY = Y + Height + 2.f;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), X - 1.f, PY - 1.f, W + 2.f, Height * 0.8f + 2.f);
			DrawRect(FLinearColor(1.f, 0.82f, 0.35f), X, PY, W * FMath::Clamp(Bar.Progress, 0.f, 1.f), Height * 0.8f);
		}
	}
}

void ABhHUD::DrawFloatTexts(ABhDirector& D, float Scale)
{
	UFont* Font = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
	if (Font == nullptr)
	{
		return;
	}
	for (const FBhFloatText& T : D.GetFloatTexts())
	{
		const FVector S = Project(T.Location);
		if (S.Z <= 0.f)
		{
			continue;
		}
		FLinearColor Color = T.Color;
		Color.A = 1.f - FMath::Clamp((T.Age / T.Life - 0.6f) / 0.4f, 0.f, 1.f);
		FCanvasTextItem Item(FVector2D(S.X, S.Y), FText::FromString(T.Text), Font, Color);
		Item.Scale = FVector2D(1.15f * Scale, 1.15f * Scale);
		Item.bCentreX = true;
		Item.bCentreY = true;
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f * Color.A));
		Canvas->DrawItem(Item);
	}
}

void ABhHUD::DrawTutorialPointer(ABhDirector& D, float Scale)
{
	FVector Top;
	if (!D.GetTutorialMarker(Top))
	{
		return;
	}
	const FVector S = Project(Top);
	if (S.Z <= 0.f)
	{
		return;
	}
	const float Time = GetWorld() != nullptr ? static_cast<float>(GetWorld()->GetRealTimeSeconds()) : 0.f;
	const float Bob = FMath::Abs(FMath::Sin(Time * 4.f)) * 14.f * Scale;
	const float X = static_cast<float>(S.X);
	const float Y = static_cast<float>(S.Y) - 40.f * Scale - Bob;
	const float Size = 18.f * Scale;
	const FLinearColor Gold(1.f, 0.82f, 0.3f, 1.f);
	const FLinearColor Shadow(0.f, 0.f, 0.f, 0.55f);
	const float Thick = 6.f * Scale;
	// A downward arrow: shaft and chevron, with a soft shadow.
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const FLinearColor& C = Pass == 0 ? Shadow : Gold;
		const float O = Pass == 0 ? 2.f * Scale : 0.f;
		DrawLine(X + O, Y - Size * 1.6f + O, X + O, Y + O, C, Thick);
		DrawLine(X - Size + O, Y - Size + O, X + O, Y + O, C, Thick);
		DrawLine(X + Size + O, Y - Size + O, X + O, Y + O, C, Thick);
	}
}

void ABhHUD::DrawSelectionBox(ABhDirector& D, float Scale)
{
	FVector2D Min;
	FVector2D Max;
	if (!D.GetSelectionBox(Min, Max))
	{
		return;
	}
	const float W = static_cast<float>(Max.X - Min.X);
	const float H = static_cast<float>(Max.Y - Min.Y);
	DrawRect(FLinearColor(0.45f, 1.f, 0.5f, 0.12f), static_cast<float>(Min.X), static_cast<float>(Min.Y), W, H);
	const FLinearColor Edge(0.55f, 1.f, 0.6f, 0.9f);
	const float T = FMath::Max(1.f, 2.f * Scale);
	const float X0 = static_cast<float>(Min.X);
	const float Y0 = static_cast<float>(Min.Y);
	const float X1 = static_cast<float>(Max.X);
	const float Y1 = static_cast<float>(Max.Y);
	DrawLine(X0, Y0, X1, Y0, Edge, T);
	DrawLine(X1, Y0, X1, Y1, Edge, T);
	DrawLine(X1, Y1, X0, Y1, Edge, T);
	DrawLine(X0, Y1, X0, Y0, Edge, T);
}
