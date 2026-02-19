#include "AC_FoleyEvents.h"
#include "Kismet/GameplayStatics.h"
#include "Components/MeshComponent.h"
#include "I_FoleyAudioBankInterface.h"
#include "VisualLogger/VisualLogger.h"
#include "Kismet/KismetSystemLibrary.h"

void UAC_FoleyEvents::PlayFoleyEvent(FGameplayTag Event, float Volume, float Pitch)
{
	if (!FoleyEventBank || !CanPlayFoley())
	{
		return;
	}

	// Cast the FoleyEventBank to the interface
	if (II_FoleyAudioBankInterface* FoleyBank = Cast<II_FoleyAudioBankInterface>(FoleyEventBank))
	{
		// Get sound from foley event bank
		USoundBase* Sound = nullptr;
		bool bSuccess = false;
		FoleyBank->Execute_GetSoundFromFoleyEvent(FoleyEventBank, Event, Sound, bSuccess);

		if (bSuccess && Sound)
		{
			// Get the owner's mesh component for the sound location
			if (AActor* Owner = GetOwner())
			{
				if (UMeshComponent* MeshComp = Owner->FindComponentByClass<UMeshComponent>())
				{
					const FVector Location = MeshComp->GetComponentLocation();

					// Play sound at location
					UGameplayStatics::PlaySoundAtLocation(
						this,
						Sound,
						Location,
						Volume,
						Pitch
					);

					// Trigger visual logger
					TriggerVisLog(Location);
				}
			}
		}
	}
}

bool UAC_FoleyEvents::CanPlayFoley() const
{
	if (AActor* Owner = GetOwner())
	{
		return Owner->GetClass()->ImplementsInterface(UI_FoleyAudioBankInterface::StaticClass());
	}
	return false;
}

void UAC_FoleyEvents::TriggerVisLog(const FVector& Location)
{
	// Check console variable to see if visual logging is enabled
	const bool bDrawVisLog = UKismetSystemLibrary::GetConsoleVariableBoolValue(TEXT("DDCvar.DrawVisLogShapesForFoleySounds"));
	if (!bDrawVisLog)
	{
		return;
	}

	// Log sphere shape to visual logger
	if (AActor* Owner = GetOwner())
	{
		const FColor Color = VisLogDebugColor.ToFColor(true);
		UE_VLOG_LOCATION(Owner, LogTemp, Log, Location, 5, Color, TEXT("%s"), *VisLogDebugText);
	}
}
