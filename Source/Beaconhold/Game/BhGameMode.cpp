// Beaconhold - game mode.
#include "Game/BhGameMode.h"

#include "Game/BhDirector.h"
#include "Game/BhOverlayHUD.h"
#include "Game/BhPlayerController.h"

#include "Engine/World.h"

ABhGameMode::ABhGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ABhPlayerController::StaticClass();
	HUDClass = ABhOverlayHUD::StaticClass();
}

ABhDirector* ABhGameMode::GetDirector()
{
	if (Director == nullptr && GetWorld() != nullptr)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Director = GetWorld()->SpawnActor<ABhDirector>(ABhDirector::StaticClass(), FTransform::Identity, Params);
	}
	return Director;
}
