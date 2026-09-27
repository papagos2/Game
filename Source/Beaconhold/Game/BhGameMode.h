// Beaconhold — game mode: no pawn (the camera belongs to the director), our controller and HUD.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "BhGameMode.generated.h"

class ABhDirector;

UCLASS()
class ABhGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABhGameMode();

	// The single director of this world, spawned on first use.
	ABhDirector* GetDirector();

private:
	UPROPERTY()
	TObjectPtr<ABhDirector> Director;
};
