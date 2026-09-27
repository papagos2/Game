// Beaconhold — player controller.
#include "Game/BhPlayerController.h"

#include "Game/BhCommon.h"
#include "Game/BhDirector.h"
#include "Game/BhGameInstance.h"
#include "Game/BhGameMode.h"
#include "UI/SBhRoot.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

ABhPlayerController::ABhPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableTouchEvents = false;
	bAutoManageActiveCameraTarget = false;
}

void ABhPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController() || GetWorld() == nullptr)
	{
		return;
	}
	// The engine's virtual joysticks are never wanted: the game has its own touch controls.
	ActivateTouchInterface(nullptr);

	if (ABhGameMode* GameMode = GetWorld()->GetAuthGameMode<ABhGameMode>())
	{
		Director = GameMode->GetDirector();
	}
	if (Director != nullptr)
	{
		Director->SetController(this);
		SetViewTarget(Director);
		Director->ShowBackdrop();
	}
	if (UBhGameInstance* GI = GetBhGameInstance())
	{
		DeactivateHandle = GI->OnAppDeactivated.AddUObject(this, &ABhPlayerController::HandleAppDeactivated);
	}
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Root = SNew(SBhRoot).Owner(this);
		Viewport->AddViewportWidgetContent(Root.ToSharedRef(), 10);
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(Root);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
}

void ABhPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBhGameInstance* GI = GetBhGameInstance())
	{
		GI->OnAppDeactivated.Remove(DeactivateHandle);
	}
	if (Root.IsValid() && GetWorld() != nullptr)
	{
		if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(Root.ToSharedRef());
		}
	}
	Root.Reset();
	Super::EndPlay(EndPlayReason);
}

ABhDirector* ABhPlayerController::GetDirector() const
{
	return Director;
}

UBhGameInstance* ABhPlayerController::GetBhGameInstance() const
{
	return GetGameInstance<UBhGameInstance>();
}

UBhAssets* ABhPlayerController::GetAssets() const
{
	UBhGameInstance* GI = GetBhGameInstance();
	return GI != nullptr ? GI->GetAssets() : nullptr;
}

bool ABhPlayerController::StartMission(int32 MissionIndex, bh::Difficulty Diff, bool bTutorial)
{
	UBhGameInstance* GI = GetBhGameInstance();
	if (Director == nullptr || GI == nullptr)
	{
		return false;
	}
	if (!Director->StartMission(MissionIndex, Diff, bTutorial))
	{
		return false;
	}
	// Starting anew replaces any mission kept for later.
	GI->ClearSuspended();
	FBhUserSettings Settings = GI->GetUserSettings();
	Settings.LastDifficulty = static_cast<int32>(Diff);
	GI->SetUserSettings(Settings);
	return true;
}

bool ABhPlayerController::ContinueSuspended()
{
	UBhGameInstance* GI = GetBhGameInstance();
	TArray<uint8> Data;
	if (Director == nullptr || GI == nullptr || !GI->LoadSuspended(Data))
	{
		return false;
	}
	if (!Director->ResumeMission(Data))
	{
		GI->ClearSuspended();
		return false;
	}
	return true;
}

void ABhPlayerController::SaveSuspendedMission()
{
	UBhGameInstance* GI = GetBhGameInstance();
	TArray<uint8> Data;
	FString Summary;
	if (Director != nullptr && GI != nullptr && Director->WriteSuspended(Data, Summary))
	{
		GI->WriteSuspended(Data, Summary);
	}
}

void ABhPlayerController::LeaveMission()
{
	if (Director == nullptr)
	{
		return;
	}
	if (Director->IsInMission() && !Director->IsMissionOver())
	{
		SaveSuspendedMission();
	}
	Director->ShowBackdrop();
}

bool ABhPlayerController::RestartMission()
{
	if (Director == nullptr || !Director->IsInMission())
	{
		return false;
	}
	return StartMission(Director->GetMissionIndex(), Director->GetDifficulty(), Director->GetTutorialEnabled());
}

void ABhPlayerController::QuitGame()
{
	if (Director != nullptr && Director->IsInMission() && !Director->IsMissionOver())
	{
		SaveSuspendedMission();
	}
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ABhPlayerController::ApplySettings(const FBhUserSettings& Settings)
{
	if (UBhGameInstance* GI = GetBhGameInstance())
	{
		GI->SetUserSettings(Settings);
	}
	if (Director != nullptr)
	{
		Director->SetVolumes(Settings.MusicVolume, Settings.SfxVolume);
	}
}

void ABhPlayerController::PlayUi(bh::Sfx Cue)
{
	if (Director != nullptr)
	{
		Director->PlayCue(Cue);
	}
}

void ABhPlayerController::HandleAppDeactivated()
{
	if (Director == nullptr || !Director->IsInMission() || Director->IsMissionOver())
	{
		return;
	}
	Director->SetPaused(true);
	SaveSuspendedMission();
	if (Root.IsValid())
	{
		Root->ShowPauseMenu();
	}
}
