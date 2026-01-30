#include "LevelVisuals.h"
#include "LevelBlock.h"
#include "EngineUtils.h"
#include "Landscape.h"

ALevelVisuals::ALevelVisuals()
{
	PrimaryActorTick.bCanEverTick = false;

	StyleIndex = 0;
}

void ALevelVisuals::BeginPlay()
{
	Super::BeginPlay();

	// Cache components by name
	CachedScene = Cast<USceneComponent>(GetDefaultSubobjectByName(TEXT("Scene")));
	CachedSkyLight = Cast<USkyLightComponent>(GetDefaultSubobjectByName(TEXT("SkyLight")));
	CachedDirectionalLight = Cast<UDirectionalLightComponent>(GetDefaultSubobjectByName(TEXT("DirectionalLight")));
	CachedExponentialHeightFog = Cast<UExponentialHeightFogComponent>(GetDefaultSubobjectByName(TEXT("ExponentialHeightFog")));
	CachedPostProcess = Cast<UPostProcessComponent>(GetDefaultSubobjectByName(TEXT("PostProcess")));
	CachedDecal = Cast<UDecalComponent>(GetDefaultSubobjectByName(TEXT("Decal")));

	UpdateLevelVisuals();
}

void ALevelVisuals::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Cache components by name during construction
	CachedScene = Cast<USceneComponent>(GetDefaultSubobjectByName(TEXT("Scene")));
	CachedSkyLight = Cast<USkyLightComponent>(GetDefaultSubobjectByName(TEXT("SkyLight")));
	CachedDirectionalLight = Cast<UDirectionalLightComponent>(GetDefaultSubobjectByName(TEXT("DirectionalLight")));
	CachedExponentialHeightFog = Cast<UExponentialHeightFogComponent>(GetDefaultSubobjectByName(TEXT("ExponentialHeightFog")));
	CachedPostProcess = Cast<UPostProcessComponent>(GetDefaultSubobjectByName(TEXT("PostProcess")));
	CachedDecal = Cast<UDecalComponent>(GetDefaultSubobjectByName(TEXT("Decal")));

	UpdateLevelVisuals();
}

void ALevelVisuals::SetLevelStyle(int32 Index)
{
	StyleIndex = Index;
	UpdateLevelVisuals();
}

void ALevelVisuals::UpdateVisuals()
{
	UpdateLevelVisuals();
}

void ALevelVisuals::UpdateLevelVisuals()
{
	// Validate StyleIndex
	if (!LevelStyles.IsValidIndex(StyleIndex))
	{
		return;
	}

	const FS_LevelStyle& CurrentStyle = LevelStyles[StyleIndex];

	// Update fog settings
	if (CachedExponentialHeightFog)
	{
		CachedExponentialHeightFog->FogInscatteringLuminance = CurrentStyle.FogColor;
		CachedExponentialHeightFog->FogDensity = static_cast<float>(CurrentStyle.FogDensity);
		CachedExponentialHeightFog->MarkRenderStateDirty();
	}

	// Update decal color
	if (CachedDecal)
	{
		// Set decal color parameter if the material has one
		// Note: This assumes the decal material has a "Color" or "DecalColor" parameter
		if (UMaterialInstanceDynamic* DecalMaterial = Cast<UMaterialInstanceDynamic>(CachedDecal->GetDecalMaterial()))
		{
			DecalMaterial->SetVectorParameterValue(TEXT("Color"), CurrentStyle.DecalColor);
		}
		CachedDecal->MarkRenderStateDirty();
	}

	// Find all LevelBlock actors in the world and update their materials
	UWorld* World = GetWorld();
	if (World)
	{
		for (TActorIterator<ALevelBlock> It(World); It; ++It)
		{
			ALevelBlock* Block = *It;
			if (Block)
			{
				// Get the color group name for this block
				FName ColorGroup = Block->ColorGroup;

				// Find the material params for this color group in the current style
				if (const FS_GridMaterialParams* Params = CurrentStyle.BlockColors.Find(ColorGroup))
				{
					// Call UpdateMaterials on the block with the params from the style
					Block->UpdateMaterials(*Params);
				}
			}
		}
	}
}
