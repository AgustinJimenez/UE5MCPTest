#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LocomotionEnums.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "BPI_SandboxCharacter_ABP.generated.h"

UINTERFACE(BlueprintType)
class UETEST1_API UBPI_SandboxCharacter_ABP : public UInterface
{
	GENERATED_BODY()
};

class UETEST1_API IBPI_SandboxCharacter_ABP
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SandboxCharacter")
	FPoseHistoryReference Get_PoseHistory();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SandboxCharacter")
	FTransform Get_InteractionTransform();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SandboxCharacter")
	void Set_InteractionTransform(FTransform InteractionTransform);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SandboxCharacter")
	void Set_NotifyTransition_ReTransition(bool ReTransition);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SandboxCharacter")
	void Set_NotifyTransition_ToLoop(bool ToLoop);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SandboxCharacter")
	E_Gait Get_Gait();
};
