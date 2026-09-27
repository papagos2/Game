// Beaconhold simulation core - RTS camera rig.
#include "BhCamera.h"

namespace bh
{
void CameraRig::Init(int MapW, int MapH, const Vec2& Start)
{
	BoundsW = static_cast<float>(MapW);
	BoundsH = static_cast<float>(MapH);
	Focus = Start;
	Distance = TargetDistance = 21.f;
	Velocity = Vec2();
	bJumping = false;
	PannedDistance = 0.f;
	Clamp();
}

void CameraRig::Pan(const Vec2& WorldDelta)
{
	bJumping = false;
	Focus += WorldDelta;
	PannedDistance += WorldDelta.Length();
	Clamp();
}

void CameraRig::Fling(const Vec2& WorldVelocity)
{
	const float MaxSpeed = 60.f;
	Velocity = WorldVelocity;
	const float Speed = Velocity.Length();
	if (Speed > MaxSpeed)
	{
		Velocity = Velocity * (MaxSpeed / Speed);
	}
	if (Speed < 1.5f)
	{
		Velocity = Vec2();
	}
}

void CameraRig::StopInertia()
{
	Velocity = Vec2();
}

void CameraRig::ZoomBy(float Factor)
{
	if (Factor <= 0.f)
	{
		return;
	}
	TargetDistance = ClampF(TargetDistance * Factor, MinDistance, MaxDistance);
}

void CameraRig::SetZoomAlpha(float Alpha)
{
	TargetDistance = LerpF(MinDistance, MaxDistance, Saturate(Alpha));
}

void CameraRig::JumpTo(const Vec2& P)
{
	bJumping = true;
	JumpTarget = P;
	Velocity = Vec2();
}

float CameraRig::ZoomAlpha() const
{
	return Saturate((Distance - MinDistance) / (MaxDistance - MinDistance));
}

void CameraRig::Update(float Dt)
{
	if (bJumping)
	{
		const float T = 1.f - std::exp(-8.f * Dt);
		Focus = Vec2::Lerp(Focus, JumpTarget, T);
		if (Vec2::Dist(Focus, JumpTarget) < 0.05f)
		{
			Focus = JumpTarget;
			bJumping = false;
		}
	}
	else if (Velocity.LengthSq() > 0.f)
	{
		Focus += Velocity * Dt;
		Velocity = Velocity * std::exp(-5.f * Dt);
		if (Velocity.LengthSq() < 0.05f)
		{
			Velocity = Vec2();
		}
	}
	Distance = LerpF(Distance, TargetDistance, 1.f - std::exp(-12.f * Dt));
	Clamp();
}

void CameraRig::Clamp()
{
	const float MarginX = 2.f;
	const float MarginTop = 3.f;
	const float MarginBottom = 1.f;
	Focus.X = BoundsW > 2.f * MarginX ? ClampF(Focus.X, MarginX, BoundsW - MarginX) : BoundsW * 0.5f;
	Focus.Y = BoundsH > MarginTop + MarginBottom ? ClampF(Focus.Y, MarginTop, BoundsH - MarginBottom) : BoundsH * 0.5f;
	TargetDistance = ClampF(TargetDistance, MinDistance, MaxDistance);
	Distance = ClampF(Distance, MinDistance * 0.9f, MaxDistance * 1.1f);
}

} // namespace bh
