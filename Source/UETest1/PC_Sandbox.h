// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PC_Sandbox.generated.h"

class UInputAction;
struct FInputActionValue;
class UEnhancedInputComponent;

/**
 * Sandbox player controller with teleport, character switching, and visual override cycling
 */
UCLASS()
class UETEST1_API APC_Sandbox : public APlayerController
{
	GENERATED_BODY()

public:
	APC_Sandbox();

protected:
	// Array of available character classes to cycle through
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox")
	TArray<TSubclassOf<APawn>> Characters;

	// Current character index in the Characters array
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox")
	int32 CurrentCharacterIndex;

	// Cached control rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox")
	FRotator CachedControlRotation;

	// Maximum distance for teleport trace
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox")
	float TeleportMaxDistance;

	// Enhanced Input Actions
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> TeleportToTargetAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> NextPawnAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> NextVisualOverrideAction;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaTime) override;

	// Teleport the controlled pawn forward to a traced location
	UFUNCTION(BlueprintCallable, Category = "Sandbox")
	void TeleportToTarget();

	// Server RPC to cycle to next pawn type
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Sandbox")
	void ServerNextPawn();

	// Server RPC to cycle visual override
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Sandbox")
	void ServerNextVisualOverride();

	// Input handlers
	void OnTeleportToTarget(const FInputActionValue& Value);
	void OnNextPawn(const FInputActionValue& Value);
	void OnNextVisualOverride(const FInputActionValue& Value);
};
