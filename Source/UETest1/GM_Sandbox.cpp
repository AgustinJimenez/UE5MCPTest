#include "GM_Sandbox.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"

AGM_Sandbox::AGM_Sandbox()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGM_Sandbox::BeginPlay()
{
	Super::BeginPlay();

	// TODO: DataDrivenCVarEngineSubsystem binding
	// This subsystem doesn't exist in this UE version
	// The CVar functionality can be re-implemented if needed
}

void AGM_Sandbox::OnDataDrivenCVarChanged(const FString& CVarName)
{
	// Handle CVar changes
	if (CVarName.Equals(TEXT("DDCvar.PawnClass"), ESearchCase::IgnoreCase))
	{
		ResetAllPlayers();
	}
}

void AGM_Sandbox::ResetAllPlayers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Get all player controllers
	TArray<AActor*> PlayerControllers;
	UGameplayStatics::GetAllActorsOfClass(World, APlayerController::StaticClass(), PlayerControllers);

	for (AActor* Actor : PlayerControllers)
	{
		APlayerController* PC = Cast<APlayerController>(Actor);
		if (PC && PC->GetPawn())
		{
			// Store the current pawn's location and rotation
			FVector Location = PC->GetPawn()->GetActorLocation();
			FRotator Rotation = PC->GetPawn()->GetActorRotation();

			// Destroy the current pawn
			PC->GetPawn()->Destroy();

			// Respawn the player
			RestartPlayer(PC);

			// Optionally set the new pawn's location to the old pawn's location
			if (PC->GetPawn())
			{
				PC->GetPawn()->SetActorLocation(Location);
				PC->GetPawn()->SetActorRotation(Rotation);
			}
		}
	}
}

void AGM_Sandbox::CyclePawn()
{
	// Get current value of DDCvar.PawnClass
	const int32 CurrentIndex = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCvar.PawnClass"));

	// Calculate next index with wrapping
	int32 NextIndex = 0;
	if (PawnClasses.Num() > 0)
	{
		NextIndex = (CurrentIndex + 1) % PawnClasses.Num();
	}

	// Execute console command to set new value
	const FString Command = FString::Printf(TEXT("DDCvar.PawnClass %d"), NextIndex);
	UKismetSystemLibrary::ExecuteConsoleCommand(this, Command, nullptr);
}

void AGM_Sandbox::CycleVisualOverride()
{
	// Get current value of DDCvar.VisualOverride
	const int32 CurrentIndex = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCvar.VisualOverride"));

	// Calculate next index with wrapping
	int32 NextIndex = 0;
	if (VisualOverrides.Num() > 0)
	{
		NextIndex = (CurrentIndex + 1) % VisualOverrides.Num();
	}

	// Execute console command to set new value
	const FString Command = FString::Printf(TEXT("DDCvar.VisualOverride %d"), NextIndex);
	UKismetSystemLibrary::ExecuteConsoleCommand(this, Command, nullptr);
}
