#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SandboxCharacter_CMC_ABP.generated.h"

UCLASS()
class UETEST1_API USandboxCharacter_CMC_ABP : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Animation")
	void DebugDraws();

	// Properties that will be set from blueprint variables
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugDraws = false;
};
