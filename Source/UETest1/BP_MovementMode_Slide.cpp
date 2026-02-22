#include "BP_MovementMode_Slide.h"
#include "MoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BP_MovementMode_Slide)

void UBP_MovementMode_Slide::Activate()
{
	InitialBoost = true;

	if (UMoverComponent* Mover = GetMoverComponent())
	{
		if (UWorld* World = Mover->GetWorld())
		{
			World->GetTimerManager().SetTimer(InitialBoostTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				InitialBoost = false;
			}), InitialBoostTime, false);
		}
	}

	Super::Activate();
}
