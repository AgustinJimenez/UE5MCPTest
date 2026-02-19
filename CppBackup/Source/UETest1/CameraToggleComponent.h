// CameraToggleComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "CameraToggleComponent.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UETEST1_API UCameraToggleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCameraToggleComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Camera references - set these in Blueprint or find automatically
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (UseComponentPicker))
	TObjectPtr<UCameraComponent> ThirdPersonCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (UseComponentPicker))
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// Input Action for toggling (assign IA_ToggleCamera)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* ToggleCameraAction;

	// Current state
	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	bool bIsFirstPerson = false;

	// Toggle function - can be called from Blueprint too
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ToggleCamera();

	// Set specific camera mode
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetFirstPersonMode(bool bFirstPerson);

private:
	bool SetupInputBindings();
	void RetryInputBindings();
	void OnToggleCameraInput(const FInputActionValue& Value);
	void UpdateCameraState();

	FTimerHandle BindingRetryTimerHandle;
};
