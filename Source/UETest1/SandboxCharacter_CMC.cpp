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
#include "InputMappingContext.h"
#include "HAL/IConsoleManager.h"

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

	// Set tick prerequisite: CharacterMovement ticks after AC_PreCMCTick
	if (CachedPreCMCTick && GetCharacterMovement())
	{
		GetCharacterMovement()->PrimaryComponentTick.AddPrerequisite(CachedPreCMCTick, CachedPreCMCTick->PrimaryComponentTick);

		// Bind to PreCMCTick delegate for pre-movement updates
		CachedPreCMCTick->Tick.AddDynamic(this, &ASandboxCharacter_CMC::OnPreCMCTick);
		bPreCMCTickBound = true;
	}

	// For simulated proxies: bind to OnCharacterMovementUpdated to detect ground transitions
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		OnCharacterMovementUpdated.AddDynamic(this, &ASandboxCharacter_CMC::OnMovementUpdatedSimulated);
	}

	// Set input mode to game-only so mouse is captured for camera look
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
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

	// Fallback: if PreCMCTick delegate wasn't bound, run movement updates here
	if (!bPreCMCTickBound)
	{
		OnPreCMCTick();
	}

	// Traversal check - call TryTraversalAction each frame (was previously in BP event graph Tick)
	if (CachedTraversalLogic && !IsRagdolling)
	{
		FBoolProperty* DoingProp = CastField<FBoolProperty>(
			CachedTraversalLogic->GetClass()->FindPropertyByName(TEXT("DoingTraversalAction")));
		bool bDoingTraversal = DoingProp ? DoingProp->GetPropertyValue_InContainer(CachedTraversalLogic) : false;

		if (!bDoingTraversal)
		{
			UFunction* TryFunc = CachedTraversalLogic->FindFunction(TEXT("TryTraversalAction"));
			if (TryFunc)
			{
				FS_TraversalCheckInputs Inputs = GetTraversalCheckInputs();
				uint8* Params = (uint8*)FMemory_Alloca(TryFunc->ParmsSize);
				FMemory::Memzero(Params, TryFunc->ParmsSize);
				for (TFieldIterator<FProperty> It(TryFunc); It; ++It)
				{
					if (It->HasAnyPropertyFlags(CPF_Parm) && It->GetFName() == FName(TEXT("Inputs")))
					{
						It->CopyCompleteValue(It->ContainerPtrToValuePtr<void>(Params), &Inputs);
						break;
					}
				}
				CachedTraversalLogic->ProcessEvent(TryFunc, Params);
			}
		}
	}
}

void ASandboxCharacter_CMC::OnPreCMCTick()
{
	// Called before CharacterMovementComponent ticks each frame
	// Matches the BP's PreCMCTick custom event flow

	// Update rotation settings based on strafe/aim state and falling
	UpdateRotation_PreCMC();

	// Update Gait based on input and sprint conditions
	Gait = GetDesiredGait();

	// Update movement physics properties
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->MaxWalkSpeed = CalculateMaxSpeed();
		CMC->MaxWalkSpeedCrouched = CalculateMaxCrouchSpeed();
		CMC->MaxAcceleration = CalculateMaxAcceleration();
		CMC->BrakingDecelerationWalking = CalculateBrakingDeceleration();
		CMC->BrakingFrictionFactor = CalculateBrakingFriction();
		CMC->GroundFriction = CalculateGroundFriction();
	}
}

// Interface implementations - populate with character's actual components
FS_CharacterPropertiesForAnimation ASandboxCharacter_CMC::Get_PropertiesForAnimation_Implementation()
{
	FS_CharacterPropertiesForAnimation Props;
	Props.InputState = CharacterInputState;
	Props.Gait = Gait;
	Props.Velocity = GetVelocity();
	Props.ActorTransform = GetActorTransform();
	Props.JustLanded = JustLanded;
	Props.LandVelocity = LandVelocity;

	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		// Movement mode
		if (CMC->IsFalling())
		{
			Props.MovementMode = E_MovementMode::InAir;
		}
		else
		{
			Props.MovementMode = E_MovementMode::OnGround;
		}

		Props.InputAcceleration = CMC->GetCurrentAcceleration();
		Props.CurrentMaxAcceleration = CMC->MaxAcceleration;
		Props.CurrentMaxDeceleration = CMC->BrakingDecelerationWalking;
	}

	// Stance
	Props.Stance = bIsCrouched ? E_Stance::Crouch : E_Stance::Stand;

	// Rotation mode
	if (CharacterInputState.WantsToAim)
	{
		Props.RotationMode = E_RotationMode::Aim;
	}
	else if (CharacterInputState.WantsToStrafe)
	{
		Props.RotationMode = E_RotationMode::Strafe;
	}
	else
	{
		Props.RotationMode = E_RotationMode::OrientToMovement;
	}

	// Aiming rotation (control rotation)
	if (Controller)
	{
		Props.AimingRotation = Controller->GetControlRotation();
	}

	// Orientation intent (actor rotation)
	Props.OrientationIntent = GetActorRotation();

	// Ground info
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		if (CMC->CurrentFloor.IsWalkableFloor())
		{
			Props.GroundNormal = CMC->CurrentFloor.HitResult.ImpactNormal;
			Props.GroundLocation = CMC->CurrentFloor.HitResult.ImpactPoint;
		}
	}

	return Props;
}

FS_CharacterPropertiesForCamera ASandboxCharacter_CMC::Get_PropertiesForCamera_Implementation()
{
	FS_CharacterPropertiesForCamera Props;
	Props.CameraStyle = CameraStyle;
	Props.Gait = Gait;
	return Props;
}

FS_CharacterPropertiesForTraversal ASandboxCharacter_CMC::Get_PropertiesForTraversal_Implementation()
{
	FS_CharacterPropertiesForTraversal Props;
	Props.Capsule = GetCapsuleComponent();
	Props.Mesh = GetMesh();
	Props.MotionWarping = CachedMotionWarping;
	// Determine movement mode from CharacterMovement state
	if (GetCharacterMovement())
	{
		if (GetCharacterMovement()->IsFalling())
		{
			Props.MovementMode = E_MovementMode::InAir;
		}
		else
		{
			Props.MovementMode = E_MovementMode::OnGround;
		}
	}
	Props.Gait = Gait;
	Props.Speed = GetVelocity().Size();
	return Props;
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
	if (!CharacterInputState.WantsToSprint || !GetCharacterMovement())
	{
		return false;
	}

	// Local pawns can have pending input consumed before Tick, so fall back to acceleration.
	FVector InputVector = IsLocallyControlled()
		? GetPendingMovementInputVector()
		: GetCharacterMovement()->GetCurrentAcceleration();
	if (InputVector.IsNearlyZero())
	{
		InputVector = GetCharacterMovement()->GetCurrentAcceleration();
	}

	if (InputVector.IsNearlyZero())
	{
		return false;
	}

	// If character rotates to movement, any movement direction can sprint.
	if (GetCharacterMovement()->bOrientRotationToMovement)
	{
		return true;
	}

	// Otherwise use control yaw as the forward reference (matches strafe-style movement).
	const float ReferenceYaw = Controller ? Controller->GetControlRotation().Yaw : GetActorRotation().Yaw;
	const float InputYaw = InputVector.ToOrientationRotator().Yaw;
	const float AbsYawDelta = FMath::Abs(FRotator::NormalizeAxis(InputYaw - ReferenceYaw));
	return AbsYawDelta < 60.0f;
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
	// Get the 2D input value, scaled by delta time for framerate-independent rotation
	FVector2D LookAxisVector = Value.Get<FVector2D>() * GetWorld()->GetDeltaSeconds();

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
	// Toggle walk only if not sprinting (matches BP behavior)
	if (!CharacterInputState.WantsToSprint)
	{
		CharacterInputState.WantsToWalk = !CharacterInputState.WantsToWalk;
	}
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
	// Toggle crouch only when not falling (matches BP behavior)
	if (GetCharacterMovement() && !GetCharacterMovement()->IsFalling())
	{
		if (bIsCrouched)
		{
			UnCrouch();
		}
		else
		{
			Crouch();
		}
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

// ===== POSSESSION & LIFECYCLE =====

void ASandboxCharacter_CMC::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Setup camera after possession (matches BP Possessed_Client chain)
	SetupCamera();
}

void ASandboxCharacter_CMC::OnWalkingOffLedge_Implementation(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta)
{
	UnCrouch();
}

void ASandboxCharacter_CMC::OnMovementUpdatedSimulated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	// For simulated proxies: detect ground state transitions
	UpdatedMovementSimulated(OldVelocity);
}

void ASandboxCharacter_CMC::UpdateInputState_Server_Implementation(FS_PlayerInputState NewInputState)
{
	CharacterInputState = NewInputState;
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

// ===== CAMERA & ROTATION =====

void ASandboxCharacter_CMC::SetupCamera()
{
	// Cache player controller
	CachedPlayerController = Cast<APlayerController>(GetController());

	if (!CachedPlayerController)
	{
		return;
	}

	// Re-cache camera components if not found yet (PossessedBy may run before BeginPlay finishes caching)
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
		// Activate GameplayCamera via reflection (BP-only component)
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
		// Fallback: activate the default camera and set view target
		CachedCamera->Activate();
		CachedPlayerController->SetViewTargetWithBlend(this);
	}
}

void ASandboxCharacter_CMC::UpdateRotation_PreCMC()
{
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!CMC)
	{
		return;
	}

	// Set rotation mode based on strafe/aim input
	const bool bWantsStrafe = CharacterInputState.WantsToStrafe || CharacterInputState.WantsToAim;
	if (bWantsStrafe)
	{
		CMC->bUseControllerDesiredRotation = true;
		CMC->bOrientRotationToMovement = false;
	}
	else
	{
		CMC->bUseControllerDesiredRotation = false;
		CMC->bOrientRotationToMovement = true;
	}

	// Set rotation rate based on movement state
	if (CMC->IsFalling())
	{
		CMC->RotationRate = FRotator(0.0, 200.0, 0.0);
	}
	else
	{
		// -1 rotation rate causes instant rotation, letting the animation blueprint
		// handle root bone rotation independently (stick flicks, pivots, turn-in-place)
		CMC->RotationRate = FRotator(0.0, -1.0, 0.0);
	}
}

// ===== TRAVERSAL =====

FS_TraversalCheckInputs ASandboxCharacter_CMC::GetTraversalCheckInputs_Implementation()
{
	FS_TraversalCheckInputs Result;
	Result.TraceForwardDirection = GetActorForwardVector();

	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!CMC)
	{
		return Result;
	}

	const EMovementMode MoveMode = CMC->MovementMode;

	if (MoveMode == MOVE_Falling || MoveMode == MOVE_Flying)
	{
		// In air: fixed forward distance, higher half-height, end offset up
		Result.TraceForwardDistance = 75.0;
		Result.TraceEndOffset = FVector(0.0, 0.0, 50.0);
		Result.TraceRadius = 30.0;
		Result.TraceHalfHeight = 86.0;
	}
	else
	{
		// On ground: scale forward distance based on forward speed
		const FVector UnrotatedVelocity = GetActorRotation().UnrotateVector(CMC->Velocity);
		Result.TraceForwardDistance = UKismetMathLibrary::MapRangeClamped(
			UnrotatedVelocity.X, 0.0, 500.0, 75.0, 350.0);
		Result.TraceRadius = 30.0;
		Result.TraceHalfHeight = 60.0;
	}

	return Result;
}

// ===== SIMULATED PROXY =====

void ASandboxCharacter_CMC::UpdatedMovementSimulated(FVector OldVelocity)
{
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!CMC)
	{
		return;
	}

	// Update ground state
	IsMovingOnGround = CMC->IsMovingOnGround();

	// Detect ground state transitions
	if (IsMovingOnGround != WasMovingOnGroundLastFrame_Simulated)
	{
		if (IsMovingOnGround)
		{
			// Transitioned to ground = simulated landing
			CustomOnLandedEvent(OldVelocity);
		}
		else
		{
			// Transitioned to air = simulated jump
			CustomOnJumpedEvent(OldVelocity.Size2D());
		}
	}

	// Store for next frame comparison
	WasMovingOnGroundLastFrame_Simulated = IsMovingOnGround;
}

void ASandboxCharacter_CMC::CustomOnLandedEvent(FVector InLandVelocity)
{
	JustLanded = true;
	LandVelocity = InLandVelocity;

	GetWorldTimerManager().ClearTimer(JustLandedTimerHandle);
	GetWorldTimerManager().SetTimer(
		JustLandedTimerHandle, this, &ASandboxCharacter_CMC::ResetJustLanded, 0.3f, false);
}

void ASandboxCharacter_CMC::CustomOnJumpedEvent(double GroundSpeedBeforeJump)
{
	// Simulated proxy jump event - state tracked via IsMovingOnGround
	// GroundSpeedBeforeJump available for animation systems if needed
}

// ===== RAGDOLL =====

void ASandboxCharacter_CMC::Ragdoll_Start()
{
	UCharacterMovementComponent* CMC = GetCharacterMovement();

	// Disable movement and set ragdolling state
	if (CMC)
	{
		CMC->SetMovementMode(MOVE_None);
	}
	IsRagdolling = true;

	// Disable capsule collision
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Enable mesh physics
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetCollisionObjectType(ECC_PhysicsBody);
		SkelMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		SkelMesh->SetAllBodiesBelowSimulatePhysics(FName(TEXT("pelvis")), true);
	}
}

void ASandboxCharacter_CMC::Ragdoll_End()
{
	IsRagdolling = false;

	// Restore movement mode
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->SetMovementMode(MOVE_Falling);
	}

	// Restore capsule collision
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// Restore mesh collision and disable physics
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		SkelMesh->SetCollisionObjectType(ECC_Pawn);
		SkelMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		SkelMesh->SetAllBodiesSimulatePhysics(false);
	}
}
