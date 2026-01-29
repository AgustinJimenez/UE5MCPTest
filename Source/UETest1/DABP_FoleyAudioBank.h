#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DABP_FoleyAudioBank.generated.h"

UCLASS(BlueprintType)
class UETEST1_API UDABP_FoleyAudioBank : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foley")
	FGameplayTag Assets;
};
