#include "LevelVisuals.h"
#include "LevelBlock.h"
#include "EngineUtils.h"
#include "Landscape.h"

ALevelVisuals::ALevelVisuals()
{
	PrimaryActorTick.bCanEverTick = false;

	StyleIndex = 0;

	// Add a default style if none exist
	if (LevelStyles.Num() == 0)
	{
		FS_LevelStyle DefaultStyle;
		DefaultStyle.FogColor = FLinearColor(0.5f, 0.7f, 1.0f); // Light blue fog
		DefaultStyle.FogDensity = 0.02;
		DefaultStyle.DecalColor = FLinearColor::White;

		// Add default block colors
		FS_GridMaterialParams WhiteParams;
		WhiteParams.GridColor = FLinearColor(0.2f, 0.2f, 0.2f);
		WhiteParams.SurfaceColor = FLinearColor::White;
		WhiteParams.GridSizes = FVector(100.0, 100.0, 10.0);
		WhiteParams.Specularity = 0.5;

		FS_GridMaterialParams BlueParams;
		BlueParams.GridColor = FLinearColor(0.1f, 0.1f, 0.3f);
		BlueParams.SurfaceColor = FLinearColor(0.0f, 0.5f, 1.0f);
		BlueParams.GridSizes = FVector(100.0, 100.0, 10.0);
		BlueParams.Specularity = 0.5;

		FS_GridMaterialParams GreenParams;
		GreenParams.GridColor = FLinearColor(0.1f, 0.3f, 0.1f);
		GreenParams.SurfaceColor = FLinearColor(0.0f, 1.0f, 0.0f);
		GreenParams.GridSizes = FVector(100.0, 100.0, 10.0);
		GreenParams.Specularity = 0.5;

		DefaultStyle.BlockColors.Add(TEXT("None"), WhiteParams);
		DefaultStyle.BlockColors.Add(TEXT("White"), WhiteParams);
		DefaultStyle.BlockColors.Add(TEXT("Blue"), BlueParams);
		DefaultStyle.BlockColors.Add(TEXT("Green"), GreenParams);

		LevelStyles.Add(DefaultStyle);
	}
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

	// Cache blueprint components by class during construction
	CachedScene = GetRootComponent();
	CachedSkyLight = FindComponentByClass<USkyLightComponent>();
	CachedDirectionalLight = FindComponentByClass<UDirectionalLightComponent>();
	CachedExponentialHeightFog = FindComponentByClass<UExponentialHeightFogComponent>();
	CachedPostProcess = FindComponentByClass<UPostProcessComponent>();
	CachedDecal = FindComponentByClass<UDecalComponent>();

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
