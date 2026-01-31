// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Teleporter_Level.generated.h"

class USceneComponent;
class UTextRenderComponent;
class UStaticMeshComponent;
class UBoxComponent;

/**
 * Level teleporter that triggers ServerTravel when overlapped
 */
UCLASS()
class UETEST1_API ATeleporter_Level : public AActor
{
	GENERATED_BODY()

public:
	ATeleporter_Level();

protected:
	// Cached components from blueprint (renamed to avoid collision with BP component names)
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CachedDefaultSceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> CachedDestinationName;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CachedPointer;

	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> CachedTrigger;

	// Destination level name
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleporter")
	FText Destination;

	virtual void BeginPlay() override;

	// Overlap event handler
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
