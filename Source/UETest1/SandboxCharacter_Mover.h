#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BPI_SandboxCharacter_Pawn.h"
#include "MoverSimulationTypes.h"
#include "CharacterPropertiesStructs.h"
#include "LocomotionEnums.h"
#include "LocomotionStructs.h"
#include "SandboxCharacter_Mover.generated.h"

class APlayerController;
class UMotionWarpingComponent;
class UCameraComponent;
class USpringArmComponent;
class UChildActorComponent;
class UCurveFloat;
class UAC_FoleyEvents;
class UAC_VisualOverrideManager;
class UInputAction;
class UInputMappingContext;
class UAC_TraversalLogic;
class UCharacterMoverComponent;
class UCapsuleComponent;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRequestInteract_Mover);

UCLASS()
class UETEST1_API ASandboxCharacter_Mover : public APawn, public IBPI_SandboxCharacter_Pawn, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	ASandboxCharacter_Mover();

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
	FOnRequestInteract_Mover OnRequestInteract;

	// ===== ENHANCED INPUT ACTIONS =====

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Move_WorldSpace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Look;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Look_Gamepad;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Sprint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Walk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Jump;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Crouch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Strafe;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Aim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> IMC_Sandbox;

	// ===== INTERFACE IMPLEMENTATIONS =====

	virtual FS_CharacterPropertiesForAnimation Get_PropertiesForAnimation_Implementation() override;
	virtual FS_CharacterPropertiesForCamera Get_PropertiesForCamera_Implementation() override;
	virtual FS_CharacterPropertiesForTraversal Get_PropertiesForTraversal_Implementation() override;
	virtual void Set_CharacterInputState_Implementation(FS_PlayerInputState DesiredInputState) override;

	// ===== MOVER INPUT PRODUCER =====

	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

	// ===== GAIT/SPEED SYSTEM =====

	UFUNCTION(BlueprintCallable, Category = "Movement|Gait")
	E_Gait GetDesiredGait();

	UFUNCTION(BlueprintCallable, Category = "Movement|Speed")
	double CalculateMaxSpeed();

	UFUNCTION(BlueprintPure, Category = "Movement|Gait")
	bool CanSprint() const;

	UFUNCTION(BlueprintPure, Category = "Movement|Input")
	bool HasMovementInput() const;

	// ===== TRAVERSAL =====

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Traversal")
	FS_TraversalCheckInputs GetTraversalCheckInputs();

	// ===== RAGDOLL =====

	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void Ragdoll_Start();

	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void Ragdoll_End();

	// ===== INTERNAL STATE =====

	UPROPERTY(BlueprintReadWrite, Category = "Movement|State")
	bool FullMovementInput;

	UPROPERTY(BlueprintReadWrite, Category = "Movement|State")
	float StrafeSpeedMap;

	// Last computed custom inputs (cached for ABP/interface queries)
	UPROPERTY(BlueprintReadWrite, Category = "Movement|State")
	FMoverCustomInputs CachedCustomInputs;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;

	// ===== INPUT HANDLERS =====

	void OnMove(const struct FInputActionValue& Value);
	void OnMoveWorldSpace(const struct FInputActionValue& Value);
	void OnLook(const struct FInputActionValue& Value);
	void OnLookGamepad(const struct FInputActionValue& Value);
	void OnSprint(const struct FInputActionValue& Value);
	void OnSprintReleased(const struct FInputActionValue& Value);
	void OnWalk(const struct FInputActionValue& Value);
	void OnJumpAction(const struct FInputActionValue& Value);
	void OnJumpReleased(const struct FInputActionValue& Value);
	void OnCrouchAction(const struct FInputActionValue& Value);
	void OnStrafe(const struct FInputActionValue& Value);
	void OnAim(const struct FInputActionValue& Value);
	void OnAimReleased(const struct FInputActionValue& Value);

	// ===== MOVER INPUT PIPELINE =====

	// Compute orientation intent based on rotation mode and movement mode
	FRotator GetOrientationIntent() const;

	// Compute rotation mode from input state
	E_RotationMode GetRotationMode() const;

	// Compute movement direction from velocity
	E_MovementDirection CalculateMovementDirection() const;

	// ===== CAMERA =====

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetupCamera();

	// ===== CACHED COMPONENTS =====

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> CachedPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMoverComponent> CachedMoverComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCapsuleComponent> CachedCapsule;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> CachedMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMotionWarpingComponent> CachedMotionWarping;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CachedCamera;

	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CachedSpringArm;

	UPROPERTY(Transient)
	TObjectPtr<UChildActorComponent> CachedVisualOverride;

	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> CachedGameplayCamera;

	UPROPERTY(Transient)
	TObjectPtr<UAC_TraversalLogic> CachedTraversalLogic;

	UPROPERTY(Transient)
	TObjectPtr<UAC_FoleyEvents> CachedFoleyEvents;

	UPROPERTY(Transient)
	TObjectPtr<UAC_VisualOverrideManager> CachedVisualOverrideManager;

	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> CachedSmartObjectAnimation;

private:
	// Timer handle for JustLanded flag reset
	FTimerHandle JustLandedTimerHandle;

	void ResetJustLanded();

	// SteeringTime accumulator
	double AccumulatedSteeringTime = 0.0;
	bool bWasSteeringLastFrame = false;

	// Accumulated movement input from Enhanced Input (consumed each ProduceInput)
	FVector2D AccumulatedMoveInput = FVector2D::ZeroVector;
	bool bMoveInputThisFrame = false;

	// Jump input tracking for Mover
	bool bJumpJustPressed = false;
	bool bJumpHeld = false;

	// Cached previous-frame values for ABP MovementMode/Gait LastFrame tracking
	uint8 PrevABPMovementMode = 0;
	uint8 PrevABPGait = 1; // Run

	// Push MovementMode/Gait to AnimInstance via FByteProperty
	void UpdateAnimInstanceEnums();
};
