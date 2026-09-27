// Beaconhold - runtime content factory.
//
// The game ships without art assets: every texture (icons, UI panels, decals, the colour
// palette) is painted and every mesh is built at runtime from the simulation core's
// descriptions (Sim/BhPainter, Sim/BhRender). This object owns all of it for the whole session.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "BhPainter.h"
#include "BhRender.h"

#include "BhAssets.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UTexture;
class UTexture2D;

UCLASS()
class UBhAssets : public UObject
{
	GENERATED_BODY()

public:
	// Paints textures and loads the engine materials. Safe to call more than once.
	void Initialize();
	bool IsInitialized() const { return bInitialized; }

	// ------------------------------------------------------------------ Palette & meshes
	bh::ColorPalette& GetPalette() { return Palette; }
	// Uploads palette colours added since the last call (call after building meshes).
	void FlushPalette();

	// Builds a mesh from render buffers: section 0 uses the palette material, section 1 the
	// shadow material. Returns nullptr for empty buffers.
	UStaticMesh* BuildMesh(const bh::RenderMesh& Mesh, const TCHAR* DebugName);
	// Lets the garbage collector reclaim a mesh made by BuildMesh once nothing uses it.
	void ReleaseMesh(UStaticMesh* Mesh);

	UStaticMesh* GetBodyMesh(bh::Archetype A, bh::Team T);
	// Mesh of the parts of a model with the given role; nullptr if it has none.
	UStaticMesh* GetRoleMesh(bh::Archetype A, bh::Team T, bh::PartRole Role, FVector& OutPivot);
	UStaticMesh* GetScaffoldMesh(int32 Footprint);
	UStaticMesh* GetRuinMesh();
	UStaticMesh* GetFxMesh(bh::FxMesh Kind);
	// One-tile quad with UVs 0..1; give it a decal material.
	UStaticMesh* GetDecalMesh();

	// ------------------------------------------------------------------ Materials
	UMaterialInterface* GetPaletteMaterial() const;
	UMaterialInterface* GetShadowMaterial() const;
	// A decal material instance of its own (so its tint can be animated).
	UMaterialInstanceDynamic* CreateDecalMaterial(bh::DecalTex Tex, const FLinearColor& Tint);
	// A decal material instance shared by everything with the same texture and tint.
	UMaterialInstanceDynamic* GetSharedDecalMaterial(bh::DecalTex Tex, const FLinearColor& Tint);
	static void SetDecalTint(UMaterialInstanceDynamic* Material, const FLinearColor& Tint);

	// ------------------------------------------------------------------ Textures
	UTexture2D* GetIcon(bh::Icon I) const;
	UTexture2D* GetUiTexture(bh::UiTex T) const;

	static UTexture2D* CreateTexture(const bh::ImageRGBA& Image, bool bSRGB, bool bNearest, FName Name);
	// Replaces the texels of a texture made by CreateTexture (same size).
	static void UpdateTexture(UTexture2D* Texture, const bh::ImageRGBA& Image);

private:
	UMaterialInterface* LoadMaterial(const TCHAR* Primary, const TCHAR* Fallback) const;
	UMaterialInstanceDynamic* MakeTexturedMaterial(UMaterialInterface* Parent, UTexture* Texture, const FLinearColor& Tint);
	UStaticMesh* FindMesh(uint64 Key) const;
	UStaticMesh* CacheMesh(uint64 Key, const bh::RenderMesh& Mesh, const TCHAR* DebugName);

	UPROPERTY()
	TObjectPtr<UTexture2D> PaletteTexture;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PaletteParent;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> DecalParent;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PaletteMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ShadowMaterial;

	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> IconTextures;

	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> UiTextures;

	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> DecalTextures;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DecalMaterials;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> Meshes;

	// Lookups into the arrays above (which keep the objects alive).
	TMap<uint64, UStaticMesh*> MeshCache;
	TMap<uint64, FVector> PivotCache;
	TMap<uint64, UMaterialInstanceDynamic*> SharedDecalCache;

	bh::ColorPalette Palette;
	uint32 UploadedPaletteRevision = 0;
	bool bInitialized = false;
};
