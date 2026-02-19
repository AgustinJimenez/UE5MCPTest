// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Teleporter_Sender.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UCapsuleComponent;
class ATeleporter_Destination;

/**
 * Teleporter that sends actors to a destination teleport point
 */
UCLASS()
class UETEST1_API ATeleporter_Sender : public AActor
{
	GENERATED_BODY()

public:
	ATeleporter_Sender();

	// Destination teleporter actor (contains TeleportPoint component)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleporter")
	TObjectPtr<AActor> Destination;

	// Update functions called by Teleporter_Destination during construction
	UFUNCTION(BlueprintCallable, Category = "Teleporter")
	void UpdateName();

	UFUNCTION(BlueprintCallable, Category = "Teleporter")
	void UpdateColor();

	UFUNCTION(BlueprintCallable, Category = "Teleporter")
	void UpdateRotation();

	UFUNCTION(BlueprintCallable, Category = "Teleporter")
	void UpdateScale();

protected:
	// Cached components from blueprint (renamed to avoid collision with BP component names)
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CachedDefaultSceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CachedPlate;

	UPROPERTY(Transient)
	TObjectPtr<UCapsuleComponent> CachedTrigger;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> CachedDestinationName;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CachedPointer;

	virtual void BeginPlay() override;

	// Trigger overlap handler
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
