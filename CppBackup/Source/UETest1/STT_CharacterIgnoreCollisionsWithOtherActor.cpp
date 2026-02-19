#include "STT_CharacterIgnoreCollisionsWithOtherActor.h"

#include "Components/ActorComponent.h"
#include "UObject/ConstructorHelpers.h"

USTT_CharacterIgnoreCollisionsWithOtherActor::USTT_CharacterIgnoreCollisionsWithOtherActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UActorComponent> SmartObjectAnimClassFinder(
		TEXT("/Game/Blueprints/SmartObjects/AC_SmartObjectAnimation"));
	if (SmartObjectAnimClassFinder.Succeeded())
	{
		SmartObjectAnimationClass = SmartObjectAnimClassFinder.Class;
	}
}

EStateTreeRunStatus USTT_CharacterIgnoreCollisionsWithOtherActor::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition)
{
	if (Character && SmartObjectAnimationClass)
	{
		if (UActorComponent* Component = Character->GetComponentByClass(SmartObjectAnimationClass))
		{
			static const FName SetIgnoreActorStateName(TEXT("SetIgnoreActorState"));
			if (UFunction* SetIgnoreFn = Component->FindFunction(SetIgnoreActorStateName))
			{
				struct FSetIgnoreActorStateParams
				{
					bool bShouldIgnore;
					AActor* OtherActor;
				};

				FSetIgnoreActorStateParams Params{ShouldIgnore, OtherActor};
				Component->ProcessEvent(SetIgnoreFn, &Params);
			}
		}
	}

	FinishTask(true);
	return EStateTreeRunStatus::Running;
}
