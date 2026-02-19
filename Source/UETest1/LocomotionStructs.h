#pragma once

#include "CoreMinimal.h"
#include "LocomotionEnums.h"
#include "LocomotionStructs.generated.h"

// Forward declarations
class UAnimationAsset;
class UBlendProfile;

// Simple input state struct - contains player input flags
USTRUCT(BlueprintType)
struct FPlayerInputState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool WantsToSprint = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool WantsToWalk = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool WantsToStrafe = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool WantsToAim = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool WantsToCrouch = false;
};

// Movement direction thresholds for animation blending
USTRUCT(BlueprintType)
struct FMovementDirectionThresholds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	double FL = 0.0; // Forward-Left threshold

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	double FR = 0.0; // Forward-Right threshold

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	double BL = 0.0; // Back-Left threshold

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	double BR = 0.0; // Back-Right threshold
};

// Character properties passed to animation blueprint
USTRUCT(BlueprintType)
struct FCharacterPropertiesForAnimation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FPlayerInputState InputState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_MovementMode MovementMode = E_MovementMode::OnGround;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_Stance Stance = E_Stance::Stand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_RotationMode RotationMode = E_RotationMode::OrientToMovement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_Gait Gait = E_Gait::Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	E_MovementDirection MovementDirection = E_MovementDirection::F;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transform")
	FTransform ActorTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector InputAcceleration = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	double CurrentMaxAcceleration = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	double CurrentMaxDeceleration = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	FRotator OrientationIntent = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	FRotator AimingRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing")
	bool JustLanded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing")
	FVector LandVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	double SteeringTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FVector GroundNormal = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground")
	FVector GroundLocation = FVector::ZeroVector;
};

// Blend stack input parameters for animation blending
USTRUCT(BlueprintType)
struct FBlendStackInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimationAsset> Anim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool Loop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	double StartTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	double BlendTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UBlendProfile> BlendProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TArray<FName> Tags;
};
