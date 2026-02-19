#pragma once

#include "CoreMinimal.h"
#include "S_GridMaterialParams.h"
#include "S_LevelStyle.generated.h"

USTRUCT(BlueprintType)
struct UETEST1_API FS_LevelStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Style")
	FLinearColor FogColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Style")
	double FogDensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Style")
	FLinearColor DecalColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Style")
	TMap<FName, FS_GridMaterialParams> BlockColors;
};
