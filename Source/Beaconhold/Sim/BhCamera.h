// Beaconhold simulation core — RTS camera rig (focus point + zoom distance, inertia, bounds).
// The presentation layer turns Focus/Distance/Pitch/Yaw into an actual camera transform.
#pragma once

#include "BhMath.h"

namespace bh
{
struct CameraRig
{
	// Fixed view setup (degrees). Yaw -90 looks "north" (towards -Y), so map rows read top-down.
	float Pitch = 56.f;
	float Yaw = -90.f;
	float Fov = 40.f;
	float MinDistance = 11.f;
	float MaxDistance = 36.f;

	Vec2 Focus;
	float Distance = 21.f;
	float TargetDistance = 21.f;
	Vec2 Velocity;
	bool bJumping = false;
	Vec2 JumpTarget;
	float BoundsW = 64.f;
	float BoundsH = 64.f;
	float PannedDistance = 0.f; // accumulated user pan (tutorial)

	void Init(int MapW, int MapH, const Vec2& Start);
	void Pan(const Vec2& WorldDelta);
	void Fling(const Vec2& WorldVelocity);
	void StopInertia();
	void ZoomBy(float Factor);
	void SetZoomAlpha(float Alpha);
	void JumpTo(const Vec2& P);
	void Update(float Dt);
	float ZoomAlpha() const;
	void Clamp();
};

} // namespace bh
