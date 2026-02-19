// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TargetDummy.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Target dummy actor that registers/unregisters itself with overlapping characters
 */
UCLASS()
class UETEST1_API ATargetDummy : public AActor
{
	GENERATED_BODY()

public:
	ATargetDummy();

protected:
	// Cached components from blueprint (renamed to avoid collision with BP component names)
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CachedDefaultSceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CachedCylinder;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CachedSphere;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CachedTrigger;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> CachedText;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CachedZone;

	virtual void BeginPlay() override;

	// Trigger overlap handlers
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
