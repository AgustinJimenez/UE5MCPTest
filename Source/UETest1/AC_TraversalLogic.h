#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TraversalTypes.h"
#include "CharacterPropertiesStructs.h"
#include "IObjectChooser.h"
#include "AC_TraversalLogic.generated.h"

class USkeletalMeshComponent;
class UCapsuleComponent;
class UMotionWarpingComponent;
class UAnimInstance;
class UAnimMontage;
class UMoverComponent;
class UChooserTable;
class UCharacterMovementComponent;

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UETEST1_API UAC_TraversalLogic : public UActorComponent
{
	GENERATED_BODY()

public:
	UAC_TraversalLogic();

	virtual void BeginPlay() override;

	// Main entry point: called by character on jump input
	UFUNCTION(BlueprintCallable, Category = "Traversal")
	bool TryTraversalAction(FS_TraversalCheckInputs Inputs);

	// Gate flag read by character via reflection
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	bool DoingTraversalAction = false;

	// Current traversal result
	UPROPERTY(BlueprintReadWrite, Category = "Traversal")
	FS_TraversalCheckResult TraversalResult;

	// Whether this is a Mover-based character (vs CMC)
	UPROPERTY(BlueprintReadWrite, Category = "Traversal")
	bool IsMoverCharacter = false;

protected:
	// Perform the traversal action (play montage, set up warping, etc.)
	UFUNCTION(BlueprintCallable, Category = "Traversal")
	void PerformTraversalAction();

	// Set motion warping targets based on TraversalResult
	void SetWarpTargets();

	// Set movement mode abstracting CMC vs Mover
	void SetTraversalMovementMode(EMovementMode NewMode);

	// Set replication behavior abstracting CMC vs Mover
	void SetReplicationBehavior(bool bClientAuthoritative);

	// Evaluate the appropriate Chooser Table for montage selection
	UAnimMontage* EvaluateTraversalChooser(
		FS_TraversalChooserInputs& Inputs,
		FS_TraversalChooserOutputs& Outputs);

	// Montage completion callback
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	// Delayed replication revert
	void OnReplicationRevertTimer();

	// RPCs for multiplayer
	UFUNCTION(Server, Reliable)
	void PerformTraversalAction_Server(FS_TraversalCheckResult Result);

	UFUNCTION(NetMulticast, Reliable)
	void PerformTraversalAction_Clients(FS_TraversalCheckResult Result);

	// Cached character properties from interface
	UPROPERTY()
	FS_CharacterPropertiesForTraversal CharacterProperties;

	// Cached components
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> CachedMesh;

	UPROPERTY()
	TObjectPtr<UCapsuleComponent> CachedCapsule;

	UPROPERTY()
	TObjectPtr<UMotionWarpingComponent> CachedMotionWarping;

	UPROPERTY()
	TObjectPtr<UMoverComponent> CachedMover;

	// Chooser Table assets
	UPROPERTY()
	TObjectPtr<UChooserTable> TraversalChooserTable_CMC;

	UPROPERTY()
	TObjectPtr<UChooserTable> TraversalChooserTable_Mover;

private:
	// Cached values for warp target calculations
	double AnimatedDistanceFromFrontLedgeToBackLedge = 0.0;
	double AnimatedDistanceFromFrontLedgeToBackFloor = 0.0;

	// Timer for delayed replication revert
	FTimerHandle ReplicationRevertTimerHandle;

	// Helper to get AnimInstance from cached mesh
	UAnimInstance* GetOwnerAnimInstance() const;

	// Helper to call GetLedgeTransforms on LevelBlock_Traversable via reflection
	bool CallGetLedgeTransforms(AActor* TraversableActor, const FVector& HitLocation,
		const FVector& ActorLocation, FS_TraversalCheckResult& OutResult);

	// Helper to compute room check position at a ledge
	FVector ComputeRoomCheckPosition(const FVector& LedgeLocation, const FVector& LedgeNormal,
		double CapsuleRadius, double CapsuleHalfHeight) const;
};
