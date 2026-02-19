#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "SmartObjectRuntime.h"
#include "STT_ClaimSlot.generated.h"

class AActor;

UCLASS()
class UETEST1_API USTT_ClaimSlot : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SmartObject")
	TObjectPtr<AActor> Actor;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SmartObject")
	FSmartObjectClaimHandle ClaimedHandle;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SmartObject")
	FSmartObjectSlotHandle SlotToBeClaimed;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SmartObject")
	TObjectPtr<AActor> SmartObject;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
};
