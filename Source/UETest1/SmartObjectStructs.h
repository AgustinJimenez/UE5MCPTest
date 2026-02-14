#pragma once

#include "CoreMinimal.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "SmartObjectStructs.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct FSmartObjectAnimationPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	TObjectPtr<UAnimMontage> MontageToPlay = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	double PlayTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	double RandomPlaytimeVariance = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	double StartTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	double Playrate = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	int32 NumLoops = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	FTransform WarpTargetTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	bool UseWarpTarget = false;
};

USTRUCT(BlueprintType)
struct FSmartObjectSelectionInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	double TargetDistance = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	double TargetAngle = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	FPoseHistoryReference PoseHistoryNode;
};

USTRUCT(BlueprintType)
struct FSmartObjectSelectionOutputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	double Cost = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	double StartTime = 0.0;
};
