#include "AITask_UseGameplayInteraction_Public.h"

#include "AIController.h"

UAITask_UseGameplayInteraction_Public* UAITask_UseGameplayInteraction_Public::UseSmartObjectWithGameplayInteraction_Public(
	AAIController* Controller,
	const FSmartObjectClaimHandle ClaimHandle,
	const bool bLockAILogic)
{
	if (Controller == nullptr || !ClaimHandle.IsValid())
	{
		return nullptr;
	}

	UAITask_UseGameplayInteraction_Public* MyTask = NewAITask<UAITask_UseGameplayInteraction_Public>(*Controller, EAITaskPriority::High);
	if (MyTask == nullptr)
	{
		return nullptr;
	}

	MyTask->SetClaimHandle(ClaimHandle);

	if (bLockAILogic)
	{
		MyTask->RequestAILogicLocking();
	}

	return MyTask;
}
