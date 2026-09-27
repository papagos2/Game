// Beaconhold - game instance: campaign progress, player settings, saving, app lifecycle, and
// the runtime content factory that lives for the whole app session.
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"

#include "BhProgress.h"

#include "BhGameInstance.generated.h"

class UBhAssets;

struct FBhUserSettings
{
	float MusicVolume = 0.6f;
	float SfxVolume = 0.9f;
	bool bAlwaysShowHealth = false;
	bool bTutorialDone = false;
	int32 LastDifficulty = 1;
};

UCLASS()
class UBhGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UBhAssets* GetAssets();

	// ------------------------------------------------------------------ Campaign
	const bh::CampaignProgress& GetProgress() const { return Progress; }
	bool RaiseBoon(bh::Boon B);
	void ResetBoons();
	// Returns true if this improved the record.
	bool RecordVictory(int32 MissionIndex, int32 Stars, bh::Difficulty Diff);
	void ResetProgress();

	// ------------------------------------------------------------------ Settings
	const FBhUserSettings& GetUserSettings() const { return Settings; }
	void SetUserSettings(const FBhUserSettings& NewSettings);

	// ------------------------------------------------------------------ Suspended mission
	bool HasSuspended() const { return bHasSuspended; }
	const FString& GetSuspendedSummary() const { return SuspendedSummary; }
	bool LoadSuspended(TArray<uint8>& OutData) const;
	void WriteSuspended(const TArray<uint8>& Data, const FString& Summary);
	void ClearSuspended();

	// Broadcast when the app is sent to the background (the mission should pause and save).
	FSimpleMulticastDelegate OnAppDeactivated;

private:
	void LoadProgress();
	void SaveProgress();
	void HandleAppDeactivate();

	UPROPERTY()
	TObjectPtr<UBhAssets> Assets;

	bh::CampaignProgress Progress;
	FBhUserSettings Settings;
	bool bHasSuspended = false;
	FString SuspendedSummary;
	FDelegateHandle DeactivateHandle;
	FDelegateHandle BackgroundHandle;
};
