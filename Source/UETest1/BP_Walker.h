// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BP_Walker.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USphereComponent;

UCLASS()
class UETEST1_API ABP_Walker : public ACharacter
{
	GENERATED_BODY()

public:
	ABP_Walker();

protected:
	virtual void BeginPlay() override;

	// Cached components
	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CachedSpringArm;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CachedCamera;

	UPROPERTY(Transient)
	TObjectPtr<USphereComponent> CachedSphere;
};
