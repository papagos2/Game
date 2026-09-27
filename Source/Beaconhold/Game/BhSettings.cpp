// Beaconhold — project settings.
#include "Game/BhSettings.h"

UBhSettings::UBhSettings()
{
	// Engine materials that sample a texture unlit; they are cooked via DirectoriesToAlwaysCook.
	PaletteMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque_OneSided.Widget3DPassThrough_Opaque_OneSided")));
	DecalMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided.Widget3DPassThrough_Translucent_OneSided")));
}
