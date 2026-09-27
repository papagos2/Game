// Beaconhold simulation core — touch/mouse gesture recognizer.
//
// Raw pointers go in (screen pixels), gestures come out:
//   tap, double tap, pan (one finger), long-press then drag = selection box, pinch (two fingers).
// In mouse mode a left-button drag becomes a selection box immediately (desktop convention).
#pragma once

#include <vector>

namespace bh
{
enum class GestureType : unsigned char
{
	Tap,
	DoubleTap,
	LongPress,
	PanStart,
	Pan,
	PanEnd,
	BoxStart,
	Box,
	BoxEnd,
	BoxCancel,
	PinchStart,
	Pinch,
	PinchEnd,
};

struct GestureEvent
{
	GestureType Type = GestureType::Tap;
	float X = 0.f;      // current position (for pinch: centre)
	float Y = 0.f;
	float PrevX = 0.f;  // previous position (for pan / pinch centre)
	float PrevY = 0.f;
	float StartX = 0.f; // gesture start position (box corner)
	float StartY = 0.f;
	float Scale = 1.f;  // pinch: distance ratio since the previous event
	float VelX = 0.f;   // pan end: pixels per second
	float VelY = 0.f;
};

class GestureRecognizer
{
public:
	// Distances in pixels, times in seconds. Call Configure when the screen size changes.
	void Configure(float ScreenHeightPixels);
	void Reset();

	void PointerDown(int Id, float X, float Y, double Time, bool bMouse);
	void PointerMove(int Id, float X, float Y, double Time);
	void PointerUp(int Id, float X, float Y, double Time);
	void Update(double Time);

	std::vector<GestureEvent> Events;

	bool IsBoxActive() const { return State == EState::Box; }
	void GetBox(float& X0, float& Y0, float& X1, float& Y1) const;
	float GetTapSlop() const { return TapSlop; }

private:
	enum class EState : unsigned char
	{
		Idle,
		Pending,
		LongPressed,
		Pan,
		Box,
		Pinch,
		WaitRelease,
	};

	struct Pointer
	{
		int Id = -1;
		float X = 0.f;
		float Y = 0.f;
		float StartX = 0.f;
		float StartY = 0.f;
		double StartTime = 0.0;
		bool bActive = false;
	};

	Pointer* FindPointer(int Id);
	int ActiveCount() const;
	void Emit(GestureType Type, float X, float Y, float PrevX, float PrevY);
	void BeginPinch();
	float PinchDistance() const;
	void PinchCenter(float& OutX, float& OutY) const;

	Pointer Pointers[2];
	EState State = EState::Idle;
	bool bMouseMode = false;
	float TapSlop = 16.f;
	float LongPressTime = 0.42f;
	float DoubleTapTime = 0.32f;
	double LastTapTime = -10.0;
	float LastTapX = 0.f;
	float LastTapY = 0.f;
	float LastPinchDist = 1.f;
	float LastCenterX = 0.f;
	float LastCenterY = 0.f;
	// velocity tracking for fling
	float VelX = 0.f;
	float VelY = 0.f;
	double LastMoveTime = 0.0;
};

} // namespace bh
