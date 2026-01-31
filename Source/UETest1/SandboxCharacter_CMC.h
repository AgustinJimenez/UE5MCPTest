#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BPI_SandboxCharacter_Pawn.h"
#include "CharacterPropertiesStructs.h"
#include "LocomotionEnums.h"
#include "SandboxCharacter_CMC.generated.h"

class UMotionWarpingComponent;
class UAC_PreCMCTick;
class UCameraComponent;
class USpringArmComponent;
class UChildActorComponent;
class UCurveFloat;
class UAC_FoleyEvents;
class UAC_VisualOverrideManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRequestInteract);

UCLASS()
class UETEST1_API ASandboxCharacter_CMC : public ACharacter, public IBPI_SandboxCharacter_Pawn
{
	GENERATED_BODY()

public:
	ASandboxCharacter_CMC();

	// ===== VARIABLES (from blueprint) =====

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Config")
	E_AnalogStickBehavior MovementStickMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	E_CameraStyle CameraStyle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Config")
	double AnalogWalkRunThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Config")
	TObjectPtr<UCurveFloat> StrafeSpeedMapCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Gait")
	E_Gait Gait;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Speed")
	FVector WalkSpeeds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Speed")
	FVector RunSpeeds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Speed")
	FVector SprintSpeeds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Speed")
	FVector WalkSpeeds_Demo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Speed")
	FVector RunSpeeds_Demo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Speed")
	FVector SprintSpeeds_Demo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Speed")
	FVector CrouchSpeeds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	bool JustLanded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	FVector LandVelocity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FS_PlayerInputState CharacterInputState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	bool WasMovingOnGroundLastFrame_Simulated;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|State")
	FVector LastUpdateVelocity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|RootMotion")
	bool UsingAttributeBasedRootMotion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll")
	bool IsRagdolling;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnRequestInteract OnRequestInteract;

	// ===== INTERFACE IMPLEMENTATION =====

	virtual FS_CharacterPropertiesForAnimation Get_PropertiesForAnimation_Implementation() override;
	virtual FS_CharacterPropertiesForCamera Get_PropertiesForCamera_Implementation() override;
	virtual FS_CharacterPropertiesForTraversal Get_PropertiesForTraversal_Implementation() override;
	virtual void Set_CharacterInputState_Implementation(FS_PlayerInputState DesiredInputState) override;

	// ===== PHYSICS CALCULATION FUNCTIONS =====

	UFUNCTION(BlueprintCallable, Category = "Movement|Physics")
	double CalculateMaxAcceleration() const;

	UFUNCTION(BlueprintCallable, Category = "Movement|Physics")
	double CalculateBrakingDeceleration() const;

	UFUNCTION(BlueprintCallable, Category = "Movement|Physics")
	double CalculateBrakingFriction() const;

	UFUNCTION(BlueprintCallable, Category = "Movement|Physics")
	double CalculateGroundFriction() const;

	UFUNCTION(BlueprintPure, Category = "Movement|Input")
	bool HasMovementInputVector() const;

	// ===== GAIT/SPEED SYSTEM =====

	UFUNCTION(BlueprintCallable, Category = "Movement|Gait")
	E_Gait GetDesiredGait();

	UFUNCTION(BlueprintCallable, Category = "Movement|Speed")
	double CalculateMaxSpeed();

	UFUNCTION(BlueprintCallable, Category = "Movement|Speed")
	double CalculateMaxCrouchSpeed();

	UFUNCTION(BlueprintPure, Category = "Movement|Gait")
	bool CanSprint() const;

	// ===== INTERNAL STATE =====

	UPROPERTY(BlueprintReadWrite, Category = "Movement|State")
	bool FullMovementInput;

	UPROPERTY(BlueprintReadWrite, Category = "Movement|State")
	float StrafeSpeedMap;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ===== CACHED COMPONENTS =====

	UPROPERTY(Transient)
	TObjectPtr<UMotionWarpingComponent> CachedMotionWarping;

	UPROPERTY(Transient)
	TObjectPtr<UAC_PreCMCTick> CachedPreCMCTick;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CachedCamera;

	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CachedSpringArm;

	UPROPERTY(Transient)
	TObjectPtr<UChildActorComponent> CachedVisualOverride;

	// Blueprint component types - use UActorComponent
	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> CachedGameplayCamera;

	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> CachedTraversalLogic;

	UPROPERTY(Transient)
	TObjectPtr<UAC_FoleyEvents> CachedFoleyEvents;

	UPROPERTY(Transient)
	TObjectPtr<UAC_VisualOverrideManager> CachedVisualOverrideManager;

	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> CachedSmartObjectAnimation;
};
