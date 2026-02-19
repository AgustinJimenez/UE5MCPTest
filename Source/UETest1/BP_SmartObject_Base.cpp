#include "BP_SmartObject_Base.h"
#include "SmartObjectComponent.h"

ABP_SmartObject_Base::ABP_SmartObject_Base()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABP_SmartObject_Base::BeginPlay()
{
	Super::BeginPlay();

	// Cache components by name (set up in blueprint)
	CachedDefaultSceneRoot = Cast<USceneComponent>(GetDefaultSubobjectByName(TEXT("DefaultSceneRoot")));
	CachedSmartObject = Cast<USmartObjectComponent>(GetDefaultSubobjectByName(TEXT("SmartObject")));
}
