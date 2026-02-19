#include "LevelButton.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Teleporter_Sender.h"
#include "TimerManager.h"

ALevelButton::ALevelButton()
{
	PrimaryActorTick.bCanEverTick = false;

	// Default values
	ExecuteConsoleCommand = false;
	PlateScale = 1.0;
	Color = FLinearColor::White;
	TextColor = FLinearColor::White;
	bDoOnceHasExecuted = false;
}

void ALevelButton::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Cache components (works for both C++ and Blueprint-created components)
	if (!CachedDefaultSceneRoot)
	{
		CachedDefaultSceneRoot = GetRootComponent();
	}

	if (!CachedName)
	{
		CachedName = FindComponentByClass<UTextRenderComponent>();
	}

	if (!CachedTrigger)
	{
		CachedTrigger = FindComponentByClass<UBoxComponent>();
	}

	if (!CachedPlate)
	{
		CachedPlate = FindComponentByClass<UStaticMeshComponent>();
	}

	UpdateName();
	UpdateColor();
	UpdateScale();
}

void ALevelButton::BeginPlay()
{
	Super::BeginPlay();

	// Re-cache components if they weren't found in OnConstruction (Blueprint component timing issue)
	if (!CachedDefaultSceneRoot)
	{
		CachedDefaultSceneRoot = GetRootComponent();
	}

	if (!CachedName)
	{
		CachedName = FindComponentByClass<UTextRenderComponent>();
	}

	if (!CachedTrigger)
	{
		CachedTrigger = FindComponentByClass<UBoxComponent>();
	}

	if (!CachedPlate)
	{
		CachedPlate = FindComponentByClass<UStaticMeshComponent>();
	}

	// Bind overlap event
	if (CachedTrigger)
	{
		UE_LOG(LogTemp, Warning, TEXT("LevelButton '%s': Trigger component found, binding overlap event"), *GetName());
		CachedTrigger->OnComponentBeginOverlap.AddDynamic(this, &ALevelButton::OnTriggerBeginOverlap);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("LevelButton '%s': Trigger component STILL NULL after BeginPlay! Listing all components..."), *GetName());

		// Debug: List all components to see what's available
		TArray<UActorComponent*> Components;
		GetComponents(Components);
		for (UActorComponent* Comp : Components)
		{
			UE_LOG(LogTemp, Warning, TEXT("  - Component: %s (Class: %s)"), *Comp->GetName(), *Comp->GetClass()->GetName());
		}
	}
}

void ALevelButton::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Debug: Print when button is triggered
	UE_LOG(LogTemp, Warning, TEXT("LevelButton '%s' triggered by %s!"), *GetName(), *OtherActor->GetName());

	// Do Once gate
	if (!bDoOnceHasExecuted)
	{
		bDoOnceHasExecuted = true;

		// Call ButtonPressed delegate
		ButtonPressed.Broadcast();

		// If ExecuteConsoleCommand is true, execute console command
		if (ExecuteConsoleCommand)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Executing console command: %s"), *ConsoleCommand);
			UKismetSystemLibrary::ExecuteConsoleCommand(this, ConsoleCommand, nullptr);
		}

		// Reset Do Once after 0.2 seconds
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ALevelButton::ResetDoOnce, 0.2f, false);
	}
}

void ALevelButton::ResetDoOnce()
{
	bDoOnceHasExecuted = false;
}

void ALevelButton::SimulatePress()
{
	// Call ButtonPressed delegate
	ButtonPressed.Broadcast();

	// If ExecuteConsoleCommand is true, execute console command
	if (ExecuteConsoleCommand)
	{
		UKismetSystemLibrary::ExecuteConsoleCommand(this, ConsoleCommand, nullptr);
	}
}

void ALevelButton::UpdateName()
{
	if (CachedName)
	{
		// Set text to ButtonName
		CachedName->SetText(ButtonName);

		// Set text color
		CachedName->SetTextRenderColor(TextColor.ToFColor(false));
	}
}

void ALevelButton::UpdateColor()
{
	if (CachedPlate)
	{
		// Set material parameter "Base Color" to Color (converted to Vector)
		CachedPlate->SetVectorParameterValueOnMaterials(FName("Base Color"), FVector(Color.R, Color.G, Color.B));
	}
}

void ALevelButton::UpdateScale()
{
	if (CachedPlate)
	{
		// Set world scale to (PlateScale, PlateScale, 0.02)
		CachedPlate->SetWorldScale3D(FVector(PlateScale, PlateScale, 0.02));
	}
}

void ALevelButton::UpdateSenders()
{
	// Get all Teleporter_Sender actors
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(this, ATeleporter_Sender::StaticClass(), FoundActors);

	// For each Teleporter_Sender, check if Destination == this
	for (AActor* Actor : FoundActors)
	{
		if (ATeleporter_Sender* Sender = Cast<ATeleporter_Sender>(Actor))
		{
			if (Sender->Destination == this)
			{
				// Update the sender
				Sender->UpdateName();
				Sender->UpdateColor();
				Sender->UpdateRotation();
				Sender->UpdateScale();
			}
		}
	}
}
