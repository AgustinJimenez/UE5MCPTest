#include "SandboxCharacter_Mover_ABP.h"
#include "BFL_HelpfulFunctions.h"
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
	APawn* OwningPawn = TryGetPawnOwner();
	if (!OwningPawn)
	{
		return;
	}

	FVector Location = OwningPawn->GetActorLocation();
	FRotator Rotation = OwningPawn->GetActorRotation();
	FVector Offset(100.0f, 0.0f, 100.0f);

	// Debug drawing will be implemented based on blueprint requirements
	// This replaces the blueprint nodes that had orphaned pins

	// Example: Draw debug string arrays
	TArray<FString> DebugStrings;
	DebugStrings.Add(TEXT("Mover Animation Blueprint"));
	DebugStrings.Add(FString::Printf(TEXT("DeltaTime: %.3f"), GetWorld()->GetDeltaSeconds()));

	UBFL_HelpfulFunctions::DebugDraw_StringArray(
		this,
		Location,
		Rotation,
		Offset,
		TEXT("Mover Debug"),
		TEXT("  "),
		DebugStrings,
		TEXT(""),  // HighlightedString - using correct parameter name
		TEXT(">>")
	);
}
