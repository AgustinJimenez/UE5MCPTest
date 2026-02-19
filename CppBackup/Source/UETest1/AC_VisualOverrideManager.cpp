#include "AC_VisualOverrideManager.h"

#include "DataDrivenCVars/DataDrivenCVars.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

UAC_VisualOverrideManager::UAC_VisualOverrideManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAC_VisualOverrideManager::BeginPlay()
{
	Super::BeginPlay();

	FindAndApplyVisualOverride();

	if (GEngine)
	{
		if (UDataDrivenCVarEngineSubsystem* Subsystem = GEngine->GetEngineSubsystem<UDataDrivenCVarEngineSubsystem>())
		{
			Subsystem->OnDataDrivenCVarDelegate.AddDynamic(this, &UAC_VisualOverrideManager::HandleDataDrivenCVarChanged);
		}
	}
}

void UAC_VisualOverrideManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GEngine)
	{
		if (UDataDrivenCVarEngineSubsystem* Subsystem = GEngine->GetEngineSubsystem<UDataDrivenCVarEngineSubsystem>())
		{
			Subsystem->OnDataDrivenCVarDelegate.RemoveDynamic(this, &UAC_VisualOverrideManager::HandleDataDrivenCVarChanged);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UAC_VisualOverrideManager::HandleDataDrivenCVarChanged(FString CVarName)
{
	if (CVarName == TEXT("DDCvar.VisualOverride"))
	{
		FindAndApplyVisualOverride();
	}
}

void UAC_VisualOverrideManager::FindAndApplyVisualOverride()
{
	if (!VisualOverride || !GetWorld())
	{
		return;
	}

	int32 VisualOverrideValue = -1;
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("DDCvar.VisualOverride")))
	{
		VisualOverrideValue = CVar->GetInt();
	}

	TArray<AActor*> OverrideActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), VisualOverride, OverrideActors);

	const bool bEnableOverride = VisualOverrideValue >= 0;
	for (AActor* Actor : OverrideActors)
	{
		if (!Actor)
		{
			continue;
		}

		Actor->SetActorHiddenInGame(!bEnableOverride);
	}
}
