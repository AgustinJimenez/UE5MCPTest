#include "AC_SmartObjectAnimation.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "MotionWarpingComponent.h"
#include "MoverComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

UAC_SmartObjectAnimation::UAC_SmartObjectAnimation()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAC_SmartObjectAnimation::BeginPlay()
{
	Super::BeginPlay();
	CacheNecessaryData();
}

void UAC_SmartObjectAnimation::PlayMontage_Multi_Implementation(const FSmartObjectAnimationPayload& AnimPayload)
{
	PlaySmartObjectMontage(AnimPayload, false);
}

void UAC_SmartObjectAnimation::SetIgnoreActorState(bool bShouldIgnore, AActor* OtherActor)
{
	SetIgnoreState_Multi(bShouldIgnore, OtherActor);
}

void UAC_SmartObjectAnimation::SetIgnoreState_Multi_Implementation(bool bShouldIgnore, AActor* OtherActor)
{
	SetIgnoreCollisionState(bShouldIgnore, OtherActor);
}

bool UAC_SmartObjectAnimation::IsMover() const
{
	return IsValid(MoverComponent);
}

void UAC_SmartObjectAnimation::CacheNecessaryData()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Get skeletal mesh component
	OwnerSkeletalMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();

	// Try to get CharacterMovementComponent; if not found, get MoverComponent
	UCharacterMovementComponent* CMC = Owner->FindComponentByClass<UCharacterMovementComponent>();
	if (IsValid(CMC))
	{
		CharacterMovementComponent = CMC;
	}
	else
	{
		MoverComponent = Owner->FindComponentByClass<UMoverComponent>();
	}
}

void UAC_SmartObjectAnimation::SetIgnoreCollisionState(bool bShouldIgnore, AActor* OtherActor)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Get all capsule components on the owner
	TArray<UCapsuleComponent*> Capsules;
	Owner->GetComponents<UCapsuleComponent>(Capsules);

	// Set ignore on each capsule component
	for (UCapsuleComponent* Capsule : Capsules)
	{
		if (Capsule)
		{
			Capsule->IgnoreActorWhenMoving(OtherActor, bShouldIgnore);
		}
	}

	// Also set ignore on the skeletal mesh
	if (OwnerSkeletalMesh)
	{
		OwnerSkeletalMesh->IgnoreActorWhenMoving(OtherActor, bShouldIgnore);
	}
}

void UAC_SmartObjectAnimation::SetIgnoreCharacterMovementCorrection(bool bIgnoreCorrections)
{
	if (UKismetSystemLibrary::IsDedicatedServer(this))
	{
		return;
	}

	if (IsValid(CharacterMovementComponent))
	{
		CharacterMovementComponent->bIgnoreClientMovementErrorChecksAndCorrection = bIgnoreCorrections;
	}
}

void UAC_SmartObjectAnimation::TryAddWarpTarget()
{
	if (!IncomingAnimationPayload.UseWarpTarget)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner) return;

	UMotionWarpingComponent* MotionWarping = Owner->FindComponentByClass<UMotionWarpingComponent>();
	if (MotionWarping)
	{
		MotionWarping->AddOrUpdateWarpTargetFromTransform(
			WarpTargetName,
			IncomingAnimationPayload.WarpTargetTransform
		);
	}
}

void UAC_SmartObjectAnimation::SetupPlayTimer()
{
	if (IncomingAnimationPayload.PlayTime <= 0.0)
	{
		return;
	}

	// Calculate timer duration: PlayTime + random variance in range [-Variance, Variance]
	const double Variance = IncomingAnimationPayload.RandomPlaytimeVariance;
	const double RandomOffset = FMath::FRandRange(-Variance, Variance);
	const double TimerDuration = IncomingAnimationPayload.PlayTime + RandomOffset;

	if (TimerDuration > 0.0)
	{
		GetWorld()->GetTimerManager().SetTimer(
			PlayTimerHandle,
			this,
			&UAC_SmartObjectAnimation::OnPlayTimerExpired,
			static_cast<float>(TimerDuration),
			false
		);
	}
}

void UAC_SmartObjectAnimation::OnPlayTimerExpired()
{
	OnMontageFinishedRequested();
}

void UAC_SmartObjectAnimation::NPCApproachAngleAndDistance(
	const FTransform& Destination,
	double& OutDistance,
	double& OutAngle) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		OutDistance = 0.0;
		OutAngle = 0.0;
		return;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();

	// Distance: length of (OwnerLocation - Destination.Location)
	OutDistance = FVector::Dist(OwnerLocation, Destination.GetLocation());

	// Angle: inverse transform owner location into destination space, normalize, atan2
	const FVector LocalDir = UKismetMathLibrary::InverseTransformLocation(Destination, OwnerLocation);
	const FVector NormalizedDir = LocalDir.GetSafeNormal();
	OutAngle = FMath::RadiansToDegrees(FMath::Atan2(NormalizedDir.Y, NormalizedDir.X));
}

UAnimMontage* UAC_SmartObjectAnimation::EvaluateDistanceAndMotionMatch(
	const FTransform& Destination,
	UObject* ProxyTable,
	double& OutCost,
	double& OutStartTime)
{
	// Cache anim instance
	UAnimInstance* AnimInstance = nullptr;
	if (OwnerSkeletalMesh)
	{
		AnimInstance = OwnerSkeletalMesh->GetAnimInstance();
	}

	// Get pose history from anim instance via BPI_SandboxCharacter_ABP interface
	FPoseHistoryReference PoseHistory;
	if (AnimInstance)
	{
		static const FName GetPoseHistoryFuncName(TEXT("Get_PoseHistory"));
		if (UFunction* Func = AnimInstance->FindFunction(GetPoseHistoryFuncName))
		{
			struct FGetPoseHistoryParams
			{
				FPoseHistoryReference ReturnValue;
			};
			FGetPoseHistoryParams Params;
			AnimInstance->ProcessEvent(Func, &Params);
			PoseHistory = Params.ReturnValue;
		}
	}

	// Calculate approach angle and distance
	double Distance = 0.0;
	double Angle = 0.0;
	NPCApproachAngleAndDistance(Destination, Distance, Angle);

	// Build selection inputs
	SmartObjectSelectionInputs.TargetDistance = Distance;
	SmartObjectSelectionInputs.TargetAngle = Angle;
	SmartObjectSelectionInputs.PoseHistoryNode = PoseHistory;

	// Return outputs from cached selection outputs
	OutCost = SmartObjectSelectionOutputs.Cost;
	OutStartTime = SmartObjectSelectionOutputs.StartTime;

	return nullptr;
}

void UAC_SmartObjectAnimation::PlaySmartObjectMontage(
	const FSmartObjectAnimationPayload& AnimPayload,
	bool bIgnoreCharacterMovementCorrections)
{
	IncomingAnimationPayload = AnimPayload;
	bIgnoreCharacterMovementServerCorrections = bIgnoreCharacterMovementCorrections;

	// Setup play timer (if PlayTime > 0, will call OnMontageFinishedRequested after duration)
	SetupPlayTimer();

	// Play the montage
	PlayCurrentMontage();

	// Add warp target after montage starts
	TryAddWarpTarget();

	// Set ignore character movement correction
	SetIgnoreCharacterMovementCorrection(bIgnoreCharacterMovementServerCorrections);
}

void UAC_SmartObjectAnimation::PlayCurrentMontage()
{
	if (!IncomingAnimationPayload.MontageToPlay)
	{
		return;
	}

	UAnimInstance* AnimInstance = nullptr;

	if (IsMover())
	{
		// For Mover characters, use MoverComponent to play montage
		if (MoverComponent)
		{
			// Play montage via MoverComponent (uses K2Node_PlayMontageOnMoverActor in BP)
			// MoverComponent handles montage playback differently
			if (OwnerSkeletalMesh)
			{
				AnimInstance = OwnerSkeletalMesh->GetAnimInstance();
			}
		}
	}
	else
	{
		// For regular characters, play on skeletal mesh
		if (OwnerSkeletalMesh)
		{
			AnimInstance = OwnerSkeletalMesh->GetAnimInstance();
		}
	}

	if (AnimInstance)
	{
		const float PlayRate = static_cast<float>(IncomingAnimationPayload.Playrate);
		const float StartPos = static_cast<float>(IncomingAnimationPayload.StartTime);

		AnimInstance->Montage_Play(
			IncomingAnimationPayload.MontageToPlay,
			PlayRate,
			EMontagePlayReturnType::MontageLength,
			StartPos
		);

		// Bind blend out delegate for loop handling
		FOnMontageBlendingOutStarted BlendOutDelegate;
		BlendOutDelegate.BindUObject(this, &UAC_SmartObjectAnimation::OnMontageBlendingOut);
		AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, IncomingAnimationPayload.MontageToPlay);
	}
}

void UAC_SmartObjectAnimation::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (bInterrupted)
	{
		// Montage was interrupted, finish
		OnMontageFinishedRequested();
		return;
	}

	// Handle looping
	if (NumberOfLoops > 0)
	{
		NumberOfLoops--;
		PlayCurrentMontage();
	}
	else
	{
		// Loop complete
		SetIgnoreCharacterMovementCorrection(false);
		OnMontageFinishedRequested();
	}
}

void UAC_SmartObjectAnimation::OnMontageFinishedRequested()
{
	// Clear play timer if still active
	if (PlayTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(PlayTimerHandle);
	}

	OwnerMontageFinished.Broadcast();
}
