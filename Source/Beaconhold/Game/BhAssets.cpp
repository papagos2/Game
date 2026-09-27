// Beaconhold - runtime content factory.
#include "Game/BhAssets.h"

#include "Game/BhCommon.h"
#include "Game/BhSettings.h"

#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "TextureResource.h"

namespace
{
// Texture parameter names of the engine's widget pass-through materials.
const FName SlateTextureParam(TEXT("SlateUI"));
const FName TintParam(TEXT("TintColorAndOpacity"));
const FName OpacityFromTextureParam(TEXT("OpacityFromTexture"));

constexpr int32 IconSize = 128;
constexpr int32 UiTextureSize = 96;
constexpr int32 DecalSize = 128;

enum class EMeshKind : uint8
{
	Body = 1,
	Role,
	Scaffold,
	Ruin,
	Fx,
	Decal,
};

uint64 MeshKey(EMeshKind Kind, uint32 A = 0, uint32 B = 0, uint32 C = 0)
{
	return (static_cast<uint64>(Kind) << 48) | (static_cast<uint64>(A & 0xFFFF) << 32) | (static_cast<uint64>(B & 0xFFFF) << 16) | static_cast<uint64>(C & 0xFFFF);
}

// Simulation images are RGBA; the textures are BGRA.
void CopyToBgra(const bh::ImageRGBA& Image, uint8* Dest)
{
	const int32 Count = Image.W * Image.H;
	for (int32 I = 0; I < Count; ++I)
	{
		const uint8* S = &Image.Px[static_cast<size_t>(I) * 4];
		uint8* D = Dest + static_cast<size_t>(I) * 4;
		D[0] = S[2];
		D[1] = S[1];
		D[2] = S[0];
		D[3] = S[3];
	}
}

FVector3f AnyTangent(const FVector3f& N)
{
	const FVector3f Axis = FMath::Abs(N.Z) < 0.9f ? FVector3f(0.f, 0.f, 1.f) : FVector3f(1.f, 0.f, 0.f);
	FVector3f T = FVector3f::CrossProduct(Axis, N);
	if (!T.Normalize())
	{
		T = FVector3f(1.f, 0.f, 0.f);
	}
	return T;
}
} // namespace

// ============================================================================ Initialization

void UBhAssets::Initialize()
{
	if (bInitialized)
	{
		return;
	}
	bInitialized = true;
	const double StartTime = FPlatformTime::Seconds();

	// Materials.
	const UBhSettings& Settings = UBhSettings::Get();
	PaletteParent = Settings.PaletteMaterial.LoadSynchronous();
	if (PaletteParent == nullptr)
	{
		PaletteParent = LoadMaterial(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque_OneSided.Widget3DPassThrough_Opaque_OneSided"),
			TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque.Widget3DPassThrough_Opaque"));
	}
	DecalParent = Settings.DecalMaterial.LoadSynchronous();
	if (DecalParent == nullptr)
	{
		DecalParent = LoadMaterial(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided.Widget3DPassThrough_Translucent_OneSided"),
			TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent"));
	}
	if (PaletteParent == nullptr || DecalParent == nullptr)
	{
		UE_LOG(LogBeaconhold, Error, TEXT("Beaconhold: engine pass-through materials not found; the world will render with the default material."));
	}

	// Palette texture (point sampled: every vertex samples a cell centre).
	bh::ImageRGBA PaletteImage;
	PaletteImage.W = bh::PaletteColumns;
	PaletteImage.H = bh::PaletteRows;
	PaletteImage.Px = Palette.GetPixels();
	PaletteTexture = CreateTexture(PaletteImage, true, true, TEXT("BhPalette"));
	UploadedPaletteRevision = Palette.GetRevision();
	PaletteMaterial = MakeTexturedMaterial(PaletteParent, PaletteTexture, FLinearColor::White);

	// Decals (shadows, rings, markers).
	for (int32 D = 0; D < static_cast<int32>(bh::DecalTex::Count); ++D)
	{
		bh::ImageRGBA Image;
		bh::PaintDecal(static_cast<bh::DecalTex>(D), DecalSize, Image);
		DecalTextures.Add(CreateTexture(Image, true, false, *FString::Printf(TEXT("BhDecal%d"), D)));
	}
	ShadowMaterial = MakeTexturedMaterial(DecalParent, DecalTextures[static_cast<int32>(bh::DecalTex::BlobShadow)], FLinearColor::White);

	// Icons and UI panels.
	for (int32 I = 0; I < bh::NumIcons; ++I)
	{
		bh::ImageRGBA Image;
		if (I != static_cast<int32>(bh::Icon::None))
		{
			bh::PaintIcon(static_cast<bh::Icon>(I), IconSize, Image);
		}
		else
		{
			Image.Init(4, 4);
		}
		IconTextures.Add(CreateTexture(Image, true, false, *FString::Printf(TEXT("BhIcon%d"), I)));
	}
	for (int32 T = 0; T < bh::NumUiTex; ++T)
	{
		bh::ImageRGBA Image;
		bh::PaintUiTexture(static_cast<bh::UiTex>(T), UiTextureSize, Image);
		UiTextures.Add(CreateTexture(Image, true, false, *FString::Printf(TEXT("BhUi%d"), T)));
	}

	UE_LOG(LogBeaconhold, Log, TEXT("Beaconhold: runtime content ready in %.0f ms"), (FPlatformTime::Seconds() - StartTime) * 1000.0);
}

UMaterialInterface* UBhAssets::LoadMaterial(const TCHAR* Primary, const TCHAR* Fallback) const
{
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Primary))
	{
		return M;
	}
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Fallback))
	{
		return M;
	}
	return UMaterial::GetDefaultMaterial(MD_Surface);
}

UMaterialInstanceDynamic* UBhAssets::MakeTexturedMaterial(UMaterialInterface* Parent, UTexture* Texture, const FLinearColor& Tint)
{
	if (Parent == nullptr)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Parent, this);
	if (Mid != nullptr)
	{
		Mid->SetTextureParameterValue(SlateTextureParam, Texture);
		Mid->SetVectorParameterValue(TintParam, Tint);
		Mid->SetScalarParameterValue(OpacityFromTextureParam, 1.f);
		DecalMaterials.Add(Mid);
	}
	return Mid;
}

// ============================================================================ Textures

UTexture2D* UBhAssets::CreateTexture(const bh::ImageRGBA& Image, bool bSRGB, bool bNearest, FName Name)
{
	if (Image.W <= 0 || Image.H <= 0 || Image.Px.size() != static_cast<size_t>(Image.W) * static_cast<size_t>(Image.H) * 4)
	{
		return nullptr;
	}
	UTexture2D* Texture = UTexture2D::CreateTransient(Image.W, Image.H, PF_B8G8R8A8, Name);
	if (Texture == nullptr)
	{
		return nullptr;
	}
	Texture->SRGB = bSRGB;
	Texture->Filter = bNearest ? TF_Nearest : TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->NeverStream = true;
	if (FTexturePlatformData* Platform = Texture->GetPlatformData())
	{
		if (Platform->Mips.Num() > 0)
		{
			FTexture2DMipMap& Mip = Platform->Mips[0];
			if (uint8* Dest = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE)))
			{
				CopyToBgra(Image, Dest);
			}
			Mip.BulkData.Unlock();
		}
	}
	Texture->UpdateResource();
	return Texture;
}

void UBhAssets::UpdateTexture(UTexture2D* Texture, const bh::ImageRGBA& Image)
{
	if (Texture == nullptr || Texture->GetSizeX() != Image.W || Texture->GetSizeY() != Image.H || Image.Px.empty())
	{
		return;
	}
	// Keep the CPU copy current too, in case the resource is ever recreated.
	if (FTexturePlatformData* Platform = Texture->GetPlatformData())
	{
		if (Platform->Mips.Num() > 0)
		{
			FTexture2DMipMap& Mip = Platform->Mips[0];
			if (uint8* Dest = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE)))
			{
				CopyToBgra(Image, Dest);
			}
			Mip.BulkData.Unlock();
		}
	}
	const int32 Bytes = Image.W * Image.H * 4;
	uint8* Data = new uint8[Bytes];
	CopyToBgra(Image, Data);
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, static_cast<uint32>(Image.W), static_cast<uint32>(Image.H));
	Texture->UpdateTextureRegions(0, 1, Region, static_cast<uint32>(Image.W * 4), 4, Data,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete[] SrcData;
			delete Regions;
		});
}

UTexture2D* UBhAssets::GetIcon(bh::Icon I) const
{
	const int32 Index = static_cast<int32>(I);
	return IconTextures.IsValidIndex(Index) ? IconTextures[Index].Get() : nullptr;
}

UTexture2D* UBhAssets::GetUiTexture(bh::UiTex T) const
{
	const int32 Index = static_cast<int32>(T);
	return UiTextures.IsValidIndex(Index) ? UiTextures[Index].Get() : nullptr;
}

void UBhAssets::FlushPalette()
{
	if (PaletteTexture == nullptr || Palette.GetRevision() == UploadedPaletteRevision)
	{
		return;
	}
	UploadedPaletteRevision = Palette.GetRevision();
	bh::ImageRGBA Image;
	Image.W = bh::PaletteColumns;
	Image.H = bh::PaletteRows;
	Image.Px = Palette.GetPixels();
	UpdateTexture(PaletteTexture, Image);
}

// ============================================================================ Materials

UMaterialInterface* UBhAssets::GetPaletteMaterial() const
{
	return PaletteMaterial != nullptr ? static_cast<UMaterialInterface*>(PaletteMaterial.Get()) : PaletteParent.Get();
}

UMaterialInterface* UBhAssets::GetShadowMaterial() const
{
	return ShadowMaterial != nullptr ? static_cast<UMaterialInterface*>(ShadowMaterial.Get()) : DecalParent.Get();
}

UMaterialInstanceDynamic* UBhAssets::CreateDecalMaterial(bh::DecalTex Tex, const FLinearColor& Tint)
{
	const int32 Index = static_cast<int32>(Tex);
	if (!DecalTextures.IsValidIndex(Index))
	{
		return nullptr;
	}
	return MakeTexturedMaterial(DecalParent, DecalTextures[Index], Tint);
}

UMaterialInstanceDynamic* UBhAssets::GetSharedDecalMaterial(bh::DecalTex Tex, const FLinearColor& Tint)
{
	const FColor Q = Tint.ToFColor(false);
	const uint64 Key = (static_cast<uint64>(Tex) << 32) | (static_cast<uint64>(Q.R) << 24) | (static_cast<uint64>(Q.G) << 16) | (static_cast<uint64>(Q.B) << 8) | static_cast<uint64>(Q.A);
	if (UMaterialInstanceDynamic** Found = SharedDecalCache.Find(Key))
	{
		return *Found;
	}
	UMaterialInstanceDynamic* Mid = CreateDecalMaterial(Tex, Tint);
	SharedDecalCache.Add(Key, Mid);
	return Mid;
}

void UBhAssets::SetDecalTint(UMaterialInstanceDynamic* Material, const FLinearColor& Tint)
{
	if (Material != nullptr)
	{
		Material->SetVectorParameterValue(TintParam, Tint);
	}
}

// ============================================================================ Meshes

UStaticMesh* UBhAssets::BuildMesh(const bh::RenderMesh& Source, const TCHAR* DebugName)
{
	if (Source.Empty())
	{
		return nullptr;
	}
	FMeshDescription Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();
	TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
	TPolygonGroupAttributesRef<FName> SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
	UVs.SetNumChannels(1);

	// The mesh builder winds front faces counter-clockwise (right-handed maths); Unreal's
	// front faces are the other way round, so each triangle is flipped on the way in.
	const bool bReverse = UBhSettings::Get().bReverseTriangleWinding;

	UStaticMesh* Mesh = NewObject<UStaticMesh>(this, MakeUniqueObjectName(this, UStaticMesh::StaticClass(), FName(DebugName)));
	TArray<FStaticMaterial>& Materials = Mesh->GetStaticMaterials();

	auto AddSection = [&](const bh::RenderSection& Section, FName Slot, UMaterialInterface* Material)
	{
		if (Section.Empty())
		{
			return;
		}
		const FPolygonGroupID Group = Description.CreatePolygonGroup();
		SlotNames[Group] = Slot;
		// Sections map to materials in polygon-group order (slot names only exist in editor builds).
		FStaticMaterial StaticMaterial(Material, Slot);
		StaticMaterial.UVChannelData.bInitialized = true;
		Materials.Add(StaticMaterial);

		TArray<FVertexInstanceID> Instances;
		Instances.Reserve(static_cast<int32>(Section.Positions.size()));
		for (size_t I = 0; I < Section.Positions.size(); ++I)
		{
			const FVertexID Vertex = Description.CreateVertex();
			Positions[Vertex] = FVector3f(Section.Positions[I].X, Section.Positions[I].Y, Section.Positions[I].Z);
			const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
			const FVector3f N = FVector3f(Section.Normals[I].X, Section.Normals[I].Y, Section.Normals[I].Z).GetSafeNormal();
			Normals[Instance] = N;
			Tangents[Instance] = AnyTangent(N);
			BinormalSigns[Instance] = 1.f;
			UVs.Set(Instance, 0, FVector2f(Section.UVs[I * 2], Section.UVs[I * 2 + 1]));
			Instances.Add(Instance);
		}
		for (size_t T = 0; T + 2 < Section.Indices.size(); T += 3)
		{
			const FVertexInstanceID A = Instances[static_cast<int32>(Section.Indices[T])];
			const FVertexInstanceID B = Instances[static_cast<int32>(Section.Indices[T + 1])];
			const FVertexInstanceID C = Instances[static_cast<int32>(Section.Indices[T + 2])];
			if (bReverse)
			{
				Description.CreateTriangle(Group, {A, C, B});
			}
			else
			{
				Description.CreateTriangle(Group, {A, B, C});
			}
		}
	};
	AddSection(Source.Opaque, FName(TEXT("Main")), GetPaletteMaterial());
	AddSection(Source.Shadow, FName(TEXT("Shadow")), GetShadowMaterial());

	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bBuildSimpleCollision = false;
	Params.bMarkPackageDirty = false;
	Params.bFastBuild = true;
	TArray<const FMeshDescription*> Descriptions;
	Descriptions.Add(&Description);
	if (!Mesh->BuildFromMeshDescriptions(Descriptions, Params))
	{
		UE_LOG(LogBeaconhold, Warning, TEXT("Beaconhold: failed to build mesh %s"), DebugName);
		return nullptr;
	}
	Meshes.Add(Mesh);
	return Mesh;
}

void UBhAssets::ReleaseMesh(UStaticMesh* Mesh)
{
	if (Mesh != nullptr)
	{
		Meshes.RemoveSingleSwap(Mesh);
	}
}

UStaticMesh* UBhAssets::FindMesh(uint64 Key) const
{
	UStaticMesh* const* Found = MeshCache.Find(Key);
	return Found != nullptr ? *Found : nullptr;
}

UStaticMesh* UBhAssets::CacheMesh(uint64 Key, const bh::RenderMesh& Mesh, const TCHAR* DebugName)
{
	UStaticMesh* Built = BuildMesh(Mesh, DebugName);
	MeshCache.Add(Key, Built);
	return Built;
}

UStaticMesh* UBhAssets::GetBodyMesh(bh::Archetype A, bh::Team T)
{
	const uint64 Key = MeshKey(EMeshKind::Body, static_cast<uint32>(A), static_cast<uint32>(T));
	if (MeshCache.Contains(Key))
	{
		return FindMesh(Key);
	}
	bh::RenderMesh Mesh;
	bh::BuildEntityBodyMesh(A, T, Palette, Mesh);
	return CacheMesh(Key, Mesh, *FString::Printf(TEXT("Body_%s"), *BhUE::ToFString(bh::ArchetypeKey(A))));
}

UStaticMesh* UBhAssets::GetRoleMesh(bh::Archetype A, bh::Team T, bh::PartRole Role, FVector& OutPivot)
{
	const uint64 Key = MeshKey(EMeshKind::Role, static_cast<uint32>(A), static_cast<uint32>(T), static_cast<uint32>(Role));
	if (MeshCache.Contains(Key))
	{
		OutPivot = PivotCache.FindRef(Key);
		return FindMesh(Key);
	}
	bh::RenderMesh Mesh;
	bh::Vec3 Pivot;
	const bool bUnit = bh::GetDef(A).Kind == bh::EntityKind::Unit;
	UStaticMesh* Built = nullptr;
	if (bh::BuildRoleMesh(bh::GetModel(A), Role, T, bUnit ? bh::UnitBakeYaw : bh::BuildingYaw, Palette, Mesh, Pivot))
	{
		Built = BuildMesh(Mesh, *FString::Printf(TEXT("Part_%s_%d"), *BhUE::ToFString(bh::ArchetypeKey(A)), static_cast<int32>(Role)));
	}
	OutPivot = BhUE::ToWorld(Pivot);
	MeshCache.Add(Key, Built);
	PivotCache.Add(Key, OutPivot);
	return Built;
}

UStaticMesh* UBhAssets::GetScaffoldMesh(int32 Footprint)
{
	const uint64 Key = MeshKey(EMeshKind::Scaffold, static_cast<uint32>(Footprint));
	if (MeshCache.Contains(Key))
	{
		return FindMesh(Key);
	}
	bh::RenderMesh Mesh;
	bh::BuildScaffoldMesh(Footprint, Palette, Mesh);
	return CacheMesh(Key, Mesh, TEXT("Scaffold"));
}

UStaticMesh* UBhAssets::GetRuinMesh()
{
	const uint64 Key = MeshKey(EMeshKind::Ruin);
	if (MeshCache.Contains(Key))
	{
		return FindMesh(Key);
	}
	bh::RenderMesh Mesh;
	bh::BuildRuinMesh(Palette, Mesh);
	return CacheMesh(Key, Mesh, TEXT("BeaconRuin"));
}

UStaticMesh* UBhAssets::GetFxMesh(bh::FxMesh Kind)
{
	const uint64 Key = MeshKey(EMeshKind::Fx, static_cast<uint32>(Kind));
	if (MeshCache.Contains(Key))
	{
		return FindMesh(Key);
	}
	bh::RenderMesh Mesh;
	bh::BuildFxMesh(Kind, Palette, Mesh);
	return CacheMesh(Key, Mesh, TEXT("Fx"));
}

UStaticMesh* UBhAssets::GetDecalMesh()
{
	const uint64 Key = MeshKey(EMeshKind::Decal);
	if (MeshCache.Contains(Key))
	{
		return FindMesh(Key);
	}
	bh::RenderMesh Mesh;
	bh::BuildDecalQuad(Mesh);
	// The quad's single section is textured by the decal material the component assigns.
	return CacheMesh(Key, Mesh, TEXT("DecalQuad"));
}
