#include "SandboxCharacter_CMC.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "AC_PreCMCTick.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/ChildActorComponent.h"
#include "AC_FoleyEvents.h"
#include "AC_VisualOverrideManager.h"
#include "Kismet/KismetMathLibrary.h"
#include "KismetAnimationLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"

ASandboxCharacter_CMC::ASandboxCharacter_CMC()
{
	PrimaryActorTick.bCanEverTick = true;

	// Initialize default values
	MovementStickMode = E_AnalogStickBehavior::FixedSpeed_WalkRun;
	CameraStyle = E_CameraStyle::Medium;
	AnalogWalkRunThreshold = 0.5;
	Gait = E_Gait::Walk;

	// Speed defaults (verify from blueprint)
	WalkSpeeds = FVector(165.0, 165.0, 165.0);
	RunSpeeds = FVector(350.0, 350.0, 350.0);
	SprintSpeeds = FVector(600.0, 600.0, 600.0);
	WalkSpeeds_Demo = FVector(165.0, 165.0, 165.0);
	RunSpeeds_Demo = FVector(350.0, 350.0, 350.0);
	SprintSpeeds_Demo = FVector(600.0, 600.0, 600.0);
	CrouchSpeeds = FVector(150.0, 150.0, 150.0);

	// State initialization
	JustLanded = false;
	LandVelocity = FVector::ZeroVector;
	WasMovingOnGroundLastFrame_Simulated = false;
	LastUpdateVelocity = FVector::ZeroVector;
	UsingAttributeBasedRootMotion = false;
	IsRagdolling = false;
}

void ASandboxCharacter_CMC::BeginPlay()
{
	Super::BeginPlay();

	// Cache components created by Blueprint
	CachedMotionWarping = FindComponentByClass<UMotionWarpingComponent>();
	CachedPreCMCTick = FindComponentByClass<UAC_PreCMCTick>();
	CachedCamera = FindComponentByClass<UCameraComponent>();
	CachedSpringArm = FindComponentByClass<USpringArmComponent>();
	CachedVisualOverride = FindComponentByClass<UChildActorComponent>();
	CachedFoleyEvents = FindComponentByClass<UAC_FoleyEvents>();
	CachedVisualOverrideManager = FindComponentByClass<UAC_VisualOverrideManager>();

	// Cache Blueprint components by name
	TArray<UActorComponent*> Components;
	GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (Component->GetName().Contains(TEXT("GameplayCamera")))
		{
			CachedGameplayCamera = Component;
		}
		else if (Component->GetName().Contains(TEXT("AC_TraversalLogic")))
		{
			CachedTraversalLogic = Component;
		}
		else if (Component->GetName().Contains(TEXT("AC_SmartObjectAnimation")))
		{
			CachedSmartObjectAnimation = Component;
		}
	}

}

void ASandboxCharacter_CMC::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Setup Enhanced Input mapping context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (IMC_Sandbox)
			{
				Subsystem->AddMappingContext(IMC_Sandbox, 0);
			}
		}
	}

	// Bind Enhanced Input actions
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Move)
		{
			EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ASandboxCharacter_CMC::OnMove);
		}

		if (IA_Move_WorldSpace)
		{
			EnhancedInputComponent->BindAction(IA_Move_WorldSpace, ETriggerEvent::Triggered, this, &ASandboxCharacter_CMC::OnMoveWorldSpace);
		}

		if (IA_Look)
		{
			EnhancedInputComponent->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ASandboxCharacter_CMC::OnLook);
		}

		if (IA_Look_Gamepad)
		{
			EnhancedInputComponent->BindAction(IA_Look_Gamepad, ETriggerEvent::Triggered, this, &ASandboxCharacter_CMC::OnLookGamepad);
		}

		if (IA_Sprint)
		{
			EnhancedInputComponent->BindAction(IA_Sprint, ETriggerEvent::Triggered, this, &ASandboxCharacter_CMC::OnSprint);
		}

		if (IA_Walk)
		{
			EnhancedInputComponent->BindAction(IA_Walk, ETriggerEvent::Triggered, this, &ASandboxCharacter_CMC::OnWalk);
		}

		if (IA_Jump)
		{
			EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Started, this, &ASandboxCharacter_CMC::OnJumpAction);
			EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ASandboxCharacter_CMC::OnJumpReleased);
		}

		if (IA_Crouch)
		{
			EnhancedInputComponent->BindAction(IA_Crouch, ETriggerEvent::Triggered, this, &ASandboxCharacter_CMC::OnCrouchAction);
		}

		if (IA_Strafe)
		{
			EnhancedInputComponent->BindAction(IA_Strafe, ETriggerEvent::Triggered, this, &ASandboxCharacter_CMC::OnStrafe);
		}

		if (IA_Aim)
		{
			EnhancedInputComponent->BindAction(IA_Aim, ETriggerEvent::Triggered, this, &ASandboxCharacter_CMC::OnAim);
		}
	}
}

void ASandboxCharacter_CMC::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update Gait based on input and sprint conditions
	Gait = GetDesiredGait();

	// Update movement physics properties
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		// Set max speed based on gait and direction
		CMC->MaxWalkSpeed = CalculateMaxSpeed();

		// Set crouch speed
		CMC->MaxWalkSpeedCrouched = CalculateMaxCrouchSpeed();

		// Set acceleration
		CMC->MaxAcceleration = CalculateMaxAcceleration();

		// Set braking deceleration
		CMC->BrakingDecelerationWalking = CalculateBrakingDeceleration();

		// Set braking friction
		CMC->BrakingFrictionFactor = CalculateBrakingFriction();

		// Set ground friction
		CMC->GroundFriction = CalculateGroundFriction();
	}
}

// Interface stub implementations (Phase 1)
FS_CharacterPropertiesForAnimation ASandboxCharacter_CMC::Get_PropertiesForAnimation_Implementation()
{
	return FS_CharacterPropertiesForAnimation();
}

FS_CharacterPropertiesForCamera ASandboxCharacter_CMC::Get_PropertiesForCamera_Implementation()
{
	return FS_CharacterPropertiesForCamera();
}

FS_CharacterPropertiesForTraversal ASandboxCharacter_CMC::Get_PropertiesForTraversal_Implementation()
{
	return FS_CharacterPropertiesForTraversal();
}

void ASandboxCharacter_CMC::Set_CharacterInputState_Implementation(FS_PlayerInputState DesiredInputState)
{
	CharacterInputState = DesiredInputState;
}

// ===== PHYSICS CALCULATION FUNCTIONS =====

bool ASandboxCharacter_CMC::HasMovementInputVector() const
{
	FVector PendingInput = GetPendingMovementInputVector();
	return !PendingInput.IsNearlyZero();
}

double ASandboxCharacter_CMC::CalculateBrakingDeceleration() const
{
	return HasMovementInputVector() ? 500.0 : 2000.0;
}

double ASandboxCharacter_CMC::CalculateBrakingFriction() const
{
	bool bIsAccelerating = GetCharacterMovement()->GetCurrentAcceleration().IsNearlyZero();
	return bIsAccelerating ? 3.0 : 0.0;
}

double ASandboxCharacter_CMC::CalculateGroundFriction() const
{
	switch (Gait)
	{
	case E_Gait::Walk:
	case E_Gait::Run:
		return 5.0;

	case E_Gait::Sprint:
	{
		double Speed = GetCharacterMovement()->Velocity.Size2D();
		return UKismetMathLibrary::MapRangeClamped(Speed, 0.0, 500.0, 5.0, 3.0);
	}

	default:
		return 5.0;
	}
}

double ASandboxCharacter_CMC::CalculateMaxAcceleration() const
{
	switch (Gait)
	{
	case E_Gait::Walk:
	case E_Gait::Run:
		return 800.0;

	case E_Gait::Sprint:
	{
		double Speed = GetCharacterMovement()->Velocity.Size2D();
		return UKismetMathLibrary::MapRangeClamped(Speed, 300.0, 700.0, 800.0, 300.0);
	}

	default:
		return 800.0;
	}
}

// ===== GAIT/SPEED SYSTEM =====

E_Gait ASandboxCharacter_CMC::GetDesiredGait()
{
	// Calculate FullMovementInput based on analog stick behavior
	// Note: IA_Move and IA_Move_WorldSpace would need Enhanced Input system integration
	// For now, using simplified logic with GetPendingMovementInputVector
	FVector2D MoveInput = FVector2D(GetPendingMovementInputVector().X, GetPendingMovementInputVector().Y);
	double InputMagnitude = MoveInput.Size();

	bool bHasFullInput = InputMagnitude >= AnalogWalkRunThreshold;

	// Set FullMovementInput based on MovementStickMode
	switch (MovementStickMode)
	{
	case E_AnalogStickBehavior::FixedSpeed_SingleGait:
	case E_AnalogStickBehavior::FixedSpeed_WalkRun:
		FullMovementInput = true;
		break;
	case E_AnalogStickBehavior::VariableSpeed_SingleGait:
	case E_AnalogStickBehavior::VariableSpeed_WalkRun:
		FullMovementInput = bHasFullInput;
		break;
	default:
		FullMovementInput = true;
		break;
	}

	// Check sprint condition
	if (CanSprint() && FullMovementInput)
	{
		return E_Gait::Sprint;
	}

	// Check walk condition
	if (CharacterInputState.WantsToWalk)
	{
		return E_Gait::Walk;
	}

	// Default based on FullMovementInput
	return FullMovementInput ? E_Gait::Run : E_Gait::Walk;
}

bool ASandboxCharacter_CMC::CanSprint() const
{
	// Get input vector (locally controlled uses pending input, simulated uses acceleration)
	FVector InputVector = IsLocallyControlled()
		? GetPendingMovementInputVector()
		: GetCharacterMovement()->GetCurrentAcceleration();

	// Convert to rotation and calculate yaw delta from actor rotation
	FRotator InputRotation = InputVector.ToOrientationRotator();
	FRotator ActorRotation = GetActorRotation();
	FRotator DeltaRotation = UKismetMathLibrary::NormalizedDeltaRotator(ActorRotation, InputRotation);

	// Check if input is forward-facing (within 50 degrees)
	double AbsYaw = FMath::Abs(DeltaRotation.Yaw);
	bool bIsForwardInput = AbsYaw < 50.0;

	// If orient rotation to movement, always allow sprint, otherwise check forward input
	bool bCanSprintBasedOnRotation = GetCharacterMovement()->bOrientRotationToMovement || bIsForwardInput;

	// Final check: wants to sprint AND rotation allows it
	return CharacterInputState.WantsToSprint && bCanSprintBasedOnRotation;
}

double ASandboxCharacter_CMC::CalculateMaxSpeed()
{
	// Calculate direction-based speed multiplier
	FVector Velocity = GetCharacterMovement()->Velocity;
	FRotator ActorRotation = GetActorRotation();
	double Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, ActorRotation);
	double AbsDirection = FMath::Abs(Direction);

	// Use curve to map direction to speed multiplier (or 0 if using controller desired rotation)
	if (StrafeSpeedMapCurve && !GetCharacterMovement()->bUseControllerDesiredRotation)
	{
		StrafeSpeedMap = StrafeSpeedMapCurve->GetFloatValue(AbsDirection);
	}
	else
	{
		StrafeSpeedMap = 0.0f;
	}

	// Select speed vector based on gait
	FVector SpeedVector;
	switch (Gait)
	{
	case E_Gait::Walk:
		SpeedVector = WalkSpeeds;
		break;
	case E_Gait::Run:
		SpeedVector = RunSpeeds;
		break;
	case E_Gait::Sprint:
		SpeedVector = SprintSpeeds;
		break;
	default:
		SpeedVector = RunSpeeds;
		break;
	}

	// Map strafe speed to appropriate range
	double ResultSpeed;
	if (StrafeSpeedMap < 1.0)
	{
		// 0-1: Forward to Strafe
		ResultSpeed = UKismetMathLibrary::MapRangeClamped(StrafeSpeedMap, 0.0, 1.0, SpeedVector.X, SpeedVector.Y);
	}
	else
	{
		// 1-2: Strafe to Backward
		ResultSpeed = UKismetMathLibrary::MapRangeClamped(StrafeSpeedMap, 1.0, 2.0, SpeedVector.Y, SpeedVector.Z);
	}

	return ResultSpeed;
}

double ASandboxCharacter_CMC::CalculateMaxCrouchSpeed()
{
	// Calculate direction-based speed multiplier
	FVector Velocity = GetCharacterMovement()->Velocity;
	FRotator ActorRotation = GetActorRotation();
	double Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, ActorRotation);
	double AbsDirection = FMath::Abs(Direction);

	// Use curve to map direction to speed multiplier (or 0 if orient rotation to movement)
	if (StrafeSpeedMapCurve && !GetCharacterMovement()->bOrientRotationToMovement)
	{
		StrafeSpeedMap = StrafeSpeedMapCurve->GetFloatValue(AbsDirection);
	}
	else
	{
		StrafeSpeedMap = 0.0f;
	}

	// Map strafe speed to appropriate range using crouch speeds
	double ResultSpeed;
	if (StrafeSpeedMap < 1.0)
	{
		// 0-1: Forward to Strafe
		ResultSpeed = UKismetMathLibrary::MapRangeClamped(StrafeSpeedMap, 0.0, 1.0, CrouchSpeeds.X, CrouchSpeeds.Y);
	}
	else
	{
		// 1-2: Strafe to Backward
		ResultSpeed = UKismetMathLibrary::MapRangeClamped(StrafeSpeedMap, 1.0, 2.0, CrouchSpeeds.Y, CrouchSpeeds.Z);
	}

	return ResultSpeed;
}

// ===== INPUT HANDLERS =====

FVector2D ASandboxCharacter_CMC::GetMovementInputScaleValue(const FVector2D& Input) const
{
	// Scale input based on analog stick behavior
	// For now, return normalized input
	// TODO: Implement analog stick scaling based on MovementStickMode
	return Input.GetSafeNormal();
}

void ASandboxCharacter_CMC::OnMove(const FInputActionValue& Value)
{
	// Get the 2D input value
	FVector2D MovementVector = Value.Get<FVector2D>();

	// Get movement input scale
	FVector2D ScaledInput = GetMovementInputScaleValue(MovementVector);

	// Get control rotation for movement direction
	const FRotator ControlRotation = GetControlRotation();
	const FRotator YawRotation(0, ControlRotation.Yaw, 0);

	// Get forward and right vectors
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// Add movement input
	AddMovementInput(RightDirection, ScaledInput.X);
	AddMovementInput(ForwardDirection, ScaledInput.Y);
}

void ASandboxCharacter_CMC::OnMoveWorldSpace(const FInputActionValue& Value)
{
	// Get the 2D input value
	FVector2D MovementVector = Value.Get<FVector2D>();

	// Normalize for world space movement
	FVector2D NormalizedInput = MovementVector.GetSafeNormal();

	// Add movement input in world space (Y=forward, X=right in world coordinates)
	AddMovementInput(FVector(0, 1, 0), NormalizedInput.X);  // World Y
	AddMovementInput(FVector(1, 0, 0), NormalizedInput.Y);  // World X
}

void ASandboxCharacter_CMC::OnLook(const FInputActionValue& Value)
{
	// Get the 2D input value
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// Add yaw and pitch input
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void ASandboxCharacter_CMC::OnLookGamepad(const FInputActionValue& Value)
{
	// Get the 2D input value
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// Add yaw and pitch input (same as OnLook for now)
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void ASandboxCharacter_CMC::OnSprint(const FInputActionValue& Value)
{
	// Set sprint flag in input state
	CharacterInputState.WantsToSprint = Value.Get<bool>();
}

void ASandboxCharacter_CMC::OnWalk(const FInputActionValue& Value)
{
	// Toggle walk state
	CharacterInputState.WantsToWalk = !CharacterInputState.WantsToWalk;
}

void ASandboxCharacter_CMC::OnJumpAction(const FInputActionValue& Value)
{
	// Call base character Jump()
	Jump();
}

void ASandboxCharacter_CMC::OnJumpReleased(const FInputActionValue& Value)
{
	// Call base character StopJumping()
	StopJumping();
}

void ASandboxCharacter_CMC::OnCrouchAction(const FInputActionValue& Value)
{
	// Toggle crouch state
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void ASandboxCharacter_CMC::OnStrafe(const FInputActionValue& Value)
{
	// Toggle strafe state
	CharacterInputState.WantsToStrafe = !CharacterInputState.WantsToStrafe;
}

void ASandboxCharacter_CMC::OnAim(const FInputActionValue& Value)
{
	// Set aim flag in input state
	CharacterInputState.WantsToAim = Value.Get<bool>();
}

// ===== PHYSICS EVENTS =====

void ASandboxCharacter_CMC::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// Set JustLanded flag and capture landing velocity
	JustLanded = true;
	LandVelocity = GetCharacterMovement()->Velocity;

	// Clear any existing timer
	GetWorldTimerManager().ClearTimer(JustLandedTimerHandle);

	// Set timer to reset JustLanded after 0.3 seconds
	GetWorldTimerManager().SetTimer(JustLandedTimerHandle, this, &ASandboxCharacter_CMC::ResetJustLanded, 0.3f, false);
}

void ASandboxCharacter_CMC::ResetJustLanded()
{
	JustLanded = false;
}
