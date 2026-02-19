#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BPI_SandboxCharacter_Pawn.h"
#include "CharacterPropertiesStructs.h"
#include "LocomotionEnums.h"
#include "SandboxCharacter_CMC.generated.h"

class APlayerController;
class UMotionWarpingComponent;
class UAC_PreCMCTick;
class UCameraComponent;
class USpringArmComponent;
class UChildActorComponent;
class UCurveFloat;
class UAC_FoleyEvents;
class UAC_VisualOverrideManager;
class UInputAction;
class UInputMappingContext;

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

	UPROPERTY(BlueprintReadWrite, Category = "Movement|State")
	bool IsMovingOnGround;

	// ===== CAMERA & ROTATION =====

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetupCamera();

	UFUNCTION(BlueprintCallable, Category = "Movement|Rotation")
	void UpdateRotation_PreCMC();

	// ===== TRAVERSAL =====

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Traversal")
	FS_TraversalCheckInputs GetTraversalCheckInputs();

	// ===== SIMULATED PROXY =====

	UFUNCTION(BlueprintCallable, Category = "Movement|Simulated")
	void UpdatedMovementSimulated(FVector OldVelocity);

	UFUNCTION(BlueprintCallable, Category = "Movement|Events")
	void CustomOnLandedEvent(FVector InLandVelocity);

	UFUNCTION(BlueprintCallable, Category = "Movement|Events")
	void CustomOnJumpedEvent(double GroundSpeedBeforeJump);

	// ===== RAGDOLL =====

	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void Ragdoll_Start();

	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void Ragdoll_End();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnWalkingOffLedge_Implementation(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta) override;

	// PreCMCTick delegate handler - runs before CharacterMovementComponent ticks
	UFUNCTION()
	void OnPreCMCTick();

	// Simulated proxy: called when CharacterMovementComponent updates
	UFUNCTION()
	void OnMovementUpdatedSimulated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	// Replicated input state for multiplayer
	UFUNCTION(Server, Reliable)
	void UpdateInputState_Server(FS_PlayerInputState NewInputState);

	// ===== INPUT HANDLERS =====

	void OnMove(const struct FInputActionValue& Value);
	void OnMoveWorldSpace(const struct FInputActionValue& Value);
	void OnLook(const struct FInputActionValue& Value);
	void OnLookGamepad(const struct FInputActionValue& Value);
	void OnSprint(const struct FInputActionValue& Value);
	void OnWalk(const struct FInputActionValue& Value);
	void OnJumpAction(const struct FInputActionValue& Value);
	void OnJumpReleased(const struct FInputActionValue& Value);
	void OnCrouchAction(const struct FInputActionValue& Value);
	void OnStrafe(const struct FInputActionValue& Value);
	void OnAim(const struct FInputActionValue& Value);

	// Helper function to get movement input scale
	FVector2D GetMovementInputScaleValue(const FVector2D& Input) const;

	// Helper to reset JustLanded flag after delay
	void ResetJustLanded();

	// ===== CACHED COMPONENTS =====

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> CachedPlayerController;

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

private:
	// Timer handle for JustLanded flag reset
	FTimerHandle JustLandedTimerHandle;

	// Whether OnPreCMCTick delegate was successfully bound
	bool bPreCMCTickBound = false;
};
