// Beaconhold — helpers for the runtime-created mesh components that draw the world.
#pragma once

#include "CoreMinimal.h"

class AActor;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

namespace BhUE
{
// A movable mesh component without collision, shadows or navigation, owned by Owner and
// optionally attached to Parent. Material overrides slot 0 when given.
UStaticMeshComponent* NewMeshComponent(AActor* Owner, UStaticMesh* Mesh, USceneComponent* Parent = nullptr, UMaterialInterface* Material = nullptr);

// Destroys a component made by NewMeshComponent and clears the pointer.
void DestroyMeshComponent(UStaticMeshComponent*& Comp);
} // namespace BhUE
