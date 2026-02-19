#include "STT_AddCooldown.h"

#include "AIC_NPC_SmartObject.h"
#include "AIController.h"

EStateTreeRunStatus USTT_AddCooldown::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition)
{
	double CurrentTime = 0.0;
	if (UWorld* World = GetWorld())
	{
		CurrentTime = World->GetTimeSeconds();
	}

	const double ExpirationTime = CurrentTime + CooldownTime;

	if (AAIC_NPC_SmartObject* SmartAI = Cast<AAIC_NPC_SmartObject>(AIController))
	{
		SmartAI->AddCooldown(CooldownName, ExpirationTime);
	}

	FinishTask(true);
	return EStateTreeRunStatus::Running;
}
