#pragma once

#include "CoreMinimal.h"
#include "LocomotionEnums.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "TraversalTypes.generated.h"

class UPrimitiveComponent;
class UAnimMontage;

USTRUCT(BlueprintType)
struct FS_TraversalCheckResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	E_TraversalActionType ActionType = E_TraversalActionType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	bool HasFrontLedge = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	FVector FrontLedgeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	FVector FrontLedgeNormal = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	bool HasBackLedge = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	FVector BackLedgeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	FVector BackLedgeNormal = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	bool HasBackFloor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	FVector BackFloorLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	double ObstacleHeight = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	double ObstacleDepth = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	double BackLedgeHeight = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	TObjectPtr<UPrimitiveComponent> HitComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	TObjectPtr<UAnimMontage> ChosenMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	double StartTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	double PlayRate = 1.0;
};

USTRUCT(BlueprintType)
struct FS_TraversalChooserInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	E_TraversalActionType ActionType = E_TraversalActionType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	bool HasFrontLedge = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	bool HasBackLedge = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	bool HasBackFloor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	double ObstacleHeight = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	double ObstacleDepth = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	double BackLedgeHeight = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	double DistanceToLedge = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	E_MovementMode MovementMode = E_MovementMode::OnGround;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	E_Gait Gait = E_Gait::Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	double Speed = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	FPoseHistoryReference PoseHistory;
};

USTRUCT(BlueprintType)
struct FS_TraversalChooserOutputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	E_TraversalActionType ActionType = E_TraversalActionType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	double MontageStartTime = 0.0;
};
