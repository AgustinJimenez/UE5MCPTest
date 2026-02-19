#include "AIC_NPC_SmartObject.h"

#include "Components/StateTreeAIComponent.h"
#include "TimerManager.h"

AAIC_NPC_SmartObject::AAIC_NPC_SmartObject()
{
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	BrainComponent = StateTreeAI;
}

void AAIC_NPC_SmartObject::AddCooldown(const FString& Name, double ExpirationTime)
{
	Cooldowns.Add(Name, ExpirationTime);
}

void AAIC_NPC_SmartObject::BeginPlay()
{
	Super::BeginPlay();

	const bool bDedicatedServer = (GetNetMode() == NM_DedicatedServer);
	const float Delay = bDedicatedServer ? DedicatedServerStartDelay : ClientStartDelay;

	if (Delay <= 0.0f)
	{
		StartStateTreeLogic();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(StartLogicHandle, this, &AAIC_NPC_SmartObject::StartStateTreeLogic, Delay, false);
	}
}

void AAIC_NPC_SmartObject::StartStateTreeLogic()
{
	if (StateTreeAI)
	{
		StateTreeAI->StartLogic();
	}
}
