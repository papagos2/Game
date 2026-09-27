// Beaconhold — shared helpers for the Unreal layer: logging and conversions between the
// simulation core (tiles, sRGB colours, std::string) and Unreal types.
#pragma once

#include "CoreMinimal.h"

#include "BhMath.h"
#include "BhMeshGen.h"
#include "BhVisuals.h"

#include <string>

DECLARE_LOG_CATEGORY_EXTERN(LogBeaconhold, Log, All);

namespace BhUE
{
// One simulation tile is one metre.
constexpr float TileSize = 100.f;

inline FVector ToWorld(const bh::Vec2& P, float HeightTiles = 0.f)
{
	return FVector(P.X * TileSize, P.Y * TileSize, HeightTiles * TileSize);
}

inline FVector ToWorld(const bh::Vec3& TilesPos)
{
	return FVector(TilesPos.X * TileSize, TilesPos.Y * TileSize, TilesPos.Z * TileSize);
}

inline bh::Vec2 ToSim(const FVector& V)
{
	return bh::Vec2(static_cast<float>(V.X / TileSize), static_cast<float>(V.Y / TileSize));
}

// Simulation facing (radians in the XY plane) to an Unreal yaw in degrees.
inline float FacingToYaw(float Facing)
{
	return FMath::RadiansToDegrees(Facing);
}

// sRGB palette colour to a linear colour for materials and Slate.
inline FLinearColor ToLinear(bh::Rgb C, float Alpha = 1.f)
{
	FLinearColor L = FLinearColor(FColor(C.R, C.G, C.B, 255));
	L.A = Alpha;
	return L;
}

inline FString ToFString(const std::string& S)
{
	return FString(UTF8_TO_TCHAR(S.c_str()));
}

inline FString ToFString(const char* S)
{
	return FString(UTF8_TO_TCHAR(S != nullptr ? S : ""));
}

inline FText ToText(const std::string& S)
{
	return FText::FromString(ToFString(S));
}

inline FText ToText(const char* S)
{
	return FText::FromString(ToFString(S));
}

inline FText TimeText(float Seconds)
{
	const int32 Total = FMath::Max(0, FMath::FloorToInt(Seconds));
	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
}
} // namespace BhUE
