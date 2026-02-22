#include "AnimNotifyState_MontageBlendOut.h"
#include "BPI_SandboxCharacter_Pawn.h"
#include "CharacterPropertiesStructs.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotifyState_MontageBlendOut::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	// Use C++ interface — bypasses the broken BP interface cast
	if (!Owner->GetClass()->ImplementsInterface(UBPI_SandboxCharacter_Pawn::StaticClass()))
	{
		return;
	}

	FS_CharacterPropertiesForAnimation Props =
		IBPI_SandboxCharacter_Pawn::Execute_Get_PropertiesForAnimation(Owner);

	// Only act on montages
	UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	if (!Montage) return;

	// The BP child (BP_NotifyState_MontageBlendOut) has its own variables
	// BlendOutCondition_0 and BlendOutTime_0 that shadow the C++ UPROPERTYs.
	// Montage instances store values on the BP variables, so read those via reflection.
	ETraversalBlendOutCondition EffectiveCondition = BlendOutCondition;
	float EffectiveBlendOutTime = BlendOutTime;

	UClass* ThisClass = GetClass();
	if (FByteProperty* BPConditionProp = FindFProperty<FByteProperty>(ThisClass, TEXT("BlendOutCondition_0")))
	{
		EffectiveCondition = static_cast<ETraversalBlendOutCondition>(
			BPConditionProp->GetPropertyValue_InContainer(this));
	}
	if (FDoubleProperty* BPBlendTimeProp = FindFProperty<FDoubleProperty>(ThisClass, TEXT("BlendOutTime_0")))
	{
		EffectiveBlendOutTime = static_cast<float>(
			BPBlendTimeProp->GetPropertyValue_InContainer(this));
	}

	// Determine blend-out condition
	bool bShouldBlendOut = false;
	switch (EffectiveCondition)
	{
	case ETraversalBlendOutCondition::ForceBlendOut:
		bShouldBlendOut = true;
		break;
	case ETraversalBlendOutCondition::WithMovementInput:
		bShouldBlendOut = !Props.InputAcceleration.IsNearlyZero();
		break;
	case ETraversalBlendOutCondition::IfFalling:
		bShouldBlendOut = (Props.MovementMode == E_MovementMode::InAir);
		break;
	}

	if (bShouldBlendOut)
	{
		UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
		if (AnimInstance)
		{
			FMontageBlendSettings BlendSettings(EffectiveBlendOutTime);

			// Resolve blend profile by name from the skeleton
			if (!BlendProfile.IsNone())
			{
				if (USkeleton* Skeleton = MeshComp->GetSkeletalMeshAsset() ?
					MeshComp->GetSkeletalMeshAsset()->GetSkeleton() : nullptr)
				{
					BlendSettings.BlendProfile = Skeleton->GetBlendProfile(BlendProfile);
				}
			}

			AnimInstance->Montage_StopWithBlendSettings(BlendSettings, Montage);
		}
	}
}
