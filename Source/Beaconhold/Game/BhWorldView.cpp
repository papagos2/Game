// Beaconhold — the visible world.
#include "Game/BhWorldView.h"

#include "Game/BhAssets.h"
#include "Game/BhCommon.h"
#include "Game/BhComponents.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
constexpr int32 ChunkSize = 16;
constexpr int32 ChunkMargin = 16; // tiles of decorative forest around the map
constexpr int32 MaxRebuildsPerFrame = 2;
constexpr float ViewMargin = 4.f; // tiles outside the view that still animate

FLinearColor WvRingColor(bh::Team T)
{
	switch (T)
	{
	case bh::Team::Player:
		return FLinearColor(0.45f, 1.f, 0.5f, 0.95f);
	case bh::Team::Enemy:
		return FLinearColor(1.f, 0.32f, 0.28f, 0.95f);
	case bh::Team::Neutral:
		break;
	}
	return FLinearColor(1.f, 0.85f, 0.35f, 0.95f);
}

bool WvInside(const FBox2D& Box, const bh::Vec2& P, float Margin)
{
	return P.X >= Box.Min.X - Margin && P.X <= Box.Max.X + Margin && P.Y >= Box.Min.Y - Margin && P.Y <= Box.Max.Y + Margin;
}
} // namespace

void FBhWorldView::Init(AActor* InOwner, UBhAssets* InAssets)
{
	Owner = InOwner;
	Assets = InAssets;
}

UStaticMeshComponent* FBhWorldView::NewDecal(bh::DecalTex Tex, const FLinearColor& Tint, int32 SortPriority)
{
	UStaticMeshComponent* Comp = BhUE::NewMeshComponent(Owner, Assets->GetDecalMesh(), nullptr, Assets->GetSharedDecalMaterial(Tex, Tint));
	if (Comp != nullptr)
	{
		Comp->SetTranslucentSortPriority(SortPriority);
		Comp->SetVisibility(false);
	}
	return Comp;
}

// ============================================================================ Build / clear

void FBhWorldView::BuildWorld(const bh::Session& S)
{
	Clear();
	if (Owner == nullptr || Assets == nullptr)
	{
		return;
	}
	const bh::World& W = S.GetWorld();
	const bh::GameMap& Map = W.GetMap();
	MapW = Map.GetWidth();
	MapH = Map.GetHeight();
	bh::BuildGround(Map, 2, Cells, GroundColors);
	bh::BuildTreeInstances(Map, Trees);
	bh::GenerateProps(Map, Props);
	bPropsStale = false;

	// Chunks cover the map plus the forest border.
	ChunkOriginX = -ChunkMargin;
	ChunkOriginY = -ChunkMargin;
	ChunksX = (MapW + ChunkMargin * 2 + ChunkSize - 1) / ChunkSize;
	ChunksY = (MapH + ChunkMargin * 2 + ChunkSize - 1) / ChunkSize;
	bh::ColorPalette& Palette = Assets->GetPalette();
	for (int32 CY = 0; CY < ChunksY; ++CY)
	{
		for (int32 CX = 0; CX < ChunksX; ++CX)
		{
			FChunk& C = Chunks.AddDefaulted_GetRef();
			const int32 X0 = ChunkOriginX + CX * ChunkSize;
			const int32 Y0 = ChunkOriginY + CY * ChunkSize;
			C.Rect = bh::TileRect(X0, Y0, X0 + ChunkSize, Y0 + ChunkSize);
			bh::RenderMesh Terrain;
			bh::BuildTerrainChunk(Cells, GroundColors, C.Rect, MapW, MapH, Palette, Terrain);
			C.TerrainMesh = Assets->BuildMesh(Terrain, TEXT("Terrain"));
			if (C.TerrainMesh != nullptr)
			{
				C.Terrain = BhUE::NewMeshComponent(Owner, C.TerrainMesh);
			}
			RebuildTrees(C, Map);
			RebuildProps(C);
		}
	}
	bh::RenderMesh Floor3D;
	bh::BuildFloorMesh(MapW, MapH, 64.f, Palette, Floor3D);
	FloorMesh = Assets->BuildMesh(Floor3D, TEXT("Floor"));
	Floor = BhUE::NewMeshComponent(Owner, FloorMesh);

	UStaticMesh* RuinMesh = Assets->GetRuinMesh();
	for (const bh::TileRect& Site : Map.BeaconSites)
	{
		UStaticMeshComponent* Ruin = BhUE::NewMeshComponent(Owner, RuinMesh);
		if (Ruin != nullptr)
		{
			Ruin->SetWorldLocationAndRotation(BhUE::ToWorld(Site.Center()), FRotator(0.f, bh::BuildingYaw, 0.f));
		}
		Ruins.Add(Ruin);
	}

	// Overlay materials (shared by all rings / cells of a colour).
	RingOwn = Assets->GetSharedDecalMaterial(bh::DecalTex::SelectionRing, WvRingColor(bh::Team::Player));
	RingEnemy = Assets->GetSharedDecalMaterial(bh::DecalTex::SelectionRing, WvRingColor(bh::Team::Enemy));
	RingNeutral = Assets->GetSharedDecalMaterial(bh::DecalTex::SelectionRing, WvRingColor(bh::Team::Neutral));
	PlaceValid = Assets->GetSharedDecalMaterial(bh::DecalTex::PlacementCell, FLinearColor(0.4f, 1.f, 0.45f, 0.75f));
	PlaceInvalid = Assets->GetSharedDecalMaterial(bh::DecalTex::PlacementCell, FLinearColor(1.f, 0.3f, 0.25f, 0.75f));

	// Entities present at the start appear without the spawn pop.
	for (const bh::Entity& E : W.GetEntities())
	{
		if (E.bAlive)
		{
			CreateVisual(E, -100.f);
		}
	}
	Assets->FlushPalette();
}

void FBhWorldView::Clear()
{
	for (FChunk& C : Chunks)
	{
		BhUE::DestroyMeshComponent(C.Terrain);
		BhUE::DestroyMeshComponent(C.Trees);
		BhUE::DestroyMeshComponent(C.Props);
		if (Assets != nullptr)
		{
			Assets->ReleaseMesh(C.TerrainMesh);
			Assets->ReleaseMesh(C.TreeMesh);
			Assets->ReleaseMesh(C.PropMesh);
		}
	}
	Chunks.Reset();
	BhUE::DestroyMeshComponent(Floor);
	if (Assets != nullptr)
	{
		Assets->ReleaseMesh(FloorMesh);
	}
	FloorMesh = nullptr;
	for (UStaticMeshComponent*& Ruin : Ruins)
	{
		BhUE::DestroyMeshComponent(Ruin);
	}
	Ruins.Reset();
	for (TPair<bh::EntityId, FEntityVisual>& Pair : Entities)
	{
		DestroyVisual(Pair.Value);
	}
	Entities.Reset();
	for (UStaticMeshComponent*& Ring : SelectionRings)
	{
		BhUE::DestroyMeshComponent(Ring);
	}
	SelectionRings.Reset();
	for (UStaticMeshComponent*& Cell : PlaceCells)
	{
		BhUE::DestroyMeshComponent(Cell);
	}
	PlaceCells.Reset();
	for (UStaticMeshComponent*& Glow : SiteGlows)
	{
		BhUE::DestroyMeshComponent(Glow);
	}
	SiteGlows.Reset();
	BhUE::DestroyMeshComponent(Ghost);
	GhostType = bh::Archetype::None;
	BhUE::DestroyMeshComponent(RallyFlag);
	BhUE::DestroyMeshComponent(RangeRing);
	BhUE::DestroyMeshComponent(TutorialRing);
	bTutorialMarker = false;
	Cells.clear();
	GroundColors.clear();
	Trees.clear();
	Props.clear();
}

FBhWorldView::FChunk* FBhWorldView::ChunkAt(int32 TileX, int32 TileY)
{
	const int32 CX = (TileX - ChunkOriginX) / ChunkSize;
	const int32 CY = (TileY - ChunkOriginY) / ChunkSize;
	if (TileX < ChunkOriginX || TileY < ChunkOriginY || CX >= ChunksX || CY >= ChunksY)
	{
		return nullptr;
	}
	return &Chunks[CY * ChunksX + CX];
}

void FBhWorldView::RebuildTrees(FChunk& C, const bh::GameMap& Map)
{
	bh::RenderMesh Mesh;
	bh::BuildTreeChunkMesh(Trees, Map, C.Rect, Assets->GetPalette(), Mesh);
	UStaticMesh* Old = C.TreeMesh;
	C.TreeMesh = Assets->BuildMesh(Mesh, TEXT("Trees"));
	if (C.TreeMesh == nullptr)
	{
		BhUE::DestroyMeshComponent(C.Trees);
	}
	else if (C.Trees == nullptr)
	{
		C.Trees = BhUE::NewMeshComponent(Owner, C.TreeMesh);
	}
	else
	{
		C.Trees->SetStaticMesh(C.TreeMesh);
	}
	Assets->ReleaseMesh(Old);
	C.bTreesDirty = false;
}

void FBhWorldView::RebuildProps(FChunk& C)
{
	bh::RenderMesh Mesh;
	bh::BuildPropChunkMesh(Props, C.Rect, Assets->GetPalette(), Mesh);
	UStaticMesh* Old = C.PropMesh;
	C.PropMesh = Assets->BuildMesh(Mesh, TEXT("Props"));
	if (C.PropMesh == nullptr)
	{
		BhUE::DestroyMeshComponent(C.Props);
	}
	else if (C.Props == nullptr)
	{
		C.Props = BhUE::NewMeshComponent(Owner, C.PropMesh);
	}
	else
	{
		C.Props->SetStaticMesh(C.PropMesh);
	}
	Assets->ReleaseMesh(Old);
	C.bPropsDirty = false;
}

// ============================================================================ Entities

FBhWorldView::FEntityVisual& FBhWorldView::CreateVisual(const bh::Entity& E, float Time)
{
	FEntityVisual V;
	V.Id = E.Id;
	V.Type = E.Type;
	V.Owner = E.Owner;
	V.Kind = E.Kind;
	const bh::ModelDef& Model = bh::GetModel(E.Type);
	const bh::ArchetypeDef& Def = bh::GetDef(E.Type);
	V.Scale = Model.Scale;
	V.Height = Model.Height * Model.Scale;
	V.Width = E.IsUnit() ? FMath::Max(0.6f, E.Radius * 2.2f) : static_cast<float>(FMath::Max(1, Def.Footprint));
	V.Pos = E.Pos;
	V.VisualYaw = E.IsUnit() ? BhUE::FacingToYaw(E.Facing) : bh::BuildingYaw;
	V.SpawnTime = Time;
	V.AttackSerial = E.AttackSerial;
	V.InitialAmount = FMath::Max(1, E.Amount);
	V.bConstructed = E.bConstructed;
	V.BuildProgress = E.BuildProgress;
	V.Body = BhUE::NewMeshComponent(Owner, Assets->GetBodyMesh(E.Type, E.Owner));
	if (V.Body != nullptr)
	{
		for (int32 R = 1; R < bh::NumPartRoles; ++R)
		{
			FVector Pivot;
			if (UStaticMesh* PartMesh = Assets->GetRoleMesh(E.Type, E.Owner, static_cast<bh::PartRole>(R), Pivot))
			{
				FPartVisual Part;
				Part.Role = static_cast<bh::PartRole>(R);
				Part.Pivot = Pivot;
				Part.Comp = BhUE::NewMeshComponent(Owner, PartMesh, V.Body);
				if (Part.Comp != nullptr)
				{
					Part.Comp->SetRelativeLocation(Pivot);
					V.Parts.Add(Part);
				}
			}
		}
	}
	FEntityVisual& Added = Entities.Add(E.Id, MoveTemp(V));
	UpdateVisual(Added, E, 1.f, Time, 0.f);
	return Added;
}

void FBhWorldView::DestroyVisual(FEntityVisual& V)
{
	for (FPartVisual& Part : V.Parts)
	{
		BhUE::DestroyMeshComponent(Part.Comp);
	}
	V.Parts.Reset();
	BhUE::DestroyMeshComponent(V.Scaffold);
	BhUE::DestroyMeshComponent(V.Body);
}

void FBhWorldView::SetVisualVisible(FEntityVisual& V, bool bVisible)
{
	if (V.bVisible == bVisible || V.Body == nullptr)
	{
		return;
	}
	V.bVisible = bVisible;
	// Parts are shown one by one by ApplyPose (cargo may stay hidden), but hidden together.
	V.Body->SetVisibility(bVisible, false);
	if (!bVisible)
	{
		for (FPartVisual& Part : V.Parts)
		{
			if (Part.Comp != nullptr)
			{
				Part.Comp->SetVisibility(false);
			}
			Part.bVisible = false;
		}
	}
}

void FBhWorldView::UpdateVisual(FEntityVisual& V, const bh::Entity& E, float Alpha, float Time, float DeltaSeconds)
{
	V.Pos = bh::Vec2::Lerp(E.PrevPos, E.Pos, Alpha);
	V.HpRatio = E.HpRatio();
	V.bConstructed = E.bConstructed;
	V.BuildProgress = E.BuildProgress;
	if (E.AttackSerial != V.AttackSerial)
	{
		V.AttackSerial = E.AttackSerial;
		V.SwingTime = Time;
	}
	const bool bMoving = E.IsUnit() && bh::Vec2::DistSq(E.PrevPos, E.Pos) > 1e-6f;
	if (E.IsUnit())
	{
		V.VisualYaw = FMath::FixedTurn(V.VisualYaw, BhUE::FacingToYaw(E.Facing), DeltaSeconds > 0.f ? 720.f * DeltaSeconds : 360.f);
	}

	bh::AnimInput In;
	In.Type = E.Type;
	In.Owner = E.Owner;
	In.Act = E.Act;
	In.Buff = E.Buff;
	In.Carry = E.CarryAmount > 0 ? E.CarryType : bh::Resource::None;
	In.bMoving = bMoving;
	In.bConstructed = E.bConstructed;
	In.BuildProgress = E.BuildProgress;
	In.Time = Time;
	In.SwingAge = Time - V.SwingTime;
	In.HitAge = Time - V.HitTime;
	In.SpawnAge = Time - V.SpawnTime;
	In.AmountRatio = E.IsResourceNode() ? static_cast<float>(E.Amount) / static_cast<float>(V.InitialAmount) : 1.f;
	In.Seed = E.Id;
	V.Last = In;
	ApplyPose(V, In);

	// Scaffolding while under construction.
	if (E.IsBuilding())
	{
		if (!E.bConstructed && V.Scaffold == nullptr)
		{
			V.Scaffold = BhUE::NewMeshComponent(Owner, Assets->GetScaffoldMesh(bh::GetDef(E.Type).Footprint));
			if (V.Scaffold != nullptr)
			{
				V.Scaffold->SetWorldLocationAndRotation(BhUE::ToWorld(E.Pos), FRotator(0.f, bh::BuildingYaw, 0.f));
			}
		}
		else if (E.bConstructed && V.Scaffold != nullptr)
		{
			BhUE::DestroyMeshComponent(V.Scaffold);
		}
	}
}

void FBhWorldView::ApplyPose(FEntityVisual& V, const bh::AnimInput& In)
{
	if (V.Body == nullptr)
	{
		return;
	}
	const bh::EntityPose Pose = bh::EvaluateEntityPose(In);
	const FVector Location = BhUE::ToWorld(V.Pos) + FVector(Pose.Offset.X, Pose.Offset.Y, Pose.Offset.Z) * BhUE::TileSize;
	const FRotator Rotation(Pose.Pitch, V.VisualYaw, Pose.Roll);
	const FVector Scale(V.Scale * Pose.ScaleXY, V.Scale * Pose.ScaleXY, FMath::Max(0.01f, V.Scale * Pose.ScaleZ));
	V.Body->SetWorldTransform(FTransform(Rotation, Location, Scale));
	SetVisualVisible(V, Pose.bVisible);
	if (!Pose.bVisible)
	{
		return;
	}
	for (FPartVisual& Part : V.Parts)
	{
		if (Part.Comp == nullptr)
		{
			continue;
		}
		const bh::PartPose P = bh::EvaluateRolePose(Part.Role, In);
		if (P.bVisible)
		{
			Part.Comp->SetRelativeTransform(FTransform(FRotator(P.Pitch, P.Yaw, P.Roll), Part.Pivot + FVector(P.Offset.X, P.Offset.Y, P.Offset.Z) * BhUE::TileSize,
				FVector(FMath::Max(0.01f, P.Scale))));
		}
		if (Part.bVisible != P.bVisible)
		{
			Part.bVisible = P.bVisible;
			Part.Comp->SetVisibility(P.bVisible);
		}
	}
}

void FBhWorldView::NotifySwing(bh::EntityId Id, float Time)
{
	if (FEntityVisual* V = Entities.Find(Id))
	{
		V->SwingTime = Time;
	}
}

void FBhWorldView::NotifyHit(bh::EntityId Id, float Time)
{
	if (FEntityVisual* V = Entities.Find(Id))
	{
		V->HitTime = Time;
	}
}

void FBhWorldView::NotifyFootprintChanged(const bh::TileRect& Rect)
{
	bPropsStale = true;
	for (int32 Y = Rect.Y0; Y < Rect.Y1; Y += 1)
	{
		for (int32 X = Rect.X0; X < Rect.X1; X += 1)
		{
			if (FChunk* C = ChunkAt(X, Y))
			{
				C->bPropsDirty = true;
			}
		}
	}
}

void FBhWorldView::SetTutorialMarker(bool bShow, const bh::Vec2& Pos)
{
	bTutorialMarker = bShow;
	TutorialPos = Pos;
}

// ============================================================================ Per frame

void FBhWorldView::Tick(bh::Session& S, float Alpha, float Time, float DeltaSeconds, const FBox2D& ViewTiles)
{
	if (Owner == nullptr || Assets == nullptr)
	{
		return;
	}
	const bh::World& W = S.GetWorld();
	for (TPair<bh::EntityId, FEntityVisual>& Pair : Entities)
	{
		Pair.Value.bSeen = false;
	}
	for (const bh::Entity& E : W.GetEntities())
	{
		if (!E.bAlive)
		{
			continue;
		}
		FEntityVisual* V = Entities.Find(E.Id);
		if (V == nullptr)
		{
			V = &CreateVisual(E, Time);
		}
		V->bSeen = true;
		// Far outside the view nothing needs animating; positions catch up on the way in.
		if (WvInside(ViewTiles, E.Pos, ViewMargin) || WvInside(ViewTiles, V->Pos, ViewMargin))
		{
			UpdateVisual(*V, E, Alpha, Time, DeltaSeconds);
		}
	}
	for (auto It = Entities.CreateIterator(); It; ++It)
	{
		FEntityVisual& V = It.Value();
		if (V.bSeen)
		{
			continue;
		}
		if (V.DeathTime < 0.f)
		{
			V.DeathTime = Time;
			BhUE::DestroyMeshComponent(V.Scaffold);
		}
		const float Age = Time - V.DeathTime;
		if (Age > bh::DeathDuration(V.Type) + 0.05f)
		{
			DestroyVisual(V);
			It.RemoveCurrent();
			continue;
		}
		bh::AnimInput In = V.Last;
		In.Time = Time;
		In.DeathAge = Age;
		In.bMoving = false;
		ApplyPose(V, In);
	}

	UpdateMapChanges(S);
	UpdateSelection(S, Time);
	UpdatePlacement(S, Time);
	UpdateMarkers(S, Time);
	Assets->FlushPalette();
}

void FBhWorldView::UpdateMapChanges(bh::Session& S)
{
	bh::GameMap& Map = S.GetWorld().GetMap();
	for (const bh::Tile& T : Map.FelledTrees)
	{
		if (FChunk* C = ChunkAt(T.X, T.Y))
		{
			C->bTreesDirty = true;
		}
	}
	Map.FelledTrees.clear();
	if (bPropsStale)
	{
		bh::GenerateProps(Map, Props);
		bPropsStale = false;
	}
	int32 Budget = MaxRebuildsPerFrame;
	for (FChunk& C : Chunks)
	{
		if (Budget <= 0)
		{
			break;
		}
		if (C.bTreesDirty)
		{
			RebuildTrees(C, Map);
			--Budget;
		}
		if (C.bPropsDirty && Budget > 0)
		{
			RebuildProps(C);
			--Budget;
		}
	}
	for (int32 I = 0; I < Ruins.Num() && I < static_cast<int32>(Map.BeaconSites.size()); ++I)
	{
		if (Ruins[I] != nullptr)
		{
			const bh::TileRect& Site = Map.BeaconSites[static_cast<size_t>(I)];
			const bool bFree = Map.At(Site.X0, Site.Y0).Occupant == bh::NoEntity;
			if (Ruins[I]->IsVisible() != bFree)
			{
				Ruins[I]->SetVisibility(bFree);
			}
		}
	}
}

void FBhWorldView::UpdateSelection(const bh::Session& S, float Time)
{
	for (TPair<bh::EntityId, FEntityVisual>& Pair : Entities)
	{
		Pair.Value.bSelected = false;
	}
	const std::vector<bh::EntityId>& Selection = S.GetControl().Selection;
	int32 Used = 0;
	const float Pulse = 1.f + 0.04f * FMath::Sin(Time * 5.f);
	for (bh::EntityId Id : Selection)
	{
		FEntityVisual* V = Entities.Find(Id);
		if (V == nullptr || V->DeathTime >= 0.f)
		{
			continue;
		}
		V->bSelected = true;
		if (Used >= SelectionRings.Num())
		{
			SelectionRings.Add(NewDecal(bh::DecalTex::SelectionRing, WvRingColor(bh::Team::Player), 1));
		}
		UStaticMeshComponent* Ring = SelectionRings[Used++];
		if (Ring == nullptr)
		{
			continue;
		}
		UMaterialInstanceDynamic* Mat = V->Owner == bh::Team::Player ? RingOwn : (V->Owner == bh::Team::Enemy ? RingEnemy : RingNeutral);
		if (Ring->GetMaterial(0) != Mat)
		{
			Ring->SetMaterial(0, Mat);
		}
		const float Size = (V->Kind == bh::EntityKind::Unit ? V->Width * 1.25f : V->Width * 1.18f) * Pulse;
		Ring->SetWorldTransform(FTransform(FRotator::ZeroRotator, BhUE::ToWorld(V->Pos, 0.03f), FVector(Size, Size, 1.f)));
		if (!Ring->IsVisible())
		{
			Ring->SetVisibility(true);
		}
	}
	for (int32 I = Used; I < SelectionRings.Num(); ++I)
	{
		if (SelectionRings[I] != nullptr && SelectionRings[I]->IsVisible())
		{
			SelectionRings[I]->SetVisibility(false);
		}
	}

	// Rally point flag and tower range for a single selected building of ours.
	const bh::World& W = S.GetWorld();
	const bh::Entity* Single = Selection.size() == 1 ? W.Find(Selection.front()) : nullptr;
	const bool bOwnBuilding = Single != nullptr && Single->IsBuilding() && Single->Owner == bh::Team::Player;
	const bool bRally = bOwnBuilding && Single->bHasRally && Single->bConstructed;
	if (bRally && RallyFlag == nullptr)
	{
		RallyFlag = BhUE::NewMeshComponent(Owner, Assets->GetFxMesh(bh::FxMesh::RallyFlag));
	}
	if (RallyFlag != nullptr)
	{
		RallyFlag->SetVisibility(bRally);
		if (bRally)
		{
			RallyFlag->SetWorldLocationAndRotation(BhUE::ToWorld(Single->RallyPoint), FRotator(0.f, 20.f * FMath::Sin(Time * 1.7f), 0.f));
		}
	}
	const bool bRange = Single != nullptr && Single->IsBuilding() && bh::GetDef(Single->Type).IsTower && Single->bConstructed;
	if (bRange && RangeRing == nullptr)
	{
		RangeRing = NewDecal(bh::DecalTex::RangeRing, FLinearColor(1.f, 0.9f, 0.55f, 0.6f), 0);
	}
	if (RangeRing != nullptr)
	{
		RangeRing->SetVisibility(bRange);
		if (bRange)
		{
			const float Reach = (W.GetRange(*Single) + static_cast<float>(bh::GetDef(Single->Type).Footprint) * 0.5f) * 2.f;
			RangeRing->SetWorldTransform(FTransform(FRotator::ZeroRotator, BhUE::ToWorld(Single->Pos, 0.02f), FVector(Reach, Reach, 1.f)));
		}
	}
}

void FBhWorldView::UpdatePlacement(const bh::Session& S, float Time)
{
	const bh::PlayerControl& C = S.GetControl();
	const bool bPlacing = C.bPlacing && C.PlaceType != bh::Archetype::None;
	if (!bPlacing)
	{
		if (Ghost != nullptr && Ghost->IsVisible())
		{
			Ghost->SetVisibility(false);
		}
		for (UStaticMeshComponent* Cell : PlaceCells)
		{
			if (Cell != nullptr && Cell->IsVisible())
			{
				Cell->SetVisibility(false);
			}
		}
		for (UStaticMeshComponent* Glow : SiteGlows)
		{
			if (Glow != nullptr && Glow->IsVisible())
			{
				Glow->SetVisibility(false);
			}
		}
		return;
	}
	const bh::TileRect Rect = C.PlacementRect();
	const bool bValid = C.PlaceState == bh::PlaceResult::Ok;
	if (Ghost == nullptr)
	{
		Ghost = BhUE::NewMeshComponent(Owner, nullptr);
	}
	if (Ghost != nullptr)
	{
		if (GhostType != C.PlaceType)
		{
			GhostType = C.PlaceType;
			Ghost->SetStaticMesh(Assets->GetBodyMesh(C.PlaceType, bh::Team::Player));
		}
		// The ghost bobs gently above the ground so it reads as "not built yet".
		const float Lift = 0.06f + 0.04f * FMath::Sin(Time * 4.f);
		Ghost->SetWorldTransform(FTransform(FRotator(0.f, bh::BuildingYaw, 0.f), BhUE::ToWorld(Rect.Center(), Lift), FVector(1.f, 1.f, bValid ? 1.f : 0.9f)));
		Ghost->SetVisibility(true);
	}
	int32 Used = 0;
	for (int32 Y = Rect.Y0; Y < Rect.Y1; ++Y)
	{
		for (int32 X = Rect.X0; X < Rect.X1; ++X)
		{
			if (Used >= PlaceCells.Num())
			{
				PlaceCells.Add(NewDecal(bh::DecalTex::PlacementCell, FLinearColor::White, 3));
			}
			UStaticMeshComponent* Cell = PlaceCells[Used++];
			if (Cell == nullptr)
			{
				continue;
			}
			UMaterialInstanceDynamic* Mat = bValid ? PlaceValid : PlaceInvalid;
			if (Cell->GetMaterial(0) != Mat)
			{
				Cell->SetMaterial(0, Mat);
			}
			Cell->SetWorldTransform(FTransform(FRotator::ZeroRotator, BhUE::ToWorld(bh::Vec2(static_cast<float>(X) + 0.5f, static_cast<float>(Y) + 0.5f), 0.04f), FVector(0.96f, 0.96f, 1.f)));
			Cell->SetVisibility(true);
		}
	}
	for (int32 I = Used; I < PlaceCells.Num(); ++I)
	{
		if (PlaceCells[I] != nullptr)
		{
			PlaceCells[I]->SetVisibility(false);
		}
	}
	// Beacons only go on beacon sites: light up the free ones.
	const bh::GameMap& Map = S.GetWorld().GetMap();
	int32 Glows = 0;
	if (bh::GetDef(C.PlaceType).NeedsBeaconSite)
	{
		for (const bh::TileRect& Site : Map.BeaconSites)
		{
			if (Map.At(Site.X0, Site.Y0).Occupant != bh::NoEntity)
			{
				continue;
			}
			if (Glows >= SiteGlows.Num())
			{
				SiteGlows.Add(NewDecal(bh::DecalTex::SoftGlow, FLinearColor(1.f, 0.8f, 0.35f, 0.55f), 0));
			}
			UStaticMeshComponent* Glow = SiteGlows[Glows++];
			if (Glow != nullptr)
			{
				const float Size = 3.2f + 0.3f * FMath::Sin(Time * 3.f);
				Glow->SetWorldTransform(FTransform(FRotator::ZeroRotator, BhUE::ToWorld(Site.Center(), 0.02f), FVector(Size, Size, 1.f)));
				Glow->SetVisibility(true);
			}
		}
	}
	for (int32 I = Glows; I < SiteGlows.Num(); ++I)
	{
		if (SiteGlows[I] != nullptr)
		{
			SiteGlows[I]->SetVisibility(false);
		}
	}
}

void FBhWorldView::UpdateMarkers(const bh::Session& S, float Time)
{
	if (bTutorialMarker && TutorialRing == nullptr)
	{
		TutorialRing = NewDecal(bh::DecalTex::SelectionRing, FLinearColor(1.f, 0.82f, 0.3f, 1.f), 4);
	}
	if (TutorialRing != nullptr)
	{
		TutorialRing->SetVisibility(bTutorialMarker);
		if (bTutorialMarker)
		{
			const float Size = 1.6f + 0.35f * FMath::Abs(FMath::Sin(Time * 3.f));
			TutorialRing->SetWorldTransform(FTransform(FRotator::ZeroRotator, BhUE::ToWorld(TutorialPos, 0.05f), FVector(Size, Size, 1.f)));
		}
	}
}

// ============================================================================ HUD queries

void FBhWorldView::GetBars(bool bShowAll, const FBox2D& ViewTiles, TArray<FBhBarInfo>& Out) const
{
	Out.Reset();
	for (const TPair<bh::EntityId, FEntityVisual>& Pair : Entities)
	{
		const FEntityVisual& V = Pair.Value;
		if (!V.bSeen || V.DeathTime >= 0.f || V.Kind == bh::EntityKind::Resource || !WvInside(ViewTiles, V.Pos, 0.5f))
		{
			continue;
		}
		const bool bDamaged = V.HpRatio < 0.999f;
		const bool bBuilding = V.Kind == bh::EntityKind::Building;
		if (!(bShowAll || V.bSelected || bDamaged || !V.bConstructed))
		{
			continue;
		}
		FBhBarInfo& Info = Out.AddDefaulted_GetRef();
		Info.Top = BhUE::ToWorld(V.Pos, V.Height * (V.bConstructed ? 1.f : FMath::Max(0.3f, V.BuildProgress)) + 0.25f);
		Info.HpRatio = V.HpRatio;
		Info.Progress = V.bConstructed ? -1.f : V.BuildProgress;
		Info.Width = V.Width;
		Info.Owner = V.Owner;
		Info.bSelected = V.bSelected;
		Info.bBuilding = bBuilding;
	}
}

bool FBhWorldView::GetEntityLocation(bh::EntityId Id, FVector& OutTop) const
{
	const FEntityVisual* V = Entities.Find(Id);
	if (V == nullptr)
	{
		return false;
	}
	OutTop = BhUE::ToWorld(V->Pos, V->Height);
	return true;
}
