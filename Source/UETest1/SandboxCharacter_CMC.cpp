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
