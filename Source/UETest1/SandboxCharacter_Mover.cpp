#include "SandboxCharacter_Mover.h"
#include "AC_TraversalLogic.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"
#include "MotionWarpingComponent.h"
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
#include "InputMappingContext.h"
#include "Curves/CurveFloat.h"
#include "HAL/IConsoleManager.h"

ASandboxCharacter_Mover::ASandboxCharacter_Mover()
{
	PrimaryActorTick.bCanEverTick = true;

	// Initialize default values (same as CMC variant)
	MovementStickMode = E_AnalogStickBehavior::FixedSpeed_WalkRun;
	CameraStyle = E_CameraStyle::Medium;
	AnalogWalkRunThreshold = 0.7;
	Gait = E_Gait::Run;

	// Speed defaults
	WalkSpeeds = FVector(200.0, 180.0, 150.0);
	RunSpeeds = FVector(500.0, 350.0, 300.0);
	SprintSpeeds = FVector(700.0, 700.0, 700.0);
	WalkSpeeds_Demo = FVector(165.0, 165.0, 165.0);
	RunSpeeds_Demo = FVector(375.0, 375.0, 375.0);
	SprintSpeeds_Demo = FVector(600.0, 600.0, 600.0);
	CrouchSpeeds = FVector(225.0, 200.0, 180.0);

	// State initialization
	CharacterInputState.WantsToStrafe = true;
	JustLanded = false;
	LandVelocity = FVector::ZeroVector;
	WasMovingOnGroundLastFrame_Simulated = true;
	LastUpdateVelocity = FVector::ZeroVector;
	UsingAttributeBasedRootMotion = false;
	IsRagdolling = false;

	// Load curve asset
	static ConstructorHelpers::FObjectFinder<UCurveFloat> StrafeSpeedCurve(TEXT("/Game/Blueprints/Data/Curve_StrafeSpeedMap"));
	if (StrafeSpeedCurve.Succeeded()) StrafeSpeedMapCurve = StrafeSpeedCurve.Object;

	// Load input actions and mapping context
	static ConstructorHelpers::FObjectFinder<UInputAction> SprintAction(TEXT("/Game/Input/IA_Sprint"));
	if (SprintAction.Succeeded()) IA_Sprint = SprintAction.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> MoveAction(TEXT("/Game/Input/IA_Move"));
	if (MoveAction.Succeeded()) IA_Move = MoveAction.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> MoveWSAction(TEXT("/Game/Input/IA_Move_WorldSpace"));
	if (MoveWSAction.Succeeded()) IA_Move_WorldSpace = MoveWSAction.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> LookAction(TEXT("/Game/Input/IA_Look"));
	if (LookAction.Succeeded()) IA_Look = LookAction.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> LookGPAction(TEXT("/Game/Input/IA_Look_Gamepad"));
	if (LookGPAction.Succeeded()) IA_Look_Gamepad = LookGPAction.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> WalkAction(TEXT("/Game/Input/IA_Walk"));
	if (WalkAction.Succeeded()) IA_Walk = WalkAction.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> JumpAction(TEXT("/Game/Input/IA_Jump"));
	if (JumpAction.Succeeded()) IA_Jump = JumpAction.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> CrouchAction(TEXT("/Game/Input/IA_Crouch"));
	if (CrouchAction.Succeeded()) IA_Crouch = CrouchAction.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> StrafeAction(TEXT("/Game/Input/IA_Strafe"));
	if (StrafeAction.Succeeded()) IA_Strafe = StrafeAction.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction> AimAction(TEXT("/Game/Input/IA_Aim"));
	if (AimAction.Succeeded()) IA_Aim = AimAction.Object;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MappingCtx(TEXT("/Game/Input/IMC_Sandbox"));
	if (MappingCtx.Succeeded()) IMC_Sandbox = MappingCtx.Object;
}

void ASandboxCharacter_Mover::BeginPlay()
{
	Super::BeginPlay();

	// Cache components created by Blueprint
	CachedMoverComponent = FindComponentByClass<UCharacterMoverComponent>();
	CachedCapsule = FindComponentByClass<UCapsuleComponent>();
	CachedMotionWarping = FindComponentByClass<UMotionWarpingComponent>();
	CachedCamera = FindComponentByClass<UCameraComponent>();
	CachedSpringArm = FindComponentByClass<USpringArmComponent>();
	CachedVisualOverride = FindComponentByClass<UChildActorComponent>();
	CachedFoleyEvents = FindComponentByClass<UAC_FoleyEvents>();
	CachedVisualOverrideManager = FindComponentByClass<UAC_VisualOverrideManager>();
	CachedTraversalLogic = FindComponentByClass<UAC_TraversalLogic>();

	// Cache skeletal mesh (BP creates it as a component on APawn)
	CachedMesh = FindComponentByClass<USkeletalMeshComponent>();

	// Cache Blueprint-only components by name
	TArray<UActorComponent*> Components;
	GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (Component->GetName().Contains(TEXT("GameplayCamera")))
		{
			CachedGameplayCamera = Component;
		}
		else if (Component->GetName().Contains(TEXT("AC_SmartObjectAnimation")))
		{
			CachedSmartObjectAnimation = Component;
		}
	}

	// Set input mode
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}

void ASandboxCharacter_Mover::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Move)
		{
			EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ASandboxCharacter_Mover::OnMove);
		}
		if (IA_Move_WorldSpace)
		{
			EIC->BindAction(IA_Move_WorldSpace, ETriggerEvent::Triggered, this, &ASandboxCharacter_Mover::OnMoveWorldSpace);
		}
		if (IA_Look)
		{
			EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ASandboxCharacter_Mover::OnLook);
		}
		if (IA_Look_Gamepad)
		{
			EIC->BindAction(IA_Look_Gamepad, ETriggerEvent::Triggered, this, &ASandboxCharacter_Mover::OnLookGamepad);
		}
		if (IA_Sprint)
		{
			EIC->BindAction(IA_Sprint, ETriggerEvent::Triggered, this, &ASandboxCharacter_Mover::OnSprint);
			EIC->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &ASandboxCharacter_Mover::OnSprintReleased);
		}
		if (IA_Walk)
		{
			EIC->BindAction(IA_Walk, ETriggerEvent::Triggered, this, &ASandboxCharacter_Mover::OnWalk);
		}
		if (IA_Jump)
		{
			EIC->BindAction(IA_Jump, ETriggerEvent::Triggered, this, &ASandboxCharacter_Mover::OnJumpAction);
			EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ASandboxCharacter_Mover::OnJumpReleased);
		}
		if (IA_Crouch)
		{
			EIC->BindAction(IA_Crouch, ETriggerEvent::Triggered, this, &ASandboxCharacter_Mover::OnCrouchAction);
		}
		if (IA_Strafe)
		{
			EIC->BindAction(IA_Strafe, ETriggerEvent::Triggered, this, &ASandboxCharacter_Mover::OnStrafe);
		}
		if (IA_Aim)
		{
			EIC->BindAction(IA_Aim, ETriggerEvent::Triggered, this, &ASandboxCharacter_Mover::OnAim);
			EIC->BindAction(IA_Aim, ETriggerEvent::Completed, this, &ASandboxCharacter_Mover::OnAimReleased);
		}
	}
}

void ASandboxCharacter_Mover::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update gait each frame
	Gait = GetDesiredGait();

	// Update SteeringTime
	const bool bHasInput = HasMovementInput();
	if (bHasInput)
	{
		AccumulatedSteeringTime += DeltaTime;
	}
	else
	{
		AccumulatedSteeringTime = 0.0;
	}
	bWasSteeringLastFrame = bHasInput;

	// Cache custom inputs for interface queries
	CachedCustomInputs.Gait = Gait;
	CachedCustomInputs.RotationMode = GetRotationMode();
	CachedCustomInputs.MovementDirection = CalculateMovementDirection();
	CachedCustomInputs.WantsToCrouch = CharacterInputState.WantsToCrouch;

	// Push MovementMode/Gait to AnimInstance
	UpdateAnimInstanceEnums();

	// Reset per-frame move input flag (consumed by ProduceInput)
	if (!bMoveInputThisFrame)
	{
		AccumulatedMoveInput = FVector2D::ZeroVector;
	}
	bMoveInputThisFrame = false;
}

// ===== MOVER INPUT PRODUCER =====

void ASandboxCharacter_Mover::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	FCharacterDefaultInputs& DefaultInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	// Movement input: transform 2D stick input to world-space 3D intent
	if (!AccumulatedMoveInput.IsNearlyZero())
	{
		const FRotator ControlRot = GetControlRotation();
		const FRotator YawRot(0, ControlRot.Yaw, 0);
		const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

		FVector MoveIntent = Forward * AccumulatedMoveInput.Y + Right * AccumulatedMoveInput.X;
		// Clamp to unit length for DirectionalIntent
		if (MoveIntent.SizeSquared() > 1.0)
		{
			MoveIntent.Normalize();
		}
		DefaultInputs.SetMoveInput(EMoveInputType::DirectionalIntent, MoveIntent);
	}
	else
	{
		DefaultInputs.SetMoveInput(EMoveInputType::None, FVector::ZeroVector);
	}

	// Orientation intent
	FRotator OrientIntent = GetOrientationIntent();
	DefaultInputs.OrientationIntent = OrientIntent.Vector();

	// Control rotation (camera)
	DefaultInputs.ControlRotation = GetControlRotation();

	// Jump
	DefaultInputs.bIsJumpJustPressed = bJumpJustPressed;
	DefaultInputs.bIsJumpPressed = bJumpHeld;
	bJumpJustPressed = false; // Consumed
}

// ===== INTERFACE IMPLEMENTATIONS =====

FS_CharacterPropertiesForAnimation ASandboxCharacter_Mover::Get_PropertiesForAnimation_Implementation()
{
	FS_CharacterPropertiesForAnimation Props;
	Props.InputState = CharacterInputState;
	Props.Gait = Gait;
	Props.Velocity = CachedMoverComponent ? CachedMoverComponent->GetVelocity() : FVector::ZeroVector;
	Props.ActorTransform = GetActorTransform();
	Props.JustLanded = JustLanded;
	Props.LandVelocity = LandVelocity;

	// Movement mode from Mover
	if (CachedMoverComponent)
	{
		if (CachedMoverComponent->IsFalling())
		{
			Props.MovementMode = E_MovementMode::InAir;
		}
		else
		{
			Props.MovementMode = E_MovementMode::OnGround;
		}
	}

	// Stance from crouch state
	Props.Stance = (CachedMoverComponent && CachedMoverComponent->IsCrouching()) ? E_Stance::Crouch : E_Stance::Stand;

	// Rotation mode
	Props.RotationMode = GetRotationMode();

	// Aiming rotation (control rotation)
	if (Controller)
	{
		Props.AimingRotation = Controller->GetControlRotation();
	}

	// Orientation intent
	Props.OrientationIntent = GetOrientationIntent();

	// Movement direction and steering time
	Props.MovementDirection = CalculateMovementDirection();
	Props.SteeringTime = AccumulatedSteeringTime;

	// Input acceleration — use Mover's last input as approximation
	if (CachedMoverComponent)
	{
		const FMoverInputCmdContext& LastInput = CachedMoverComponent->GetLastInputCmd();
		if (const FCharacterDefaultInputs* LastDefaults = LastInput.InputCollection.FindDataByType<FCharacterDefaultInputs>())
		{
			FVector MoveIn = LastDefaults->GetMoveInput();
			// Scale by max speed to approximate acceleration vector
			Props.InputAcceleration = MoveIn * CalculateMaxSpeed();
		}
		Props.CurrentMaxAcceleration = 800.0;
		Props.CurrentMaxDeceleration = 2000.0;
	}

	// Ground info from Mover floor check
	if (CachedMoverComponent)
	{
		FHitResult FloorHit;
		if (CachedMoverComponent->TryGetFloorCheckHitResult(FloorHit))
		{
			Props.GroundNormal = FloorHit.ImpactNormal;
			Props.GroundLocation = FloorHit.ImpactPoint;
		}
	}

	return Props;
}

FS_CharacterPropertiesForCamera ASandboxCharacter_Mover::Get_PropertiesForCamera_Implementation()
{
	FS_CharacterPropertiesForCamera Props;
	Props.CameraStyle = CameraStyle;
	Props.Gait = Gait;
	Props.Stance = (CachedMoverComponent && CachedMoverComponent->IsCrouching()) ? E_Stance::Crouch : E_Stance::Stand;

	if (CharacterInputState.WantsToAim)
	{
		Props.CameraMode = E_CameraMode::Aim;
	}
	else if (CharacterInputState.WantsToStrafe)
	{
		Props.CameraMode = E_CameraMode::Strafe;
	}
	else
	{
		Props.CameraMode = E_CameraMode::FreeCam;
	}

	return Props;
}

FS_CharacterPropertiesForTraversal ASandboxCharacter_Mover::Get_PropertiesForTraversal_Implementation()
{
	FS_CharacterPropertiesForTraversal Props;
	Props.Capsule = CachedCapsule;
	Props.Mesh = CachedMesh;
	Props.MotionWarping = CachedMotionWarping;

	if (CachedMoverComponent)
	{
		if (CachedMoverComponent->IsFalling())
		{
			Props.MovementMode = E_MovementMode::InAir;
		}
		else
		{
			Props.MovementMode = E_MovementMode::OnGround;
		}
	}
	Props.Gait = Gait;
	Props.Speed = CachedMoverComponent ? CachedMoverComponent->GetVelocity().Size() : 0.0;
	return Props;
}

void ASandboxCharacter_Mover::Set_CharacterInputState_Implementation(FS_PlayerInputState DesiredInputState)
{
	CharacterInputState = DesiredInputState;
}

// ===== GAIT/SPEED SYSTEM =====

bool ASandboxCharacter_Mover::HasMovementInput() const
{
	return !AccumulatedMoveInput.IsNearlyZero();
}

E_Gait ASandboxCharacter_Mover::GetDesiredGait()
{
	double InputMagnitude = AccumulatedMoveInput.Size();
	bool bHasFullInput = InputMagnitude >= AnalogWalkRunThreshold;

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

	if (CanSprint() && FullMovementInput)
	{
		return E_Gait::Sprint;
	}

	if (CharacterInputState.WantsToWalk)
	{
		return E_Gait::Walk;
	}

	return FullMovementInput ? E_Gait::Run : E_Gait::Walk;
}

bool ASandboxCharacter_Mover::CanSprint() const
{
	if (!CharacterInputState.WantsToSprint || !CachedMoverComponent)
	{
		return false;
	}

	if (AccumulatedMoveInput.IsNearlyZero())
	{
		return false;
	}

	// If orient-to-movement mode, any direction can sprint
	E_RotationMode RotMode = GetRotationMode();
	if (RotMode == E_RotationMode::OrientToMovement)
	{
		return true;
	}

	// In strafe/aim mode, only forward-ish input can sprint
	const FRotator ControlRot = GetControlRotation();
	const FRotator YawRot(0, ControlRot.Yaw, 0);
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	FVector InputDir = Forward * AccumulatedMoveInput.Y + Right * AccumulatedMoveInput.X;
	InputDir.Normalize();

	const float ReferenceYaw = ControlRot.Yaw;
	const float InputYaw = InputDir.ToOrientationRotator().Yaw;
	const float AbsYawDelta = FMath::Abs(FRotator::NormalizeAxis(InputYaw - ReferenceYaw));
	return AbsYawDelta < 60.0f;
}

double ASandboxCharacter_Mover::CalculateMaxSpeed()
{
	// Calculate direction-based speed multiplier
	FVector Velocity = CachedMoverComponent ? CachedMoverComponent->GetVelocity() : FVector::ZeroVector;
	FRotator ActorRotation = GetActorRotation();
	double Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, ActorRotation);
	double AbsDirection = FMath::Abs(Direction);

	// Use curve to map direction to speed multiplier
	E_RotationMode RotMode = GetRotationMode();
	if (StrafeSpeedMapCurve && RotMode != E_RotationMode::OrientToMovement)
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
		ResultSpeed = UKismetMathLibrary::MapRangeClamped(StrafeSpeedMap, 0.0, 1.0, SpeedVector.X, SpeedVector.Y);
	}
	else
	{
		ResultSpeed = UKismetMathLibrary::MapRangeClamped(StrafeSpeedMap, 1.0, 2.0, SpeedVector.Y, SpeedVector.Z);
	}

	return ResultSpeed;
}

// ===== ORIENTATION & ROTATION =====

E_RotationMode ASandboxCharacter_Mover::GetRotationMode() const
{
	if (CharacterInputState.WantsToAim)
	{
		return E_RotationMode::Aim;
	}
	else if (CharacterInputState.WantsToStrafe)
	{
		return E_RotationMode::Strafe;
	}
	return E_RotationMode::OrientToMovement;
}

FRotator ASandboxCharacter_Mover::GetOrientationIntent() const
{
	E_RotationMode RotMode = GetRotationMode();

	if (RotMode == E_RotationMode::Strafe || RotMode == E_RotationMode::Aim)
	{
		// In strafe/aim mode: orient toward control rotation (camera direction)
		return FRotator(0.0, GetControlRotation().Yaw, 0.0);
	}

	// OrientToMovement: orient toward movement direction
	if (CachedMoverComponent)
	{
		FVector Vel = CachedMoverComponent->GetVelocity();
		if (Vel.Size2D() > 1.0)
		{
			return Vel.ToOrientationRotator();
		}
	}

	// Fallback: current actor rotation
	return GetActorRotation();
}

E_MovementDirection ASandboxCharacter_Mover::CalculateMovementDirection() const
{
	FVector Vel = CachedMoverComponent ? CachedMoverComponent->GetVelocity() : FVector::ZeroVector;
	if (Vel.Size2D() < 1.0)
	{
		return E_MovementDirection::F;
	}

	const double Direction = UKismetAnimationLibrary::CalculateDirection(Vel, GetActorRotation());

	if (Direction >= -60.0 && Direction <= 60.0)
	{
		return E_MovementDirection::F;
	}
	else if (Direction > 60.0 && Direction <= 90.0)
	{
		return E_MovementDirection::RL;
	}
	else if (Direction > 90.0 && Direction <= 135.0)
	{
		return E_MovementDirection::RR;
	}
	else if (Direction > 135.0 || Direction < -135.0)
	{
		return E_MovementDirection::B;
	}
	else if (Direction >= -135.0 && Direction < -90.0)
	{
		return E_MovementDirection::LL;
	}
	else
	{
		return E_MovementDirection::LR;
	}
}

// ===== INPUT HANDLERS =====

void ASandboxCharacter_Mover::OnMove(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	AccumulatedMoveInput = MovementVector;
	bMoveInputThisFrame = true;

	// Also call AddMovementInput for Mover compatibility
	const FRotator ControlRotation = GetControlRotation();
	const FRotator YawRotation(0, ControlRotation.Yaw, 0);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(RightDirection, MovementVector.X);
	AddMovementInput(ForwardDirection, MovementVector.Y);
}

void ASandboxCharacter_Mover::OnMoveWorldSpace(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	AccumulatedMoveInput = MovementVector;
	bMoveInputThisFrame = true;

	AddMovementInput(FVector(0, 1, 0), MovementVector.X);
	AddMovementInput(FVector(1, 0, 0), MovementVector.Y);
}

void ASandboxCharacter_Mover::OnLook(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void ASandboxCharacter_Mover::OnLookGamepad(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>() * GetWorld()->GetDeltaSeconds();
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void ASandboxCharacter_Mover::OnSprint(const FInputActionValue& Value)
{
	CharacterInputState.WantsToSprint = true;
}

void ASandboxCharacter_Mover::OnSprintReleased(const FInputActionValue& Value)
{
	CharacterInputState.WantsToSprint = false;
}

void ASandboxCharacter_Mover::OnWalk(const FInputActionValue& Value)
{
	if (!CharacterInputState.WantsToSprint)
	{
		CharacterInputState.WantsToWalk = !CharacterInputState.WantsToWalk;
	}
}

void ASandboxCharacter_Mover::OnJumpAction(const FInputActionValue& Value)
{
	// Gate check: not falling, not doing traversal, not ragdolling
	bool bIsFalling = CachedMoverComponent && CachedMoverComponent->IsFalling();

	if (CachedTraversalLogic && !bIsFalling && !CachedTraversalLogic->DoingTraversalAction && !IsRagdolling)
	{
		FS_TraversalCheckInputs Inputs = GetTraversalCheckInputs();
		bool bShouldJumpInstead = CachedTraversalLogic->TryTraversalAction(Inputs);
		if (bShouldJumpInstead)
		{
			// Signal jump to Mover via input flags
			bJumpJustPressed = true;
			bJumpHeld = true;
			if (CachedMoverComponent)
			{
				CachedMoverComponent->Jump();
			}
		}
		return;
	}

	// Default: jump
	bJumpJustPressed = true;
	bJumpHeld = true;
	if (CachedMoverComponent)
	{
		CachedMoverComponent->Jump();
	}
}

void ASandboxCharacter_Mover::OnJumpReleased(const FInputActionValue& Value)
{
	bJumpHeld = false;
}

void ASandboxCharacter_Mover::OnCrouchAction(const FInputActionValue& Value)
{
	if (!CachedMoverComponent) return;

	// Toggle crouch only when not falling
	if (!CachedMoverComponent->IsFalling())
	{
		if (CachedMoverComponent->IsCrouching())
		{
			CachedMoverComponent->UnCrouch();
			CharacterInputState.WantsToCrouch = false;
		}
		else
		{
			CachedMoverComponent->Crouch();
			CharacterInputState.WantsToCrouch = true;
		}
	}
}

void ASandboxCharacter_Mover::OnStrafe(const FInputActionValue& Value)
{
	CharacterInputState.WantsToStrafe = !CharacterInputState.WantsToStrafe;
}

void ASandboxCharacter_Mover::OnAim(const FInputActionValue& Value)
{
	CharacterInputState.WantsToAim = true;
}

void ASandboxCharacter_Mover::OnAimReleased(const FInputActionValue& Value)
{
	CharacterInputState.WantsToAim = false;
}

// ===== POSSESSION & LIFECYCLE =====

void ASandboxCharacter_Mover::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	SetupCamera();
}

void ASandboxCharacter_Mover::SetupCamera()
{
	CachedPlayerController = Cast<APlayerController>(GetController());

	if (!CachedPlayerController)
	{
		return;
	}

	// Re-cache camera components if not found yet
	if (!CachedCamera)
	{
		CachedCamera = FindComponentByClass<UCameraComponent>();
	}
	if (!CachedSpringArm)
	{
		CachedSpringArm = FindComponentByClass<USpringArmComponent>();
	}
	if (!CachedGameplayCamera)
	{
		TArray<UActorComponent*> AllComponents;
		GetComponents(AllComponents);
		for (UActorComponent* Component : AllComponents)
		{
			if (Component->GetName().Contains(TEXT("GameplayCamera")))
			{
				CachedGameplayCamera = Component;
				break;
			}
		}
	}

	// Check CVar for new GameplayCamera system
	static const auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("DDCVar.NewGameplayCameraSystem.Enable"));
	const bool bUseGameplayCamera = CVar && CVar->GetBool();

	if (bUseGameplayCamera && CachedGameplayCamera)
	{
		static const FName ActivateFuncName(TEXT("ActivateCameraForPlayerController"));
		if (UFunction* Func = CachedGameplayCamera->FindFunction(ActivateFuncName))
		{
			struct FActivateParams
			{
				APlayerController* PlayerController;
			};
			FActivateParams Params;
			Params.PlayerController = CachedPlayerController;
			CachedGameplayCamera->ProcessEvent(Func, &Params);
		}
	}
	else if (CachedCamera)
	{
		CachedCamera->Activate();
		CachedPlayerController->SetViewTargetWithBlend(this);
	}
}

// ===== TRAVERSAL =====

FS_TraversalCheckInputs ASandboxCharacter_Mover::GetTraversalCheckInputs_Implementation()
{
	FS_TraversalCheckInputs Result;
	Result.TraceForwardDirection = GetActorForwardVector();

	if (!CachedMoverComponent)
	{
		return Result;
	}

	if (CachedMoverComponent->IsFalling() || CachedMoverComponent->IsFlying())
	{
		// In air: fixed forward distance, higher half-height
		Result.TraceForwardDistance = 75.0;
		Result.TraceEndOffset = FVector(0.0, 0.0, 50.0);
		Result.TraceRadius = 30.0;
		Result.TraceHalfHeight = 86.0;
	}
	else
	{
		// On ground: scale forward distance based on forward speed
		const FVector Velocity = CachedMoverComponent->GetVelocity();
		const FVector UnrotatedVelocity = GetActorRotation().UnrotateVector(Velocity);
		Result.TraceForwardDistance = UKismetMathLibrary::MapRangeClamped(
			UnrotatedVelocity.X, 0.0, 500.0, 75.0, 350.0);
		Result.TraceRadius = 30.0;
		Result.TraceHalfHeight = 60.0;
	}

	return Result;
}

// ===== RAGDOLL =====

void ASandboxCharacter_Mover::Ragdoll_Start()
{
	IsRagdolling = true;

	// Disable capsule collision
	if (CachedCapsule)
	{
		CachedCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Enable mesh physics
	if (CachedMesh)
	{
		CachedMesh->SetCollisionObjectType(ECC_PhysicsBody);
		CachedMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CachedMesh->SetAllBodiesBelowSimulatePhysics(FName(TEXT("pelvis")), true);
	}
}

void ASandboxCharacter_Mover::Ragdoll_End()
{
	IsRagdolling = false;

	// Restore capsule collision
	if (CachedCapsule)
	{
		CachedCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// Restore mesh collision and disable physics
	if (CachedMesh)
	{
		CachedMesh->SetCollisionObjectType(ECC_Pawn);
		CachedMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		CachedMesh->SetAllBodiesSimulatePhysics(false);
	}
}

// ===== LANDING =====

void ASandboxCharacter_Mover::ResetJustLanded()
{
	JustLanded = false;
}

// ===== ANIM INSTANCE ENUMS =====

void ASandboxCharacter_Mover::UpdateAnimInstanceEnums()
{
	if (!CachedMesh) return;
	UAnimInstance* AnimInst = CachedMesh->GetAnimInstance();
	if (!AnimInst) return;

	UClass* ABPClass = AnimInst->GetClass();

	// MovementMode: 0=OnGround, 1=InAir
	uint8 CurrentMovementMode = 0;
	if (CachedMoverComponent)
	{
		CurrentMovementMode = CachedMoverComponent->IsFalling() ? 1 : 0;
	}

	uint8 CurrentGait = static_cast<uint8>(Gait);

	auto SetByteVar = [&](const TCHAR* VarName, uint8 Value)
	{
		if (FByteProperty* Prop = FindFProperty<FByteProperty>(ABPClass, VarName))
		{
			Prop->SetPropertyValue_InContainer(AnimInst, Value);
		}
	};

	SetByteVar(TEXT("MovementMode_LastFrame"), PrevABPMovementMode);
	SetByteVar(TEXT("MovementMode"), CurrentMovementMode);
	SetByteVar(TEXT("Gait_LastFrame"), PrevABPGait);
	SetByteVar(TEXT("Gait"), CurrentGait);

	PrevABPMovementMode = CurrentMovementMode;
	PrevABPGait = CurrentGait;
}
