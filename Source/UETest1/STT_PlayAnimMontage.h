#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "SmartObjectRuntime.h"
#include "STT_PlayAnimMontage.generated.h"

class AActor;
class UAC_SmartObjectAnimation;
class UAnimMontage;
class UCharacterMoverComponent;

UCLASS()
class UETEST1_API USTT_PlayAnimMontage : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

public:
	USTT_PlayAnimMontage(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<AActor> Actor;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UAnimMontage> MontageToPlay;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	float StartTime = 0.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	float PlayTime = 0.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	float PlayTimeVariance = 0.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	float PlayRate = 1.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	int32 NumberOfLoops = 0;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	bool IgnoreCollision = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<UCharacterMoverComponent> MoverComponent;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	FSmartObjectClaimHandle SlotHandle;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Input")
	TObjectPtr<AActor> SmartObjectActor;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;

private:
	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAC_SmartObjectAnimation> SmartObjectAnimComponent;

	UFUNCTION()
	void OnAnimationFinished();
};
