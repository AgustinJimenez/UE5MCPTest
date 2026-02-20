#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "SmartObjectRuntime.h"
#include "STT_UseSmartObject.generated.h"

class AAIController;

UCLASS()
class UETEST1_API USTT_UseSmartObject : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SmartObject")
	TObjectPtr<AAIController> AIController;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SmartObject")
	FSmartObjectClaimHandle ClaimedHandle;
};
