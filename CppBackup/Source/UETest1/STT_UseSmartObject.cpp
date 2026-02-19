#include "STT_UseSmartObject.h"

#include "AIController.h"
#include "AITask_UseGameplayInteraction_Public.h"
#include "SmartObjectBlueprintFunctionLibrary.h"

EStateTreeRunStatus USTT_UseSmartObject::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition)
{
	if (!USmartObjectBlueprintFunctionLibrary::IsValidSmartObjectClaimHandle(ClaimedHandle) || !AIController)
	{
		FinishTask(false);
		return EStateTreeRunStatus::Running;
	}

	ActiveTask = UAITask_UseGameplayInteraction_Public::UseSmartObjectWithGameplayInteraction_Public(AIController, ClaimedHandle, bLockAILogic);
	if (!ActiveTask)
	{
		FinishTask(false);
		return EStateTreeRunStatus::Running;
	}

	ActiveTask->OnFinishedDelegate().AddDynamic(this, &USTT_UseSmartObject::HandleUseFinished);
	ActiveTask->OnSucceededDelegate().AddDynamic(this, &USTT_UseSmartObject::HandleUseSucceeded);
	ActiveTask->OnFailedDelegate().AddDynamic(this, &USTT_UseSmartObject::HandleUseFailed);
	ActiveTask->OnMoveToFailedDelegate().AddDynamic(this, &USTT_UseSmartObject::HandleUseMoveToFailed);

	ActiveTask->ReadyForActivation();

	return EStateTreeRunStatus::Running;
}

void USTT_UseSmartObject::HandleUseFinished()
{
	FinishTask(true);
}

void USTT_UseSmartObject::HandleUseSucceeded()
{
	FinishTask(true);
}

void USTT_UseSmartObject::HandleUseFailed()
{
	FinishTask(false);
}

void USTT_UseSmartObject::HandleUseMoveToFailed()
{
	FinishTask(false);
}
