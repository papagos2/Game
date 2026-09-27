// Beaconhold — full-screen input layer under the HUD. Touches and mouse presses that no HUD
// widget takes land here and are forwarded (in viewport pixels) to the simulation's gesture
// recognizer: tap, double tap, pan, pinch, long-press box select; on desktop left-drag box
// select, right-click command, right-drag pan and the wheel to zoom.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ABhDirector;
class ABhPlayerController;

class SBhInputLayer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBhInputLayer) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ABhPlayerController>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	ABhDirector* GetDirector() const;
	static FVector2D ToPixels(const FGeometry& MyGeometry, const FPointerEvent& Event);

	TWeakObjectPtr<ABhPlayerController> Owner;
	bool bLeftDown = false;
	bool bRightDown = false;
	bool bRightDragged = false;
	FVector2D RightStart = FVector2D::ZeroVector;
	FVector2D RightLast = FVector2D::ZeroVector;
	FVector2D LeftLast = FVector2D::ZeroVector;
	TMap<int32, FVector2D> TouchLast; // last position of each active finger
};
