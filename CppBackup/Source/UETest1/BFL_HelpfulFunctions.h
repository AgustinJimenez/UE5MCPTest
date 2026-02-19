#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "S_DebugGraphLineProperties.h"
#include "BFL_HelpfulFunctions.generated.h"

UCLASS()
class UETEST1_API UBFL_HelpfulFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void DrawDebugArrowWithCircle(
		UObject* WorldContextObject,
		bool DrawCircle,
		bool DrawAxis,
		bool DrawArrow,
		bool DrawString,
		const FString& String,
		FVector StringOffset,
		FVector Center,
		FVector Direction,
		FVector Offset,
		double Radius = 0.0,
		double Length = 50.0,
		double Size = 50.0,
		int32 Segments = 100,
		FLinearColor Color = FLinearColor::White,
		double Duration = 0.0,
		double Thickness = 1.0);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void DrawDebugAngleThresholds(
		UObject* WorldContextObject,
		FVector Center,
		FVector Offset,
		FRotator InRotation,
		const TArray<double>& YawAngles,
		double StartRadius = 0.0,
		double EndRadius = 0.0,
		FLinearColor Color = FLinearColor::White,
		float Duration = 0.0f,
		float Thickness = 1.0f);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void AddToStringHistoryArray(
		UObject* WorldContextObject,
		UPARAM(ref) TArray<FString>& InOutValues,
		const FString& NewValue,
		int32 MaxHistoryCount);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void DebugDraw_BoolStates(
		UObject* WorldContextObject,
		FVector Location,
		FRotator Rotation,
		FVector Offset,
		const TArray<FString>& BoolNames,
		const TArray<bool>& BoolValues);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void DebugDraw_StringArray(
		UObject* WorldContextObject,
		FVector Location,
		FRotator Rotation,
		FVector Offset,
		const FString& Label,
		const FString& Prefix,
		const TArray<FString>& Strings,
		const FString& HighlightedString,
		const FString& Highlight);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void DebugDraw_ObjectNameArray(
		UObject* WorldContextObject,
		FVector Location,
		FRotator Rotation,
		FVector Offset,
		const FString& ArrayLabel,
		const TArray<UObject*>& Objects);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void DebugDraw_MultiLineGraph(
		UObject* WorldContextObject,
		FVector Location,
		FRotator Rotation,
		FVector Offset,
		float XLength,
		float YLength,
		FVector2D GraphSize,
		FVector2D MinValue,
		FVector2D MaxValue,
		const FString& XaxisLabel,
		const FString& YaxisLabel,
		const TArray<FS_DebugGraphLineProperties>& Lines);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static TArray<FString> GetObjectNames(
		UObject* WorldContextObject,
		const TArray<UObject*>& Objects);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static TSubclassOf<class APawn> GetPawnClassWithCVAR(
		UObject* WorldContextObject,
		const TArray<TSubclassOf<class APawn>>& PawnClasses,
		TSubclassOf<class APawn> DefaultPawnClass);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static TSubclassOf<class AActor> GetVisualOverrideWithCVAR(
		UObject* WorldContextObject,
		const TArray<TSubclassOf<class AActor>>& VisualOverrides);
};
