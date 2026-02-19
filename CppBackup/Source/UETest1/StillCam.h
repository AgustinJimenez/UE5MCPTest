// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StillCam.generated.h"

class UCameraComponent;

/**
 * Simple camera actor that can look at and/or follow a target actor
 */
UCLASS()
class UETEST1_API AStillCam : public AActor
{
	GENERATED_BODY()

public:
	AStillCam();

protected:
	// Camera component (cached from blueprint at runtime)
	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CameraComponent;

	// Target actor to look at and/or follow
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StillCam")
	TObjectPtr<AActor> TargetActor;

	// If true, camera will rotate to look at the target
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StillCam")
	bool LookAtTarget;

	// If true, camera will follow the target's movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StillCam")
	bool FollowTarget;

	// Cached location of target from previous frame
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StillCam")
	FVector LastTargetLocation;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
};
