#pragma once

#include "CoreMinimal.h"
#include "LocomotionStructs.generated.h"

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
