#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeConditionBlueprintBase.h"
#include "STC_CheckCooldown.generated.h"

class AAIController;

UCLASS()
class UETEST1_API USTC_CheckCooldown : public UStateTreeConditionBlueprintBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooldown")
	TObjectPtr<AAIController> AIController;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cooldown")
	FString CooldownName;
};
