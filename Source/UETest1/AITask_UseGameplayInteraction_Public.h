#pragma once

#include "CoreMinimal.h"
#include "AI/AITask_UseGameplayInteraction.h"
#include "AITask_UseGameplayInteraction_Public.generated.h"

UCLASS()
class UETEST1_API UAITask_UseGameplayInteraction_Public : public UAITask_UseGameplayInteraction
{
	GENERATED_BODY()

public:
	static UAITask_UseGameplayInteraction_Public* UseSmartObjectWithGameplayInteraction_Public(AAIController* Controller, const FSmartObjectClaimHandle ClaimHandle, const bool bLockAILogic = true);

	FGenericGameplayTaskDelegate& OnFinishedDelegate() { return OnFinished; }
	FGenericGameplayTaskDelegate& OnSucceededDelegate() { return OnSucceeded; }
	FGenericGameplayTaskDelegate& OnFailedDelegate() { return OnFailed; }
	FGenericGameplayTaskDelegate& OnMoveToFailedDelegate() { return OnMoveToFailed; }
};
