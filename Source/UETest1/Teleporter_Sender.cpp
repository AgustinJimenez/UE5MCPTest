// Copyright Epic Games, Inc. All Rights Reserved.

#include "Teleporter_Sender.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"

ATeleporter_Sender::ATeleporter_Sender()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATeleporter_Sender::BeginPlay()
{
	Super::BeginPlay();

	// Cache blueprint components by name
	CachedDefaultSceneRoot = Cast<USceneComponent>(GetDefaultSubobjectByName(TEXT("DefaultSceneRoot")));
	CachedPlate = Cast<UStaticMeshComponent>(GetDefaultSubobjectByName(TEXT("Plate")));
	CachedTrigger = Cast<UCapsuleComponent>(GetDefaultSubobjectByName(TEXT("Trigger")));
	CachedDestinationName = Cast<UTextRenderComponent>(GetDefaultSubobjectByName(TEXT("DestinationName")));
	CachedPointer = Cast<UStaticMeshComponent>(GetDefaultSubobjectByName(TEXT("Pointer")));

	// Bind overlap event to Trigger component
	if (CachedTrigger)
	{
		CachedTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATeleporter_Sender::OnTriggerBeginOverlap);
	}
}

void ATeleporter_Sender::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || !Destination)
	{
		return;
	}

	// Get TeleportPoint component from Destination actor
	USceneComponent* TeleportPoint = Cast<USceneComponent>(Destination->GetDefaultSubobjectByName(TEXT("TeleportPoint")));
	if (!TeleportPoint)
	{
		return;
	}

	// Get teleport location and actor rotation
	FVector DestLocation = TeleportPoint->GetComponentLocation();
	FRotator DestRotation = OtherActor->GetActorRotation();

	// Teleport the actor
	OtherActor->K2_TeleportTo(DestLocation, DestRotation);
}

void ATeleporter_Sender::UpdateName()
{
	// TODO: Implement - update destination name display
	// This function is called by Teleporter_Destination during construction
}

void ATeleporter_Sender::UpdateColor()
{
	// TODO: Implement - update color to match destination
	// This function is called by Teleporter_Destination during construction
}

void ATeleporter_Sender::UpdateRotation()
{
	// TODO: Implement - update pointer rotation to point at destination
	// This function is called by Teleporter_Destination during construction
}

void ATeleporter_Sender::UpdateScale()
{
	// TODO: Implement - update scale to match destination
	// This function is called by Teleporter_Destination during construction
}
