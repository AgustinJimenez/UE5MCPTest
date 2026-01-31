#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GM_Sandbox.generated.h"

class UDataDrivenCVarEngineSubsystem;

UCLASS()
class UETEST1_API AGM_Sandbox : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGM_Sandbox();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox")
	TArray<TSubclassOf<APawn>> PawnClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox")
	TArray<TSubclassOf<AActor>> VisualOverrides;

	UFUNCTION(BlueprintCallable, Category = "Sandbox")
	void ResetAllPlayers();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnDataDrivenCVarChanged(const FString& CVarName);
};
