#pragma once

#include "CoreMinimal.h"
#include "FoleyEventSide.generated.h"

UENUM(BlueprintType)
enum class EFoleyEventSide : uint8
{
	Left UMETA(DisplayName = "Left"),
	None UMETA(DisplayName = "None"),
	Right UMETA(DisplayName = "Right"),
};
