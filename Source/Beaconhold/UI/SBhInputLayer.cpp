// Beaconhold — full-screen input layer.
#include "UI/SBhInputLayer.h"

#include "Game/BhDirector.h"
#include "Game/BhPlayerController.h"

#include "InputCoreTypes.h"

namespace
{
constexpr int32 MousePointerId = 0;
constexpr float RightDragThreshold = 6.f; // pixels
} // namespace

void SBhInputLayer::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
}

ABhDirector* SBhInputLayer::GetDirector() const
{
	const ABhPlayerController* PC = Owner.Get();
	return PC != nullptr ? PC->GetDirector() : nullptr;
}

FVector2D SBhInputLayer::ToPixels(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
	// The layer covers the whole viewport, so local units times the layout scale are
	// viewport pixels, which is what the camera projection expects.
	const FVector2D Local = MyGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	return Local * MyGeometry.GetAccumulatedLayoutTransform().GetScale();
}

FReply SBhInputLayer::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ABhDirector* D = GetDirector();
	if (D == nullptr || MouseEvent.IsTouchEvent())
	{
		return FReply::Unhandled();
	}
	const FVector2D P = ToPixels(MyGeometry, MouseEvent);
	const FKey Button = MouseEvent.GetEffectingButton();
	if (Button == EKeys::LeftMouseButton)
	{
		bLeftDown = true;
		LeftLast = P;
		D->PointerDown(MousePointerId, static_cast<float>(P.X), static_cast<float>(P.Y), true);
	}
	else if (Button == EKeys::RightMouseButton || Button == EKeys::MiddleMouseButton)
	{
		bRightDown = true;
		bRightDragged = false;
		RightStart = P;
		RightLast = P;
	}
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SBhInputLayer::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ABhDirector* D = GetDirector();
	if (MouseEvent.IsTouchEvent())
	{
		return FReply::Unhandled();
	}
	const FVector2D P = ToPixels(MyGeometry, MouseEvent);
	const FKey Button = MouseEvent.GetEffectingButton();
	if (Button == EKeys::LeftMouseButton && bLeftDown)
	{
		bLeftDown = false;
		if (D != nullptr)
		{
			D->PointerUp(MousePointerId, static_cast<float>(P.X), static_cast<float>(P.Y));
		}
	}
	else if ((Button == EKeys::RightMouseButton || Button == EKeys::MiddleMouseButton) && bRightDown)
	{
		bRightDown = false;
		if (D != nullptr && !bRightDragged && Button == EKeys::RightMouseButton)
		{
			D->MouseCommand(static_cast<float>(P.X), static_cast<float>(P.Y));
		}
	}
	FReply Reply = FReply::Handled();
	if (!bLeftDown && !bRightDown)
	{
		Reply.ReleaseMouseCapture();
	}
	return Reply;
}

FReply SBhInputLayer::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	ABhDirector* D = GetDirector();
	if (D == nullptr || MouseEvent.IsTouchEvent() || (!bLeftDown && !bRightDown))
	{
		return FReply::Unhandled();
	}
	const FVector2D P = ToPixels(MyGeometry, MouseEvent);
	if (bLeftDown)
	{
		LeftLast = P;
		D->PointerMove(MousePointerId, static_cast<float>(P.X), static_cast<float>(P.Y));
	}
	if (bRightDown)
	{
		if (!bRightDragged && FVector2D::Distance(P, RightStart) > RightDragThreshold)
		{
			bRightDragged = true;
		}
		if (bRightDragged)
		{
			D->PanByScreen(static_cast<float>(P.X - RightLast.X), static_cast<float>(P.Y - RightLast.Y));
		}
		RightLast = P;
	}
	return FReply::Handled();
}

FReply SBhInputLayer::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (ABhDirector* D = GetDirector())
	{
		D->MouseWheel(MouseEvent.GetWheelDelta());
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SBhInputLayer::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	ABhDirector* D = GetDirector();
	if (D == nullptr)
	{
		return FReply::Unhandled();
	}
	const FVector2D P = ToPixels(MyGeometry, InTouchEvent);
	const int32 Id = static_cast<int32>(InTouchEvent.GetPointerIndex()) + 1;
	TouchLast.Add(Id, P);
	D->PointerDown(Id, static_cast<float>(P.X), static_cast<float>(P.Y), false);
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SBhInputLayer::OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	ABhDirector* D = GetDirector();
	if (D == nullptr)
	{
		return FReply::Unhandled();
	}
	const FVector2D P = ToPixels(MyGeometry, InTouchEvent);
	const int32 Id = static_cast<int32>(InTouchEvent.GetPointerIndex()) + 1;
	TouchLast.Add(Id, P);
	D->PointerMove(Id, static_cast<float>(P.X), static_cast<float>(P.Y));
	return FReply::Handled();
}

FReply SBhInputLayer::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	const int32 Id = static_cast<int32>(InTouchEvent.GetPointerIndex()) + 1;
	TouchLast.Remove(Id);
	if (ABhDirector* D = GetDirector())
	{
		const FVector2D P = ToPixels(MyGeometry, InTouchEvent);
		D->PointerUp(Id, static_cast<float>(P.X), static_cast<float>(P.Y));
	}
	return FReply::Handled().ReleaseMouseCapture();
}

void SBhInputLayer::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	// Losing capture mid-gesture (app switch, focus change) must not leave a pointer hanging.
	ABhDirector* D = GetDirector();
	const int32 TouchId = static_cast<int32>(CaptureLostEvent.PointerIndex) + 1;
	if (const FVector2D* Last = TouchLast.Find(TouchId))
	{
		if (D != nullptr)
		{
			D->PointerUp(TouchId, static_cast<float>(Last->X), static_cast<float>(Last->Y));
		}
		TouchLast.Remove(TouchId);
		return;
	}
	if (bLeftDown && D != nullptr)
	{
		D->PointerUp(MousePointerId, static_cast<float>(LeftLast.X), static_cast<float>(LeftLast.Y));
	}
	bLeftDown = false;
	bRightDown = false;
	bRightDragged = false;
}
