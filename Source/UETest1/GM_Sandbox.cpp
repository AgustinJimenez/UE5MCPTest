#include "GM_Sandbox.h"
#include "PC_Sandbox.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

AGM_Sandbox::AGM_Sandbox()
{
	PrimaryActorTick.bCanEverTick = false;

	// Set PlayerController to C++ class (safe - no blueprint dependencies)
	PlayerControllerClass = APC_Sandbox::StaticClass();

	// NOTE: DefaultPawnClass is loaded in BeginPlay, NOT via ConstructorHelpers.
	// FClassFinder in the constructor forces early asset loading during CDO creation,
	// which changes module init load order and breaks LiveLink-dependent MetaHuman BPs.
}

void AGM_Sandbox::BeginPlay()
{
	Super::BeginPlay();

	// Load DefaultPawnClass at runtime (avoids ConstructorHelpers CDO loading order issues)
	if (!DefaultPawnClass)
	{
		UClass* PawnBP = LoadClass<APawn>(nullptr, TEXT("/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC_C"));
		if (PawnBP)
		{
			DefaultPawnClass = PawnBP;
		}
	}

	// Load PawnClasses at runtime (safer than ConstructorHelpers for complex BPs)
	if (PawnClasses.Num() == 0)
	{
		UClass* PawnClass0 = LoadClass<APawn>(nullptr, TEXT("/Game/Blueprints/SandboxCharacter_CMC.SandboxCharacter_CMC_C"));
		UClass* PawnClass1 = LoadClass<APawn>(nullptr, TEXT("/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C"));
		if (PawnClass0) PawnClasses.Add(PawnClass0);
		if (PawnClass1) PawnClasses.Add(PawnClass1);
	}

	// Load VisualOverrides at runtime (excludes BP_Kellan due to LiveLink issues)
	if (VisualOverrides.Num() == 0)
	{
		UClass* Visual0 = LoadClass<AActor>(nullptr, TEXT("/Game/Blueprints/RetargetedCharacters/BP_Echo.BP_Echo_C"));
		UClass* Visual1 = LoadClass<AActor>(nullptr, TEXT("/Game/Blueprints/RetargetedCharacters/BP_Twinblast.BP_Twinblast_C"));
		UClass* Visual2 = LoadClass<AActor>(nullptr, TEXT("/Game/Blueprints/RetargetedCharacters/BP_Manny.BP_Manny_C"));
		UClass* Visual3 = LoadClass<AActor>(nullptr, TEXT("/Game/Blueprints/RetargetedCharacters/BP_Quinn.BP_Quinn_C"));
		UClass* Visual4 = LoadClass<AActor>(nullptr, TEXT("/Game/Blueprints/RetargetedCharacters/BP_UE4_Mannequin.BP_UE4_Mannequin_C"));
		if (Visual0) VisualOverrides.Add(Visual0);
		if (Visual1) VisualOverrides.Add(Visual1);
		if (Visual2) VisualOverrides.Add(Visual2);
		if (Visual3) VisualOverrides.Add(Visual3);
		if (Visual4) VisualOverrides.Add(Visual4);
	}
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
