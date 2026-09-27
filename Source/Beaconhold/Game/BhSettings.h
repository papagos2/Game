// Beaconhold — project settings (Project Settings > Game > Beaconhold, stored in DefaultGame.ini).
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPtr.h"

#include "BhSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Beaconhold"))
class UBhSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBhSettings();

	/**
	 * The mesh builder emits triangles counter-clockwise seen from the front (right-handed
	 * maths); Unreal's front faces are the opposite winding, so indices are reversed on upload.
	 * Turn this off only if every generated mesh renders inside-out.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	bool bReverseTriangleWinding = true;

	/** Unlit material sampling a texture parameter "SlateUI" (tint "TintColorAndOpacity"). Used for all opaque geometry. */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> PaletteMaterial;

	/** Translucent variant of the palette material, used for shadows, rings and markers. */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> DecalMaterial;

	/** Overall loudness of the synthesized audio. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MasterVolume = 1.f;

	static const UBhSettings& Get() { return *GetDefault<UBhSettings>(); }
};
