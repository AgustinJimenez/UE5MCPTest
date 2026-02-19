#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "STT_SetCharacterInputState.generated.h"

class APawn;

UCLASS()
class UETEST1_API USTT_SetCharacterInputState : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditInstanceOnly, Category = "AI")
	TObjectPtr<APawn> Character;

	UPROPERTY(EditInstanceOnly, Category = "AI")
	bool WantsToWalk = false;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
};
