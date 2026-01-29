#include "BP_Manny.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ABP_Manny::ABP_Manny()
{
	PrimaryActorTick.bCanEverTick = false;

	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(DefaultSceneRoot);

	Manny = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Manny"));
	Manny->SetupAttachment(DefaultSceneRoot);
}

void ABP_Manny::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ABP_Manny::HandleDeferredBeginPlay);
	}
}

void ABP_Manny::HandleDeferredBeginPlay()
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
