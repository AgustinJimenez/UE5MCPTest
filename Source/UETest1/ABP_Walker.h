// ABP_Walker.h - Animation Blueprint base class for BP_Walker

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ABP_Walker.generated.h"

class ABP_Walker;

/**
 * C++ base class for the ABP_Walker Animation Blueprint.
 *
 * Handles camera aim target computation (BlueprintUpdateAnimation) and
 * teleport flag initialization (BlueprintBeginPlay) in C++.
 * The AnimGraph remains in the BP subclass.
 */
UCLASS()
class UETEST1_API UABP_Walker : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	/** Camera aim target in skeletal-mesh local space, consumed by the Control Rig node. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FVector CameraAimTarget;

	/** One-frame teleport flag for the Control Rig. True on first tick only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bTeleport = false;

private:
	/** Cached owning pawn (BP_Walker). */
	UPROPERTY(Transient)
	TWeakObjectPtr<ABP_Walker> CachedWalker;

	/** Whether we still need to clear the teleport flag on the next tick. */
	bool bPendingTeleportClear = false;
};
