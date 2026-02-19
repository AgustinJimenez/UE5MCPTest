#include "BP_UE4_Mannequin.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ABP_UE4_Mannequin::ABP_UE4_Mannequin()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABP_UE4_Mannequin::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ABP_UE4_Mannequin::HandleDeferredBeginPlay);
	}
}

void ABP_UE4_Mannequin::HandleDeferredBeginPlay()
{
	USceneComponent* Root = GetRootComponent();
	if (!Root)
	{
		return;
	}

	USceneComponent* Parent1 = Root->GetAttachParent();
	if (!Parent1)
	{
		return;
	}

	USceneComponent* Parent2 = Parent1->GetAttachParent();
	if (!Parent2)
	{
		return;
	}

	USceneComponent* Child = Root->GetChildComponent(0);
	if (!Child)
	{
		return;
	}

	Child->AddTickPrerequisiteComponent(Parent2);
}
