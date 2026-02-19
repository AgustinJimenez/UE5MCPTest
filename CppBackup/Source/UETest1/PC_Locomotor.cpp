#include "PC_Locomotor.h"

#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

APC_Locomotor::APC_Locomotor()
{
	static ConstructorHelpers::FObjectFinder<UInputAction> MoveAction(TEXT("/Game/Input/IA_Move.IA_Move"));
	if (MoveAction.Succeeded())
	{
		IA_Move = MoveAction.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> LookAction(TEXT("/Game/Input/IA_Look.IA_Look"));
	if (LookAction.Succeeded())
	{
		IA_Look = LookAction.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> LookGamepadAction(TEXT("/Game/Input/IA_Look_Gamepad.IA_Look_Gamepad"));
	if (LookGamepadAction.Succeeded())
	{
		IA_Look_Gamepad = LookGamepadAction.Object;
	}
}

void APC_Locomotor::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	if (IA_Move)
	{
		EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &APC_Locomotor::HandleMove);
	}

	if (IA_Look)
	{
		EnhancedInput->BindAction(IA_Look, ETriggerEvent::Triggered, this, &APC_Locomotor::HandleLook);
	}

	if (IA_Look_Gamepad)
	{
		EnhancedInput->BindAction(IA_Look_Gamepad, ETriggerEvent::Triggered, this, &APC_Locomotor::HandleLookGamepad);
	}
}

void APC_Locomotor::HandleMove(const FInputActionValue& Value)
{
	const FVector2D RawInput = Value.Get<FVector2D>();
	if (RawInput.IsNearlyZero())
	{
		return;
	}

	const FVector2D Input = RawInput.GetSafeNormal();
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	const FRotator ControlRot = GetControlRotation();
	const FVector Forward = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::Y);

	ControlledPawn->AddMovementInput(Right, Input.X);
	ControlledPawn->AddMovementInput(Forward, Input.Y);
}

void APC_Locomotor::HandleLook(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();
	AddYawInput(LookInput.X);
	AddPitchInput(LookInput.Y);
}

void APC_Locomotor::HandleLookGamepad(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	const FVector2D ScaledInput = LookInput * DeltaSeconds;
	AddYawInput(ScaledInput.X);
	AddPitchInput(ScaledInput.Y);
}
