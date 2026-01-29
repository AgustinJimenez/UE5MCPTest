#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "I_FoleyAudioBankInterface.generated.h"

UINTERFACE(BlueprintType)
class UETEST1_API UI_FoleyAudioBankInterface : public UInterface
{
	GENERATED_BODY()
};

class UETEST1_API II_FoleyAudioBankInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Foley")
	bool CanPlayFoleyEvents();
};
