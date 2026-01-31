#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BP_Echo.generated.h"

UCLASS()
class UETEST1_API ABP_Echo : public AActor
{
	GENERATED_BODY()

public:
	ABP_Echo();

protected:
	virtual void BeginPlay() override;

private:
	void HandleDeferredBeginPlay();
};
