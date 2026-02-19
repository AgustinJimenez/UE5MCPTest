#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/DecalComponent.h"
#include "S_LevelStyle.h"
#include "LevelVisuals.generated.h"

UCLASS()
class UETEST1_API ALevelVisuals : public AActor
{
	GENERATED_BODY()

public:
	ALevelVisuals();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Visuals")
	int32 StyleIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Visuals")
	TArray<FS_LevelStyle> LevelStyles;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Visuals")
	TObjectPtr<class ALandscape> Landscape;

	UFUNCTION(BlueprintCallable, Category = "Level Visuals")
	void UpdateLevelVisuals();

	UFUNCTION(BlueprintCallable, Category = "Level Visuals")
	void SetLevelStyle(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Level Visuals")
	void UpdateVisuals();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Level Visuals")
	FS_LevelStyle GetLevelStyle() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	// Cached components (retrieved by name at BeginPlay)
	UPROPERTY()
	TObjectPtr<USceneComponent> CachedScene;

	UPROPERTY()
	TObjectPtr<USkyLightComponent> CachedSkyLight;

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> CachedDirectionalLight;

	UPROPERTY()
	TObjectPtr<UExponentialHeightFogComponent> CachedExponentialHeightFog;

	UPROPERTY()
	TObjectPtr<UPostProcessComponent> CachedPostProcess;

	UPROPERTY()
	TObjectPtr<UDecalComponent> CachedDecal;
};
