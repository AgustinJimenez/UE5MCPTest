// Fill out your copyright notice in the Description page of Project Settings.

#include "BP_Walker.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"

ABP_Walker::ABP_Walker()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABP_Walker::BeginPlay()
{
	Super::BeginPlay();

	// Cache components by name
	CachedSpringArm = Cast<USpringArmComponent>(GetDefaultSubobjectByName(TEXT("SpringArm")));
	CachedCamera = Cast<UCameraComponent>(GetDefaultSubobjectByName(TEXT("Camera")));
	CachedSphere = Cast<USphereComponent>(GetDefaultSubobjectByName(TEXT("Sphere")));
}
