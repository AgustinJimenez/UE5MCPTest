// Copyright Epic Games, Inc. All Rights Reserved.

#include "PC_Sandbox.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "GameFramework/GameModeBase.h"

APC_Sandbox::APC_Sandbox()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set default values
	CurrentCharacterIndex = 0;
	TeleportMaxDistance = 5000.0f;
	CachedControlRotation = FRotator::ZeroRotator;
}

void APC_Sandbox::BeginPlay()
{
	Super::BeginPlay();
}

void APC_Sandbox::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Bind teleport action
		if (TeleportToTargetAction)
		{
			EnhancedInputComponent->BindAction(TeleportToTargetAction, ETriggerEvent::Triggered, this, &APC_Sandbox::OnTeleportToTarget);
		}

		// Bind next pawn action
		if (NextPawnAction)
		{
			EnhancedInputComponent->BindAction(NextPawnAction, ETriggerEvent::Triggered, this, &APC_Sandbox::OnNextPawn);
		}

		// Bind next visual override action
		if (NextVisualOverrideAction)
		{
			EnhancedInputComponent->BindAction(NextVisualOverrideAction, ETriggerEvent::Triggered, this, &APC_Sandbox::OnNextVisualOverride);
		}
	}
}

void APC_Sandbox::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Hide virtual joystick on mobile when gamepad is connected
	FString PlatformName = UGameplayStatics::GetPlatformName();
	bool bIsAndroid = UKismetStringLibrary::EqualEqual_StriStri(PlatformName, TEXT("Android"));
	bool bIsIOS = UKismetStringLibrary::EqualEqual_StriStri(PlatformName, TEXT("iOS"));

	if (bIsAndroid || bIsIOS)
	{
		bool bGamepadConnected = UKismetSystemLibrary::IsControllerAssignedToGamepad(0);
		if (bGamepadConnected)
		{
			SetVirtualJoystickVisibility(false);
		}
	}
}

void APC_Sandbox::TeleportToTarget()
{
	// Get eye view point
	FVector TraceStart;
	FRotator ViewRotation;
	GetActorEyesViewPoint(TraceStart, ViewRotation);

	// Calculate trace end
	FVector ForwardVector = UKismetMathLibrary::GetForwardVector(ViewRotation);
	FVector TraceEnd = TraceStart + (ForwardVector * TeleportMaxDistance);

	// Perform sphere trace
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(GetPawn());

	FHitResult HitResult;
	bool bHit = UKismetSystemLibrary::SphereTraceSingle(
		GetWorld(),
		TraceStart,
		TraceEnd,
		32.0f,
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false,
		ActorsToIgnore,
		EDrawDebugTrace::ForDuration,
		HitResult,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		0.5f
	);

	// Calculate teleport location
	FVector TeleportLocation;
	if (bHit)
	{
		// Hit something - teleport to hit location + normal offset
		TeleportLocation = HitResult.Location + (HitResult.ImpactNormal * 50.0f);
	}
	else
	{
		// No hit - teleport to max distance
		TeleportLocation = TraceEnd;
	}

	// Teleport the pawn
	if (APawn* ControlledPawn = GetPawn())
	{
		FHitResult SweepHitResult;
		ControlledPawn->K2_SetActorLocation(TeleportLocation, false, SweepHitResult, true);
	}
}

void APC_Sandbox::ServerNextPawn_Implementation()
{
	// Get game mode and cycle pawn
	if (AGameModeBase* GameMode = UGameplayStatics::GetGameMode(this))
	{
		// Try to call CyclePawn on GM_Sandbox if it exists
		// This requires GM_Sandbox to have a CyclePawn function
		// For now, we'll just call it via UFunction
		if (UFunction* CyclePawnFunc = GameMode->FindFunction(FName("CyclePawn")))
		{
			GameMode->ProcessEvent(CyclePawnFunc, nullptr);
		}
	}
}

void APC_Sandbox::ServerNextVisualOverride_Implementation()
{
	// Get game mode and cycle visual override
	if (AGameModeBase* GameMode = UGameplayStatics::GetGameMode(this))
	{
		// Try to call CycleVisualOverride on GM_Sandbox if it exists
		if (UFunction* CycleVisualOverrideFunc = GameMode->FindFunction(FName("CycleVisualOverride")))
		{
			GameMode->ProcessEvent(CycleVisualOverrideFunc, nullptr);
		}
	}
}

void APC_Sandbox::OnTeleportToTarget(const FInputActionValue& Value)
{
	TeleportToTarget();
}

void APC_Sandbox::OnNextPawn(const FInputActionValue& Value)
{
	ServerNextPawn();
}

void APC_Sandbox::OnNextVisualOverride(const FInputActionValue& Value)
{
	ServerNextVisualOverride();
}
