// Beaconhold - code-driven effects.
#include "Game/BhFx.h"

#include "Game/BhAssets.h"
#include "Game/BhCommon.h"
#include "Game/BhComponents.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
constexpr float FxTwoPi = 6.28318531f;
constexpr int32 MaxRings = 10;

bh::FxMesh FxProjectileKind(bh::Archetype Source)
{
	switch (Source)
	{
	case bh::Archetype::Watchtower:
		return bh::FxMesh::TowerBolt;
	case bh::Archetype::Sage:
		return bh::FxMesh::Bolt;
	case bh::Archetype::Hexer:
		return bh::FxMesh::Gloom;
	case bh::Archetype::Thornback:
	case bh::Archetype::ThornSpire:
		return bh::FxMesh::ThornDart;
	default:
		return bh::FxMesh::Arrow;
	}
}

// Peak height of a projectile's arc, in tiles.
float FxArcHeight(bh::FxMesh Kind, float TotalDist)
{
	if (Kind == bh::FxMesh::Bolt || Kind == bh::FxMesh::Gloom)
	{
		return FMath::Min(0.5f, 0.08f * TotalDist);
	}
	return FMath::Min(1.4f, 0.18f * TotalDist);
}

float FxSmoothStep(float A, float B, float X)
{
	const float T = FMath::Clamp((X - A) / FMath::Max(0.0001f, B - A), 0.f, 1.f);
	return T * T * (3.f - 2.f * T);
}
} // namespace

void FBhFx::Init(AActor* InOwner, UBhAssets* InAssets)
{
	Owner = InOwner;
	Assets = InAssets;
}

float FBhFx::RandomFloat()
{
	// xorshift32: deterministic enough for visuals, no global state.
	RandomState ^= RandomState << 13;
	RandomState ^= RandomState >> 17;
	RandomState ^= RandomState << 5;
	return static_cast<float>(RandomState & 0xFFFFFF) / static_cast<float>(0x1000000);
}

UStaticMeshComponent* FBhFx::NewComponent(bh::FxMesh Kind)
{
	UStaticMeshComponent* Comp = BhUE::NewMeshComponent(Owner, Assets != nullptr ? Assets->GetFxMesh(Kind) : nullptr);
	if (Comp != nullptr)
	{
		Comp->SetVisibility(false);
	}
	return Comp;
}

void FBhFx::Clear()
{
	for (TArray<FParticle>& Pool : Particles)
	{
		for (FParticle& P : Pool)
		{
			P.bActive = false;
			if (P.Comp != nullptr)
			{
				P.Comp->SetVisibility(false);
			}
		}
	}
	for (FRing& R : Rings)
	{
		R.bActive = false;
		if (R.Comp != nullptr)
		{
			R.Comp->SetVisibility(false);
		}
	}
	for (TPair<uint32, FProjectileVisual>& Pair : Projectiles)
	{
		ReleaseProjectileComp(Pair.Value.Kind, Pair.Value.Comp);
	}
	Projectiles.Reset();
}

FBhFx::FParticle& FBhFx::AcquireParticle(bh::FxMesh Kind)
{
	TArray<FParticle>& Pool = Particles[static_cast<int32>(Kind)];
	int32 Oldest = 0;
	float OldestAge = -1.f;
	for (int32 I = 0; I < Pool.Num(); ++I)
	{
		if (!Pool[I].bActive)
		{
			return Pool[I];
		}
		const float Age = Pool[I].Age / FMath::Max(0.01f, Pool[I].Life);
		if (Age > OldestAge)
		{
			OldestAge = Age;
			Oldest = I;
		}
	}
	if (Pool.Num() < MaxParticlesPerKind)
	{
		FParticle& P = Pool.AddDefaulted_GetRef();
		P.Comp = NewComponent(Kind);
		return P;
	}
	return Pool[Oldest];
}

void FBhFx::Burst(const FVector& Location, const FBhBurst& B)
{
	for (int32 I = 0; I < B.Count; ++I)
	{
		FParticle& P = AcquireParticle(B.Kind);
		if (P.Comp == nullptr)
		{
			continue;
		}
		const float Angle = RandomFloat() * FxTwoPi;
		const float Side = 1.f - B.UpBias * 0.7f;
		const float Up = FMath::Lerp(0.2f, 1.f, B.UpBias) * (0.6f + 0.4f * RandomFloat());
		FVector Dir(FMath::Cos(Angle) * Side, FMath::Sin(Angle) * Side, Up);
		Dir.Normalize();
		P.Location = Location + FVector((RandomFloat() - 0.5f) * B.Spread * 2.f, (RandomFloat() - 0.5f) * B.Spread * 2.f, RandomFloat() * B.Spread * 0.5f);
		P.Velocity = Dir * B.Speed * (0.6f + 0.6f * RandomFloat());
		P.Rotation = FRotator(RandomFloat() * 360.f, RandomFloat() * 360.f, 0.f);
		P.Spin = FRotator((RandomFloat() - 0.5f) * 720.f, (RandomFloat() - 0.5f) * 720.f, 0.f);
		P.Age = 0.f;
		P.Life = B.Life * (0.75f + 0.5f * RandomFloat());
		P.Size = B.Size * (0.7f + 0.6f * RandomFloat());
		P.Gravity = B.Gravity;
		P.bActive = true;
		P.Comp->SetWorldTransform(FTransform(P.Rotation, P.Location, FVector(0.01f)));
		P.Comp->SetVisibility(true);
	}
}

void FBhFx::GroundRing(const FVector& Location, float StartRadius, float EndRadius, float Life, const FLinearColor& Color, bh::DecalTex Tex)
{
	if (Assets == nullptr)
	{
		return;
	}
	FRing* Ring = nullptr;
	for (FRing& R : Rings)
	{
		if (!R.bActive && R.Tex == Tex)
		{
			Ring = &R;
			break;
		}
	}
	if (Ring == nullptr && Rings.Num() < MaxRings)
	{
		Ring = &Rings.AddDefaulted_GetRef();
		Ring->Tex = Tex;
		Ring->Material = Assets->CreateDecalMaterial(Tex, Color);
		Ring->Comp = BhUE::NewMeshComponent(Owner, Assets->GetDecalMesh(), nullptr, Ring->Material);
		if (Ring->Comp != nullptr)
		{
			Ring->Comp->SetTranslucentSortPriority(2);
		}
	}
	if (Ring == nullptr)
	{
		// Pool exhausted: recycle the oldest ring with the same texture.
		float Oldest = -1.f;
		for (FRing& R : Rings)
		{
			if (R.Tex == Tex && R.Age / FMath::Max(0.01f, R.Life) > Oldest)
			{
				Oldest = R.Age / FMath::Max(0.01f, R.Life);
				Ring = &R;
			}
		}
	}
	if (Ring == nullptr || Ring->Comp == nullptr)
	{
		return;
	}
	Ring->Location = Location + FVector(0.f, 0.f, 4.f);
	Ring->StartRadius = StartRadius;
	Ring->EndRadius = EndRadius;
	Ring->Age = 0.f;
	Ring->Life = FMath::Max(0.05f, Life);
	Ring->Color = Color;
	Ring->bActive = true;
	UBhAssets::SetDecalTint(Ring->Material, Color);
	Ring->Comp->SetWorldTransform(FTransform(FRotator::ZeroRotator, Ring->Location, FVector(StartRadius * 2.f, StartRadius * 2.f, 1.f)));
	Ring->Comp->SetVisibility(true);
}

UStaticMeshComponent* FBhFx::AcquireProjectileComp(bh::FxMesh Kind)
{
	TArray<UStaticMeshComponent*>& Free = FreeProjectileComps[static_cast<int32>(Kind)];
	if (Free.Num() > 0)
	{
		return Free.Pop();
	}
	return NewComponent(Kind);
}

void FBhFx::ReleaseProjectileComp(bh::FxMesh Kind, UStaticMeshComponent* Comp)
{
	if (Comp != nullptr)
	{
		Comp->SetVisibility(false);
		FreeProjectileComps[static_cast<int32>(Kind)].Add(Comp);
	}
}

void FBhFx::SyncProjectiles(const bh::World& W, float Alpha)
{
	for (TPair<uint32, FProjectileVisual>& Pair : Projectiles)
	{
		Pair.Value.bSeen = false;
	}
	for (const bh::Projectile& P : W.GetProjectiles())
	{
		if (!P.bAlive)
		{
			continue;
		}
		FProjectileVisual* V = Projectiles.Find(P.Id);
		if (V == nullptr)
		{
			FProjectileVisual New;
			New.Kind = FxProjectileKind(P.SourceType);
			New.Comp = AcquireProjectileComp(New.Kind);
			V = &Projectiles.Add(P.Id, New);
		}
		V->bSeen = true;
		if (V->Comp == nullptr)
		{
			continue;
		}
		const bh::Vec2 Ground = bh::Vec2::Lerp(P.PrevPos, P.Pos, Alpha);
		const float T = FMath::Clamp(bh::Vec2::Dist(P.Start, Ground) / FMath::Max(0.01f, P.TotalDist), 0.f, 1.f);
		const float Z = FMath::Lerp(0.65f, 0.45f, T) + 4.f * FxArcHeight(V->Kind, P.TotalDist) * T * (1.f - T);
		const FVector Location = BhUE::ToWorld(Ground, Z);
		FVector Dir = V->bHasLast ? (Location - V->LastLocation) : (BhUE::ToWorld(P.TargetPos, 0.45f) - Location);
		if (!Dir.Normalize())
		{
			Dir = FVector(1.f, 0.f, 0.f);
		}
		V->Comp->SetWorldLocationAndRotation(Location, Dir.Rotation());
		V->Comp->SetVisibility(true);
		V->LastLocation = Location;
		V->bHasLast = true;
	}
	for (auto It = Projectiles.CreateIterator(); It; ++It)
	{
		if (!It.Value().bSeen)
		{
			ReleaseProjectileComp(It.Value().Kind, It.Value().Comp);
			It.RemoveCurrent();
		}
	}
}

void FBhFx::Tick(float DeltaSeconds)
{
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	for (TArray<FParticle>& Pool : Particles)
	{
		for (FParticle& P : Pool)
		{
			if (!P.bActive || P.Comp == nullptr)
			{
				continue;
			}
			P.Age += Dt;
			if (P.Age >= P.Life)
			{
				P.bActive = false;
				P.Comp->SetVisibility(false);
				continue;
			}
			P.Velocity.Z -= P.Gravity * Dt;
			P.Location += P.Velocity * Dt;
			if (P.Gravity > 0.f && P.Location.Z < 3.f)
			{
				// Settle on the ground.
				P.Location.Z = 3.f;
				P.Velocity *= FVector(0.4f, 0.4f, -0.25f);
				P.Spin *= 0.5f;
			}
			P.Rotation += P.Spin * Dt;
			const float K = P.Age / P.Life;
			const float Scale = P.Size * FMath::Min(1.f, K * 6.f) * (1.f - FxSmoothStep(0.6f, 1.f, K));
			P.Comp->SetWorldTransform(FTransform(P.Rotation, P.Location, FVector(FMath::Max(0.01f, Scale))));
		}
	}
	for (FRing& R : Rings)
	{
		if (!R.bActive || R.Comp == nullptr)
		{
			continue;
		}
		R.Age += Dt;
		if (R.Age >= R.Life)
		{
			R.bActive = false;
			R.Comp->SetVisibility(false);
			continue;
		}
		const float K = R.Age / R.Life;
		const float Radius = FMath::Lerp(R.StartRadius, R.EndRadius, 1.f - (1.f - K) * (1.f - K));
		const float Diameter = FMath::Max(0.01f, Radius * 2.f);
		R.Comp->SetWorldTransform(FTransform(FRotator::ZeroRotator, R.Location, FVector(Diameter, Diameter, 1.f)));
		FLinearColor C = R.Color;
		C.A *= 1.f - FxSmoothStep(0.5f, 1.f, K);
		UBhAssets::SetDecalTint(R.Material, C);
	}
}
