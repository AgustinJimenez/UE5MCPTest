#include "SandboxCharacter_Mover_ABP.h"
#include "GameFramework/Pawn.h"

void USandboxCharacter_Mover_ABP::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (bDebugDraws)
	{
		DebugDraws();
	}
}

void USandboxCharacter_Mover_ABP::DebugDraws()
{
	// TODO: Implement debug drawing
}
