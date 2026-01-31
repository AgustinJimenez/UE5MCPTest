// Copyright Epic Games, Inc. All Rights Reserved.

#include "Teleporter_Level.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"

ATeleporter_Level::ATeleporter_Level()
{
	PrimaryActorTick.bCanEverTick = false;

	// Initialize destination to empty
	Destination = FText::GetEmpty();
}

void ATeleporter_Level::BeginPlay()
{
	Super::BeginPlay();

	// Cache blueprint components by name
	CachedDefaultSceneRoot = Cast<USceneComponent>(GetDefaultSubobjectByName(TEXT("DefaultSceneRoot")));
	CachedDestinationName = Cast<UTextRenderComponent>(GetDefaultSubobjectByName(TEXT("DestinationName")));
	CachedPointer = Cast<UStaticMeshComponent>(GetDefaultSubobjectByName(TEXT("Pointer")));
	CachedTrigger = Cast<UBoxComponent>(GetDefaultSubobjectByName(TEXT("Trigger")));

	// Bind overlap event
	if (CachedTrigger)
	{
		CachedTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATeleporter_Level::OnTriggerBeginOverlap);
	}
}

void ATeleporter_Level::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Check if we have authority and are on server
	if (HasAuthority() && GetNetMode() != NM_Client)
	{
		// Build level path
		FString DestinationString = Destination.ToString();
		FString LevelPath = FString::Printf(TEXT("/Game/Levels/%s?listen"), *DestinationString);

		// Execute ServerTravel
		if (UWorld* World = GetWorld())
		{
			World->ServerTravel(LevelPath);
		}
	}
}
