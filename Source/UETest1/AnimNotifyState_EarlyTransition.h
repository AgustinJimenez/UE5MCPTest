#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "LocomotionEnums.h"
#include "AnimNotifyState_EarlyTransition.generated.h"

UCLASS(Blueprintable, meta = (DisplayName = "Early Transition"))
class UETEST1_API UAnimNotifyState_EarlyTransition : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EarlyTransition")
	EEarlyTransitionDestination TransitionDestination = EEarlyTransitionDestination::ReTransition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EarlyTransition")
	EEarlyTransitionCondition TransitionCondition = EEarlyTransitionCondition::Always;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EarlyTransition")
	E_Gait GaitNotEqual = E_Gait::Walk;
};
