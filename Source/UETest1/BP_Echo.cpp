#include "BP_Echo.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ABP_Echo::ABP_Echo()
{
	PrimaryActorTick.bCanEverTick = false;

	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	SetRootComponent(DefaultSceneRoot);

	EchoBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EchoBody"));
	EchoBody->SetupAttachment(DefaultSceneRoot);

	EchoHair = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("EchoHair"));
	EchoHair->SetupAttachment(EchoBody);
}

void ABP_Echo::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ABP_Echo::HandleDeferredBeginPlay);
	}
}

void ABP_Echo::HandleDeferredBeginPlay()
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
