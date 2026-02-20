#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SmartObjectStructs.h"
#include "AC_SmartObjectAnimation.generated.h"

class USkeletalMeshComponent;
class UCharacterMovementComponent;
class UAnimMontage;
class UAnimInstance;
class UMotionWarpingComponent;
class UMoverComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOwnerMontageFinished);

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UETEST1_API UAC_SmartObjectAnimation : public UActorComponent
{
	GENERATED_BODY()

public:
	UAC_SmartObjectAnimation();

	virtual void BeginPlay() override;

	// Delegate broadcast when montage finishes
	UPROPERTY(BlueprintAssignable, Category = "SmartObject")
	FOnOwnerMontageFinished OwnerMontageFinished;

	// Replicated multicast: plays montage on all clients
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "SmartObject")
	void PlayMontage_Multi(FSmartObjectAnimationPayload AnimPayload);

	// Internal: sets up and plays the montage
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	void PlaySmartObjectMontage(const FSmartObjectAnimationPayload& AnimPayload, bool bIgnoreCharacterMovementCorrections);

	// Called when montage is considered finished (by timer or loop completion)
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	void OnMontageFinishedRequested();

	// Entry point for collision ignore (calls replicated SetIgnoreState_Multi)
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	void SetIgnoreActorState(bool bShouldIgnore, AActor* OtherActor);

	// Replicated multicast: sets collision ignore on all clients
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "SmartObject")
	void SetIgnoreState_Multi(bool bShouldIgnore, AActor* OtherActor);

	// Actual collision ignore implementation
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	void SetIgnoreCollisionState(bool bShouldIgnore, AActor* OtherActor);

	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	void SetIgnoreCharacterMovementCorrection(bool bIgnoreCorrections);

	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	void CacheNecessaryData();

	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	void TryAddWarpTarget();

	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	void SetupPlayTimer();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SmartObject")
	bool IsMover() const;

	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	void NPCApproachAngleAndDistance(const FTransform& Destination, double& OutDistance, double& OutAngle) const;

	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	UAnimMontage* EvaluateDistanceAndMotionMatch(const FTransform& Destination, UObject* ProxyTable, double& OutCost, double& OutStartTime);

protected:
	UPROPERTY(BlueprintReadWrite, Category = "SmartObject")
	TObjectPtr<USkeletalMeshComponent> OwnerSkeletalMesh;

	UPROPERTY(BlueprintReadWrite, Category = "SmartObject")
	FSmartObjectAnimationPayload IncomingAnimationPayload;

	UPROPERTY(BlueprintReadWrite, Category = "SmartObject")
	int32 NumberOfLoops = 0;

	UPROPERTY(BlueprintReadWrite, Category = "SmartObject")
	FName WarpTargetName;

	UPROPERTY(BlueprintReadWrite, Category = "SmartObject")
	TObjectPtr<UCharacterMovementComponent> CharacterMovementComponent;

	UPROPERTY(BlueprintReadWrite, Category = "SmartObject")
	FSmartObjectSelectionInputs SmartObjectSelectionInputs;

	UPROPERTY(BlueprintReadWrite, Category = "SmartObject")
	FSmartObjectSelectionOutputs SmartObjectSelectionOutputs;

	UPROPERTY(BlueprintReadWrite, Category = "SmartObject")
	TObjectPtr<UMoverComponent> MoverComponent;

	UPROPERTY(BlueprintReadWrite, Category = "SmartObject")
	bool bIgnoreCharacterMovementServerCorrections = false;

private:
	FTimerHandle PlayTimerHandle;

	void OnPlayTimerExpired();

	// Montage completion callback
	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	// Replay montage for looping
	void PlayCurrentMontage();
};
