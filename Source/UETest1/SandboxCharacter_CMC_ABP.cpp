#include "SandboxCharacter_CMC_ABP.h"
#include "BFL_HelpfulFunctions.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"

void USandboxCharacter_CMC_ABP::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Check if we have a valid owning actor
	APawn* OwningPawn = TryGetPawnOwner();
	HasOwningActor = (OwningPawn != nullptr);

	if (!HasOwningActor)
	{
		return;
	}

	// Update console variable driven variables (thread-safe caching)
	Update_CVarDrivenVariables();

	// Get properties from character
	Update_PropertiesFromCharacter();

	// Update logic (only if not using thread-safe animation)
	if (!UseThreadSafeUpdateAnimation)
	{
		Update_Logic();
	}
}

void USandboxCharacter_CMC_ABP::NativePostEvaluateAnimation()
{
	Super::NativePostEvaluateAnimation();

	if (HasOwningActor && DebugExperimentalStateMachine)
	{
		Debug_ExperimentalStateMachine();
	}
}

void USandboxCharacter_CMC_ABP::Update_CVarDrivenVariables()
{
	// Cache console variables for thread-safe access
	OffsetRootBoneEnabled = UKismetSystemLibrary::GetConsoleVariableBoolValue(TEXT("a.animnode.offsetrootbone.enable"));
	MMDatabaseLOD = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCvar.MMDatabaseLOD"));
	OffsetRootTranslationRadius = UKismetSystemLibrary::GetConsoleVariableFloatValue(TEXT("DDCvar.OffsetRootBone.TranslationRadius"));
	UseThreadSafeUpdateAnimation = UKismetSystemLibrary::GetConsoleVariableBoolValue(TEXT("DDCVar.ThreadSafeAnimationUpdate.Enable"));
	DebugExperimentalStateMachine = UKismetSystemLibrary::GetConsoleVariableBoolValue(TEXT("DDCvar.DrawCharacterDebugShapes"));

	// Get experimental state machine setting
	bool ExperimentalStateMachineEnabled = UKismetSystemLibrary::GetConsoleVariableBoolValue(TEXT("DDCVar.ExperimentalStateMachine.Enable"));

	// Check for component tags
	USkeletalMeshComponent* SkelMeshComp = GetOwningComponent();
	bool ForceStateMachineSetup = SkelMeshComp && SkelMeshComp->ComponentHasTag(TEXT("Force SM Setup"));
	bool ForceMMSetup = SkelMeshComp && SkelMeshComp->ComponentHasTag(TEXT("Force MM Setup"));

	UseExperimentalStateMachine = (ExperimentalStateMachineEnabled || ForceStateMachineSetup) && !ForceMMSetup;

	// Get locomotion setup and convert to bool for UseExperimentalStateMachine
	LocomotionSetup = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCVar.LocomotionSetupCMC"));
	UseExperimentalStateMachine = (LocomotionSetup != 0);
}

void USandboxCharacter_CMC_ABP::Update_PropertiesFromCharacter()
{
	// Get properties from character via interface
	AActor* OwningActor = GetOwningActor();
	if (!OwningActor)
	{
		return;
	}

	// Call Get_PropertiesForAnimation interface function (implemented in character class)
	// This requires the character to implement BPI_SandboxCharacter_Pawn interface
	// For now, stub this out - the actual interface call will be implemented
	// when we verify the character class has the C++ implementation

	// TODO: Implement interface call when character C++ is ready
	// CharacterProperties = OwningActor->Get_PropertiesForAnimation();
}

void USandboxCharacter_CMC_ABP::Update_Logic()
{
	// Main update logic sequence
	Update_Trajectory();
	Update_EssentialValues();
	Update_States();

	// Only update direction and rotation if using experimental state machine
	if (UseExperimentalStateMachine)
	{
		Update_MovementDirection();
		Update_TargetRotation();
	}
}

void USandboxCharacter_CMC_ABP::Update_Trajectory()
{
	// TODO: Implement trajectory update logic from blueprint
	// This will update Trajectory, TrajectoryGenerationData_Idle, TrajectoryGenerationData_Moving
}

void USandboxCharacter_CMC_ABP::Update_EssentialValues()
{
	// TODO: Implement essential values update from blueprint
	// This will update movement state, velocity, acceleration, etc.
}

void USandboxCharacter_CMC_ABP::Update_States()
{
	// TODO: Implement state update logic from blueprint
	// This will update MovementState, Gait, Stance, etc.
}

void USandboxCharacter_CMC_ABP::Update_MovementDirection()
{
	// TODO: Implement movement direction update from blueprint
	// This will update MovementDirection, MovementDirectionBias, etc.
}

void USandboxCharacter_CMC_ABP::Update_TargetRotation()
{
	// TODO: Implement target rotation update from blueprint
	// This will update TargetRotation, TargetRotationDelta, etc.
}

void USandboxCharacter_CMC_ABP::Debug_ExperimentalStateMachine()
{
	// TODO: Implement debug visualization from blueprint
	// This is a large function with debug drawing logic
}

void USandboxCharacter_CMC_ABP::DebugDraws()
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
	DebugStrings.Add(TEXT("CMC Animation Blueprint"));
	DebugStrings.Add(FString::Printf(TEXT("DeltaTime: %.3f"), GetWorld()->GetDeltaSeconds()));

	UBFL_HelpfulFunctions::DebugDraw_StringArray(
		this,
		Location,
		Rotation,
		Offset,
		TEXT("CMC Debug"),
		TEXT("  "),
		DebugStrings,
		TEXT(""),  // HighlightedString - using correct parameter name
		TEXT(">>")
	);
}
