// Beaconhold simulation core — gesture recognizer.
#include "BhGestures.h"

#include "BhMath.h"

namespace bh
{
void GestureRecognizer::Configure(float ScreenHeightPixels)
{
	TapSlop = MaxF(10.f, ScreenHeightPixels * 0.018f);
}

void GestureRecognizer::Reset()
{
	State = EState::Idle;
	for (Pointer& P : Pointers)
	{
		P = Pointer();
	}
	Events.clear();
}

GestureRecognizer::Pointer* GestureRecognizer::FindPointer(int Id)
{
	for (Pointer& P : Pointers)
	{
		if (P.bActive && P.Id == Id)
		{
			return &P;
		}
	}
	return nullptr;
}

int GestureRecognizer::ActiveCount() const
{
	int Count = 0;
	for (const Pointer& P : Pointers)
	{
		Count += P.bActive ? 1 : 0;
	}
	return Count;
}

void GestureRecognizer::Emit(GestureType Type, float X, float Y, float PrevX, float PrevY)
{
	GestureEvent E;
	E.Type = Type;
	E.X = X;
	E.Y = Y;
	E.PrevX = PrevX;
	E.PrevY = PrevY;
	for (const Pointer& P : Pointers)
	{
		if (P.bActive)
		{
			E.StartX = P.StartX;
			E.StartY = P.StartY;
			break;
		}
	}
	Events.push_back(E);
}

float GestureRecognizer::PinchDistance() const
{
	const float Dx = Pointers[0].X - Pointers[1].X;
	const float Dy = Pointers[0].Y - Pointers[1].Y;
	return MaxF(1.f, std::sqrt(Dx * Dx + Dy * Dy));
}

void GestureRecognizer::PinchCenter(float& OutX, float& OutY) const
{
	OutX = (Pointers[0].X + Pointers[1].X) * 0.5f;
	OutY = (Pointers[0].Y + Pointers[1].Y) * 0.5f;
}

void GestureRecognizer::BeginPinch()
{
	State = EState::Pinch;
	LastPinchDist = PinchDistance();
	PinchCenter(LastCenterX, LastCenterY);
	Emit(GestureType::PinchStart, LastCenterX, LastCenterY, LastCenterX, LastCenterY);
}

void GestureRecognizer::PointerDown(int Id, float X, float Y, double Time, bool bMouse)
{
	if (FindPointer(Id) != nullptr || ActiveCount() >= 2)
	{
		return;
	}
	Pointer* Slot = !Pointers[0].bActive ? &Pointers[0] : &Pointers[1];
	Slot->Id = Id;
	Slot->X = Slot->StartX = X;
	Slot->Y = Slot->StartY = Y;
	Slot->StartTime = Time;
	Slot->bActive = true;

	if (ActiveCount() == 1)
	{
		State = EState::Pending;
		bMouseMode = bMouse;
		VelX = VelY = 0.f;
		LastMoveTime = Time;
		return;
	}
	switch (State)
	{
	case EState::Pending:
	case EState::LongPressed:
		BeginPinch();
		break;
	case EState::Pan:
		Emit(GestureType::PanEnd, Slot->X, Slot->Y, Slot->X, Slot->Y);
		BeginPinch();
		break;
	case EState::Box:
		Emit(GestureType::BoxCancel, X, Y, X, Y);
		State = EState::WaitRelease;
		break;
	case EState::Idle:
	case EState::Pinch:
	case EState::WaitRelease:
		break;
	}
}

void GestureRecognizer::PointerMove(int Id, float X, float Y, double Time)
{
	Pointer* P = FindPointer(Id);
	if (P == nullptr)
	{
		return;
	}
	const float PrevX = P->X;
	const float PrevY = P->Y;
	P->X = X;
	P->Y = Y;
	const float Dx = X - P->StartX;
	const float Dy = Y - P->StartY;
	const float Moved = std::sqrt(Dx * Dx + Dy * Dy);

	switch (State)
	{
	case EState::Pending:
		if (Moved > TapSlop)
		{
			if (bMouseMode)
			{
				State = EState::Box;
				Emit(GestureType::BoxStart, X, Y, P->StartX, P->StartY);
				Emit(GestureType::Box, X, Y, P->StartX, P->StartY);
			}
			else
			{
				State = EState::Pan;
				Emit(GestureType::PanStart, X, Y, P->StartX, P->StartY);
				Emit(GestureType::Pan, X, Y, P->StartX, P->StartY);
				LastMoveTime = Time;
			}
		}
		break;
	case EState::LongPressed:
		if (Moved > TapSlop * 0.5f)
		{
			State = EState::Box;
			Emit(GestureType::BoxStart, X, Y, P->StartX, P->StartY);
			Emit(GestureType::Box, X, Y, P->StartX, P->StartY);
		}
		break;
	case EState::Pan:
	{
		Emit(GestureType::Pan, X, Y, PrevX, PrevY);
		const double Dt = Time - LastMoveTime;
		if (Dt > 1e-4)
		{
			const float InstX = static_cast<float>((X - PrevX) / Dt);
			const float InstY = static_cast<float>((Y - PrevY) / Dt);
			VelX = LerpF(VelX, InstX, 0.6f);
			VelY = LerpF(VelY, InstY, 0.6f);
		}
		LastMoveTime = Time;
		break;
	}
	case EState::Box:
		Emit(GestureType::Box, X, Y, P->StartX, P->StartY);
		break;
	case EState::Pinch:
	{
		if (ActiveCount() == 2)
		{
			const float Dist = PinchDistance();
			float Cx = 0.f;
			float Cy = 0.f;
			PinchCenter(Cx, Cy);
			GestureEvent E;
			E.Type = GestureType::Pinch;
			E.X = Cx;
			E.Y = Cy;
			E.PrevX = LastCenterX;
			E.PrevY = LastCenterY;
			E.Scale = Dist / LastPinchDist;
			Events.push_back(E);
			LastPinchDist = Dist;
			LastCenterX = Cx;
			LastCenterY = Cy;
		}
		break;
	}
	case EState::Idle:
	case EState::WaitRelease:
		break;
	}
}

void GestureRecognizer::PointerUp(int Id, float X, float Y, double Time)
{
	Pointer* P = FindPointer(Id);
	if (P == nullptr)
	{
		return;
	}
	P->X = X;
	P->Y = Y;
	switch (State)
	{
	case EState::Pending:
	case EState::LongPressed:
	{
		const float Dx = X - LastTapX;
		const float Dy = Y - LastTapY;
		if (Time - LastTapTime <= DoubleTapTime && std::sqrt(Dx * Dx + Dy * Dy) <= TapSlop * 3.f)
		{
			Emit(GestureType::DoubleTap, X, Y, X, Y);
			LastTapTime = -10.0;
		}
		else
		{
			Emit(GestureType::Tap, X, Y, X, Y);
			LastTapTime = Time;
			LastTapX = X;
			LastTapY = Y;
		}
		break;
	}
	case EState::Pan:
	{
		GestureEvent E;
		E.Type = GestureType::PanEnd;
		E.X = X;
		E.Y = Y;
		E.PrevX = X;
		E.PrevY = Y;
		const bool bFresh = Time - LastMoveTime < 0.08;
		E.VelX = bFresh ? VelX : 0.f;
		E.VelY = bFresh ? VelY : 0.f;
		Events.push_back(E);
		break;
	}
	case EState::Box:
		Emit(GestureType::BoxEnd, X, Y, P->StartX, P->StartY);
		break;
	case EState::Pinch:
		Emit(GestureType::PinchEnd, X, Y, X, Y);
		State = EState::WaitRelease;
		break;
	case EState::Idle:
	case EState::WaitRelease:
		break;
	}
	P->bActive = false;
	if (ActiveCount() == 0)
	{
		State = EState::Idle;
	}
}

void GestureRecognizer::Update(double Time)
{
	if (State != EState::Pending || bMouseMode || ActiveCount() != 1)
	{
		return;
	}
	for (const Pointer& P : Pointers)
	{
		if (P.bActive && Time - P.StartTime >= LongPressTime)
		{
			State = EState::LongPressed;
			Emit(GestureType::LongPress, P.X, P.Y, P.X, P.Y);
			break;
		}
	}
}

void GestureRecognizer::GetBox(float& X0, float& Y0, float& X1, float& Y1) const
{
	for (const Pointer& P : Pointers)
	{
		if (P.bActive)
		{
			X0 = P.StartX;
			Y0 = P.StartY;
			X1 = P.X;
			Y1 = P.Y;
			return;
		}
	}
	X0 = Y0 = X1 = Y1 = 0.f;
}

} // namespace bh
