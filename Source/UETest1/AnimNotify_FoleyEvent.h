#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "FoleyEventSide.h"
#include "AnimNotify_FoleyEvent.generated.h"

class UPrimaryDataAsset;

UCLASS(Blueprintable, meta = (DisplayName = "Foley Event"))
class UETEST1_API UAnimNotify_FoleyEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_FoleyEvent();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foley")
	FGameplayTag Event;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foley")
	EFoleyEventSide Side = EFoleyEventSide::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foley", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foley", meta = (ClampMin = "0.0"))
	float PitchMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foley")
	TObjectPtr<UPrimaryDataAsset> DefaultBank;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	FLinearColor VisLogDebugColor = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	FString VisLogDebugText;
};
