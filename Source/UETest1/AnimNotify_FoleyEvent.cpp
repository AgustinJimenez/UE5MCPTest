#include "AnimNotify_FoleyEvent.h"
#include "AC_FoleyEvents.h"
#include "Components/SkeletalMeshComponent.h"

UAnimNotify_FoleyEvent::UAnimNotify_FoleyEvent()
{
	Side = EFoleyEventSide::None;
	VolumeMultiplier = 1.0f;
	PitchMultiplier = 1.0f;
	VisLogDebugColor = FLinearColor::Green;
}

void UAnimNotify_FoleyEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !Event.IsValid())
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	// Find the AC_FoleyEvents component on the owner
	UAC_FoleyEvents* FoleyComponent = Owner->FindComponentByClass<UAC_FoleyEvents>();
	if (FoleyComponent)
	{
		FoleyComponent->PlayFoleyEvent(Event, VolumeMultiplier, PitchMultiplier);
	}
}
