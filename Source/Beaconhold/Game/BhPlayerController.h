// Beaconhold — player controller: owns the Slate UI and connects it to the director and the
// saved campaign. Input goes through the UI's full-screen input layer, not the engine's input
// bindings, so touch and mouse share one gesture recognizer.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "BhSynth.h"
#include "BhTypes.h"

#include "BhPlayerController.generated.h"

class ABhDirector;
class SBhRoot;
class UBhAssets;
class UBhGameInstance;
struct FBhUserSettings;

UCLASS()
class ABhPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABhPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	ABhDirector* GetDirector() const;
	UBhGameInstance* GetBhGameInstance() const;
	UBhAssets* GetAssets() const;

	// ------------------------------------------------------------------ Actions used by the UI
	bool StartMission(int32 MissionIndex, bh::Difficulty Diff, bool bTutorial);
	bool ContinueSuspended();
	// Back to the main menu. An unfinished mission is kept so it can be continued.
	void LeaveMission();
	bool RestartMission();
	void QuitGame();
	void ApplySettings(const FBhUserSettings& Settings);
	void PlayUi(bh::Sfx Cue);

private:
	void HandleAppDeactivated();
	void SaveSuspendedMission();

	UPROPERTY()
	TObjectPtr<ABhDirector> Director;

	TSharedPtr<SBhRoot> Root;
	FDelegateHandle DeactivateHandle;
};
