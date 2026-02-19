#include "BP_SmartBench.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

ABP_SmartBench::ABP_SmartBench()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABP_SmartBench::BeginPlay()
{
	Super::BeginPlay();

	// Cache components by name
	CachedStaticMesh = Cast<UStaticMeshComponent>(GetDefaultSubobjectByName(TEXT("StaticMesh")));
}
