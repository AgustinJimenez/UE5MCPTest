#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelButton.generated.h"

class USceneComponent;
class UTextRenderComponent;
class UBoxComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
struct FHitResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FButtonPressedDelegate);

UCLASS()
class UETEST1_API ALevelButton : public AActor
{
	GENERATED_BODY()

public:
	ALevelButton();

	// Variables from blueprint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FText ButtonName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	bool ExecuteConsoleCommand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FString ConsoleCommand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FLinearColor Color;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FLinearColor TextColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	double PlateScale;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "Button")
	FButtonPressedDelegate ButtonPressed;

	// Functions
	UFUNCTION(BlueprintCallable, Category = "Button")
	void SimulatePress();

	UFUNCTION(BlueprintCallable, Category = "Button")
	void UpdateName();

	UFUNCTION(BlueprintCallable, Category = "Button")
	void UpdateColor();

	UFUNCTION(BlueprintCallable, Category = "Button")
	void UpdateScale();

	UFUNCTION(BlueprintCallable, Category = "Button")
	void UpdateSenders();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	// Overlap handler
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// Do Once gate state
	bool bDoOnceHasExecuted;

	void ResetDoOnce();

	// Cached components
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CachedDefaultSceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> CachedName;

	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> CachedTrigger;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CachedPlate;
};
