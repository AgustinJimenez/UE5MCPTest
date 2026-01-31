#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
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

	// Blueprint function implementations - simplified without struct dependency
	UFUNCTION(BlueprintCallable, Category = "Foley")
	void PlayFoleyEvent(FGameplayTag Event, float Volume = 1.0f, float Pitch = 1.0f);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Foley")
	bool CanPlayFoley() const;

	UFUNCTION(BlueprintCallable, Category = "Foley|Debug")
	void TriggerVisLog(const FVector& Location);
};
