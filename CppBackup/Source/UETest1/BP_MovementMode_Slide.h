#pragma once

#include "CoreMinimal.h"
#include "BP_MovementMode_Walking.h"
#include "BP_MovementMode_Slide.generated.h"

/**
 * Slide movement mode - inherits from our smooth walking mode.
 * Can add slide-specific behavior by overriding GenerateMove_Implementation.
 */
UCLASS(Blueprintable, BlueprintType)
class UETEST1_API UBP_MovementMode_Slide : public UBP_MovementMode_Walking
{
	GENERATED_BODY()

public:
	UBP_MovementMode_Slide()
	{
		// Slide-specific defaults - can be adjusted
		Acceleration = 500.0f;  // Lower acceleration for sliding feel
		Deceleration = 300.0f;  // Lower deceleration - slides longer
		TurningStrength = 3.0f; // Less turning control while sliding
	}
};
