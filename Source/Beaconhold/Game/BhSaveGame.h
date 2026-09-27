// Beaconhold — saved data: campaign progress and settings, and a suspended mission.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"

#include "BhSaveGame.generated.h"

UCLASS()
class UBhProgressSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	TArray<int32> BestStars;

	UPROPERTY()
	TArray<bool> Completed;

	UPROPERTY()
	TArray<bool> CompletedHard;

	UPROPERTY()
	TArray<int32> BoonRanks;

	UPROPERTY()
	float MusicVolume = 0.6f;

	UPROPERTY()
	float SfxVolume = 0.9f;

	UPROPERTY()
	bool bAlwaysShowHealth = false;

	UPROPERTY()
	bool bTutorialDone = false;

	UPROPERTY()
	int32 LastDifficulty = 1;
};

// A mission in progress, written when the app is sent to the background or the player leaves
// to the menu, so it can be continued exactly where it was.
UCLASS()
class UBhSuspendSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	TArray<uint8> Data;

	UPROPERTY()
	FString Summary;
};
