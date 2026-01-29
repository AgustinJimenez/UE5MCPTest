#include "AC_PreCMCTick.h"

UAC_PreCMCTick::UAC_PreCMCTick()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAC_PreCMCTick::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	Tick.Broadcast();
}
