#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CharacterPropertiesStructs.h"
#include "BPI_SandboxCharacter_Pawn.generated.h"

UINTERFACE(BlueprintType)
class UETEST1_API UBPI_SandboxCharacter_Pawn : public UInterface
{
	GENERATED_BODY()
};

class UETEST1_API IBPI_SandboxCharacter_Pawn
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SandboxCharacter")
	FS_CharacterPropertiesForAnimation Get_PropertiesForAnimation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SandboxCharacter")
	FS_CharacterPropertiesForCamera Get_PropertiesForCamera();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SandboxCharacter")
	FS_CharacterPropertiesForTraversal Get_PropertiesForTraversal();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SandboxCharacter")
	void Set_CharacterInputState(FS_PlayerInputState DesiredInputState);
};
