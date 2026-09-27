// Beaconhold — the visible world: terrain, forest, props, ruins, units and buildings (with
// procedural animation), selection rings, placement preview and markers. It mirrors the
// simulation every frame and owns every world component it creates.
#pragma once

#include "CoreMinimal.h"

#include "BhRender.h"
#include "BhSession.h"

class AActor;
class UBhAssets;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;

// What the HUD needs to draw a health or progress bar above an entity.
struct FBhBarInfo
{
	FVector Top = FVector::ZeroVector; // world position just above the model
	float HpRatio = 1.f;
	float Progress = -1.f; // construction progress, or negative
	float Width = 1.f;     // tiles
	bh::Team Owner = bh::Team::Neutral;
	bool bSelected = false;
	bool bBuilding = false;
};

class FBhWorldView
{
public:
	void Init(AActor* InOwner, UBhAssets* InAssets);
	// Builds terrain, trees, props, ruins and the initial entities for the session's map.
	void BuildWorld(const bh::Session& S);
	// Removes everything (mission change).
	void Clear();

	// Per frame. Time is the presentation clock (seconds); ViewTiles the visible ground area.
	void Tick(bh::Session& S, float Alpha, float Time, float DeltaSeconds, const FBox2D& ViewTiles);

	// Event hooks from the simulation.
	void NotifySwing(bh::EntityId Id, float Time);
	void NotifyHit(bh::EntityId Id, float Time);
	void NotifyFootprintChanged(const bh::TileRect& Rect);
	void SetTutorialMarker(bool bShow, const bh::Vec2& Pos);

	void GetBars(bool bShowAll, const FBox2D& ViewTiles, TArray<FBhBarInfo>& Out) const;
	bool GetEntityLocation(bh::EntityId Id, FVector& OutTop) const;

private:
	struct FPartVisual
	{
		bh::PartRole Role = bh::PartRole::Static;
		UStaticMeshComponent* Comp = nullptr;
		FVector Pivot = FVector::ZeroVector;
		bool bVisible = true;
	};

	struct FEntityVisual
	{
		bh::EntityId Id = bh::NoEntity;
		bh::Archetype Type = bh::Archetype::None;
		bh::Team Owner = bh::Team::Neutral;
		bh::EntityKind Kind = bh::EntityKind::Unit;
		UStaticMeshComponent* Body = nullptr;
		UStaticMeshComponent* Scaffold = nullptr;
		TArray<FPartVisual> Parts;
		bh::Vec2 Pos;
		float VisualYaw = 0.f;
		float Scale = 1.f;
		float Height = 1.f; // tiles, including scale
		float Width = 1.f;  // tiles
		float SpawnTime = -100.f;
		float DeathTime = -1.f;
		float HitTime = -100.f;
		float SwingTime = -100.f;
		uint32 AttackSerial = 0;
		int32 InitialAmount = 1;
		float HpRatio = 1.f;
		bool bConstructed = true;
		float BuildProgress = 1.f;
		bool bSeen = false;
		bool bVisible = true;
		bool bSelected = false;
		bh::AnimInput Last; // last animation state (used while dying)
	};

	struct FChunk
	{
		bh::TileRect Rect;
		UStaticMeshComponent* Terrain = nullptr;
		UStaticMeshComponent* Trees = nullptr;
		UStaticMeshComponent* Props = nullptr;
		UStaticMesh* TerrainMesh = nullptr;
		UStaticMesh* TreeMesh = nullptr;
		UStaticMesh* PropMesh = nullptr;
		bool bTreesDirty = false;
		bool bPropsDirty = false;
	};

	FEntityVisual& CreateVisual(const bh::Entity& E, float Time);
	void DestroyVisual(FEntityVisual& V);
	void UpdateVisual(FEntityVisual& V, const bh::Entity& E, float Alpha, float Time, float DeltaSeconds);
	void ApplyPose(FEntityVisual& V, const bh::AnimInput& In);
	void SetVisualVisible(FEntityVisual& V, bool bVisible);
	void RebuildTrees(FChunk& C, const bh::GameMap& Map);
	void RebuildProps(FChunk& C);
	void UpdateMapChanges(bh::Session& S);
	void UpdateSelection(const bh::Session& S, float Time);
	void UpdatePlacement(const bh::Session& S, float Time);
	void UpdateMarkers(const bh::Session& S, float Time);
	FChunk* ChunkAt(int32 TileX, int32 TileY);
	UStaticMeshComponent* NewDecal(bh::DecalTex Tex, const FLinearColor& Tint, int32 SortPriority);

	AActor* Owner = nullptr;
	UBhAssets* Assets = nullptr;

	// Map data kept for chunk rebuilds.
	int32 MapW = 0;
	int32 MapH = 0;
	std::vector<bh::GroundCell> Cells;
	std::vector<bh::Rgb> GroundColors;
	std::vector<bh::TreeInstance> Trees;
	std::vector<bh::PropInstance> Props;
	bool bPropsStale = false; // regenerate props (a footprint changed)

	TArray<FChunk> Chunks;
	int32 ChunkOriginX = 0;
	int32 ChunkOriginY = 0;
	int32 ChunksX = 0;
	int32 ChunksY = 0;
	UStaticMeshComponent* Floor = nullptr;
	UStaticMesh* FloorMesh = nullptr;
	TArray<UStaticMeshComponent*> Ruins;

	TMap<bh::EntityId, FEntityVisual> Entities;

	// Overlays.
	TArray<UStaticMeshComponent*> SelectionRings;
	UMaterialInstanceDynamic* RingOwn = nullptr;
	UMaterialInstanceDynamic* RingEnemy = nullptr;
	UMaterialInstanceDynamic* RingNeutral = nullptr;
	UStaticMeshComponent* Ghost = nullptr;
	bh::Archetype GhostType = bh::Archetype::None;
	TArray<UStaticMeshComponent*> PlaceCells;
	UMaterialInstanceDynamic* PlaceValid = nullptr;
	UMaterialInstanceDynamic* PlaceInvalid = nullptr;
	TArray<UStaticMeshComponent*> SiteGlows;
	UStaticMeshComponent* RallyFlag = nullptr;
	UStaticMeshComponent* RangeRing = nullptr;
	UStaticMeshComponent* TutorialRing = nullptr;
	bool bTutorialMarker = false;
	bh::Vec2 TutorialPos;
};
