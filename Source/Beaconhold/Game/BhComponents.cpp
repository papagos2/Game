// Beaconhold — helpers for runtime-created mesh components.
#include "Game/BhComponents.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

namespace BhUE
{
UStaticMeshComponent* NewMeshComponent(AActor* Owner, UStaticMesh* Mesh, USceneComponent* Parent, UMaterialInterface* Material)
{
	if (Owner == nullptr)
	{
		return nullptr;
	}
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetCastShadow(false);
	Comp->SetCanEverAffectNavigation(false);
	Comp->bReceivesDecals = false;
	Comp->SetStaticMesh(Mesh);
	if (Material != nullptr)
	{
		Comp->SetMaterial(0, Material);
	}
	if (Parent != nullptr)
	{
		Comp->SetupAttachment(Parent);
	}
	Comp->RegisterComponent();
	Owner->AddInstanceComponent(Comp);
	return Comp;
}

void DestroyMeshComponent(UStaticMeshComponent*& Comp)
{
	if (Comp != nullptr)
	{
		Comp->DestroyComponent();
		Comp = nullptr;
	}
}
} // namespace BhUE
