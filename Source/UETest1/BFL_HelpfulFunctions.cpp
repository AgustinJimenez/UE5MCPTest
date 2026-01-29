#include "BFL_HelpfulFunctions.h"

#include "Algo/Reverse.h"
#include "DrawDebugLibrary.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	constexpr float kDebugStringHeight = 5.0f;
	constexpr float kLineSpacingStep = 5.0f;
	constexpr float kNamePadding = 1.0f;
	constexpr float kRotationYawOffset = 90.0f;

	FDrawDebugStringSettings MakeDefaultStringSettings()
	{
		FDrawDebugStringSettings Settings;
		Settings.Height = kDebugStringHeight;
		Settings.bMonospaced = true;
		return Settings;
	}

	FDrawDebugLineStyle MakeLineStyle(const FLinearColor& Color)
	{
	FDrawDebugLineStyle LineStyle;
	LineStyle.Color = Color;
	LineStyle.Thickness = 0.5f;
	LineStyle.LineType = EDrawDebugLineType::Solid;
	return LineStyle;
}

	FDrawDebugLineStyle MakeLineStyleWithDefaults(const FLinearColor& Color, float Thickness)
	{
		FDrawDebugLineStyle LineStyle;
		LineStyle.Color = Color;
		LineStyle.Thickness = Thickness;
		LineStyle.LineType = EDrawDebugLineType::Solid;
		return LineStyle;
	}

	FDrawDebugLineStyle MakeAxesLineStyle()
	{
		FDrawDebugLineStyle LineStyle;
		LineStyle.Color = FLinearColor(1.0f, 0.0f, 1.0f, 0.0f);
		LineStyle.Thickness = 0.01f;
		LineStyle.LineType = EDrawDebugLineType::Solid;
		return LineStyle;
	}

	FDrawDebugLineStyle MakeLegendTextStyle()
	{
		FDrawDebugLineStyle LineStyle;
		LineStyle.Color = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f);
		LineStyle.Thickness = 0.0f;
		LineStyle.LineType = EDrawDebugLineType::Solid;
		return LineStyle;
	}

	FRotator MakeTextRotation(const FRotator& Rotation)
	{
		return FRotator(Rotation.Roll, Rotation.Yaw + kRotationYawOffset, Rotation.Pitch);
	}
}

void UBFL_HelpfulFunctions::DrawDebugArrowWithCircle(
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
	double Radius,
	double Length,
	double Size,
	int32 Segments,
	FLinearColor Color,
	double Duration,
	double Thickness)
{
	const FVector Origin = Center + Offset;

	const FVector DirectionNormalized = Direction.GetSafeNormal(0.0001);

	if (DrawCircle)
	{
		const FRotator DirectionRotation = UKismetMathLibrary::Conv_VectorToRotator(Direction);
		const FVector YAxis = UKismetMathLibrary::GetForwardVector(DirectionRotation);
		const FVector ZAxis = UKismetMathLibrary::GetRightVector(DirectionRotation);

		UKismetSystemLibrary::DrawDebugCircle(
			WorldContextObject,
			Origin,
			static_cast<float>(Radius),
			Segments,
			Color,
			static_cast<float>(Duration),
			static_cast<float>(Thickness),
			YAxis,
			ZAxis,
			DrawAxis,
			EDrawDebugSceneDepthPriorityGroup::World);
	}

	FVector ArrowEnd = Origin;
	if (DrawArrow)
	{
		const FVector ArrowStart = Origin + DirectionNormalized * Radius;
		ArrowEnd = ArrowStart + DirectionNormalized * Length;

		UKismetSystemLibrary::DrawDebugArrow(
			WorldContextObject,
			ArrowStart,
			ArrowEnd,
			static_cast<float>(Size),
			Color,
			static_cast<float>(Duration),
			static_cast<float>(Thickness),
			EDrawDebugSceneDepthPriorityGroup::World);
	}

	if (DrawString)
	{
		const FVector TextLocation = ArrowEnd + StringOffset;
		const FLinearColor TextColor = Color * FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

		UKismetSystemLibrary::DrawDebugString(
			WorldContextObject,
			TextLocation,
			String,
			nullptr,
			TextColor,
			static_cast<float>(Duration));
	}
}

void UBFL_HelpfulFunctions::DrawDebugAngleThresholds(
	UObject* WorldContextObject,
	FVector Center,
	FVector Offset,
	FRotator InRotation,
	const TArray<double>& YawAngles,
	double StartRadius,
	double EndRadius,
	FLinearColor Color,
	float Duration,
	float Thickness)
{
	const FVector Origin = Center + Offset;

	for (const double YawAngle : YawAngles)
	{
		const FRotator AdjustedRotation(InRotation.Pitch, InRotation.Yaw + YawAngle, InRotation.Roll);
		const FVector RotationVector = UKismetMathLibrary::Conv_RotatorToVector(AdjustedRotation);

		const FVector LineStart = Origin + RotationVector * StartRadius;
		const FVector LineEnd = Origin + RotationVector * EndRadius;

		UKismetSystemLibrary::DrawDebugLine(
			WorldContextObject,
			LineStart,
			LineEnd,
			Color,
			Duration,
			Thickness,
			EDrawDebugSceneDepthPriorityGroup::World);
	}
}

void UBFL_HelpfulFunctions::AddToStringHistoryArray(
	UObject* WorldContextObject,
	TArray<FString>& InOutValues,
	const FString& NewValue,
	int32 MaxHistoryCount)
{
	InOutValues.Insert(NewValue, 0);

	if (InOutValues.IsValidIndex(MaxHistoryCount))
	{
		InOutValues.RemoveAt(MaxHistoryCount);
	}
}

void UBFL_HelpfulFunctions::DebugDraw_BoolStates(
	UObject* WorldContextObject,
	FVector Location,
	FRotator Rotation,
	FVector Offset,
	const TArray<FString>& BoolNames,
	const TArray<bool>& BoolValues)
{
	const FDrawDebugStringSettings Settings = MakeDefaultStringSettings();
	const FDebugDrawer Drawer = UDrawDebugLibrary::MakeVisualLoggerDebugDrawer(
		WorldContextObject,
		TEXT("Draw Debug Library"),
		EDrawDebugLogVerbosity::Display,
		true,
		true);

	const FVector TextLoc = Location + Rotation.RotateVector(Offset);
	const FRotator TextRot = MakeTextRotation(Rotation);
	const FDrawDebugLineStyle NameStyle = MakeLineStyle(FLinearColor::Black);

	const int32 Count = BoolNames.Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FString& Name = BoolNames[Index];
		const bool bState = BoolValues.IsValidIndex(Index) ? BoolValues[Index] : false;

		const FVector LineOffset = Rotation.RotateVector(FVector(0.0f, 0.0f, static_cast<float>(Index) * kLineSpacingStep));
		const FVector LineLoc = TextLoc - LineOffset;

		UDrawDebugLibrary::DrawDebugString(
			Drawer,
			Name,
			LineLoc,
			TextRot,
			NameStyle,
			false,
			Settings);

		const FVector NameSize = UDrawDebugLibrary::DrawDebugStringDimensions(Name, Settings);
		const FVector ValueOffset = Rotation.RotateVector(FVector(0.0f, NameSize.X + kNamePadding, 0.0f));
		const FVector ValueLoc = LineLoc + ValueOffset;

		const FLinearColor StateColor = bState ? FLinearColor::Green : FLinearColor::Red;
		const FDrawDebugLineStyle StateStyle = MakeLineStyle(StateColor);
		const FString StateString = bState ? TEXT("True") : TEXT("False");

		UDrawDebugLibrary::DrawDebugString(
			Drawer,
			StateString,
			ValueLoc,
			TextRot,
			StateStyle,
			false,
			Settings);
	}
}

void UBFL_HelpfulFunctions::DebugDraw_StringArray(
	UObject* WorldContextObject,
	FVector Location,
	FRotator Rotation,
	FVector Offset,
	const FString& Label,
	const FString& Prefix,
	const TArray<FString>& Strings,
	const FString& HighlightedString,
	const FString& Highlight)
{
	const FDrawDebugStringSettings Settings = MakeDefaultStringSettings();
	const FDebugDrawer Drawer = UDrawDebugLibrary::MakeVisualLoggerDebugDrawer(
		WorldContextObject,
		TEXT("Draw Debug Library"),
		EDrawDebugLogVerbosity::Display,
		true,
		true);
	const FDrawDebugLineStyle LineStyle = MakeLineStyle(FLinearColor::Black);

	const FVector TextLoc = Location + Rotation.RotateVector(Offset);
	const FRotator TextRot = MakeTextRotation(Rotation);

	FString CombinedString;
	if (!Label.IsEmpty())
	{
		CombinedString = Label;
	}

	for (const FString& Item : Strings)
	{
		if (CombinedString.IsEmpty())
		{
			CombinedString = Item;
			continue;
		}

		const bool bHighlighted = Item.Equals(HighlightedString, ESearchCase::IgnoreCase);
		CombinedString += TEXT("\r\n");
		CombinedString += Prefix;
		CombinedString += Item;
		if (bHighlighted)
		{
			CombinedString += Highlight;
		}
	}

	UDrawDebugLibrary::DrawDebugString(
		Drawer,
		CombinedString,
		TextLoc,
		TextRot,
		LineStyle,
		false,
		Settings);
}

void UBFL_HelpfulFunctions::DebugDraw_ObjectNameArray(
	UObject* WorldContextObject,
	FVector Location,
	FRotator Rotation,
	FVector Offset,
	const FString& ArrayLabel,
	const TArray<UObject*>& Objects)
{
	const FDrawDebugStringSettings Settings = MakeDefaultStringSettings();
	const FDebugDrawer Drawer = UDrawDebugLibrary::MakeVisualLoggerDebugDrawer(
		WorldContextObject,
		TEXT("Draw Debug Library"),
		EDrawDebugLogVerbosity::Display,
		true,
		true);
	const FDrawDebugLineStyle LineStyle = MakeLineStyle(FLinearColor::Black);

	const FVector TextLoc = Location + Rotation.RotateVector(Offset);
	const FRotator TextRot = MakeTextRotation(Rotation);

	FString CombinedString = ArrayLabel;
	for (UObject* Object : Objects)
	{
		CombinedString += TEXT("\r\n");
		CombinedString += UKismetSystemLibrary::GetDisplayName(Object);
	}

	UDrawDebugLibrary::DrawDebugString(
		Drawer,
		CombinedString,
		TextLoc,
		TextRot,
		LineStyle,
		false,
		Settings);
}

void UBFL_HelpfulFunctions::DebugDraw_MultiLineGraph(
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
	const TArray<FS_DebugGraphLineProperties>& Lines)
{
	FVector2D EffectiveSize = GraphSize;
	if (EffectiveSize.IsNearlyZero())
	{
		EffectiveSize = FVector2D(XLength, YLength);
	}

	const FVector GraphLoc = Location + Rotation.RotateVector(Offset);
	const FRotator GraphRot = MakeTextRotation(Rotation);

	const FDrawDebugStringSettings AxisLabelSettings = []()
	{
		FDrawDebugStringSettings Settings;
		Settings.Height = kDebugStringHeight;
		Settings.bMonospaced = true;
		return Settings;
	}();

	FDrawDebugGraphAxesSettings AxesSettings;
	AxesSettings.XaxisLabel = XaxisLabel;
	AxesSettings.YaxisLabel = YaxisLabel;
	AxesSettings.AxisLabelSettings = AxisLabelSettings;

	const FDebugDrawer Drawer = UDrawDebugLibrary::MakeVisualLoggerDebugDrawer(
		WorldContextObject,
		TEXT("Draw Debug Library"),
		EDrawDebugLogVerbosity::Display,
		true,
		true);

	const FDrawDebugLineStyle TextLineStyle = MakeLineStyleWithDefaults(FLinearColor::Black, 0.5f);
	const FDrawDebugLineStyle AxesLineStyle = MakeAxesLineStyle();

	UDrawDebugLibrary::DrawDebugGraphAxes(
		Drawer,
		GraphLoc,
		GraphRot,
		EffectiveSize.X,
		EffectiveSize.Y,
		TextLineStyle,
		AxesLineStyle,
		false,
		AxesSettings);

	const FVector2D HalfExtents2D = EffectiveSize / 2.0f;
	const FVector BoxOffset = Rotation.RotateVector(FVector(0.0f, HalfExtents2D.X, HalfExtents2D.Y));
	const FVector BoxLoc = GraphLoc + BoxOffset;
	const FRotator BoxRot = FRotator(GraphRot.Pitch, GraphRot.Yaw, GraphRot.Roll + kRotationYawOffset);
	const FDrawDebugLineStyle BoxStyle = MakeLineStyleWithDefaults(FLinearColor::Black, 0.5f);

	UDrawDebugLibrary::DrawDebugBox(
		Drawer,
		BoxLoc,
		BoxRot,
		BoxStyle,
		false,
		FVector(HalfExtents2D.X, HalfExtents2D.Y, 0.0f));

	TArray<FS_DebugGraphLineProperties> LinesCopy = Lines;
	Algo::Reverse(LinesCopy);

	TArray<FString> LineNames;
	TArray<FLinearColor> LineColors;
	LineNames.Reserve(LinesCopy.Num());
	LineColors.Reserve(LinesCopy.Num());

	for (const FS_DebugGraphLineProperties& Line : LinesCopy)
	{
		LineNames.Add(Line.Name);
		LineColors.Add(Line.Color);

		const int32 NumValues = Line.Values.Num();
		if (NumValues <= 0)
		{
			continue;
		}

		TArray<float> Xvalues;
		UDrawDebugLibrary::MakeLinearlySpacedFloatArray(Xvalues, 0.0f, EffectiveSize.X, NumValues);

		TArray<float> Yvalues;
		Yvalues.Reserve(NumValues);
		for (double Value : Line.Values)
		{
			Yvalues.Add(static_cast<float>(Value));
		}

		const FDrawDebugLineStyle LineStyle = MakeLineStyleWithDefaults(Line.Color, 0.5f);

		UDrawDebugLibrary::DrawDebugGraphLine(
			Drawer,
			GraphLoc,
			GraphRot,
			Xvalues,
			Yvalues,
			0.0f,
			EffectiveSize.X,
			static_cast<float>(MinValue.Y),
			static_cast<float>(MaxValue.Y),
			EffectiveSize.X,
			EffectiveSize.Y,
			LineStyle,
			false);
	}

	const FDrawDebugStringSettings LegendSettings = []()
	{
		FDrawDebugStringSettings Settings;
		Settings.Height = kDebugStringHeight;
		Settings.bMonospaced = true;
		return Settings;
	}();

	const FDrawDebugLineStyle LegendTextStyle = MakeLegendTextStyle();

	UDrawDebugLibrary::DrawDebugGraphLegend(
		Drawer,
		GraphLoc,
		GraphRot,
		LineColors,
		LineNames,
		0.0f,
		EffectiveSize.Y,
		LegendTextStyle,
		LegendTextStyle,
		0.0f,
		false,
		LegendSettings);
}

TArray<FString> UBFL_HelpfulFunctions::GetObjectNames(
	UObject* WorldContextObject,
	const TArray<UObject*>& Objects)
{
	TArray<FString> Names;
	Names.Reserve(Objects.Num());

	for (UObject* Object : Objects)
	{
		Names.Add(UKismetSystemLibrary::GetDisplayName(Object));
	}

	return Names;
}

TSubclassOf<APawn> UBFL_HelpfulFunctions::GetPawnClassWithCVAR(
	UObject* WorldContextObject,
	const TArray<TSubclassOf<APawn>>& PawnClasses,
	TSubclassOf<APawn> DefaultPawnClass)
{
	const int32 Index = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCvar.PawnClass"));
	if (PawnClasses.IsValidIndex(Index))
	{
		const TSubclassOf<APawn> Candidate = PawnClasses[Index];
		if (UKismetSystemLibrary::IsValidClass(Candidate.Get()))
		{
			return Candidate;
		}
	}

	return DefaultPawnClass;
}

TSubclassOf<AActor> UBFL_HelpfulFunctions::GetVisualOverrideWithCVAR(
	UObject* WorldContextObject,
	const TArray<TSubclassOf<AActor>>& VisualOverrides)
{
	const int32 Index = UKismetSystemLibrary::GetConsoleVariableIntValue(TEXT("DDCvar.VisualOverride"));
	if (VisualOverrides.IsValidIndex(Index))
	{
		return VisualOverrides[Index];
	}

	return nullptr;
}
