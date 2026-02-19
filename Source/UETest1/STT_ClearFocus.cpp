#include "STT_ClearFocus.h"

#include "AIController.h"

EStateTreeRunStatus USTT_ClearFocus::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition)
{
	if (AIController)
	{
		AIController->K2_ClearFocus();
	}

	return EStateTreeRunStatus::Succeeded;
}
