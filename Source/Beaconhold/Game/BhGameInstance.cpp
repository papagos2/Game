// Beaconhold - game instance.
#include "Game/BhGameInstance.h"

#include "Game/BhAssets.h"
#include "Game/BhCommon.h"
#include "Game/BhSaveGame.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"

namespace
{
const TCHAR* ProgressSlot = TEXT("BeaconholdProgress");
const TCHAR* SuspendSlot = TEXT("BeaconholdSuspended");
constexpr int32 SaveUser = 0;
} // namespace

void UBhGameInstance::Init()
{
	Super::Init();
	LoadProgress();
	bHasSuspended = false;
	if (UGameplayStatics::DoesSaveGameExist(SuspendSlot, SaveUser))
	{
		if (const UBhSuspendSave* Saved = Cast<UBhSuspendSave>(UGameplayStatics::LoadGameFromSlot(SuspendSlot, SaveUser)))
		{
			bHasSuspended = Saved->Data.Num() > 0;
			SuspendedSummary = Saved->Summary;
		}
	}
	DeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &UBhGameInstance::HandleAppDeactivate);
	BackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this, &UBhGameInstance::HandleAppDeactivate);
}

void UBhGameInstance::Shutdown()
{
	FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(DeactivateHandle);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(BackgroundHandle);
	Super::Shutdown();
}

UBhAssets* UBhGameInstance::GetAssets()
{
	if (Assets == nullptr)
	{
		Assets = NewObject<UBhAssets>(this);
		Assets->Initialize();
	}
	return Assets;
}

void UBhGameInstance::HandleAppDeactivate()
{
	OnAppDeactivated.Broadcast();
}

// ============================================================================ Campaign

bool UBhGameInstance::RaiseBoon(bh::Boon B)
{
	if (!Progress.RaiseBoon(B))
	{
		return false;
	}
	SaveProgress();
	return true;
}

void UBhGameInstance::ResetBoons()
{
	Progress.ResetBoons();
	SaveProgress();
}

bool UBhGameInstance::RecordVictory(int32 MissionIndex, int32 Stars, bh::Difficulty Diff)
{
	const bool bImproved = Progress.RecordVictory(MissionIndex, Stars, Diff);
	if (MissionIndex == 0)
	{
		Settings.bTutorialDone = true;
	}
	SaveProgress();
	return bImproved;
}

void UBhGameInstance::ResetProgress()
{
	const FBhUserSettings Keep = Settings;
	Progress = bh::CampaignProgress();
	Settings = FBhUserSettings();
	Settings.MusicVolume = Keep.MusicVolume;
	Settings.SfxVolume = Keep.SfxVolume;
	Settings.bAlwaysShowHealth = Keep.bAlwaysShowHealth;
	ClearSuspended();
	SaveProgress();
}

void UBhGameInstance::SetUserSettings(const FBhUserSettings& NewSettings)
{
	Settings = NewSettings;
	SaveProgress();
}

// ============================================================================ Saving

void UBhGameInstance::LoadProgress()
{
	Progress = bh::CampaignProgress();
	Settings = FBhUserSettings();
	if (!UGameplayStatics::DoesSaveGameExist(ProgressSlot, SaveUser))
	{
		return;
	}
	const UBhProgressSave* Saved = Cast<UBhProgressSave>(UGameplayStatics::LoadGameFromSlot(ProgressSlot, SaveUser));
	if (Saved == nullptr)
	{
		UE_LOG(LogBeaconhold, Warning, TEXT("Beaconhold: progress save unreadable; starting fresh."));
		return;
	}
	for (int32 I = 0; I < bh::MaxCampaignMissions; ++I)
	{
		Progress.BestStars[I] = Saved->BestStars.IsValidIndex(I) ? FMath::Clamp(Saved->BestStars[I], 0, 3) : 0;
		Progress.Completed[I] = Saved->Completed.IsValidIndex(I) && Saved->Completed[I];
		Progress.CompletedHard[I] = Saved->CompletedHard.IsValidIndex(I) && Saved->CompletedHard[I];
	}
	for (int32 B = 0; B < bh::NumBoons; ++B)
	{
		Progress.BoonRanks[B] = Saved->BoonRanks.IsValidIndex(B) ? FMath::Clamp(Saved->BoonRanks[B], 0, bh::MaxBoonRank) : 0;
	}
	// A save edited by hand (or from a future balance change) must not overspend renown.
	if (Progress.AvailableRenown() < 0)
	{
		Progress.ResetBoons();
	}
	Settings.MusicVolume = FMath::Clamp(Saved->MusicVolume, 0.f, 1.f);
	Settings.SfxVolume = FMath::Clamp(Saved->SfxVolume, 0.f, 1.f);
	Settings.bAlwaysShowHealth = Saved->bAlwaysShowHealth;
	Settings.bTutorialDone = Saved->bTutorialDone;
	Settings.LastDifficulty = FMath::Clamp(Saved->LastDifficulty, 0, 2);
}

void UBhGameInstance::SaveProgress()
{
	UBhProgressSave* Save = Cast<UBhProgressSave>(UGameplayStatics::CreateSaveGameObject(UBhProgressSave::StaticClass()));
	if (Save == nullptr)
	{
		return;
	}
	for (int32 I = 0; I < bh::MaxCampaignMissions; ++I)
	{
		Save->BestStars.Add(Progress.BestStars[I]);
		Save->Completed.Add(Progress.Completed[I]);
		Save->CompletedHard.Add(Progress.CompletedHard[I]);
	}
	for (int32 B = 0; B < bh::NumBoons; ++B)
	{
		Save->BoonRanks.Add(Progress.BoonRanks[B]);
	}
	Save->MusicVolume = Settings.MusicVolume;
	Save->SfxVolume = Settings.SfxVolume;
	Save->bAlwaysShowHealth = Settings.bAlwaysShowHealth;
	Save->bTutorialDone = Settings.bTutorialDone;
	Save->LastDifficulty = Settings.LastDifficulty;
	if (!UGameplayStatics::SaveGameToSlot(Save, ProgressSlot, SaveUser))
	{
		UE_LOG(LogBeaconhold, Warning, TEXT("Beaconhold: could not write progress."));
	}
}

bool UBhGameInstance::LoadSuspended(TArray<uint8>& OutData) const
{
	OutData.Reset();
	if (!UGameplayStatics::DoesSaveGameExist(SuspendSlot, SaveUser))
	{
		return false;
	}
	const UBhSuspendSave* Saved = Cast<UBhSuspendSave>(UGameplayStatics::LoadGameFromSlot(SuspendSlot, SaveUser));
	if (Saved == nullptr || Saved->Data.Num() == 0)
	{
		return false;
	}
	OutData = Saved->Data;
	return true;
}

void UBhGameInstance::WriteSuspended(const TArray<uint8>& Data, const FString& Summary)
{
	UBhSuspendSave* Save = Cast<UBhSuspendSave>(UGameplayStatics::CreateSaveGameObject(UBhSuspendSave::StaticClass()));
	if (Save == nullptr)
	{
		return;
	}
	Save->Data = Data;
	Save->Summary = Summary;
	if (UGameplayStatics::SaveGameToSlot(Save, SuspendSlot, SaveUser))
	{
		bHasSuspended = true;
		SuspendedSummary = Summary;
	}
}

void UBhGameInstance::ClearSuspended()
{
	if (UGameplayStatics::DoesSaveGameExist(SuspendSlot, SaveUser))
	{
		UGameplayStatics::DeleteGameInSlot(SuspendSlot, SaveUser);
	}
	bHasSuspended = false;
	SuspendedSummary.Reset();
}
