#include "LevelVisuals.h"
#include "LevelBlock.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "Containers/Ticker.h"

ALevelVisuals::ALevelVisuals()
{
	PrimaryActorTick.bCanEverTick = false;

	StyleIndex = 0;
}

void ALevelVisuals::BeginPlay()
{
	Super::BeginPlay();

	// Cache blueprint components by class
	CachedScene = GetRootComponent();
	CachedSkyLight = FindComponentByClass<USkyLightComponent>();
	CachedDirectionalLight = FindComponentByClass<UDirectionalLightComponent>();
	CachedExponentialHeightFog = FindComponentByClass<UExponentialHeightFogComponent>();
	CachedPostProcess = FindComponentByClass<UPostProcessComponent>();
	CachedDecal = FindComponentByClass<UDecalComponent>();

	UpdateLevelVisuals();
}

void ALevelVisuals::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Initialize LevelStyles if empty or invalid
	if (LevelStyles.Num() == 0 || !LevelStyles[0].BlockColors.Contains(TEXT("Floor")))
	{
		LevelStyles.Empty();

		// Style 2 (Purple fog, orange decal) - matches original default
		FS_LevelStyle Style2;
		Style2.FogColor = FLinearColor(0.539931f, 0.447917f, 1.0f);
		Style2.FogDensity = 0.02;
		Style2.DecalColor = FLinearColor(1.0f, 0.5f, 0.0f, 0.4f);

		FS_GridMaterialParams FloorParams;
		FloorParams.GridColor = FLinearColor(0.5f, 0.5f, 0.5f);
		FloorParams.SurfaceColor = FLinearColor(0.258463f, 0.236978f, 0.541667f);
		FloorParams.GridSizes = FVector(100.0, 200.0, 800.0);
		FloorParams.Specularity = 0.5;

		FS_GridMaterialParams BlocksParams;
		BlocksParams.GridColor = FLinearColor(0.177083f, 0.177083f, 0.177083f);
		BlocksParams.SurfaceColor = FLinearColor(0.510417f, 0.510417f, 0.510417f);
		BlocksParams.GridSizes = FVector(100.0, 100.0, 10.0);
		BlocksParams.Specularity = 0.5;

		FS_GridMaterialParams TraversableParams;
		TraversableParams.GridColor = FLinearColor(0.7f, 0.7f, 0.7f);
		TraversableParams.SurfaceColor = FLinearColor(0.850000f, 0.264066f, 0.132812f);
		TraversableParams.GridSizes = FVector(100.0, 100.0, 10.0);
		TraversableParams.Specularity = 0.5;

		Style2.BlockColors.Add(TEXT("Floor"), FloorParams);
		Style2.BlockColors.Add(TEXT("Blocks"), BlocksParams);
		Style2.BlockColors.Add(TEXT("Blocks_Traversable"), TraversableParams);
		Style2.BlockColors.Add(TEXT("Orange"), BlocksParams);  // Fallback

		LevelStyles.Add(Style2);
	}

	// Cache blueprint components by class during construction
	CachedScene = GetRootComponent();
	CachedSkyLight = FindComponentByClass<USkyLightComponent>();
	CachedDirectionalLight = FindComponentByClass<UDirectionalLightComponent>();
	CachedExponentialHeightFog = FindComponentByClass<UExponentialHeightFogComponent>();
	CachedPostProcess = FindComponentByClass<UPostProcessComponent>();
	CachedDecal = FindComponentByClass<UDecalComponent>();

	// Update visuals - this will also refresh all LevelBlock actors in the level
	// This ensures blocks get correct colors even if they constructed before LevelVisuals
	UpdateLevelVisuals();

	// Force a second update on next tick to catch any blocks that constructed before this actor
	// Use FTSTicker instead of timer because GetWorld() can be null during OnConstruction in editor
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
	{
		if (IsValid(this))
		{
			UpdateLevelVisuals();
		}
		return false; // Return false to execute only once
	}));
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
	// Validate StyleIndex - fallback to 0 if invalid
	int32 EffectiveStyleIndex = StyleIndex;
	if (!LevelStyles.IsValidIndex(EffectiveStyleIndex))
	{
		EffectiveStyleIndex = 0;
		if (!LevelStyles.IsValidIndex(EffectiveStyleIndex))
		{
			// No valid styles at all
			return;
		}
	}

	const FS_LevelStyle& CurrentStyle = LevelStyles[EffectiveStyleIndex];

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
