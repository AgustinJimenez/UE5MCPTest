#include "BP_Quinn.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ABP_Quinn::ABP_Quinn()
{
	PrimaryActorTick.bCanEverTick = false;

	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(DefaultSceneRoot);

	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMesh->SetupAttachment(DefaultSceneRoot);
}

void ABP_Quinn::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ABP_Quinn::HandleDeferredBeginPlay);
	}
}

void ABP_Quinn::HandleDeferredBeginPlay()
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
