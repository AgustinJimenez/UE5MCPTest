#pragma once

#include "CoreMinimal.h"
#include "S_GridMaterialParams.generated.h"

USTRUCT(BlueprintType)
struct UETEST1_API FS_GridMaterialParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FLinearColor GridColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FLinearColor SurfaceColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FVector GridSizes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	double Specularity;
};
