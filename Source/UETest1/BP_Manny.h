#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BP_Manny.generated.h"

UCLASS()
class UETEST1_API ABP_Manny : public AActor
{
	GENERATED_BODY()

public:
	ABP_Manny();

protected:
	virtual void BeginPlay() override;

private:
	void HandleDeferredBeginPlay();
};
