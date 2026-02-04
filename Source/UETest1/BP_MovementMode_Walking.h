#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/WalkingMode.h"
#include "BP_MovementMode_Walking.generated.h"

struct FCustomSmoothWalkingState;

/**
 * Custom smooth walking mode - reimplements USmoothWalkingMode behavior.
 * Inherits from UWalkingMode (which is exported) and adds smooth movement.
 */
UCLASS(Blueprintable, BlueprintType)
class UETEST1_API UBP_MovementMode_Walking : public UWalkingMode
{
	GENERATED_BODY()

public:
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;
	virtual void GenerateMove_Implementation(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	// Blueprint hook used by BP_MovementMode_Walking to apply gait-based tuning (walk/run/sprint).
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mover")
	void GenerateWalkMove(FMoverTickStartData& StartState, float DeltaSeconds, const FVector& DesiredVelocity,
		const FQuat& DesiredFacing, const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity);
	virtual void GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds, const FVector& DesiredVelocity,
		const FQuat& DesiredFacing, const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity);

protected:
	/** Generate smooth walk move - the core smooth walking logic */
	void GenerateSmoothWalkMove(FMoverTickStartData& StartState, float DeltaSeconds, const FVector& DesiredVelocity,
		const FQuat& DesiredFacing, const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity) const;

public:
	// ========== Velocity Controls (from SmoothWalkingMode) ==========

	// Base acceleration when desired velocity > current velocity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
	float Acceleration = 1500.0f;

	// Base deceleration when desired velocity < current velocity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
	float Deceleration = 1500.0f;

	// 1 = emulates default walking mode, 0 = direct velocity change
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", ClampMax = "1"))
	float DirectionalAccelerationFactor = 1.0f;

	// Turning force (0-100, 10 = strong)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Smooth Walking Settings", meta = (ClampMin = "0", UIMin = "0"))
	float TurningStrength = 10.0f;

	// Velocity smoothing when accelerating
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Smooth Walking Settings", meta = (ClampMin = "0", ForceUnits = "s"))
	float AccelerationSmoothingTime = 0.1f;

	// Velocity smoothing when decelerating
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Smooth Walking Settings", meta = (ClampMin = "0", ForceUnits = "s"))
	float DecelerationSmoothingTime = 0.1f;

	// Compensation for smoothing lag when accelerating
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", ClampMax = "1"))
	float AccelerationSmoothingCompensation = 0.0f;

	// Compensation for smoothing lag when decelerating
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", ClampMax = "1"))
	float DecelerationSmoothingCompensation = 0.0f;

	// Velocity snap threshold
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float VelocityDeadzoneThreshold = 0.01f;

	// Acceleration snap threshold
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float AccelerationDeadzoneThreshold = 0.001f;

	// How quickly internal velocity adjusts to outside influences
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", ForceUnits = "s"))
	float OutsideInfluenceSmoothingTime = 0.05f;

	// ========== Facing Controls ==========

	// Smoothing applied to facing direction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Smooth Walking Settings", meta = (ClampMin = "0", ForceUnits = "s"))
	float FacingSmoothingTime = 0.25f;

	// Use double spring for smoother S-shaped facing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings")
	bool bSmoothFacingWithDoubleSpring = true;

	// Facing snap threshold
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", ForceUnits = "deg"))
	float FacingDeadzoneThreshold = 0.1f;

	// Angular velocity snap threshold
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Advanced Smooth Walking Settings", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float AngularVelocityDeadzoneThreshold = 0.01f;

	// Max speed override (-1 = use CommonLegacySettings)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mover|Walking Settings", meta = (ForceUnits = "cm/s"))
	float MaxSpeedOverride = -1.0f;
};
