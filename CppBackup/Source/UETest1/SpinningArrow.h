#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Curves/RichCurve.h"
#include "SpinningArrow.generated.h"

class UCurveFloat;
class UTimelineComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class UETEST1_API ASpinningArrow : public AActor
{
	GENERATED_BODY()

public:
	ASpinningArrow();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleUpDownUpdate(float Value);

	UFUNCTION()
	void HandleYawUpdate(float Value);

	UFUNCTION()
	void HandleTimelineFinished();

	void CacheComponents();
	void ApplySpinnerTransform();
	void BuildCurve(UCurveFloat* Curve, const TArray<TPair<float, float>>& Keys, ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode) const;

	UPROPERTY(VisibleAnywhere)
	UTimelineComponent* SpinTimeline = nullptr;

	UPROPERTY(Transient)
	UCurveFloat* UpDownCurve = nullptr;

	UPROPERTY(Transient)
	UCurveFloat* YawCurve = nullptr;

	UPROPERTY(Transient)
	USceneComponent* SpinnerComponent = nullptr;

	UPROPERTY(Transient)
	UStaticMeshComponent* ArrowComponent = nullptr;

	float CurrentUpDown = 0.0f;
	float CurrentYaw = 0.0f;
};
