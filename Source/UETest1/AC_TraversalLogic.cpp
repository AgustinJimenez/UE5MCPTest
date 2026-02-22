#include "AC_TraversalLogic.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "MotionWarpingComponent.h"
#include "AnimationWarpingLibrary.h"
#include "MoverComponent.h"
#include "Chooser.h"
#include "IObjectChooser.h"
#include "BPI_SandboxCharacter_Pawn.h"
#include "BPI_SandboxCharacter_ABP.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "HAL/IConsoleManager.h"

UAC_TraversalLogic::UAC_TraversalLogic()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAC_TraversalLogic::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Get character properties via interface (use reflection — BP implements BP interface, not C++ UINTERFACE)
	if (Owner->GetClass()->ImplementsInterface(UBPI_SandboxCharacter_Pawn::StaticClass()))
	{
		CharacterProperties = IBPI_SandboxCharacter_Pawn::Execute_Get_PropertiesForTraversal(Owner);
	}
	else
	{
		static const FName FuncName(TEXT("Get_PropertiesForTraversal"));
		if (UFunction* Func = Owner->FindFunction(FuncName))
		{
			Owner->ProcessEvent(Func, &CharacterProperties);
		}
	}

	// Cache components from properties
	CachedCapsule = CharacterProperties.Capsule;
	CachedMesh = CharacterProperties.Mesh;
	CachedMotionWarping = CharacterProperties.MotionWarping;

	// Detect Mover character
	CachedMover = Owner->FindComponentByClass<UMoverComponent>();
	IsMoverCharacter = IsValid(CachedMover);

	// Load Chooser Table assets
	TraversalChooserTable_CMC = Cast<UChooserTable>(StaticLoadObject(
		UChooserTable::StaticClass(), nullptr,
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Traversal/CHT_TraversalMontages_CMC.CHT_TraversalMontages_CMC")));

	TraversalChooserTable_Mover = Cast<UChooserTable>(StaticLoadObject(
		UChooserTable::StaticClass(), nullptr,
		TEXT("/Game/Characters/UEFN_Mannequin/Animations/Traversal/CHT_TraversalMontages_Mover.CHT_TraversalMontages_Mover")));
}

UAnimInstance* UAC_TraversalLogic::GetOwnerAnimInstance() const
{
	if (CachedMesh)
	{
		return CachedMesh->GetAnimInstance();
	}
	return nullptr;
}

FVector UAC_TraversalLogic::ComputeRoomCheckPosition(const FVector& LedgeLocation, const FVector& LedgeNormal,
	double CapsuleRadius, double CapsuleHalfHeight) const
{
	FVector Offset = LedgeNormal * (CapsuleRadius + 2.0);
	Offset.Z += (CapsuleHalfHeight + 2.0);
	return LedgeLocation + Offset;
}

bool UAC_TraversalLogic::CallGetLedgeTransforms(AActor* TraversableActor, const FVector& HitLocation,
	const FVector& ActorLocation, FS_TraversalCheckResult& OutResult)
{
	if (!TraversableActor) return false;

	static const FName GetLedgeTransformsFuncName(TEXT("GetLedgeTransforms"));
	UFunction* Func = TraversableActor->FindFunction(GetLedgeTransformsFuncName);
	if (!Func) return false;

	// Build params matching the BP function signature:
	// GetLedgeTransforms(FVector HitLocation, FVector ActorLocation, INOUT S_TraversalCheckResult)
	struct FGetLedgeTransformsParams
	{
		FVector HitLocation;
		FVector InActorLocation;
		FS_TraversalCheckResult TraversalTraceResultInOut;
	};

	FGetLedgeTransformsParams Params;
	Params.HitLocation = HitLocation;
	Params.InActorLocation = ActorLocation;
	Params.TraversalTraceResultInOut = OutResult;

	TraversableActor->ProcessEvent(Func, &Params);

	OutResult = Params.TraversalTraceResultInOut;
	return true;
}

bool UAC_TraversalLogic::TryTraversalAction(FS_TraversalCheckInputs Inputs)
{
	AActor* Owner = GetOwner();
	if (!Owner) return true;

	// Refresh character properties via interface (C++ or BP)
	if (Owner->GetClass()->ImplementsInterface(UBPI_SandboxCharacter_Pawn::StaticClass()))
	{
		CharacterProperties = IBPI_SandboxCharacter_Pawn::Execute_Get_PropertiesForTraversal(Owner);
	}
	else
	{
		static const FName FuncName(TEXT("Get_PropertiesForTraversal"));
		if (UFunction* Func = Owner->FindFunction(FuncName))
		{
			Owner->ProcessEvent(Func, &CharacterProperties);
		}
	}

	// Step 1: Read debug CVars
	int32 DrawDebugLevel = 0;
	float DrawDebugDuration = 0.0f;
	{
		static const auto* DebugLevelCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("DDCvar.Traversal.DrawDebugLevel"));
		static const auto* DebugDurationCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("DDCvar.Traversal.DrawDebugDuration"));
		if (DebugLevelCVar) DrawDebugLevel = DebugLevelCVar->GetInt();
		if (DebugDurationCVar) DrawDebugDuration = DebugDurationCVar->GetFloat();
	}

	const EDrawDebugTrace::Type TraceDebugType = (DrawDebugLevel >= 3)
		? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

	// Step 2: Cache values
	const FVector ActorLocation = Owner->GetActorLocation();
	double CapsuleRadius = 0.0;
	double CapsuleHalfHeight = 0.0;
	if (CachedCapsule)
	{
		CapsuleRadius = CachedCapsule->GetScaledCapsuleRadius();
		CapsuleHalfHeight = CachedCapsule->GetScaledCapsuleHalfHeight();
	}

	// Initialize check result
	FS_TraversalCheckResult CheckResult;

	// Step 2.1: Forward capsule trace
	const FVector TraceStart = ActorLocation + Inputs.TraceOriginOffset;
	const FVector TraceEnd = TraceStart
		+ (Inputs.TraceForwardDirection * Inputs.TraceForwardDistance)
		+ Inputs.TraceEndOffset;

	FHitResult ForwardHit;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Owner);

	bool bForwardHit = UKismetSystemLibrary::CapsuleTraceSingle(
		this, TraceStart, TraceEnd,
		Inputs.TraceRadius, Inputs.TraceHalfHeight,
		ETraceTypeQuery::TraceTypeQuery3, // Custom traversal channel
		false, ActorsToIgnore,
		TraceDebugType,
		ForwardHit, true,
		FLinearColor::Black, FLinearColor::Black,
		DrawDebugDuration);

	if (!bForwardHit)
	{
		return true; // Failed: nothing hit
	}

	// Check if hit actor is a LevelBlock_Traversable (via class name since Cast fails for BP class)
	AActor* HitActor = ForwardHit.GetActor();
	if (!HitActor || !HitActor->GetClass()->GetName().Contains(TEXT("LevelBlock_Traversable")))
	{
		return true; // Failed: not a traversable
	}

	CheckResult.HitComponent = ForwardHit.GetComponent();
	const FVector HitLocation = ForwardHit.ImpactPoint;

	// Step 2.2: Get ledge transforms from traversable actor
	if (!CallGetLedgeTransforms(HitActor, HitLocation, ActorLocation, CheckResult))
	{
		return true; // Failed: couldn't get ledge data
	}

	// Debug: draw ledge positions
	if (DrawDebugLevel >= 1)
	{
		if (CheckResult.HasFrontLedge)
		{
			DrawDebugSphere(GetWorld(), CheckResult.FrontLedgeLocation, 10.0f, 12,
				FColor::Green, false, DrawDebugDuration, 0, 1.0f);
		}
		if (CheckResult.HasBackLedge)
		{
			DrawDebugSphere(GetWorld(), CheckResult.BackLedgeLocation, 10.0f, 12,
				FColor(0, 163, 255), false, DrawDebugDuration, 0, 1.0f);
		}
	}

	// Step 3.1: Validate front ledge exists
	if (!CheckResult.HasFrontLedge)
	{
		return true; // Failed: no front ledge
	}

	// Step 3.2: Room check at front ledge (zero-length capsule sweep = overlap test)
	const FVector FrontRoomPos = ComputeRoomCheckPosition(
		CheckResult.FrontLedgeLocation, CheckResult.FrontLedgeNormal,
		CapsuleRadius, CapsuleHalfHeight);

	FHitResult RoomHit;
	bool bRoomBlocked = UKismetSystemLibrary::CapsuleTraceSingle(
		this, FrontRoomPos, FrontRoomPos, // Same start/end = overlap test
		CapsuleRadius, CapsuleHalfHeight,
		ETraceTypeQuery::TraceTypeQuery1, // Visibility
		false, ActorsToIgnore,
		TraceDebugType,
		RoomHit, true,
		FLinearColor::Red, FLinearColor::Green,
		DrawDebugDuration);

	// NOR(bBlockingHit, bInitialOverlap) - must be clear
	if (RoomHit.bBlockingHit || RoomHit.bStartPenetrating)
	{
		CheckResult.HasFrontLedge = false;
		return true; // Failed: no room at front ledge
	}

	// Step 3.3: Calculate obstacle height
	CheckResult.ObstacleHeight = FMath::Abs(ActorLocation.Z - CheckResult.FrontLedgeLocation.Z);

	// Step 3.4: Top sweep across obstacle
	FVector BackRoomPos = FVector::ZeroVector;
	if (CheckResult.HasBackLedge)
	{
		BackRoomPos = ComputeRoomCheckPosition(
			CheckResult.BackLedgeLocation, CheckResult.BackLedgeNormal,
			CapsuleRadius, CapsuleHalfHeight);
	}
	else
	{
		// If no back ledge, sweep in forward direction from front ledge
		BackRoomPos = FrontRoomPos + Inputs.TraceForwardDirection * 100.0;
	}

	FHitResult TopSweepHit;
	bool bTopBlocked = UKismetSystemLibrary::CapsuleTraceSingle(
		this, FrontRoomPos, BackRoomPos,
		CapsuleRadius, CapsuleHalfHeight,
		ETraceTypeQuery::TraceTypeQuery1, // Visibility
		false, ActorsToIgnore,
		TraceDebugType,
		TopSweepHit, true,
		FLinearColor::Red, FLinearColor::Green,
		DrawDebugDuration);

	// Step 3.5: Calculate obstacle depth
	if (!bTopBlocked)
	{
		// Clear path: depth = XY distance between front and back ledge
		const FVector Delta = CheckResult.FrontLedgeLocation - CheckResult.BackLedgeLocation;
		CheckResult.ObstacleDepth = FVector(Delta.X, Delta.Y, 0.0).Size();
	}
	else
	{
		// Blocked: depth = XY distance from front ledge to impact point; invalidate back ledge
		const FVector Delta = TopSweepHit.ImpactPoint - CheckResult.FrontLedgeLocation;
		CheckResult.ObstacleDepth = FVector(Delta.X, Delta.Y, 0.0).Size();
		CheckResult.HasBackLedge = false;
	}

	// Step 3.6: Back floor trace (downward from back room position)
	if (CheckResult.HasBackLedge)
	{
		const FVector BackFloorStart = BackRoomPos;
		// Trace far enough down to reach ground level from above the back ledge
		const double BackFloorTraceDist = CapsuleHalfHeight * 2.0 + CheckResult.ObstacleHeight + 200.0;
		const FVector BackFloorEnd = BackFloorStart - FVector(0.0, 0.0, BackFloorTraceDist);

		FHitResult BackFloorHit;
		bool bBackFloorHit = UKismetSystemLibrary::CapsuleTraceSingle(
			this, BackFloorStart, BackFloorEnd,
			CapsuleRadius, CapsuleHalfHeight,
			ETraceTypeQuery::TraceTypeQuery1, // Visibility
			false, ActorsToIgnore,
			TraceDebugType,
			BackFloorHit, true,
			FLinearColor::Red, FLinearColor::Green,
			DrawDebugDuration);

		if (bBackFloorHit)
		{
			CheckResult.HasBackFloor = true;
			CheckResult.BackFloorLocation = BackFloorHit.ImpactPoint;
			CheckResult.BackLedgeHeight = FMath::Abs(
				CheckResult.BackLedgeLocation.Z - BackFloorHit.ImpactPoint.Z);
		}
		else
		{
			CheckResult.HasBackFloor = false;
		}
	}

	// Step 4.1: Set interaction transform on ABP for pose matching
	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	FPoseHistoryReference PoseHistory;

	if (AnimInstance)
	{
		// Set interaction transform via BPI_SandboxCharacter_ABP interface
		static const FName SetInteractionFuncName(TEXT("Set_InteractionTransform"));
		if (UFunction* SetInteractionFunc = AnimInstance->FindFunction(SetInteractionFuncName))
		{
			struct FSetInteractionParams
			{
				FTransform InteractionTransform;
			};
			FSetInteractionParams SetParams;
			SetParams.InteractionTransform = FTransform(
				FRotationMatrix::MakeFromZ(CheckResult.FrontLedgeNormal).ToQuat(),
				CheckResult.FrontLedgeLocation);
			AnimInstance->ProcessEvent(SetInteractionFunc, &SetParams);
		}

		// Get PoseHistory via BPI_SandboxCharacter_ABP interface
		static const FName GetPoseHistoryFuncName(TEXT("Get_PoseHistory"));
		if (UFunction* GetPoseHistoryFunc = AnimInstance->FindFunction(GetPoseHistoryFuncName))
		{
			struct FGetPoseHistoryParams
			{
				FPoseHistoryReference ReturnValue;
			};
			FGetPoseHistoryParams PoseParams;
			AnimInstance->ProcessEvent(GetPoseHistoryFunc, &PoseParams);
			PoseHistory = PoseParams.ReturnValue;
		}
	}

	// Step 4.2: Build Chooser inputs
	FS_TraversalChooserInputs ChooserInputs;
	ChooserInputs.ActionType = CheckResult.ActionType;
	ChooserInputs.HasFrontLedge = CheckResult.HasFrontLedge;
	ChooserInputs.HasBackLedge = CheckResult.HasBackLedge;
	ChooserInputs.HasBackFloor = CheckResult.HasBackFloor;
	ChooserInputs.ObstacleHeight = CheckResult.ObstacleHeight;
	ChooserInputs.ObstacleDepth = CheckResult.ObstacleDepth;
	ChooserInputs.BackLedgeHeight = CheckResult.BackLedgeHeight;
	ChooserInputs.MovementMode = CharacterProperties.MovementMode;
	ChooserInputs.Gait = CharacterProperties.Gait;
	ChooserInputs.Speed = CharacterProperties.Speed;
	ChooserInputs.PoseHistory = PoseHistory;

	// DistanceToLedge: distance from mesh world location to front ledge
	if (CachedMesh)
	{
		ChooserInputs.DistanceToLedge = FVector::Dist(
			CheckResult.FrontLedgeLocation,
			CachedMesh->GetComponentLocation());
	}

	// Evaluate Chooser Table
	FS_TraversalChooserOutputs ChooserOutputs;
	UAnimMontage* ChosenMontage = EvaluateTraversalChooser(ChooserInputs, ChooserOutputs);

	// Store results in CheckResult
	CheckResult.ActionType = ChooserOutputs.ActionType;
	CheckResult.StartTime = ChooserOutputs.MontageStartTime;
	CheckResult.PlayRate = 1.0;
	CheckResult.ChosenMontage = ChosenMontage;

	// Step 5.1: Validate chosen action
	if (CheckResult.ActionType == E_TraversalActionType::None)
	{
		return true; // Failed: no valid montage selected
	}

	// Debug print
	if (DrawDebugLevel >= 1)
	{
		FString DebugMsg = FString::Printf(
			TEXT("Has Front Ledge: %s\nHas Back Ledge: %s\nHas Back Floor: %s\n")
			TEXT("Obstacle Height: %.1f\nObstacle Depth: %.1f\nBack Ledge Height: %.1f"),
			CheckResult.HasFrontLedge ? TEXT("true") : TEXT("false"),
			CheckResult.HasBackLedge ? TEXT("true") : TEXT("false"),
			CheckResult.HasBackFloor ? TEXT("true") : TEXT("false"),
			CheckResult.ObstacleHeight, CheckResult.ObstacleDepth, CheckResult.BackLedgeHeight);
		GEngine->AddOnScreenDebugMessage(1, 2.0f, FColor(0, 168, 255), DebugMsg);

		FString ActionMsg = FString::Printf(TEXT("%s\n%s"),
			*UEnum::GetDisplayValueAsText(CheckResult.ActionType).ToString(),
			ChosenMontage ? *ChosenMontage->GetName() : TEXT("None"));
		GEngine->AddOnScreenDebugMessage(2, 2.0f, FColor(255, 0, 209), ActionMsg);
	}

	// Step 5.2: Commit result and trigger traversal
	TraversalResult = CheckResult;
	PerformTraversalAction();
	PerformTraversalAction_Server(TraversalResult);

	return false; // Success: traversal started
}

UAnimMontage* UAC_TraversalLogic::EvaluateTraversalChooser(
	FS_TraversalChooserInputs& Inputs,
	FS_TraversalChooserOutputs& Outputs)
{
	UChooserTable* CHT = IsMoverCharacter ? TraversalChooserTable_Mover : TraversalChooserTable_CMC;
	if (!CHT) return nullptr;

	UAnimInstance* AnimInstance = GetOwnerAnimInstance();

	// Build context in the same order as CHT ContextData:
	// [0] UAnimInstance (object), [1] Inputs (struct), [2] Outputs (struct)
	FChooserEvaluationContext Context;
	Context.AddObjectParam(AnimInstance);
	Context.AddStructParam(Inputs);
	Context.AddStructParam(Outputs);

	UObject* Result = nullptr;
	UChooserTable::EvaluateChooser(Context, CHT,
		FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda(
			[&Result](UObject* InResult) -> FObjectChooserBase::EIteratorStatus
			{
				Result = InResult;
				return FObjectChooserBase::EIteratorStatus::Stop;
			}));

	// Outputs is already populated by OutputStructColumn::SetOutputs during evaluation
	return Cast<UAnimMontage>(Result);
}

void UAC_TraversalLogic::PerformTraversalAction()
{
	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	if (!AnimInstance || !TraversalResult.ChosenMontage) return;

	// Play montage
	const float PlayRate = static_cast<float>(TraversalResult.PlayRate);
	const float StartPos = static_cast<float>(TraversalResult.StartTime);

	if (IsMoverCharacter && CachedMover)
	{
		// Mover path: play via MoverComponent
		// For Mover characters, still play on AnimInstance directly
		// The K2Node_PlayMontageOnMoverActor internally does this + queues root motion layered move
		AnimInstance->Montage_Play(TraversalResult.ChosenMontage, PlayRate,
			EMontagePlayReturnType::MontageLength, StartPos, true);
	}
	else
	{
		// CMC path: play on AnimInstance
		AnimInstance->Montage_Play(TraversalResult.ChosenMontage, PlayRate,
			EMontagePlayReturnType::MontageLength, StartPos, true);
	}

	// Bind blend-out delegate (release gate + restore movement mode)
	FOnMontageBlendingOutStarted BlendOutDelegate;
	BlendOutDelegate.BindUObject(this, &UAC_TraversalLogic::OnMontageBlendingOut);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, TraversalResult.ChosenMontage);

	// Bind end delegate (restore collision after blend-out completes)
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UAC_TraversalLogic::OnMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, TraversalResult.ChosenMontage);

	// Set state
	DoingTraversalAction = true;

	// Setup warp targets
	SetWarpTargets();

	// Disable collision with entire obstacle actor (covers all components)
	if (CachedCapsule && TraversalResult.HitComponent)
	{
		if (AActor* HitActor = TraversalResult.HitComponent->GetOwner())
		{
			CachedCapsule->IgnoreActorWhenMoving(HitActor, true);
		}
	}

	// Set movement mode to Flying (prevents falling during traversal)
	SetTraversalMovementMode(MOVE_Flying);

	// Enable client-authoritative movement
	SetReplicationBehavior(true);
}

void UAC_TraversalLogic::SetWarpTargets()
{
	if (!CachedMotionWarping || !TraversalResult.ChosenMontage) return;

	// FrontLedge warp target (always set)
	{
		FVector TargetLocation = TraversalResult.FrontLedgeLocation + FVector(0.0, 0.0, 0.5);
		FRotator TargetRotation = FRotationMatrix::MakeFromX(-TraversalResult.FrontLedgeNormal).Rotator();
		CachedMotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(
			FName(TEXT("FrontLedge")), TargetLocation, TargetRotation);
	}

	const E_TraversalActionType ActionType = TraversalResult.ActionType;

	// BackLedge warp target (Hurdle and Vault only)
	if (ActionType == E_TraversalActionType::Hurdle || ActionType == E_TraversalActionType::Vault)
	{
		TArray<FMotionWarpingWindowData> BackLedgeWindows;
		UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(
			TraversalResult.ChosenMontage, FName(TEXT("BackLedge")), BackLedgeWindows);

		if (BackLedgeWindows.Num() > 0)
		{
			float EndTime = BackLedgeWindows[0].EndTime;
			float CurveValue = 0.0f;
			UAnimationWarpingLibrary::GetCurveValueFromAnimation(
				TraversalResult.ChosenMontage, FName(TEXT("Distance_From_Ledge")),
				EndTime, CurveValue);
			AnimatedDistanceFromFrontLedgeToBackLedge = CurveValue;

			CachedMotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(
				FName(TEXT("BackLedge")), TraversalResult.BackLedgeLocation, FRotator::ZeroRotator);
		}
	}
	else
	{
		CachedMotionWarping->RemoveWarpTarget(FName(TEXT("BackLedge")));
	}

	// BackFloor warp target (Hurdle only)
	if (ActionType == E_TraversalActionType::Hurdle)
	{
		TArray<FMotionWarpingWindowData> BackFloorWindows;
		UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(
			TraversalResult.ChosenMontage, FName(TEXT("BackFloor")), BackFloorWindows);

		if (BackFloorWindows.Num() > 0)
		{
			float EndTime = BackFloorWindows[0].EndTime;
			float CurveValue = 0.0f;
			UAnimationWarpingLibrary::GetCurveValueFromAnimation(
				TraversalResult.ChosenMontage, FName(TEXT("Distance_From_Ledge")),
				EndTime, CurveValue);
			AnimatedDistanceFromFrontLedgeToBackFloor = CurveValue;

			// Compute landing position using animated distances
			double HorizontalOffset = FMath::Abs(
				AnimatedDistanceFromFrontLedgeToBackLedge - AnimatedDistanceFromFrontLedgeToBackFloor);
			FVector OffsetVector = TraversalResult.BackLedgeNormal * HorizontalOffset;
			FVector LandingXY = TraversalResult.BackLedgeLocation + OffsetVector;

			FVector TargetLocation = FVector(LandingXY.X, LandingXY.Y,
				TraversalResult.BackFloorLocation.Z);

			CachedMotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(
				FName(TEXT("BackFloor")), TargetLocation, FRotator::ZeroRotator);
		}
	}
	else
	{
		CachedMotionWarping->RemoveWarpTarget(FName(TEXT("BackFloor")));
	}
}

void UAC_TraversalLogic::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	// Release traversal gate so next action can start
	DoingTraversalAction = false;

	// Set movement mode based on action type
	// Vault -> Falling (character is in air on other side)
	// Hurdle/Mantle/None -> Walking
	EMovementMode NewMode = (TraversalResult.ActionType == E_TraversalActionType::Vault)
		? MOVE_Falling : MOVE_Walking;
	SetTraversalMovementMode(NewMode);

	// NOTE: Collision restoration is deferred to OnMontageEnded (after blend-out completes)
	// to prevent the capsule from colliding with the obstacle while the character is still
	// blending out of the traversal animation.
}

void UAC_TraversalLogic::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// Restore collision with obstacle actor (safe now — blend-out is complete)
	if (CachedCapsule && TraversalResult.HitComponent)
	{
		if (AActor* HitActor = TraversalResult.HitComponent->GetOwner())
		{
			CachedCapsule->IgnoreActorWhenMoving(HitActor, false);
		}
	}

	// Delayed revert of replication behavior (0.2s)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ReplicationRevertTimerHandle,
			this,
			&UAC_TraversalLogic::OnReplicationRevertTimer,
			0.2f,
			false);
	}
}

void UAC_TraversalLogic::OnReplicationRevertTimer()
{
	SetReplicationBehavior(false);
}

void UAC_TraversalLogic::SetTraversalMovementMode(EMovementMode NewMode)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (IsMoverCharacter && CachedMover)
	{
		// Mover path: map EMovementMode to Mover mode name
		FName ModeName;
		switch (NewMode)
		{
		case MOVE_Falling:
			ModeName = FName(TEXT("Falling"));
			break;
		case MOVE_Flying:
			ModeName = FName(TEXT("Flying"));
			break;
		default:
			ModeName = FName(TEXT("Walking"));
			break;
		}
		CachedMover->QueueNextMode(ModeName);
	}
	else
	{
		// CMC path: set directly on CharacterMovementComponent
		if (ACharacter* Character = Cast<ACharacter>(Owner))
		{
			if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
			{
				CMC->SetMovementMode(NewMode);
			}
		}
	}
}

void UAC_TraversalLogic::SetReplicationBehavior(bool bClientAuthoritative)
{
	if (IsMoverCharacter)
	{
		// Mover handles replication differently — no-op
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (ACharacter* Character = Cast<ACharacter>(Owner))
	{
		if (UCharacterMovementComponent* CMC = Character->GetCharacterMovement())
		{
			CMC->bIgnoreClientMovementErrorChecksAndCorrection = bClientAuthoritative;
			CMC->bServerAcceptClientAuthoritativePosition = bClientAuthoritative;
		}
	}
}

// RPCs

void UAC_TraversalLogic::PerformTraversalAction_Server_Implementation(FS_TraversalCheckResult Result)
{
	TraversalResult = Result;
	PerformTraversalAction_Clients(Result);
}

void UAC_TraversalLogic::PerformTraversalAction_Clients_Implementation(FS_TraversalCheckResult Result)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Only execute on authority (server already handled it)
	if (Owner->HasAuthority())
	{
		TraversalResult = Result;
		PerformTraversalAction();
	}
}
