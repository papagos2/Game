// Beaconhold - code-driven effects: pooled mesh particles, projectile visuals and fading ground
// markers. No particle assets are needed; every effect is a few small palette meshes animated
// on the CPU (cheap enough for phones at the counts an RTS needs).
#pragma once

#include "CoreMinimal.h"

#include "BhRender.h"

class AActor;
class UBhAssets;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

struct FBhBurst
{
	bh::FxMesh Kind = bh::FxMesh::Spark;
	int32 Count = 6;
	float Speed = 250.f;    // cm/s
	float UpBias = 0.6f;    // 0 = sideways spray, 1 = straight up
	float Life = 0.6f;      // seconds
	float Size = 1.f;       // scale multiplier
	float Gravity = 900.f;  // cm/s^2
	float Spread = 20.f;    // cm, spawn jitter
};

class FBhFx
{
public:
	void Init(AActor* InOwner, UBhAssets* InAssets);
	// Removes every active effect (mission change).
	void Clear();

	void Burst(const FVector& Location, const FBhBurst& Burst);
	// Ring on the ground that expands (or shrinks) and fades. Radius in tiles.
	void GroundRing(const FVector& Location, float StartRadius, float EndRadius, float Life, const FLinearColor& Color, bh::DecalTex Tex = bh::DecalTex::MoveMarker);

	// Projectiles are mirrored from the simulation every frame.
	void SyncProjectiles(const bh::World& W, float Alpha);

	void Tick(float DeltaSeconds);

private:
	struct FParticle
	{
		UStaticMeshComponent* Comp = nullptr;
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		FRotator Spin = FRotator::ZeroRotator;
		float Age = 0.f;
		float Life = 1.f;
		float Size = 1.f;
		float Gravity = 0.f;
		bool bActive = false;
	};

	struct FRing
	{
		UStaticMeshComponent* Comp = nullptr;
		UMaterialInstanceDynamic* Material = nullptr;
		FVector Location = FVector::ZeroVector;
		float StartRadius = 1.f;
		float EndRadius = 1.f;
		float Age = 0.f;
		float Life = 1.f;
		FLinearColor Color = FLinearColor::White;
		bh::DecalTex Tex = bh::DecalTex::MoveMarker;
		bool bActive = false;
	};

	struct FProjectileVisual
	{
		UStaticMeshComponent* Comp = nullptr;
		bh::FxMesh Kind = bh::FxMesh::Arrow;
		FVector LastLocation = FVector::ZeroVector;
		bool bHasLast = false;
		bool bSeen = false;
	};

	UStaticMeshComponent* NewComponent(bh::FxMesh Kind);
	FParticle& AcquireParticle(bh::FxMesh Kind);
	UStaticMeshComponent* AcquireProjectileComp(bh::FxMesh Kind);
	void ReleaseProjectileComp(bh::FxMesh Kind, UStaticMeshComponent* Comp);

	AActor* Owner = nullptr;
	UBhAssets* Assets = nullptr;
	static constexpr int32 NumKinds = static_cast<int32>(bh::FxMesh::Count);
	static constexpr int32 MaxParticlesPerKind = 40;
	TArray<FParticle> Particles[NumKinds];
	TArray<FRing> Rings;
	TMap<uint32, FProjectileVisual> Projectiles;
	TArray<UStaticMeshComponent*> FreeProjectileComps[NumKinds];
	uint32 RandomState = 0x9E3779B9u;
	float RandomFloat();
};
