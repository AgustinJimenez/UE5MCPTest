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
	UPROPERTY(EditInstanceOnly, Category = "Input")
	TObjectPtr<AActor> Actor;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	TObjectPtr<UObject> AnimationProxyTable;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	float CostThreshold = 0.0f;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	FSmartObjectClaimHandle ClaimedHandle;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	FTransform Destination;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	float MaximumDistanceThreshold = 0.0f;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	float MinimumVelocityCheck = 0.0f;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	bool NeedsEvaluation = false;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;

private:
	UPROPERTY()
	TObjectPtr<UAC_SmartObjectAnimation> SmartObjectAnimComponent;

	UPROPERTY()
	TObjectPtr<UMoverComponent> PossibleOwnerMoverComponent;

	UPROPERTY()
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
