// Copyright Epic Games, Inc. All Rights Reserved.

#include "StillCam.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"

AStillCam::AStillCam()
{
	PrimaryActorTick.bCanEverTick = true;

	// Initialize variables
	LookAtTarget = false;
	FollowTarget = false;
	LastTargetLocation = FVector::ZeroVector;
	CameraComponent = nullptr;
}

void AStillCam::BeginPlay()
{
	Super::BeginPlay();

	// Cache blueprint camera component by name
	CameraComponent = Cast<UCameraComponent>(GetDefaultSubobjectByName(TEXT("Camera")));
}

void AStillCam::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Branch 0: Look at target
	if (LookAtTarget && TargetActor)
	{
		FVector SelfLocation = K2_GetActorLocation();
		FVector TargetLocation = TargetActor->K2_GetActorLocation();
		FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(SelfLocation, TargetLocation);
		K2_SetActorRotation(LookAtRotation, false);
	}

	// Branch 1: Follow target
	if (FollowTarget && TargetActor)
	{
		FVector TargetLocation = TargetActor->K2_GetActorLocation();
		FVector DeltaLocation = TargetLocation - LastTargetLocation;
		FHitResult SweepHitResult;
		K2_AddActorWorldOffset(DeltaLocation, false, SweepHitResult, false);
		LastTargetLocation = TargetLocation;
	}
	else if (TargetActor)
	{
		// Not following but update last target location
		LastTargetLocation = TargetActor->K2_GetActorLocation();
	}
}
