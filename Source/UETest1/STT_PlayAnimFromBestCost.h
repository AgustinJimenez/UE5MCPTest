#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "SmartObjectRuntime.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "STT_PlayAnimFromBestCost.generated.h"

class AActor;
class UAC_SmartObjectAnimation;
class UMoverComponent;
class AAIController;

UCLASS()
class UETEST1_API USTT_PlayAnimFromBestCost : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<AActor> Actor;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UObject> AnimationProxyTable;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	float CostThreshold = 0.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	FSmartObjectClaimHandle ClaimedHandle;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	FTransform Destination;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	float MaximumDistanceThreshold = 0.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	float MinimumVelocityCheck = 0.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	bool NeedsEvaluation = false;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;

private:
	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAC_SmartObjectAnimation> SmartObjectAnimComponent;

	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMoverComponent> PossibleOwnerMoverComponent;

	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	TWeakObjectPtr<AAIController> CachedAIController;

	// Get pathed distance (via nav path) or straight-line distance to destination
	double NPCApproachAngleAndPathedDistance() const;

	// Get velocity from MoverComponent if available, else from Actor
	FVector GetActorVelocity() const;

	// Build and play the montage payload
	void EvaluateAndPlay(bool bCheckCostThreshold);

	UFUNCTION()
	void OnMovementCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);

	UFUNCTION()
	void OnMontageFinished();
};
