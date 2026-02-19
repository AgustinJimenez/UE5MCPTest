// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Teleporter_Destination.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class UETEST1_API ATeleporter_Destination : public AActor
{
	GENERATED_BODY()

public:
	ATeleporter_Destination();

	virtual void OnConstruction(const FTransform& Transform) override;

	// Variables
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleporter")
	FText DestinationName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleporter")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleporter")
	double PlateScale = 1.0;

	// Functions
	UFUNCTION(BlueprintCallable, Category = "Teleporter")
	void UpdateName();

	UFUNCTION(BlueprintCallable, Category = "Teleporter")
	void UpdateColor();

	UFUNCTION(BlueprintCallable, Category = "Teleporter")
	void UpdateScale();

	UFUNCTION(BlueprintCallable, Category = "Teleporter")
	void UpdateSenders();

private:
	// Cached components (renamed to avoid collision with blueprint components)
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CachedDefaultSceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CachedPlate;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> CachedName;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CachedTeleportPoint;
};
