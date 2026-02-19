#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AC_PreCMCTick.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPreCMCTick);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UETEST1_API UAC_PreCMCTick : public UActorComponent
{
	GENERATED_BODY()

public:
	UAC_PreCMCTick();

	UPROPERTY(EditInstanceOnly, Category = "Pre CMC Tick", meta = (DisplayName = "As CBP Sandbox Character"))
	TObjectPtr<AActor> AsCBPSandboxCharacter;

	UPROPERTY(EditInstanceOnly, BlueprintAssignable, BlueprintCallable, Category = "Pre CMC Tick")
	FPreCMCTick Tick;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
