#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SandboxCharacter_CMC_ABP.generated.h"

/**
 * Animation Blueprint for SandboxCharacter_CMC.
 *
 * All animation logic is implemented in the BP function graphs.
 * C++ class is kept minimal to avoid shadowing BP variables.
 */
UCLASS()
class UETEST1_API USandboxCharacter_CMC_ABP : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
