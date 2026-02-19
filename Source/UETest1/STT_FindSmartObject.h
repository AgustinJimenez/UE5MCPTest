#pragma once

#include "CoreMinimal.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectRequestTypes.h"
#include "STT_FindSmartObject.generated.h"

class AActor;

UENUM(BlueprintType)
enum class ESmartObjectSearchType : uint8
{
	ClosestDistance,
	FarthestDistance,
	FirstFound,
	Random
};

UCLASS()
class UETEST1_API USTT_FindSmartObject : public UStateTreeTaskBlueprintBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SmartObject")
	TObjectPtr<AActor> Actor;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SmartObject")
	ESmartObjectSearchType SearchType = ESmartObjectSearchType::ClosestDistance;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SmartObject")
	FVector SearchBoxExtents = FVector::ZeroVector;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SmartObject")
	TObjectPtr<AActor> SmartObject;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SmartObject")
	FSmartObjectSlotHandle CandidateSlot;

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) override;

private:
	FSmartObjectRequestResult FindSlotByDistance(const TArray<FSmartObjectRequestResult>& Results, bool bClosest) const;
};
