#include "STT_PlayAnimMontage.h"

#include "AC_SmartObjectAnimation.h"
#include "Animation/AnimMontage.h"
#include "SmartObjectSubsystem.h"

USTT_PlayAnimMontage::USTT_PlayAnimMontage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

EStateTreeRunStatus USTT_PlayAnimMontage::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition)
{
	// If no montage, finish immediately
	if (!MontageToPlay)
	{
		OnAnimationFinished();
		return EStateTreeRunStatus::Running;
	}

	// Get AC_SmartObjectAnimation component from Actor
	if (Actor)
	{
		SmartObjectAnimComponent = Actor->FindComponentByClass<UAC_SmartObjectAnimation>();
	}

	if (!SmartObjectAnimComponent)
	{
		OnAnimationFinished();
		return EStateTreeRunStatus::Running;
	}

	// Bind to OwnerMontageFinished delegate
	SmartObjectAnimComponent->OwnerMontageFinished.AddDynamic(this, &USTT_PlayAnimMontage::OnAnimationFinished);

	// Build the payload struct
	FSmartObjectAnimationPayload Payload;
	Payload.MontageToPlay = MontageToPlay;
	Payload.PlayTime = static_cast<double>(PlayTime);
	Payload.RandomPlaytimeVariance = static_cast<double>(PlayTimeVariance);
	Payload.StartTime = static_cast<double>(StartTime);
	Payload.Playrate = static_cast<double>(PlayRate);
	Payload.NumLoops = NumberOfLoops;

	// Get slot transform from SmartObjectSubsystem for warp target
	if (USmartObjectSubsystem* SOSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USmartObjectSubsystem>() : nullptr)
	{
		Payload.UseWarpTarget = SOSubsystem->GetSlotTransform(SlotHandle, Payload.WarpTargetTransform);
	}

	// Call the replicated multicast function directly
	SmartObjectAnimComponent->PlayMontage_Multi(Payload);

	return EStateTreeRunStatus::Running;
}

void USTT_PlayAnimMontage::OnAnimationFinished()
{
	FinishTask(true);
}
