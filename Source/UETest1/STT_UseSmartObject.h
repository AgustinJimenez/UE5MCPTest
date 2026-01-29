#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "SmartObjectRuntime.h"
#include "STT_UseSmartObject.generated.h"

class AAIController;
class UAITask_UseGameplayInteraction_Public;

UCLASS()
class UETEST1_API USTT_UseSmartObject : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditInstanceOnly, Category = "AI")
	TObjectPtr<AAIController> AIController;

	UPROPERTY(EditInstanceOnly, Category = "SmartObject")
	FSmartObjectClaimHandle ClaimedHandle;

	UPROPERTY(EditDefaultsOnly, Category = "SmartObject")
	bool bLockAILogic = true;

	UPROPERTY()
	TObjectPtr<UAITask_UseGameplayInteraction_Public> ActiveTask;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;

	UFUNCTION()
	void HandleUseFinished();

	UFUNCTION()
	void HandleUseSucceeded();

	UFUNCTION()
	void HandleUseFailed();

	UFUNCTION()
	void HandleUseMoveToFailed();
};
