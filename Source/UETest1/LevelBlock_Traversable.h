#pragma once

#include "CoreMinimal.h"
#include "LevelBlock.h"
#include "Components/SplineComponent.h"
#include "LevelBlock_Traversable.generated.h"

UCLASS()
class UETEST1_API ALevelBlock_Traversable : public ALevelBlock
{
	GENERATED_BODY()

public:
	ALevelBlock_Traversable();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversable")
	TArray<TObjectPtr<USplineComponent>> Ledges;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversable")
	TMap<TObjectPtr<USplineComponent>, TObjectPtr<USplineComponent>> OppositeLedges;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversable")
	double MinLedgeWidth;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	// Spline components for ledges
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversable", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineComponent> Ledge_1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversable", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineComponent> Ledge_2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversable", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineComponent> Ledge_3;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traversable", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineComponent> Ledge_4;
};
