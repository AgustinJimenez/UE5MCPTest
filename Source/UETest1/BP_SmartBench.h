#pragma once

#include "CoreMinimal.h"
#include "BP_SmartObject_Base.h"
#include "BP_SmartBench.generated.h"

class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class UETEST1_API ABP_SmartBench : public ABP_SmartObject_Base
{
	GENERATED_BODY()

public:
	ABP_SmartBench();

	// Variable from blueprint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartObject")
	TObjectPtr<USphereComponent> SmartAreaClaimCollisionSphere;

protected:
	virtual void BeginPlay() override;

	// Cached components
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CachedStaticMesh;
};
