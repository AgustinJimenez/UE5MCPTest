#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BP_Manny.generated.h"

class USceneComponent;
class USkeletalMeshComponent;

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

	UPROPERTY(VisibleAnywhere)
	USceneComponent* DefaultSceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* Manny = nullptr;
};
