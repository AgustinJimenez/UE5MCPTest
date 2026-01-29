#include "STT_SetCharacterInputState.h"

#include "BPI_SandboxCharacter_Pawn.h"

EStateTreeRunStatus USTT_SetCharacterInputState::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition)
{
	if (Character && Character->GetClass()->ImplementsInterface(UBPI_SandboxCharacter_Pawn::StaticClass()))
	{
		FS_PlayerInputState InputState;
		InputState.WantsToWalk = WantsToWalk;
		IBPI_SandboxCharacter_Pawn::Execute_Set_CharacterInputState(Character, InputState);
	}

	return EStateTreeRunStatus::Succeeded;
}
