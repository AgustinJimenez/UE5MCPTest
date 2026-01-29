#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AC_FoleyEvents.generated.h"

class UPrimaryDataAsset;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UETEST1_API UAC_FoleyEvents : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foley")
	TObjectPtr<UPrimaryDataAsset> FoleyEventBank;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	FString VisLogDebugText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	FLinearColor VisLogDebugColor = FLinearColor::Green;
};
