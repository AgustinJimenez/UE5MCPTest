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
