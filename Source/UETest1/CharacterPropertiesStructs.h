#pragma once

#include "CoreMinimal.h"
#include "LocomotionEnums.h"
#include "CharacterPropertiesStructs.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;
class UMotionWarpingComponent;

USTRUCT(BlueprintType)
struct FS_PlayerInputState
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

USTRUCT(BlueprintType)
struct FS_CharacterPropertiesForAnimation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FS_PlayerInputState InputState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	E_MovementMode MovementMode = E_MovementMode::OnGround;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	E_Stance Stance = E_Stance::Stand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	E_RotationMode RotationMode = E_RotationMode::OrientToMovement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	E_Gait Gait = E_Gait::Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	E_MovementDirection MovementDirection = E_MovementDirection::F;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FTransform ActorTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FVector InputAcceleration = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	double CurrentMaxAcceleration = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	double CurrentMaxDeceleration = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FRotator OrientationIntent = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FRotator AimingRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool JustLanded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FVector LandVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	double SteeringTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FVector GroundNormal = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FVector GroundLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FS_CharacterPropertiesForCamera
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	E_CameraStyle CameraStyle = E_CameraStyle::Medium;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	E_CameraMode CameraMode = E_CameraMode::FreeCam;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	E_Gait Gait = E_Gait::Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	E_Stance Stance = E_Stance::Stand;
};

USTRUCT(BlueprintType)
struct FS_CharacterPropertiesForTraversal
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	TObjectPtr<UMotionWarpingComponent> MotionWarping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	E_MovementMode MovementMode = E_MovementMode::OnGround;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	E_Gait Gait = E_Gait::Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	double Speed = 0.0;
};
