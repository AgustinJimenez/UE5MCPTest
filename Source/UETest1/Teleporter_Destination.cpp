// Fill out your copyright notice in the Description page of Project Settings.

#include "Teleporter_Destination.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Teleporter_Sender.h"

ATeleporter_Destination::ATeleporter_Destination()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATeleporter_Destination::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Cache components by name
	CachedDefaultSceneRoot = Cast<USceneComponent>(GetDefaultSubobjectByName(TEXT("DefaultSceneRoot")));
	CachedPlate = Cast<UStaticMeshComponent>(GetDefaultSubobjectByName(TEXT("Plate")));
	CachedName = Cast<UTextRenderComponent>(GetDefaultSubobjectByName(TEXT("Name")));
	CachedTeleportPoint = Cast<USceneComponent>(GetDefaultSubobjectByName(TEXT("TeleportPoint")));

	// Run construction script logic
	UpdateName();
	UpdateColor();
	UpdateScale();
	UpdateSenders();
}

void ATeleporter_Destination::UpdateName()
{
	if (CachedName)
	{
		CachedName->SetText(DestinationName);
	}
}

void ATeleporter_Destination::UpdateColor()
{
	if (CachedPlate)
	{
		// Convert LinearColor to Vector
		FVector ColorAsVector = FVector(Color.R, Color.G, Color.B);
		CachedPlate->SetVectorParameterValueOnMaterials(FName("Base Color"), ColorAsVector);
	}
}

void ATeleporter_Destination::UpdateScale()
{
	if (CachedPlate)
	{
		// Set world scale: (PlateScale, PlateScale, 0.02)
		CachedPlate->SetWorldScale3D(FVector(PlateScale, PlateScale, 0.02));
	}
}

void ATeleporter_Destination::UpdateSenders()
{
	// Get all Teleporter_Sender actors in the level
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(this, ATeleporter_Sender::StaticClass(), FoundActors);

	// For each sender, if its Destination is this actor, update it
	for (AActor* Actor : FoundActors)
	{
		if (ATeleporter_Sender* Sender = Cast<ATeleporter_Sender>(Actor))
		{
			// Check if this sender points to us
			if (Sender->Destination == this)
			{
				Sender->UpdateName();
				Sender->UpdateColor();
				Sender->UpdateRotation();
				Sender->UpdateScale();
			}
		}
	}
}
