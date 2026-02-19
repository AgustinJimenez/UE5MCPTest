#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SandboxCharacter_Mover_ABP.generated.h"

class UMoverComponent;

UCLASS()
class UETEST1_API USandboxCharacter_Mover_ABP : public UAnimInstance
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
