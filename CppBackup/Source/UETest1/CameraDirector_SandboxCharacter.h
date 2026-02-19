#pragma once

#include "CoreMinimal.h"
#include "Directors/BlueprintCameraDirector.h"
#include "CameraDirector_SandboxCharacter.generated.h"

/**
 * Camera director for the sandbox character.
 * Inherits from UBlueprintCameraDirectorEvaluator which is the base for Blueprint camera directors.
 */
UCLASS(Blueprintable, BlueprintType)
class UETEST1_API UCameraDirector_SandboxCharacter : public UBlueprintCameraDirectorEvaluator
{
	GENERATED_BODY()

public:
	// Override GetWorld to provide our own implementation (base class doesn't export it)
	virtual UWorld* GetWorld() const override;
};
