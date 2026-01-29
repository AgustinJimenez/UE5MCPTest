#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BP_UE4_Mannequin.generated.h"

class USceneComponent;
class USkeletalMeshComponent;

UCLASS()
class UETEST1_API ABP_UE4_Mannequin : public AActor
{
	GENERATED_BODY()

public:
	ABP_UE4_Mannequin();

protected:
	virtual void BeginPlay() override;

private:
	void HandleDeferredBeginPlay();

	UPROPERTY(VisibleAnywhere)
	USceneComponent* DefaultSceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* SkeletalMesh = nullptr;
};
