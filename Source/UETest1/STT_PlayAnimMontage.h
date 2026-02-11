#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "SmartObjectRuntime.h"
#include "STT_PlayAnimMontage.generated.h"

class AActor;
class UActorComponent;
class UAnimMontage;
class UCharacterMoverComponent;

UCLASS()
class UETEST1_API USTT_PlayAnimMontage : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

public:
	USTT_PlayAnimMontage(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	UPROPERTY(EditInstanceOnly, Category = "Input")
	TObjectPtr<AActor> Actor;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	TObjectPtr<UAnimMontage> MontageToPlay;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	float StartTime = 0.0f;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	float PlayTime = 0.0f;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	float PlayTimeVariance = 0.0f;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	float PlayRate = 1.0f;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	int32 NumberOfLoops = 0;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	bool IgnoreCollision = false;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	TObjectPtr<UCharacterMoverComponent> MoverComponent;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	FSmartObjectClaimHandle SlotHandle;

	UPROPERTY(EditInstanceOnly, Category = "Input")
	TObjectPtr<AActor> SmartObjectActor;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;

private:
	UPROPERTY()
	TSubclassOf<UActorComponent> SmartObjectAnimationClass;

	UPROPERTY()
	TObjectPtr<UActorComponent> SmartObjectAnimComponent;

	UFUNCTION()
	void OnAnimationFinished();
};
