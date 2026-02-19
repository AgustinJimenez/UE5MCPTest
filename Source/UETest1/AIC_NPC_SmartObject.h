#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AIC_NPC_SmartObject.generated.h"

class UStateTreeAIComponent;

UCLASS()
class UETEST1_API AAIC_NPC_SmartObject : public AAIController
{
	GENERATED_BODY()

public:
	AAIC_NPC_SmartObject();
	void AddCooldown(const FString& Name, double ExpirationTime);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UStateTreeAIComponent> CachedStateTreeAI;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AI")
	TMap<FString, double> Cooldowns;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI")
	float DedicatedServerStartDelay = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI")
	float ClientStartDelay = 2.0f;

private:
	void StartStateTreeLogic();

	FTimerHandle StartLogicHandle;
};
