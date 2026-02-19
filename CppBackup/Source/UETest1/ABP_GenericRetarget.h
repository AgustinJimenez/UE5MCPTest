#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/IKRetargetProfile.h"
#include "ABP_GenericRetarget.generated.h"

UCLASS()
class UETEST1_API UABP_GenericRetarget : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retarget")
	TObjectPtr<UIKRetargeter> IKRetargeter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retarget")
	TMap<FName, TObjectPtr<UIKRetargeter>> IKRetargeter_Map;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retarget")
	FRetargetProfile RetargetProfile;

private:
	void SetupRetargeterFromComponentTag();
};
