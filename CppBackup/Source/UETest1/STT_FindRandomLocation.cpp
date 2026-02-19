#include "STT_FindRandomLocation.h"

#include "AIController.h"
#include "NavigationSystem.h"

EStateTreeRunStatus USTT_FindRandomLocation::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition)
{
	FVector Origin = FVector::ZeroVector;
	if (AIReference)
	{
		if (APawn* Pawn = AIReference->GetPawn())
		{
			Origin = Pawn->GetActorLocation();
		}
	}
	FVector OutLocation = Origin;

	if (AIReference)
	{
		const UWorld* World = AIReference->GetWorld();
		if (World)
		{
			if (const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
			{
				FNavLocation NavLocation;
				if (NavSys->GetRandomReachablePointInRadius(Origin, SearchRadius, NavLocation))
				{
					RandomLocation = NavLocation.Location;
				}
				else
				{
					RandomLocation = Origin;
				}
			}
			else
			{
				RandomLocation = Origin;
			}
		}
		else
		{
			RandomLocation = Origin;
		}
	}
	else
	{
		RandomLocation = Origin;
	}

	return EStateTreeRunStatus::Succeeded;
}
