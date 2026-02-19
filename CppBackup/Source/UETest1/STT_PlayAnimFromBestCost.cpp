#include "STT_PlayAnimFromBestCost.h"

#include "AC_SmartObjectAnimation.h"
#include "AIController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "SmartObjectSubsystem.h"
#include "MoverComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "Kismet/KismetSystemLibrary.h"

EStateTreeRunStatus USTT_PlayAnimFromBestCost::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition)
{
	if (!Actor)
	{
		FinishTask(false);
		return EStateTreeRunStatus::Running;
	}

	// Cache AIController and bind MoveCompleted delegate
	AAIController* AIC = UAIBlueprintHelperLibrary::GetAIController(Actor);
	CachedAIController = AIC;
	if (AIC)
	{
		AIC->ReceiveMoveCompleted.AddDynamic(this, &USTT_PlayAnimFromBestCost::OnMovementCompleted);
	}

	// Cache SmartObjectAnimComponent and bind MontageFinished delegate
	SmartObjectAnimComponent = Actor->FindComponentByClass<UAC_SmartObjectAnimation>();
	if (SmartObjectAnimComponent)
	{
		SmartObjectAnimComponent->OwnerMontageFinished.AddDynamic(this, &USTT_PlayAnimFromBestCost::OnMontageFinished);
	}

	// Cache MoverComponent
	PossibleOwnerMoverComponent = Actor->FindComponentByClass<UMoverComponent>();

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus USTT_PlayAnimFromBestCost::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime)
{
	if (!NeedsEvaluation)
	{
		return EStateTreeRunStatus::Running;
	}

	// Check velocity condition
	const FVector Velocity = GetActorVelocity();
	const double Speed = Velocity.Size();
	if (Speed < static_cast<double>(MinimumVelocityCheck))
	{
		return EStateTreeRunStatus::Running;
	}

	// Check distance condition
	const double Distance = NPCApproachAngleAndPathedDistance();
	if (Distance > static_cast<double>(MaximumDistanceThreshold))
	{
		return EStateTreeRunStatus::Running;
	}

	// Both conditions met — evaluate and play
	// On dedicated server, also check cost threshold
	const bool bIsDedicatedServer = UKismetSystemLibrary::IsDedicatedServer(this);
	EvaluateAndPlay(!bIsDedicatedServer);

	return EStateTreeRunStatus::Running;
}

void USTT_PlayAnimFromBestCost::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition)
{
	// Unbind delegates
	if (AAIController* AIC = CachedAIController.Get())
	{
		AIC->ReceiveMoveCompleted.RemoveDynamic(this, &USTT_PlayAnimFromBestCost::OnMovementCompleted);
	}

	if (SmartObjectAnimComponent)
	{
		SmartObjectAnimComponent->OwnerMontageFinished.RemoveDynamic(this, &USTT_PlayAnimFromBestCost::OnMontageFinished);
	}
}

double USTT_PlayAnimFromBestCost::NPCApproachAngleAndPathedDistance() const
{
	if (!Actor)
	{
		return 0.0;
	}

	// Try to use pathed distance from AI navigation path
	AAIController* AIC = UAIBlueprintHelperLibrary::GetAIController(Actor);
	if (AIC)
	{
		UNavigationPath* NavPath = UAIBlueprintHelperLibrary::GetCurrentPath(AIC);
		if (IsValid(NavPath))
		{
			return NavPath->GetPathLength();
		}
	}

	// Fallback: straight-line distance
	const FVector ActorLocation = Actor->GetActorLocation();
	return FVector::Dist(ActorLocation, Destination.GetLocation());
}

FVector USTT_PlayAnimFromBestCost::GetActorVelocity() const
{
	if (IsValid(PossibleOwnerMoverComponent))
	{
		return PossibleOwnerMoverComponent->GetVelocity();
	}

	if (Actor)
	{
		return Actor->GetVelocity();
	}

	return FVector::ZeroVector;
}

void USTT_PlayAnimFromBestCost::EvaluateAndPlay(bool bCheckCostThreshold)
{
	if (!IsValid(SmartObjectAnimComponent))
	{
		return;
	}

	// Evaluate distance and motion match
	double Cost = 0.0;
	double StartTime = 0.0;
	UAnimMontage* Montage = SmartObjectAnimComponent->EvaluateDistanceAndMotionMatch(
		Destination, AnimationProxyTable, Cost, StartTime);

	if (!IsValid(Montage))
	{
		return;
	}

	// On non-dedicated server, check cost threshold
	if (bCheckCostThreshold && Cost > static_cast<double>(CostThreshold))
	{
		return;
	}

	// Build payload
	FSmartObjectAnimationPayload Payload;
	Payload.MontageToPlay = Montage;
	Payload.StartTime = StartTime;

	// Get warp target from SmartObject subsystem
	if (USmartObjectSubsystem* SOSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USmartObjectSubsystem>() : nullptr)
	{
		Payload.UseWarpTarget = SOSubsystem->GetSlotTransform(ClaimedHandle, Payload.WarpTargetTransform);
	}

	// Play montage
	SmartObjectAnimComponent->PlayMontage_Multi(Payload);

	// Done evaluating
	NeedsEvaluation = false;
}

void USTT_PlayAnimFromBestCost::OnMovementCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	if (Result == EPathFollowingResult::Success)
	{
		// NPC arrived — enable evaluation on next tick
		NeedsEvaluation = true;
	}
	else
	{
		// Movement failed — finish task with failure
		FinishTask(false);
	}
}

void USTT_PlayAnimFromBestCost::OnMontageFinished()
{
	FinishTask(true);
}
