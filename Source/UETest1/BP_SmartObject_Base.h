#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BP_SmartObject_Base.generated.h"

class USmartObjectComponent;

UCLASS()
class UETEST1_API ABP_SmartObject_Base : public AActor
{
	GENERATED_BODY()

public:
	ABP_SmartObject_Base();

protected:
	virtual void BeginPlay() override;

	// Cached components (renamed to avoid collision with blueprint components)
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CachedDefaultSceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<USmartObjectComponent> CachedSmartObject;
};
