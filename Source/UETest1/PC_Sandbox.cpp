// Copyright Epic Games, Inc. All Rights Reserved.

#include "PC_Sandbox.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "SandboxCharacter_CMC.h"
#include "UObject/Field.h"
#include "UObject/UnrealType.h"

namespace
{
	bool TryReadBPInputState(UObject* Object, bool& bOutWantsSprint, bool& bOutWantsWalk, FString& OutGaitName)
	{
		if (!Object)
		{
			return false;
		}

		UClass* Class = Object->GetClass();
		if (!Class)
		{
			return false;
		}

		bool bAnyRead = false;
		bOutWantsSprint = false;
		bOutWantsWalk = false;
		OutGaitName = TEXT("Unknown");

		if (const FStructProperty* InputStateProp = FindFProperty<FStructProperty>(Class, TEXT("CharacterInputState")))
		{
			void* InputStatePtr = InputStateProp->ContainerPtrToValuePtr<void>(Object);
			if (InputStatePtr && InputStateProp->Struct)
			{
				for (TFieldIterator<FProperty> It(InputStateProp->Struct); It; ++It)
				{
					const FProperty* MemberProp = *It;
					if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(MemberProp))
					{
						const FString MemberName = MemberProp->GetName();
						if (MemberName.Contains(TEXT("WantsToSprint")))
						{
							bOutWantsSprint = BoolProp->GetPropertyValue_InContainer(InputStatePtr);
							bAnyRead = true;
						}
						else if (MemberName.Contains(TEXT("WantsToWalk")))
						{
							bOutWantsWalk = BoolProp->GetPropertyValue_InContainer(InputStatePtr);
							bAnyRead = true;
						}
					}
				}
			}
		}

		if (const FByteProperty* GaitProp = FindFProperty<FByteProperty>(Class, TEXT("Gait")))
		{
			const uint8 GaitValue = GaitProp->GetPropertyValue_InContainer(Object);
			if (GaitProp->Enum)
			{
				OutGaitName = GaitProp->Enum->GetNameStringByValue(GaitValue);
			}
			else
			{
				OutGaitName = FString::FromInt(static_cast<int32>(GaitValue));
			}
			bAnyRead = true;
		}

		return bAnyRead;
	}
}

APC_Sandbox::APC_Sandbox()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set default values
	CurrentCharacterIndex = 0;
	TeleportMaxDistance = 5000.0f;
	CachedControlRotation = FRotator::ZeroRotator;

	// Note: IMC_Sandbox is loaded at BeginPlay, not in constructor
	// ConstructorHelpers can fail for game content that loads asynchronously
}

void APC_Sandbox::BeginPlay()
{
	Super::BeginPlay();

	// Force-remove Shift debug exec bindings at runtime so LeftShift can be used for sprint.
	if (PlayerInput)
	{
		const int32 Before = PlayerInput->DebugExecBindings.Num();
		PlayerInput->DebugExecBindings.RemoveAll([](const FKeyBind& Bind)
		{
			const bool bLeftPrev = (Bind.Key == EKeys::LeftShift) && Bind.Command.Contains(TEXT("DebugManager.CycleToPreviousColumn"));
			const bool bRightNext = (Bind.Key == EKeys::RightShift) && Bind.Command.Contains(TEXT("DebugManager.CycleToNextColumn"));
			return bLeftPrev || bRightNext;
		});
		const int32 Removed = Before - PlayerInput->DebugExecBindings.Num();
		if (Removed > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("PC_Sandbox::BeginPlay - Removed %d Shift debug exec binding(s)"), Removed);
		}
	}

	// Load IMC_Sandbox at runtime if not already set
	if (!IMC_Sandbox)
	{
		IMC_Sandbox = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Sandbox.IMC_Sandbox"));
		UE_LOG(LogTemp, Warning, TEXT("PC_Sandbox::BeginPlay - Loaded IMC_Sandbox: %s"), IMC_Sandbox ? TEXT("SUCCESS") : TEXT("FAILED"));
	}

	// Add IMC_Sandbox mapping context for character controls (Sprint, Walk, Move, etc.)
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (LocalPlayer)
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
		if (Subsystem && IMC_Sandbox)
		{
			Subsystem->AddMappingContext(IMC_Sandbox, 0);
			UE_LOG(LogTemp, Warning, TEXT("PC_Sandbox::BeginPlay - Added IMC_Sandbox mapping context with priority 0"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PC_Sandbox::BeginPlay - Subsystem=%s, IMC_Sandbox=%s"),
				Subsystem ? TEXT("valid") : TEXT("NULL"),
				IMC_Sandbox ? TEXT("valid") : TEXT("NULL"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PC_Sandbox::BeginPlay - LocalPlayer is NULL!"));
	}
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

	// Temporary runtime fallback: enforce sprint speed/gait for SandboxCharacter_CMC blueprint instances.
	if (APawn* ControlledPawn = GetPawn())
	{
		if (ACharacter* CharacterPawn = Cast<ACharacter>(ControlledPawn))
		{
			bool bWantsSprint = false;
			bool bWantsWalk = false;
			FString GaitName;
			if (TryReadBPInputState(ControlledPawn, bWantsSprint, bWantsWalk, GaitName))
			{
				if (UCharacterMovementComponent* CMC = CharacterPawn->GetCharacterMovement())
				{
					const bool bHasMoveInput = ControlledPawn->GetPendingMovementInputVector().Size2D() > 0.1f || CMC->GetCurrentAcceleration().Size2D() > 0.1f;
					const bool bShouldSprint = bWantsSprint && bHasMoveInput;
					const float TargetMaxWalkSpeed = bShouldSprint ? 700.0f : 500.0f;
					if (!FMath::IsNearlyEqual(CMC->MaxWalkSpeed, TargetMaxWalkSpeed, 1.0f))
					{
						CMC->MaxWalkSpeed = TargetMaxWalkSpeed;
					}

					// Keep animation state aligned with speed override for the BP enum.
					if (FByteProperty* GaitProp = FindFProperty<FByteProperty>(ControlledPawn->GetClass(), TEXT("Gait")))
					{
						const uint8 TargetGait = bShouldSprint ? 2 : 1; // E_Gait: Walk=0, Run=1, Sprint=2
						GaitProp->SetPropertyValue_InContainer(ControlledPawn, TargetGait);
					}
				}
			}
		}
	}

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
