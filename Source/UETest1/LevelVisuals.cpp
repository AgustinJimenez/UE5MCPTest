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

	// FORCE reinitialize to apply corrected BlockColors from working project
	// Initialize LevelStyles if not exactly 3 styles or invalid
	if (true)  // Temporarily force reinit
	// if (LevelStyles.Num() != 3 || !LevelStyles[0].BlockColors.Contains(TEXT("Floor")))
	{
		LevelStyles.Empty();

		// Named color constants for readability
		const FLinearColor DarkGrayFog(0.261349f, 0.261177f, 0.302083f);  // Dark button fog
		const FLinearColor BlueFog(0.484375f, 0.655383f, 1.0f);
		const FLinearColor PurpleFog(0.539931f, 0.447917f, 1.0f);

		const FLinearColor BlackDecal(0.0f, 0.0f, 0.0f, 0.5f);
		const FLinearColor OrangeDecal(1.0f, 0.5f, 0.0f, 0.4f);

		const FLinearColor DarkGray(0.121569f, 0.121569f, 0.121569f);
		const FLinearColor MediumDarkGray(0.177083f, 0.177083f, 0.177083f);
		const FLinearColor DarkGrid(0.2f, 0.2f, 0.2f);
		const FLinearColor MediumGray(0.3f, 0.3f, 0.3f);
		const FLinearColor LightGray(0.5f, 0.5f, 0.5f);
		const FLinearColor MediumLightGray(0.510417f, 0.510417f, 0.510417f);
		const FLinearColor VeryLightGray(0.7f, 0.7f, 0.7f);

		const FLinearColor DarkSurface(0.25f, 0.25f, 0.25f);
		const FLinearColor PurpleSurface(0.258463f, 0.236978f, 0.541667f);
		const FLinearColor OrangeSurface(0.850000f, 0.264066f, 0.132812f);

		// Style 0 (Dark atmosphere - dark gray fog + density)
		FS_LevelStyle Style0;
		Style0.FogColor = DarkGrayFog;  // Dark gray, not light purple!
		Style0.FogDensity = 0.3;  // Dense fog = dark scene
		// DecalColor not set - uses constructor default

		FS_GridMaterialParams Floor0;
		Floor0.GridColor = MediumGray;
		Floor0.SurfaceColor = DarkGray;  // Very dark floor (0.121569)
		Floor0.GridSizes = FVector(100.0, 200.0, 800.0);
		Floor0.Specularity = 0.0;  // No specularity for dark style

		FS_GridMaterialParams Blocks0;
		Blocks0.GridColor = DarkGray;
		Blocks0.SurfaceColor = MediumGray;
		Blocks0.GridSizes = FVector(100.0, 100.0, 10.0);
		Blocks0.Specularity = 0.5;

		FS_GridMaterialParams Traversable0;
		Traversable0.GridColor = MediumGray;
		Traversable0.SurfaceColor = MediumGray;  // Must set to avoid bright green default
		Traversable0.GridSizes = FVector(100.0, 100.0, 10.0);
		Traversable0.Specularity = 0.5;

		Style0.BlockColors.Add(TEXT("Floor"), Floor0);
		Style0.BlockColors.Add(TEXT("Blocks"), Blocks0);
		Style0.BlockColors.Add(TEXT("Blocks_Traversable"), Traversable0);
		Style0.BlockColors.Add(TEXT("Orange"), Blocks0);

		LevelStyles.Add(Style0);

		// Style 1 (Blue fog, black decal, gray blocks)
		FS_LevelStyle Style1;
		Style1.FogColor = BlueFog;
		Style1.FogDensity = 0.2;
		Style1.DecalColor = BlackDecal;

		FS_GridMaterialParams Floor1;
		Floor1.GridColor = DarkGrid;
		Floor1.SurfaceColor = MediumLightGray;
		Floor1.GridSizes = FVector(100.0, 200.0, 800.0);
		Floor1.Specularity = 0.5;

		FS_GridMaterialParams Blocks1;
		Blocks1.GridColor = DarkGrid;
		Blocks1.SurfaceColor = MediumLightGray;
		Blocks1.GridSizes = FVector(100.0, 100.0, 10.0);
		Blocks1.Specularity = 0.5;

		FS_GridMaterialParams Traversable1;
		Traversable1.GridColor = LightGray;
		Traversable1.SurfaceColor = DarkSurface;
		Traversable1.GridSizes = FVector(100.0, 100.0, 10.0);
		Traversable1.Specularity = 0.5;

		Style1.BlockColors.Add(TEXT("Floor"), Floor1);
		Style1.BlockColors.Add(TEXT("Blocks"), Blocks1);
		Style1.BlockColors.Add(TEXT("Blocks_Traversable"), Traversable1);
		Style1.BlockColors.Add(TEXT("Orange"), Blocks1);

		LevelStyles.Add(Style1);

		// Style 2 (Purple fog, orange decal, orange traversable blocks)
		FS_LevelStyle Style2;
		Style2.FogColor = PurpleFog;
		Style2.FogDensity = 0.2;
		Style2.DecalColor = OrangeDecal;

		FS_GridMaterialParams Floor2;
		Floor2.GridColor = LightGray;
		Floor2.SurfaceColor = PurpleSurface;
		Floor2.GridSizes = FVector(100.0, 200.0, 800.0);
		Floor2.Specularity = 0.5;

		FS_GridMaterialParams Blocks2;
		Blocks2.GridColor = MediumDarkGray;
		Blocks2.SurfaceColor = MediumLightGray;
		Blocks2.GridSizes = FVector(100.0, 100.0, 10.0);
		Blocks2.Specularity = 0.5;

		FS_GridMaterialParams Traversable2;
		Traversable2.GridColor = VeryLightGray;
		Traversable2.SurfaceColor = OrangeSurface;
		Traversable2.GridSizes = FVector(100.0, 100.0, 10.0);
		Traversable2.Specularity = 0.5;

		FS_GridMaterialParams Orange2;
		Orange2.GridColor = MediumDarkGray;
		Orange2.SurfaceColor = MediumLightGray;
		Orange2.GridSizes = FVector(100.0, 100.0, 10.0);
		Orange2.Specularity = 0.5;

		Style2.BlockColors.Add(TEXT("Floor"), Floor2);
		Style2.BlockColors.Add(TEXT("Blocks"), Blocks2);
		Style2.BlockColors.Add(TEXT("Blocks_Traversable"), Traversable2);
		Style2.BlockColors.Add(TEXT("Orange"), Orange2);

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

FS_LevelStyle ALevelVisuals::GetLevelStyle() const
{
	if (LevelStyles.IsValidIndex(StyleIndex))
	{
		return LevelStyles[StyleIndex];
	}
	return FS_LevelStyle();
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
			UE_LOG(LogTemp, Error, TEXT("LevelVisuals: No valid styles available!"));
			return;
		}
	}

	const FS_LevelStyle& CurrentStyle = LevelStyles[EffectiveStyleIndex];
	UE_LOG(LogTemp, Warning, TEXT("LevelVisuals: Updating to style %d (FogColor R=%.2f G=%.2f B=%.2f)"),
		EffectiveStyleIndex, CurrentStyle.FogColor.R, CurrentStyle.FogColor.G, CurrentStyle.FogColor.B);

	// Update fog settings
	if (CachedExponentialHeightFog)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Updating fog component"));
		CachedExponentialHeightFog->SetFogInscatteringColor(CurrentStyle.FogColor);
		CachedExponentialHeightFog->SetFogDensity(static_cast<float>(CurrentStyle.FogDensity));
		CachedExponentialHeightFog->MarkRenderStateDirty();
		CachedExponentialHeightFog->RecreateRenderState_Concurrent();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  Fog component is NULL!"));
	}

	// Update decal color
	if (CachedDecal)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Updating decal component (DecalColor R=%.2f G=%.2f B=%.2f A=%.2f)"),
			CurrentStyle.DecalColor.R, CurrentStyle.DecalColor.G, CurrentStyle.DecalColor.B, CurrentStyle.DecalColor.A);

		// Create dynamic material instance if needed
		UMaterialInterface* CurrentMaterial = CachedDecal->GetDecalMaterial();
		UMaterialInstanceDynamic* DecalMaterial = Cast<UMaterialInstanceDynamic>(CurrentMaterial);
		if (!DecalMaterial && CurrentMaterial)
		{
			DecalMaterial = UMaterialInstanceDynamic::Create(CurrentMaterial, this);
			CachedDecal->SetDecalMaterial(DecalMaterial);
			UE_LOG(LogTemp, Warning, TEXT("    Created dynamic material instance for decal"));
		}

		if (DecalMaterial)
		{
			DecalMaterial->SetVectorParameterValue(TEXT("Color"), CurrentStyle.DecalColor);
			UE_LOG(LogTemp, Warning, TEXT("    Set decal color parameter"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("    Failed to get/create dynamic material for decal"));
		}
		CachedDecal->MarkRenderStateDirty();
		CachedDecal->RecreateRenderState_Concurrent();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  Decal component is NULL!"));
	}

	// Find all LevelBlock actors in the world and update their materials
	UWorld* World = GetWorld();
	if (World)
	{
		int32 BlocksUpdated = 0;
		int32 LoggedBlocks = 0;
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
					// Log first 3 blocks for debugging
					if (LoggedBlocks < 3)
					{
						UE_LOG(LogTemp, Warning, TEXT("    Block '%s' ColorGroup '%s' -> SurfaceColor R=%.2f G=%.2f B=%.2f"),
							*Block->GetName(), *ColorGroup.ToString(), Params->SurfaceColor.R, Params->SurfaceColor.G, Params->SurfaceColor.B);
						LoggedBlocks++;
					}

					// Call UpdateMaterials on the block with the params from the style
					Block->UpdateMaterials(*Params);
					BlocksUpdated++;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("    Block '%s' has ColorGroup '%s' which is not in style %d"),
						*Block->GetName(), *ColorGroup.ToString(), EffectiveStyleIndex);
				}
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("  Updated %d LevelBlock actors"), BlocksUpdated);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  World is NULL - cannot update LevelBlocks!"));
	}
}
