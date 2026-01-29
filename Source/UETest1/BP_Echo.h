#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BP_Echo.generated.h"

class USceneComponent;
class USkeletalMeshComponent;

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

	UPROPERTY(VisibleAnywhere)
	USceneComponent* DefaultSceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* EchoBody = nullptr;

	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* EchoHair = nullptr;
};
