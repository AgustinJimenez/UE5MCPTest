// CameraToggleComponent.cpp
#include "CameraToggleComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

UCameraToggleComponent::UCameraToggleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCameraToggleComponent::BeginPlay()
{
	Super::BeginPlay();

	// Auto-find cameras if not set
	if (AActor* Owner = GetOwner())
	{
		TArray<UCameraComponent*> Cameras;
		Owner->GetComponents<UCameraComponent>(Cameras);

		for (UCameraComponent* Cam : Cameras)
		{
			if (!FirstPersonCamera && Cam->GetName().Contains(TEXT("FirstPerson")))
			{
				FirstPersonCamera = Cam;
				UE_LOG(LogTemp, Log, TEXT("CameraToggleComponent: Found FirstPersonCamera: %s"), *Cam->GetName());
			}
			else if (!ThirdPersonCamera && Cam->GetName().Contains(TEXT("Camera")) && !Cam->GetName().Contains(TEXT("FirstPerson")) && !Cam->GetName().Contains(TEXT("Gameplay")))
			{
				ThirdPersonCamera = Cam;
				UE_LOG(LogTemp, Log, TEXT("CameraToggleComponent: Found ThirdPersonCamera: %s"), *Cam->GetName());
			}
		}
	}

	// Try to bind immediately, but also schedule retry for after possession
	if (!SetupInputBindings())
	{
		// Retry after a short delay when controller should be available
		GetWorld()->GetTimerManager().SetTimer(
			BindingRetryTimerHandle,
			this,
			&UCameraToggleComponent::RetryInputBindings,
			0.1f,
			true,  // Loop
			0.1f   // Initial delay
		);
	}

	UpdateCameraState();
}

void UCameraToggleComponent::RetryInputBindings()
{
	if (SetupInputBindings())
	{
		// Success - stop retrying
		GetWorld()->GetTimerManager().ClearTimer(BindingRetryTimerHandle);
	}
}

void UCameraToggleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool UCameraToggleComponent::SetupInputBindings()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return false;

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Log, TEXT("CameraToggleComponent: No controller yet, will retry..."));
		return false;
	}

	if (!PC->InputComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("CameraToggleComponent: No InputComponent yet, will retry..."));
		return false;
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PC->InputComponent))
	{
		if (ToggleCameraAction)
		{
			EnhancedInput->BindAction(ToggleCameraAction, ETriggerEvent::Started, this, &UCameraToggleComponent::OnToggleCameraInput);
			UE_LOG(LogTemp, Log, TEXT("CameraToggleComponent: Input bound successfully to action %s"), *ToggleCameraAction->GetName());
			return true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("CameraToggleComponent: ToggleCameraAction not set!"));
		}
	}
	return false;
}

void UCameraToggleComponent::OnToggleCameraInput(const FInputActionValue& Value)
{
	ToggleCamera();
}

void UCameraToggleComponent::ToggleCamera()
{
	bIsFirstPerson = !bIsFirstPerson;
	UpdateCameraState();

	UE_LOG(LogTemp, Log, TEXT("Camera toggled to: %s"), bIsFirstPerson ? TEXT("First Person") : TEXT("Third Person"));
}

void UCameraToggleComponent::SetFirstPersonMode(bool bFirstPerson)
{
	bIsFirstPerson = bFirstPerson;
	UpdateCameraState();
}

void UCameraToggleComponent::UpdateCameraState()
{
	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetActive(bIsFirstPerson);
	}

	if (ThirdPersonCamera)
	{
		ThirdPersonCamera->SetActive(!bIsFirstPerson);
	}

	// Optionally hide mesh in first person
	if (AActor* Owner = GetOwner())
	{
		if (USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>())
		{
			// Only hide in first person if you want
			// Mesh->SetOwnerNoSee(bIsFirstPerson);
		}
	}
}
