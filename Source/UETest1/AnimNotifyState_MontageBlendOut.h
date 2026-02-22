#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "LocomotionEnums.h"
#include "AnimNotifyState_MontageBlendOut.generated.h"

UCLASS(Blueprintable, meta = (DisplayName = "Montage Blend Out"))
class UETEST1_API UAnimNotifyState_MontageBlendOut : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlendOut")
	ETraversalBlendOutCondition BlendOutCondition = ETraversalBlendOutCondition::ForceBlendOut;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlendOut", meta = (ClampMin = "0.0"))
	float BlendOutTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlendOut")
	FName BlendProfile;
};
