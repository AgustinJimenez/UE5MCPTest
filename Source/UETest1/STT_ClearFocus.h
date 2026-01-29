#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "STT_ClearFocus.generated.h"

class AAIController;

UCLASS()
class UETEST1_API USTT_ClearFocus : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditInstanceOnly, Category = "AI")
	TObjectPtr<AAIController> AIController;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
};
