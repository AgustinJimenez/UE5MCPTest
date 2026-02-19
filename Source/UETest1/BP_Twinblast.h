#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BP_Twinblast.generated.h"

UCLASS()
class UETEST1_API ABP_Twinblast : public AActor
{
	GENERATED_BODY()

public:
	ABP_Twinblast();

protected:
	virtual void BeginPlay() override;

private:
	void HandleDeferredBeginPlay();
};
