#include "GM_Sandbox.h"
#include "Kismet/GameplayStatics.h"
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
