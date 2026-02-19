#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BP_Quinn.generated.h"

UCLASS()
class UETEST1_API ABP_Quinn : public AActor
{
	GENERATED_BODY()

public:
	ABP_Quinn();

protected:
	virtual void BeginPlay() override;

private:
	void HandleDeferredBeginPlay();
};
