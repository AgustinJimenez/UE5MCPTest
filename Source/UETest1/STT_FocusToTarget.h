#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "STT_FocusToTarget.generated.h"

class AAIController;
class AActor;

UCLASS()
class UETEST1_API USTT_FocusToTarget : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditInstanceOnly, Category = "AI")
	TObjectPtr<AAIController> AIController;

	UPROPERTY(EditInstanceOnly, Category = "AI")
	TObjectPtr<AActor> TargetToFocus;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
};
