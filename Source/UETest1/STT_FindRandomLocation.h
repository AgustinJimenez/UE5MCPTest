#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "STT_FindRandomLocation.generated.h"

class AAIController;

UCLASS()
class UETEST1_API USTT_FindRandomLocation : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditInstanceOnly, Category = "AI")
	TObjectPtr<AAIController> AIReference;

	UPROPERTY(EditInstanceOnly, Category = "AI")
	float SearchRadius = 0.0f;

	UPROPERTY(EditInstanceOnly, Category = "AI")
	FVector RandomLocation = FVector::ZeroVector;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
};
