#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BP_Quinn.generated.h"

class USceneComponent;
class USkeletalMeshComponent;

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

	UPROPERTY(VisibleAnywhere)
	USceneComponent* DefaultSceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* SkeletalMesh = nullptr;
};
