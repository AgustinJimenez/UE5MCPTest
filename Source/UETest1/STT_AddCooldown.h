#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "GameplayTagContainer.h"
#include "STT_AddCooldown.generated.h"

class AAIController;

UCLASS()
class UETEST1_API USTT_AddCooldown : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "AI")
	FString CooldownName;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "AI")
	FGameplayTag CooldownTag;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "AI")
	double CooldownTime = 0.0;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "AI")
	TObjectPtr<AAIController> AIController;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
};
