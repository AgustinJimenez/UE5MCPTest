// ABP_Walker.cpp - Animation Blueprint base class for BP_Walker

#include "ABP_Walker.h"
#include "BP_Walker.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UABP_Walker::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// Replicate BlueprintBeginPlay: set Teleport = true, clear it next tick
	bTeleport = true;
	bPendingTeleportClear = true;

	// Cache owning pawn
	if (APawn* Pawn = TryGetPawnOwner())
	{
		CachedWalker = Cast<ABP_Walker>(Pawn);
	}
}

void UABP_Walker::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Handle one-frame teleport clear (mirrors "Delay Until Next Tick" then set false)
	if (bPendingTeleportClear)
	{
		bTeleport = false;
		bPendingTeleportClear = false;
	}

	// Re-cache if needed (e.g. pawn changed)
	if (!CachedWalker.IsValid())
	{
		if (APawn* Pawn = TryGetPawnOwner())
		{
			CachedWalker = Cast<ABP_Walker>(Pawn);
		}
		if (!CachedWalker.IsValid())
		{
			return;
		}
	}

	ABP_Walker* Walker = CachedWalker.Get();

	// Get the camera component via the cached pointer on BP_Walker.
	// BP_Walker.h exposes CachedCamera as a protected UPROPERTY(Transient).
	// We access it via reflection since it is protected.
	UCameraComponent* Camera = nullptr;
	if (FObjectProperty* CamProp = CastField<FObjectProperty>(Walker->GetClass()->FindPropertyByName(TEXT("CachedCamera"))))
	{
		Camera = Cast<UCameraComponent>(CamProp->GetObjectPropertyValue(CamProp->ContainerPtrToValuePtr<void>(Walker)));
	}

	// If no C++ CachedCamera, try BP "Camera" variable
	if (!Camera)
	{
		if (FObjectProperty* CamProp = CastField<FObjectProperty>(Walker->GetClass()->FindPropertyByName(TEXT("Camera"))))
		{
			Camera = Cast<UCameraComponent>(CamProp->GetObjectPropertyValue(CamProp->ContainerPtrToValuePtr<void>(Walker)));
		}
	}

	if (!Camera)
	{
		return;
	}

	// Replicate the BP camera aim target computation:
	// 1. Get control rotation forward vector
	const FRotator ControlRot = Walker->GetControlRotation();
	const FVector Forward = UKismetMathLibrary::GetForwardVector(ControlRot);

	// 2. Aim world position = CameraWorldLocation + Forward * 10000
	const FVector CameraWorldLoc = Camera->GetComponentLocation();
	const FVector AimWorldPos = CameraWorldLoc + (Forward * 10000.0);

	// 3. Transform aim position into skeletal mesh local space
	USkeletalMeshComponent* Mesh = Walker->GetMesh();
	if (Mesh)
	{
		const FTransform MeshWorldTransform = Mesh->GetComponentTransform();
		CameraAimTarget = MeshWorldTransform.InverseTransformPosition(AimWorldPos);
	}
}
