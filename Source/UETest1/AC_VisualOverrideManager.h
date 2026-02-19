#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AC_VisualOverrideManager.generated.h"

class AActor;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UETEST1_API UAC_VisualOverrideManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UAC_VisualOverrideManager();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Visual Override")
	TSubclassOf<AActor> VisualOverride;

	UFUNCTION()
	void HandleDataDrivenCVarChanged(FString CVarName);

	void FindAndApplyVisualOverride();
};
