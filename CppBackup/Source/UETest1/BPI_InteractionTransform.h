#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "BPI_InteractionTransform.generated.h"

UINTERFACE(BlueprintType)
class UETEST1_API UBPI_InteractionTransform : public UInterface
{
	GENERATED_BODY()
};

class UETEST1_API IBPI_InteractionTransform
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	void SetInteractionTransform_Old(FTransform InteractionTransform);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	FTransform GetInteractionTransform_Old();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	FPoseHistoryReference Get_PoseHistory_Old();
};
