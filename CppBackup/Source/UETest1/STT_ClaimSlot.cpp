#include "STT_ClaimSlot.h"

#include "SmartObjectBlueprintFunctionLibrary.h"

EStateTreeRunStatus USTT_ClaimSlot::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition)
{
	ClaimedHandle = USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsClaimed(
		this,
		SlotToBeClaimed,
		Actor,
		ESmartObjectClaimPriority::Normal);

	FinishTask(true);
	return EStateTreeRunStatus::Running;
}
