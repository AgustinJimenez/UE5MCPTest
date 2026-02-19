#include "ABP_GenericRetarget.h"
#include "Components/SkeletalMeshComponent.h"

void UABP_GenericRetarget::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	SetupRetargeterFromComponentTag();
}

void UABP_GenericRetarget::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	SetupRetargeterFromComponentTag();
}

void UABP_GenericRetarget::SetupRetargeterFromComponentTag()
{
	// Get the owning skeletal mesh component's first component tag
	USkeletalMeshComponent* MeshComp = GetSkelMeshComponent();
	if (!MeshComp || MeshComp->ComponentTags.Num() == 0)
	{
		return;
	}

	const FName Tag = MeshComp->ComponentTags[0];

	// Look up the IKRetargeter from the map using the tag
	if (TObjectPtr<UIKRetargeter>* Found = IKRetargeter_Map.Find(Tag))
	{
		IKRetargeter = *Found;
	}

	if (!IKRetargeter)
	{
		return;
	}

	// Copy the retarget profile from the retarget asset
	const FRetargetProfile* CurrentProfile = IKRetargeter->GetCurrentProfile();
	if (CurrentProfile)
	{
		RetargetProfile = *CurrentProfile;
	}

	// Note: The original BP's UpdateRetargetProfile function iterates chain settings
	// and checks a ChainCurveMap for curve-driven blending. However, ChainCurveMap
	// is a function-local variable that is never populated, making the curve blending
	// a no-op in the base class. The profile is used as-is.
}
