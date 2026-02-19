#pragma once

#include "CoreMinimal.h"
#include "S_DebugGraphLineProperties.generated.h"

USTRUCT(BlueprintType)
struct FS_DebugGraphLineProperties
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debug")
	FString Name;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debug")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debug")
	TArray<double> Values;
};
