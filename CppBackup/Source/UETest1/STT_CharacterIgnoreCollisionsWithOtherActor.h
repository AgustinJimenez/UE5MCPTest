#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "STT_CharacterIgnoreCollisionsWithOtherActor.generated.h"

class AActor;
class UActorComponent;

UCLASS()
class UETEST1_API USTT_CharacterIgnoreCollisionsWithOtherActor : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

public:
	USTT_CharacterIgnoreCollisionsWithOtherActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	UPROPERTY(EditInstanceOnly, Category = "AI")
	TObjectPtr<AActor> Character;

	UPROPERTY(EditInstanceOnly, Category = "AI")
	TObjectPtr<AActor> OtherActor;

	UPROPERTY(EditInstanceOnly, Category = "AI")
	bool ShouldIgnore = false;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TSubclassOf<UActorComponent> SmartObjectAnimationClass;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;
};
