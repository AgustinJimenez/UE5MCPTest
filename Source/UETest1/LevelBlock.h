#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "S_GridMaterialParams.h"
#include "LevelBlock.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ALevelButton;

UCLASS()
class UETEST1_API ALevelBlock : public AActor
{
	GENERATED_BODY()

public:
	ALevelBlock();

	// Variables from blueprint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelBlock")
	TObjectPtr<ALevelButton> RandomizeButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelBlock")
	FTransform InitialTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelBlock")
	FTransform MinTransformOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelBlock")
	FTransform MaxTransformOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelBlock")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelBlock")
	bool AutoNameFromHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelBlock")
	bool UseLevelVisualsColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelBlock")
	FName ColorGroup;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelBlock")
	FS_GridMaterialParams MaterialParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelBlock")
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelBlock")
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	// Functions
	UFUNCTION(BlueprintCallable, Category = "LevelBlock")
	void UpdateMaterials(const FS_GridMaterialParams& Params);

	UFUNCTION(BlueprintCallable, Category = "LevelBlock")
	void UpdateText();

	UFUNCTION(BlueprintCallable, Category = "LevelBlock")
	void RandomizeOffset();

	UFUNCTION(BlueprintCallable, Category = "LevelBlock")
	void ResetOffset();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	// Cached components
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CachedDefaultSceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CachedStaticMesh;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> CachedTextRender;
};
