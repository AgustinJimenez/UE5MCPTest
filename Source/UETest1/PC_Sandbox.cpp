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
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "UObject/UnrealType.h"
#include "SandboxCharacter_CMC.h"

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

	// Sprint/gait/speed override via reflection — only needed for BP-only pawns.
	// ASandboxCharacter_CMC handles this natively in OnPreCMCTick(), so skip it.
	if (APawn* ControlledPawn = GetPawn())
	{
		if (!Cast<ASandboxCharacter_CMC>(ControlledPawn))
		{
			if (ACharacter* CharacterPawn = Cast<ACharacter>(ControlledPawn))
			{
				const bool bShiftHeld = IsInputKeyDown(EKeys::LeftShift);
				UClass* PawnClass = ControlledPawn->GetClass();

				// Set WantsToSprint on CharacterInputState struct
				if (FStructProperty* InputStateProp = FindFProperty<FStructProperty>(PawnClass, TEXT("CharacterInputState")))
				{
					void* StructPtr = InputStateProp->ContainerPtrToValuePtr<void>(ControlledPawn);
					for (TFieldIterator<FProperty> It(InputStateProp->Struct); It; ++It)
					{
						if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(*It))
						{
							if (It->GetName().Contains(TEXT("WantsToSprint")))
							{
								BoolProp->SetPropertyValue_InContainer(StructPtr, bShiftHeld);
							}
						}
					}
				}

				if (UCharacterMovementComponent* CMC = CharacterPawn->GetCharacterMovement())
				{
					const bool bHasMoveInput = ControlledPawn->GetPendingMovementInputVector().Size2D() > 0.1f || CMC->GetCurrentAcceleration().Size2D() > 0.1f;
					const bool bShouldSprint = bShiftHeld && bHasMoveInput;

					// Set Gait (FEnumProperty or FByteProperty)
					const uint8 TargetGait = bShouldSprint ? 2 : 1; // Walk=0, Run=1, Sprint=2
					if (FByteProperty* GaitProp = FindFProperty<FByteProperty>(PawnClass, TEXT("Gait")))
					{
						GaitProp->SetPropertyValue_InContainer(ControlledPawn, TargetGait);
					}
					else if (FEnumProperty* GaitEnumProp = FindFProperty<FEnumProperty>(PawnClass, TEXT("Gait")))
					{
						FNumericProperty* UnderlyingProp = GaitEnumProp->GetUnderlyingProperty();
						if (UnderlyingProp)
						{
							UnderlyingProp->SetIntPropertyValue(GaitEnumProp->ContainerPtrToValuePtr<void>(ControlledPawn), (int64)TargetGait);
						}
					}

					// Read SprintSpeeds.X and RunSpeeds.X to set MaxWalkSpeed directly
					double SprintSpeedX = 700.0;
					double RunSpeedX = 500.0;
					if (FProperty* SprintProp = FindFProperty<FProperty>(PawnClass, TEXT("SprintSpeeds")))
					{
						const FVector* SprintVec = SprintProp->ContainerPtrToValuePtr<FVector>(ControlledPawn);
						if (SprintVec) SprintSpeedX = SprintVec->X;
					}
					if (FProperty* RunProp = FindFProperty<FProperty>(PawnClass, TEXT("RunSpeeds")))
					{
						const FVector* RunVec = RunProp->ContainerPtrToValuePtr<FVector>(ControlledPawn);
						if (RunVec) RunSpeedX = RunVec->X;
					}

					CMC->MaxWalkSpeed = bShouldSprint ? SprintSpeedX : RunSpeedX;
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
