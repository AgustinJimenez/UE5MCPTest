#include "LevelBlock.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetTextLibrary.h"
#include "LevelButton.h"

ALevelBlock::ALevelBlock()
{
	PrimaryActorTick.bCanEverTick = false;

	// Initialize defaults
	AutoNameFromHeight = false;
	UseLevelVisualsColor = false;

	// Set default material params so blocks aren't black
	MaterialParams.GridColor = FLinearColor(0.2f, 0.2f, 0.2f);
	MaterialParams.SurfaceColor = FLinearColor::White;
	MaterialParams.GridSizes = FVector(100.0, 100.0, 10.0);
	MaterialParams.Specularity = 0.5;
}

void ALevelBlock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Cache blueprint components
	CachedDefaultSceneRoot = GetRootComponent();
	CachedStaticMesh = FindComponentByClass<UStaticMeshComponent>();
	CachedTextRender = FindComponentByClass<UTextRenderComponent>();

	// Create dynamic material instance
	if (CachedStaticMesh)
	{
		// Use BaseMaterial if set, otherwise use the existing material on the mesh
		UMaterialInterface* MaterialToUse = BaseMaterial ? BaseMaterial.Get() : CachedStaticMesh->GetMaterial(0);
		if (MaterialToUse)
		{
			DynamicMaterial = CachedStaticMesh->CreateDynamicMaterialInstance(0, MaterialToUse);
		}
	}

	// Apply material parameters
	if (UseLevelVisualsColor)
	{
		// Try to get LevelVisuals actor and use its color mapping
		// Note: This requires LevelVisuals to be converted to C++ first
		// For now, fall back to using MaterialParams
		UpdateMaterials(MaterialParams);
	}
	else
	{
		UpdateMaterials(MaterialParams);
	}

	// Update text display
	UpdateText();

	// Store initial transform
	InitialTransform = GetActorTransform();

	// Bind to Randomize Button's ButtonPressed event if it exists
	if (RandomizeButton)
	{
		RandomizeButton->ButtonPressed.AddDynamic(this, &ALevelBlock::RandomizeOffset);
	}
}

void ALevelBlock::UpdateMaterials(const FS_GridMaterialParams& Params)
{
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("Grid Color"), Params.GridColor);
		DynamicMaterial->SetVectorParameterValue(TEXT("Surface Color"), Params.SurfaceColor);
		DynamicMaterial->SetScalarParameterValue(TEXT("Grid 1 Size"), Params.GridSizes.X);
		DynamicMaterial->SetScalarParameterValue(TEXT("Grid 2 Size"), Params.GridSizes.Y);
		DynamicMaterial->SetScalarParameterValue(TEXT("Grid 3 Size"), Params.GridSizes.Z);
		DynamicMaterial->SetScalarParameterValue(TEXT("Base Specular"), Params.Specularity);

		if (CachedTextRender)
		{
			CachedTextRender->SetTextRenderColor(Params.GridColor.ToFColor(false));
		}
	}
}

void ALevelBlock::UpdateText()
{
	if (!CachedTextRender)
	{
		return;
	}

	if (AutoNameFromHeight)
	{
		double Height = GetActorScale3D().Z;

		// Create number formatting options for 2 decimal places
		FNumberFormattingOptions NumberFormat;
		NumberFormat.MinimumIntegralDigits = 1;
		NumberFormat.MaximumIntegralDigits = 324;
		NumberFormat.MinimumFractionalDigits = 0;
		NumberFormat.MaximumFractionalDigits = 2;
		NumberFormat.UseGrouping = true;
		NumberFormat.RoundingMode = ERoundingMode::HalfToEven;

		FText FormattedHeight = FText::AsNumber(Height, &NumberFormat);
		FText FormattedText = FText::Format(FText::FromString(TEXT("{0} M")), FormattedHeight);
		CachedTextRender->SetText(FormattedText);
	}
	else
	{
		CachedTextRender->SetText(Name);
	}
}

void ALevelBlock::RandomizeOffset()
{
	// Reset to initial transform
	FHitResult SweepHitResult;
	K2_SetActorTransform(InitialTransform, false, SweepHitResult, false);

	// Generate random location offset
	FVector RandomLocation(
		FMath::FRandRange(MinTransformOffset.GetLocation().X, MaxTransformOffset.GetLocation().X),
		FMath::FRandRange(MinTransformOffset.GetLocation().Y, MaxTransformOffset.GetLocation().Y),
		FMath::FRandRange(MinTransformOffset.GetLocation().Z, MaxTransformOffset.GetLocation().Z)
	);

	// Generate random rotation offset
	FRotator RandomRotation(
		FMath::FRandRange(MinTransformOffset.GetRotation().Rotator().Pitch, MaxTransformOffset.GetRotation().Rotator().Pitch),
		FMath::FRandRange(MinTransformOffset.GetRotation().Rotator().Yaw, MaxTransformOffset.GetRotation().Rotator().Yaw),
		FMath::FRandRange(MinTransformOffset.GetRotation().Rotator().Roll, MaxTransformOffset.GetRotation().Rotator().Roll)
	);

	// Generate random scale offset and add to initial transform scale
	FVector RandomScale(
		FMath::FRandRange(MinTransformOffset.GetScale3D().X, MaxTransformOffset.GetScale3D().X) + InitialTransform.GetScale3D().X,
		FMath::FRandRange(MinTransformOffset.GetScale3D().Y, MaxTransformOffset.GetScale3D().Y) + InitialTransform.GetScale3D().Y,
		FMath::FRandRange(MinTransformOffset.GetScale3D().Z, MaxTransformOffset.GetScale3D().Z) + InitialTransform.GetScale3D().Z
	);

	// Create offset transform and add it locally
	FTransform OffsetTransform(RandomRotation, RandomLocation, RandomScale);
	FHitResult AddSweepHitResult;
	K2_AddActorLocalTransform(OffsetTransform, false, AddSweepHitResult, false);

	UpdateText();
}

void ALevelBlock::ResetOffset()
{
	FHitResult SweepHitResult;
	K2_SetActorTransform(InitialTransform, false, SweepHitResult, false);
	UpdateText();
}
