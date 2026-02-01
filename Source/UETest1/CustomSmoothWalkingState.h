#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "CustomSmoothWalkingState.generated.h"

/**
 * Internal state data for smooth walking movement.
 * Custom implementation based on engine's FSmoothWalkingState (which is private).
 */
USTRUCT()
struct FCustomSmoothWalkingState : public FMoverDataStructBase
{
	GENERATED_BODY()

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual FMoverDataStructBase* Clone() const override { return new FCustomSmoothWalkingState(*this); }

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override
	{
		Ar << SpringVelocity;
		Ar << SpringAcceleration;
		Ar << IntermediateVelocity;
		Ar << IntermediateFacing;
		Ar << IntermediateAngularVelocity;
		bOutSuccess = true;
		return true;
	}

	virtual void ToString(FAnsiStringBuilderBase& Out) const override
	{
		Out.Appendf("CustomSmoothWalkingState: SpringVel=(%.2f,%.2f,%.2f)", SpringVelocity.X, SpringVelocity.Y, SpringVelocity.Z);
	}

	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override
	{
		const FCustomSmoothWalkingState* AuthState = static_cast<const FCustomSmoothWalkingState*>(&AuthorityState);
		return !SpringVelocity.Equals(AuthState->SpringVelocity, 1.0f);
	}

	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override
	{
		const FCustomSmoothWalkingState* FromState = static_cast<const FCustomSmoothWalkingState*>(&From);
		const FCustomSmoothWalkingState* ToState = static_cast<const FCustomSmoothWalkingState*>(&To);

		SpringVelocity = FMath::Lerp(FromState->SpringVelocity, ToState->SpringVelocity, Pct);
		SpringAcceleration = FMath::Lerp(FromState->SpringAcceleration, ToState->SpringAcceleration, Pct);
		IntermediateVelocity = FMath::Lerp(FromState->IntermediateVelocity, ToState->IntermediateVelocity, Pct);
		IntermediateFacing = FQuat::Slerp(FromState->IntermediateFacing, ToState->IntermediateFacing, Pct);
		IntermediateAngularVelocity = FMath::Lerp(FromState->IntermediateAngularVelocity, ToState->IntermediateAngularVelocity, Pct);
	}

	// Velocity of internal velocity spring
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Experimental")
	FVector SpringVelocity = FVector::ZeroVector;

	// Acceleration of internal velocity spring
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Experimental")
	FVector SpringAcceleration = FVector::ZeroVector;

	// Intermediate velocity which the velocity spring tracks as a target
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Experimental")
	FVector IntermediateVelocity = FVector::ZeroVector;

	// Intermediate facing direction when using a double spring
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Experimental")
	FQuat IntermediateFacing = FQuat::Identity;

	// Angular velocity of the intermediate spring when using a double spring
	UPROPERTY(BlueprintReadOnly, Category = "Mover|Experimental")
	FVector IntermediateAngularVelocity = FVector::ZeroVector;
};
