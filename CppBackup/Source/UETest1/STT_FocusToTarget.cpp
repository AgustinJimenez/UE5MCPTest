#include "STT_FocusToTarget.h"

#include "AIController.h"

EStateTreeRunStatus USTT_FocusToTarget::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition)
{
	if (IsValid(AIController) && IsValid(TargetToFocus))
	{
		AIController->K2_SetFocus(TargetToFocus);
	}

	return EStateTreeRunStatus::Succeeded;
}
