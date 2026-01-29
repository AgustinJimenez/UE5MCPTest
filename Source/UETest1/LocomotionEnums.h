#pragma once

#include "CoreMinimal.h"
#include "LocomotionEnums.generated.h"

UENUM(BlueprintType)
enum class E_Gait : uint8
{
	Walk UMETA(DisplayName = "Walk"),
	Run UMETA(DisplayName = "Run"),
	Sprint UMETA(DisplayName = "Sprint"),
};

UENUM(BlueprintType)
enum class E_MovementMode : uint8
{
	OnGround UMETA(DisplayName = "OnGround"),
	InAir UMETA(DisplayName = "InAir"),
	Sliding UMETA(DisplayName = "Sliding"),
	Traversing UMETA(DisplayName = "Traversing"),
};

UENUM(BlueprintType)
enum class E_Stance : uint8
{
	Stand UMETA(DisplayName = "Stand"),
	Crouch UMETA(DisplayName = "Crouch"),
};

UENUM(BlueprintType)
enum class E_RotationMode : uint8
{
	OrientToMovement UMETA(DisplayName = "OrientToMovement"),
	Strafe UMETA(DisplayName = "Strafe"),
	Aim UMETA(DisplayName = "Aim"),
};

UENUM(BlueprintType)
enum class E_MovementDirection : uint8
{
	F UMETA(DisplayName = "F"),
	B UMETA(DisplayName = "B"),
	LL UMETA(DisplayName = "LL"),
	LR UMETA(DisplayName = "LR"),
	RL UMETA(DisplayName = "RL"),
	RR UMETA(DisplayName = "RR"),
};

UENUM(BlueprintType)
enum class E_CameraStyle : uint8
{
	Close UMETA(DisplayName = "Close"),
	Medium UMETA(DisplayName = "Medium"),
	Far UMETA(DisplayName = "Far"),
	Debug UMETA(DisplayName = "Debug"),
};

UENUM(BlueprintType)
enum class E_CameraMode : uint8
{
	FreeCam UMETA(DisplayName = "FreeCam"),
	Strafe UMETA(DisplayName = "Strafe"),
	Aim UMETA(DisplayName = "Aim"),
	TwinStick UMETA(DisplayName = "TwinStick"),
};

UENUM(BlueprintType)
enum class EEarlyTransitionDestination : uint8
{
	ReTransition UMETA(DisplayName = "Re-Transition"),
	TransitionToLoop UMETA(DisplayName = "Transition To Loop"),
};

UENUM(BlueprintType)
enum class EEarlyTransitionCondition : uint8
{
	GaitNotEqual UMETA(DisplayName = "GaitNotEqual"),
	Always UMETA(DisplayName = "Always"),
};

UENUM(BlueprintType)
enum class ETraversalBlendOutCondition : uint8
{
	ForceBlendOut UMETA(DisplayName = "Force Blend Out"),
	WithMovementInput UMETA(DisplayName = "With Movement Input"),
	IfFalling UMETA(DisplayName = "If Falling"),
};
