#include "SpinningArrow.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"

ASpinningArrow::ASpinningArrow()
{
	PrimaryActorTick.bCanEverTick = false;

	SpinTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("Timeline"));
}

void ASpinningArrow::BeginPlay()
{
	Super::BeginPlay();

	CacheComponents();

	if (!UpDownCurve)
	{
		UpDownCurve = NewObject<UCurveFloat>(this, TEXT("Timeline_UpDown_Curve"));
		if (UpDownCurve)
		{
			BuildCurve(UpDownCurve, {
				{0.0f, 0.0f},
				{1.0f, 1.0f},
				{2.0f, 0.0f},
			}, RCIM_Cubic, RCTM_User);
		}
	}

	if (!YawCurve)
	{
		YawCurve = NewObject<UCurveFloat>(this, TEXT("Timeline_Yaw_Curve"));
		if (YawCurve)
		{
			BuildCurve(YawCurve, {
				{0.0f, 0.0f},
				{1.0f, 180.0f},
				{2.0f, 360.0f},
			}, RCIM_Linear, RCTM_Auto);
		}
	}

	if (SpinTimeline)
	{
		SpinTimeline->SetLooping(false);
		SpinTimeline->SetTimelineLengthMode(ETimelineLengthMode::TL_TimelineLength);
		SpinTimeline->SetTimelineLength(2.0f);

		if (UpDownCurve)
		{
			FOnTimelineFloat UpDownDelegate;
			UpDownDelegate.BindUFunction(this, FName("HandleUpDownUpdate"));
			SpinTimeline->AddInterpFloat(UpDownCurve, UpDownDelegate, FName("Up/Down"));
		}

		if (YawCurve)
		{
			FOnTimelineFloat YawDelegate;
			YawDelegate.BindUFunction(this, FName("HandleYawUpdate"));
			SpinTimeline->AddInterpFloat(YawCurve, YawDelegate, FName("Yaw"));
		}

		FOnTimelineEvent FinishedDelegate;
		FinishedDelegate.BindUFunction(this, FName("HandleTimelineFinished"));
		SpinTimeline->SetTimelineFinishedFunc(FinishedDelegate);

		SpinTimeline->Play();
	}
}

void ASpinningArrow::HandleUpDownUpdate(float Value)
{
	CurrentUpDown = Value;
	ApplySpinnerTransform();
}

void ASpinningArrow::HandleYawUpdate(float Value)
{
	CurrentYaw = Value;
	ApplySpinnerTransform();
}

void ASpinningArrow::HandleTimelineFinished()
{
	if (SpinTimeline)
	{
		SpinTimeline->PlayFromStart();
	}
}

void ASpinningArrow::CacheComponents()
{
	if (SpinnerComponent && ArrowComponent)
	{
		return;
	}

	TInlineComponentArray<USceneComponent*> SceneComponents;
	GetComponents(SceneComponents);
	for (USceneComponent* Component : SceneComponents)
	{
		if (!Component)
		{
			continue;
		}

		if (!SpinnerComponent && Component->GetFName() == TEXT("Spinner"))
		{
			SpinnerComponent = Component;
			continue;
		}

		if (!ArrowComponent && Component->GetFName() == TEXT("Arrow"))
		{
			ArrowComponent = Cast<UStaticMeshComponent>(Component);
		}
	}
}

void ASpinningArrow::ApplySpinnerTransform()
{
	if (!SpinnerComponent)
	{
		CacheComponents();
		if (!SpinnerComponent)
		{
			return;
		}
	}

	if (!SpinnerComponent)
	{
		return;
	}

	FVector Location = FVector::ZeroVector;
	Location.Z = CurrentUpDown * 25.0f;
	FRotator Rotation = FRotator(0.0f, CurrentYaw, 0.0f);
	SpinnerComponent->SetRelativeLocationAndRotation(Location, Rotation);
}

void ASpinningArrow::BuildCurve(UCurveFloat* Curve, const TArray<TPair<float, float>>& Keys, ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode) const
{
	if (!Curve)
	{
		return;
	}

	FRichCurve& RichCurve = Curve->FloatCurve;
	RichCurve.Reset();
	for (const TPair<float, float>& Key : Keys)
	{
		const FKeyHandle Handle = RichCurve.AddKey(Key.Key, Key.Value);
		FRichCurveKey& RichKey = RichCurve.GetKey(Handle);
		RichKey.InterpMode = InterpMode;
		RichKey.TangentMode = TangentMode;
	}
}
