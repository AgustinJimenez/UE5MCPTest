#include "MCPServer.h"
#include "Engine/Blueprint.h"
#include "Animation/AnimBlueprint.h"
#include "WidgetBlueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Blueprint/BlueprintExtension.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "BlueprintEditorLibrary.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_SetFieldsInStruct.h"
#include "EdGraphSchema_K2.h"
#include "Engine/UserDefinedStruct.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/TimelineTemplate.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "ObjectTools.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/RichCurve.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Async/Async.h"
#include "UObject/SavePackage.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "Components/ActorComponent.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "LevelVisuals.h"


FString FMCPServer::HandleListBlueprints(const TSharedPtr<FJsonObject>& Params)
{
	FString PathFilter = TEXT("/Game/");
	if (Params.IsValid() && Params->HasField(TEXT("path")))
	{
		PathFilter = Params->GetStringField(TEXT("path"));
	}

	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> Assets;
	AssetRegistry.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), Assets);

	// Also include Animation Blueprints
	TArray<FAssetData> AnimAssets;
	AssetRegistry.Get().GetAssetsByClass(UAnimBlueprint::StaticClass()->GetClassPathName(), AnimAssets);
	Assets.Append(AnimAssets);

	TArray<TSharedPtr<FJsonValue>> BlueprintArray;
	for (const FAssetData& Asset : Assets)
	{
		FString PackagePath = Asset.PackagePath.ToString();
		if (PackagePath.StartsWith(PathFilter))
		{
			TSharedPtr<FJsonObject> BPObj = MakeShared<FJsonObject>();
			BPObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
			BPObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
			BlueprintArray.Add(MakeShared<FJsonValueObject>(BPObj));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("blueprints"), BlueprintArray);
	Data->SetNumberField(TEXT("count"), BlueprintArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleCheckAllBlueprints(const TSharedPtr<FJsonObject>& Params)
{
	FString PathFilter = TEXT("/Game/");
	if (Params.IsValid() && Params->HasField(TEXT("path")))
	{
		PathFilter = Params->GetStringField(TEXT("path"));
	}

	bool bIncludeWarnings = false;
	if (Params.IsValid() && Params->HasField(TEXT("include_warnings")))
	{
		bIncludeWarnings = Params->GetBoolField(TEXT("include_warnings"));
	}

	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> Assets;
	AssetRegistry.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), Assets);

	// Also include Animation Blueprints
	TArray<FAssetData> AnimAssets;
	AssetRegistry.Get().GetAssetsByClass(UAnimBlueprint::StaticClass()->GetClassPathName(), AnimAssets);
	Assets.Append(AnimAssets);

	// Also include Widget Blueprints (UMG widgets, EditorUtilityWidgets)
	TArray<FAssetData> WidgetAssets;
	AssetRegistry.Get().GetAssetsByClass(UWidgetBlueprint::StaticClass()->GetClassPathName(), WidgetAssets);
	Assets.Append(WidgetAssets);

	TArray<TSharedPtr<FJsonValue>> BlueprintsWithErrors;
	int32 TotalChecked = 0;
	int32 TotalErrors = 0;
	int32 TotalWarnings = 0;

	for (const FAssetData& Asset : Assets)
	{
		FString PackagePath = Asset.PackagePath.ToString();
		if (!PackagePath.StartsWith(PathFilter))
		{
			continue;
		}

		FString BlueprintPath = Asset.GetObjectPathString();
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);

		if (!Blueprint)
		{
			continue;
		}

		TotalChecked++;

		// Compile the blueprint
		FCompilerResultsLog CompileLog;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog);

		// Count errors and warnings
		int32 ErrorCount = 0;
		int32 WarningCount = 0;
		TArray<TSharedPtr<FJsonValue>> ErrorsArray;
		TArray<TSharedPtr<FJsonValue>> WarningsArray;

		for (const TSharedRef<FTokenizedMessage>& Message : CompileLog.Messages)
		{
			if (Message->GetSeverity() == EMessageSeverity::Error)
			{
				ErrorCount++;
				TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
				MsgObj->SetStringField(TEXT("message"), Message->ToText().ToString());
				ErrorsArray.Add(MakeShared<FJsonValueObject>(MsgObj));
			}
			else if (Message->GetSeverity() == EMessageSeverity::Warning)
			{
				WarningCount++;
				TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
				MsgObj->SetStringField(TEXT("message"), Message->ToText().ToString());
				WarningsArray.Add(MakeShared<FJsonValueObject>(MsgObj));
			}
		}

		TotalErrors += ErrorCount;
		TotalWarnings += WarningCount;

		// Only include blueprints with errors (or warnings if requested)
		if (ErrorCount > 0 || (bIncludeWarnings && WarningCount > 0))
		{
			TSharedPtr<FJsonObject> BPObj = MakeShared<FJsonObject>();
			BPObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
			BPObj->SetStringField(TEXT("path"), BlueprintPath);
			BPObj->SetNumberField(TEXT("error_count"), ErrorCount);
			BPObj->SetNumberField(TEXT("warning_count"), WarningCount);
			BPObj->SetArrayField(TEXT("errors"), ErrorsArray);
			if (bIncludeWarnings)
			{
				BPObj->SetArrayField(TEXT("warnings"), WarningsArray);
			}
			BlueprintsWithErrors.Add(MakeShared<FJsonValueObject>(BPObj));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("total_checked"), TotalChecked);
	Data->SetNumberField(TEXT("total_errors"), TotalErrors);
	Data->SetNumberField(TEXT("total_warnings"), TotalWarnings);
	Data->SetNumberField(TEXT("blueprints_with_issues"), BlueprintsWithErrors.Num());
	Data->SetArrayField(TEXT("blueprints"), BlueprintsWithErrors);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));
	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);

	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Blueprint->GetName());
	Data->SetStringField(TEXT("path"), Blueprint->GetPathName());

	if (Blueprint->ParentClass)
	{
		Data->SetStringField(TEXT("parent_class"), Blueprint->ParentClass->GetName());
	}

	// Count variables, components, graphs
	Data->SetNumberField(TEXT("variable_count"), Blueprint->NewVariables.Num());

	int32 ComponentCount = 0;
	if (Blueprint->SimpleConstructionScript)
	{
		ComponentCount = Blueprint->SimpleConstructionScript->GetAllNodes().Num();
	}
	Data->SetNumberField(TEXT("component_count"), ComponentCount);
	Data->SetNumberField(TEXT("graph_count"), Blueprint->UbergraphPages.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadVariables(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));
	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);

	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> VarArray;
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		TSharedPtr<FJsonObject> VarObj = MakeShared<FJsonObject>();
		VarObj->SetStringField(TEXT("name"), Var.VarName.ToString());
		VarObj->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());

		if (Var.VarType.PinSubCategoryObject.IsValid())
		{
			VarObj->SetStringField(TEXT("subtype"), Var.VarType.PinSubCategoryObject->GetName());
		}

		VarObj->SetBoolField(TEXT("is_array"), Var.VarType.IsArray());
		VarObj->SetBoolField(TEXT("is_instance_editable"), Var.HasMetaData(TEXT("ExposeOnSpawn")) || (Var.PropertyFlags & CPF_Edit) != 0);
		VarObj->SetBoolField(TEXT("is_blueprint_read_only"), (Var.PropertyFlags & CPF_BlueprintReadOnly) != 0);

		VarArray.Add(MakeShared<FJsonValueObject>(VarObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("variables"), VarArray);
	Data->SetNumberField(TEXT("count"), VarArray.Num());

	return MakeResponse(true, Data);
}

/**
 * HandleReadClassDefaults - Read Blueprint Class Default Object (CDO) properties
 *
 * CRITICAL LIMITATION (2026-02-03):
 * This function reads the Blueprint Class Default Object, which contains class-level default values.
 * It does NOT read property overrides set on level actor instances in the Details panel.
 *
 * WHY THIS MATTERS:
 * - When you place a Blueprint actor in a level and modify properties in the Details panel,
 *   those overrides are stored in the LEVEL FILE (.umap), not the Blueprint asset (.uasset)
 * - Blueprint CDO values are often "first iteration" or placeholder values
 * - The actual working behavior typically comes from level instance overrides
 * - Using CDO values for C++ conversion can result in incorrect implementations
 *
 * WHEN TO USE EACH COMMAND:
 * - read_class_defaults: Reading Blueprint schema (available properties, types, metadata)
 * - read_actor_properties: Reading actual working values from a level instance
 *
 * EXAMPLE - LevelVisuals FogColor:
 * - CDO value (from this function): Light purple (0.79, 0.75, 1.0) - WRONG for dark mode
 * - Level instance value: Dark gray (0.261, 0.261, 0.302) - CORRECT working value
 *
 * SOLUTION WORKFLOW:
 * When copying reference data from a working project:
 * 1. Copy the LEVEL FILE (.umap), not just Blueprint assets
 * 2. Open the level in Unreal Editor
 * 3. Use read_actor_properties to get actual instance values
 * 4. Use those values in your C++ implementation
 */
FString FMCPServer::HandleReadClassDefaults(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));
	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);

	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	// Get the generated class and its CDO
	// NOTE: This is the class-level CDO, not a level instance with property overrides!
	UClass* GeneratedClass = Blueprint->GeneratedClass;
	if (!GeneratedClass)
	{
		return MakeError(TEXT("Blueprint has no generated class"));
	}

	UObject* CDO = GeneratedClass->GetDefaultObject();
	if (!CDO)
	{
		return MakeError(TEXT("Failed to get class default object"));
	}

	TArray<TSharedPtr<FJsonValue>> PropertyArray;

	// Iterate through all properties (including inherited ones)
	for (TFieldIterator<FProperty> PropIt(GeneratedClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		// Skip properties that are not editable or visible
		if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			continue;
		}

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Property->GetName());
		PropObj->SetStringField(TEXT("type"), Property->GetCPPType());
		PropObj->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));

		// Get the property value from CDO
		void* PropertyValue = Property->ContainerPtrToValuePtr<void>(CDO);
		FString ValueString;
		Property->ExportTextItem_Direct(ValueString, PropertyValue, nullptr, nullptr, PPF_None);
		PropObj->SetStringField(TEXT("value"), ValueString);

		// Check if it's an object property
		if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
		{
			UObject* ObjectValue = ObjectProp->GetObjectPropertyValue(PropertyValue);
			if (ObjectValue)
			{
				PropObj->SetStringField(TEXT("object_value"), ObjectValue->GetPathName());
				PropObj->SetStringField(TEXT("object_class"), ObjectValue->GetClass()->GetName());
			}
		}
		// Check if it's an array property
		else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, PropertyValue);
			PropObj->SetNumberField(TEXT("array_size"), ArrayHelper.Num());

			// For object arrays, list the objects
			if (FObjectProperty* InnerObjectProp = CastField<FObjectProperty>(ArrayProp->Inner))
			{
				TArray<TSharedPtr<FJsonValue>> ObjectArray;
				for (int32 i = 0; i < ArrayHelper.Num(); ++i)
				{
					UObject* ArrayObject = InnerObjectProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(i));
					if (ArrayObject)
					{
						TSharedPtr<FJsonObject> ArrayObjData = MakeShared<FJsonObject>();
						ArrayObjData->SetStringField(TEXT("path"), ArrayObject->GetPathName());
						ArrayObjData->SetStringField(TEXT("class"), ArrayObject->GetClass()->GetName());
						ObjectArray.Add(MakeShared<FJsonValueObject>(ArrayObjData));
					}
				}
				PropObj->SetArrayField(TEXT("array_values"), ObjectArray);
			}
		}

		PropertyArray.Add(MakeShared<FJsonValueObject>(PropObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("properties"), PropertyArray);
	Data->SetNumberField(TEXT("count"), PropertyArray.Num());
	Data->SetStringField(TEXT("class_name"), GeneratedClass->GetName());
	Data->SetStringField(TEXT("parent_class"), GeneratedClass->GetSuperClass() ? GeneratedClass->GetSuperClass()->GetName() : TEXT("None"));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadComponents(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));
	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);

	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> CompArray;

	if (Blueprint->SimpleConstructionScript)
	{
		const TArray<USCS_Node*>& Nodes = Blueprint->SimpleConstructionScript->GetAllNodes();
		for (USCS_Node* Node : Nodes)
		{
			if (!Node) continue;

			TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
			CompObj->SetStringField(TEXT("name"), Node->GetVariableName().ToString());

			if (Node->ComponentClass)
			{
				CompObj->SetStringField(TEXT("class"), Node->ComponentClass->GetName());
			}

			if (Node->ComponentTemplate)
			{
				CompObj->SetStringField(TEXT("template_name"), Node->ComponentTemplate->GetName());
			}

			// Get parent
			if (USCS_Node* Parent = Blueprint->SimpleConstructionScript->FindParentNode(Node))
			{
				CompObj->SetStringField(TEXT("parent"), Parent->GetVariableName().ToString());
			}

			CompArray.Add(MakeShared<FJsonValueObject>(CompObj));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("components"), CompArray);
	Data->SetNumberField(TEXT("count"), CompArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadComponentProperties(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")) || !Params->HasField(TEXT("component_name")))
	{
		return MakeError(TEXT("Missing 'path' or 'component_name' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	if (!Blueprint->SimpleConstructionScript)
	{
		return MakeError(TEXT("Blueprint has no SimpleConstructionScript"));
	}

	// Find the component node
	USCS_Node* TargetNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			TargetNode = Node;
			break;
		}
	}

	if (!TargetNode || !TargetNode->ComponentTemplate)
	{
		return MakeError(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}

	UObject* ComponentTemplate = TargetNode->ComponentTemplate;
	UClass* ComponentClass = ComponentTemplate->GetClass();

	TArray<TSharedPtr<FJsonValue>> PropsArray;

	// Iterate through all properties
	for (TFieldIterator<FProperty> PropIt(ComponentClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property) continue;

		// Skip properties that aren't editable
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Property->GetName());
		PropObj->SetStringField(TEXT("type"), Property->GetClass()->GetName());

		// Get property category
		FString Category = Property->GetMetaData(TEXT("Category"));
		if (!Category.IsEmpty())
		{
			PropObj->SetStringField(TEXT("category"), Category);
		}

		// Get property value as string
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ComponentTemplate);
		FString ValueStr;
		Property->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, ComponentTemplate, PPF_None);
		PropObj->SetStringField(TEXT("value"), ValueStr);

		// For object/class properties, also export the object path
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
		{
			UObject* ObjValue = ObjProp->GetObjectPropertyValue(ValuePtr);
			if (ObjValue)
			{
				PropObj->SetStringField(TEXT("object_value"), ObjValue->GetPathName());
				PropObj->SetStringField(TEXT("object_class"), ObjValue->GetClass()->GetName());
			}
		}
		else if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
		{
			UClass* ClassValue = Cast<UClass>(ClassProp->GetObjectPropertyValue(ValuePtr));
			if (ClassValue)
			{
				PropObj->SetStringField(TEXT("class_value"), ClassValue->GetPathName());
				PropObj->SetStringField(TEXT("class_name"), ClassValue->GetName());
			}
		}

		PropsArray.Add(MakeShared<FJsonValueObject>(PropObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("properties"), PropsArray);
	Data->SetNumberField(TEXT("count"), PropsArray.Num());
	Data->SetStringField(TEXT("component_name"), ComponentName);
	Data->SetStringField(TEXT("component_class"), ComponentClass->GetName());

	return MakeResponse(true, Data);
}

static void SerializePinType(const FEdGraphPinType& PinType, TSharedPtr<FJsonObject>& OutObj);

FString FMCPServer::HandleReadEventGraph(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));
	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);

	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> GraphsArray;

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph) continue;

		TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
		GraphObj->SetStringField(TEXT("name"), Graph->GetName());

		TArray<TSharedPtr<FJsonValue>> NodesArray;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
			NodeObj->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
			NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
			NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

			// Get node-specific info
			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("Event"));
				NodeObj->SetStringField(TEXT("event_name"), EventNode->GetFunctionName().ToString());
			}
			else if (UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("FunctionCall"));
				NodeObj->SetStringField(TEXT("function_name"), FuncNode->GetFunctionName().ToString());
			}
			else if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("VariableGet"));
				NodeObj->SetStringField(TEXT("variable_name"), GetNode->GetVarName().ToString());
			}
			else if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("VariableSet"));
				NodeObj->SetStringField(TEXT("variable_name"), SetNode->GetVarName().ToString());
			}
			else
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("Other"));
			}

			// Pin defaults (useful for nodes like Delay)
			TArray<TSharedPtr<FJsonValue>> PinsArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;

				TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
				PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
				PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));

				TSharedPtr<FJsonObject> PinTypeObj = MakeShared<FJsonObject>();
				SerializePinType(Pin->PinType, PinTypeObj);
				PinObj->SetObjectField(TEXT("type"), PinTypeObj);

				if (!Pin->DefaultValue.IsEmpty())
				{
					PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
				}

				if (!Pin->DefaultTextValue.IsEmpty())
				{
					PinObj->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
				}

				if (Pin->DefaultObject)
				{
					PinObj->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
				}

				PinObj->SetBoolField(TEXT("is_linked"), Pin->LinkedTo.Num() > 0);

				PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
			}
			NodeObj->SetArrayField(TEXT("pins"), PinsArray);

			// Get pin connections
			TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output) continue;

				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin || !LinkedPin->GetOwningNode()) continue;

					TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
					ConnObj->SetStringField(TEXT("from_pin"), Pin->PinName.ToString());
					ConnObj->SetStringField(TEXT("to_node"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
					ConnObj->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
					ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
				}
			}
			NodeObj->SetArrayField(TEXT("connections"), ConnectionsArray);

			NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
		}

		GraphObj->SetArrayField(TEXT("nodes"), NodesArray);
		GraphObj->SetNumberField(TEXT("node_count"), NodesArray.Num());
		GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("graphs"), GraphsArray);
	Data->SetNumberField(TEXT("count"), GraphsArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadEventGraphDetailed(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));
	const int32 MaxNodes = Params->HasField(TEXT("max_nodes")) ? Params->GetIntegerField(TEXT("max_nodes")) : -1;
	const int32 StartIndex = Params->HasField(TEXT("start_index")) ? Params->GetIntegerField(TEXT("start_index")) : 0;
	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);

	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> GraphsArray;

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph) continue;

		TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
		GraphObj->SetStringField(TEXT("name"), Graph->GetName());

		TArray<TSharedPtr<FJsonValue>> NodesArray;
		int32 NodeIndex = 0;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			if (NodeIndex++ < StartIndex) continue;
			if (MaxNodes >= 0 && NodesArray.Num() >= MaxNodes) break;

			TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
			NodeObj->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
			NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
			NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("Event"));
				NodeObj->SetStringField(TEXT("event_name"), EventNode->GetFunctionName().ToString());
			}
			else if (UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("FunctionCall"));
				NodeObj->SetStringField(TEXT("function_name"), FuncNode->GetFunctionName().ToString());
			}
			else if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("VariableGet"));
				NodeObj->SetStringField(TEXT("variable_name"), GetNode->GetVarName().ToString());
			}
			else if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("VariableSet"));
				NodeObj->SetStringField(TEXT("variable_name"), SetNode->GetVarName().ToString());
			}
			else
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("Other"));
			}

			TArray<TSharedPtr<FJsonValue>> PinsArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;

				TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
				PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
				PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));

				TSharedPtr<FJsonObject> PinTypeObj = MakeShared<FJsonObject>();
				SerializePinType(Pin->PinType, PinTypeObj);
				PinObj->SetObjectField(TEXT("type"), PinTypeObj);

				if (!Pin->DefaultValue.IsEmpty())
				{
					PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
				}

				if (!Pin->DefaultTextValue.IsEmpty())
				{
					PinObj->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
				}

				if (Pin->DefaultObject)
				{
					PinObj->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
				}

				PinObj->SetBoolField(TEXT("is_linked"), Pin->LinkedTo.Num() > 0);

				PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
			}
			NodeObj->SetArrayField(TEXT("pins"), PinsArray);

			TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output) continue;

				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin || !LinkedPin->GetOwningNode()) continue;

					TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
					ConnObj->SetStringField(TEXT("from_pin"), Pin->PinName.ToString());
					ConnObj->SetStringField(TEXT("to_node"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
					ConnObj->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
					ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
				}
			}
			NodeObj->SetArrayField(TEXT("connections"), ConnectionsArray);

			NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
		}

		GraphObj->SetArrayField(TEXT("nodes"), NodesArray);
		GraphObj->SetNumberField(TEXT("node_count"), NodesArray.Num());
		GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("graphs"), GraphsArray);
	Data->SetNumberField(TEXT("count"), GraphsArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadFunctionGraphs(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));
	const FString FilterName = Params->HasField(TEXT("name")) ? Params->GetStringField(TEXT("name")) : FString();
	const int32 MaxNodes = Params->HasField(TEXT("max_nodes")) ? Params->GetIntegerField(TEXT("max_nodes")) : -1;
	const int32 StartIndex = Params->HasField(TEXT("start_index")) ? Params->GetIntegerField(TEXT("start_index")) : 0;
	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);

	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> GraphsArray;

	// Collect all function graphs: regular + interface implementation graphs
	TArray<UEdGraph*> AllFunctionGraphs;
	AllFunctionGraphs.Append(Blueprint->FunctionGraphs);
	for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
	{
		AllFunctionGraphs.Append(Interface.Graphs);
	}

	for (UEdGraph* Graph : AllFunctionGraphs)
	{
		if (!Graph) continue;
		if (!FilterName.IsEmpty() && Graph->GetName() != FilterName) continue;

		TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
		GraphObj->SetStringField(TEXT("name"), Graph->GetName());
		GraphObj->SetStringField(TEXT("graph_type"), TEXT("Function"));

		TArray<TSharedPtr<FJsonValue>> NodesArray;
		int32 NodeIndex = 0;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			if (NodeIndex++ < StartIndex) continue;
			if (MaxNodes >= 0 && NodesArray.Num() >= MaxNodes) break;

			TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
			NodeObj->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
			NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
			NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("Event"));
				NodeObj->SetStringField(TEXT("event_name"), EventNode->GetFunctionName().ToString());
			}
			else if (UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("FunctionCall"));
				NodeObj->SetStringField(TEXT("function_name"), FuncNode->GetFunctionName().ToString());
			}
			else if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("VariableGet"));
				NodeObj->SetStringField(TEXT("variable_name"), GetNode->GetVarName().ToString());
			}
			else if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("VariableSet"));
				NodeObj->SetStringField(TEXT("variable_name"), SetNode->GetVarName().ToString());
			}
			else if (Cast<UK2Node_FunctionEntry>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("FunctionEntry"));
			}
			else if (Cast<UK2Node_FunctionResult>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("FunctionResult"));
			}
			else
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("Other"));
			}

			// For K2Node_PropertyAccess, extract the property path via reflection
			if (Node->GetClass()->GetName() == TEXT("K2Node_PropertyAccess"))
			{
				// Get TextPath (FText) via reflection - avoids needing private header include
				FTextProperty* TextPathProp = CastField<FTextProperty>(Node->GetClass()->FindPropertyByName(TEXT("TextPath")));
				if (TextPathProp)
				{
					const FText& PathText = TextPathProp->GetPropertyValue_InContainer(Node);
					if (!PathText.IsEmpty())
					{
						NodeObj->SetStringField(TEXT("property_path"), PathText.ToString());
					}
				}
				// Also get the Path array (TArray<FString>) for segment-level detail
				FArrayProperty* PathArrayProp = CastField<FArrayProperty>(Node->GetClass()->FindPropertyByName(TEXT("Path")));
				if (PathArrayProp)
				{
					FScriptArrayHelper ArrayHelper(PathArrayProp, PathArrayProp->ContainerPtrToValuePtr<void>(Node));
					TArray<TSharedPtr<FJsonValue>> PathSegments;
					FStrProperty* InnerProp = CastField<FStrProperty>(PathArrayProp->Inner);
					if (InnerProp)
					{
						for (int32 i = 0; i < ArrayHelper.Num(); i++)
						{
							FString Segment = InnerProp->GetPropertyValue(ArrayHelper.GetRawPtr(i));
							PathSegments.Add(MakeShared<FJsonValueString>(Segment));
						}
					}
					if (PathSegments.Num() > 0)
					{
						NodeObj->SetArrayField(TEXT("path_segments"), PathSegments);
					}
				}
			}

			TArray<TSharedPtr<FJsonValue>> PinsArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;

				TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
				PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
				PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));

				TSharedPtr<FJsonObject> PinTypeObj = MakeShared<FJsonObject>();
				SerializePinType(Pin->PinType, PinTypeObj);
				PinObj->SetObjectField(TEXT("type"), PinTypeObj);

				if (!Pin->DefaultValue.IsEmpty())
				{
					PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
				}

				if (!Pin->DefaultTextValue.IsEmpty())
				{
					PinObj->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
				}

				if (Pin->DefaultObject)
				{
					PinObj->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
				}

				PinObj->SetBoolField(TEXT("is_linked"), Pin->LinkedTo.Num() > 0);

				PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
			}
			NodeObj->SetArrayField(TEXT("pins"), PinsArray);

			TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output) continue;

				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin || !LinkedPin->GetOwningNode()) continue;

					TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
					ConnObj->SetStringField(TEXT("from_pin"), Pin->PinName.ToString());
					ConnObj->SetStringField(TEXT("to_node"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
					ConnObj->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
					ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
				}
			}
			NodeObj->SetArrayField(TEXT("connections"), ConnectionsArray);

			NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
		}

		GraphObj->SetArrayField(TEXT("nodes"), NodesArray);
		GraphObj->SetNumberField(TEXT("node_count"), NodesArray.Num());
		GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("graphs"), GraphsArray);
	Data->SetNumberField(TEXT("count"), GraphsArray.Num());

	return MakeResponse(true, Data);
}

static TArray<TSharedPtr<FJsonValue>> SerializeRichCurveKeys(const FRichCurve& Curve)
{
	TArray<TSharedPtr<FJsonValue>> KeysArray;
	const TArray<FRichCurveKey> Keys = Curve.GetCopyOfKeys();
	for (const FRichCurveKey& Key : Keys)
	{
		TSharedPtr<FJsonObject> KeyObj = MakeShared<FJsonObject>();
		KeyObj->SetNumberField(TEXT("time"), Key.Time);
		KeyObj->SetNumberField(TEXT("value"), Key.Value);
		KeyObj->SetNumberField(TEXT("arrive_tangent"), Key.ArriveTangent);
		KeyObj->SetNumberField(TEXT("leave_tangent"), Key.LeaveTangent);
		KeyObj->SetNumberField(TEXT("arrive_tangent_weight"), Key.ArriveTangentWeight);
		KeyObj->SetNumberField(TEXT("leave_tangent_weight"), Key.LeaveTangentWeight);
		KeyObj->SetStringField(TEXT("interp_mode"), UEnum::GetValueAsString(Key.InterpMode));
		KeyObj->SetStringField(TEXT("tangent_mode"), UEnum::GetValueAsString(Key.TangentMode));
		KeyObj->SetStringField(TEXT("tangent_weight_mode"), UEnum::GetValueAsString(Key.TangentWeightMode));
		KeysArray.Add(MakeShared<FJsonValueObject>(KeyObj));
	}

	return KeysArray;
}

FString FMCPServer::HandleReadTimelines(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	const FString Path = Params->GetStringField(TEXT("path"));
	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> TimelinesArray;

	for (UTimelineTemplate* Timeline : Blueprint->Timelines)
	{
		if (!Timeline) continue;

		TSharedPtr<FJsonObject> TimelineObj = MakeShared<FJsonObject>();
		TimelineObj->SetStringField(TEXT("name"), Timeline->GetVariableName().ToString());
		TimelineObj->SetStringField(TEXT("update_function"), Timeline->GetUpdateFunctionName().ToString());
		TimelineObj->SetStringField(TEXT("finished_function"), Timeline->GetFinishedFunctionName().ToString());
		TimelineObj->SetNumberField(TEXT("length"), Timeline->TimelineLength);
		TimelineObj->SetStringField(TEXT("length_mode"), UEnum::GetValueAsString(Timeline->LengthMode));
		TimelineObj->SetBoolField(TEXT("auto_play"), Timeline->bAutoPlay);
		TimelineObj->SetBoolField(TEXT("loop"), Timeline->bLoop);
		TimelineObj->SetBoolField(TEXT("replicated"), Timeline->bReplicated);
		TimelineObj->SetBoolField(TEXT("ignore_time_dilation"), Timeline->bIgnoreTimeDilation);
		TimelineObj->SetStringField(TEXT("tick_group"), UEnum::GetValueAsString(Timeline->TimelineTickGroup));

		TArray<TSharedPtr<FJsonValue>> EventTracksArray;
		for (const FTTEventTrack& Track : Timeline->EventTracks)
		{
			TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
			TrackObj->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
			TrackObj->SetStringField(TEXT("function_name"), Track.GetFunctionName().ToString());
			TrackObj->SetBoolField(TEXT("is_external_curve"), Track.bIsExternalCurve);
			if (Track.CurveKeys)
			{
				TrackObj->SetArrayField(TEXT("keys"), SerializeRichCurveKeys(Track.CurveKeys->FloatCurve));
			}
			EventTracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
		}
		TimelineObj->SetArrayField(TEXT("event_tracks"), EventTracksArray);

		TArray<TSharedPtr<FJsonValue>> FloatTracksArray;
		for (const FTTFloatTrack& Track : Timeline->FloatTracks)
		{
			TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
			TrackObj->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
			TrackObj->SetStringField(TEXT("property_name"), Track.GetPropertyName().ToString());
			TrackObj->SetBoolField(TEXT("is_external_curve"), Track.bIsExternalCurve);
			if (Track.CurveFloat)
			{
				TrackObj->SetArrayField(TEXT("keys"), SerializeRichCurveKeys(Track.CurveFloat->FloatCurve));
			}
			FloatTracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
		}
		TimelineObj->SetArrayField(TEXT("float_tracks"), FloatTracksArray);

		TArray<TSharedPtr<FJsonValue>> VectorTracksArray;
		for (const FTTVectorTrack& Track : Timeline->VectorTracks)
		{
			TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
			TrackObj->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
			TrackObj->SetStringField(TEXT("property_name"), Track.GetPropertyName().ToString());
			TrackObj->SetBoolField(TEXT("is_external_curve"), Track.bIsExternalCurve);
			if (Track.CurveVector)
			{
				TSharedPtr<FJsonObject> KeysObj = MakeShared<FJsonObject>();
				KeysObj->SetArrayField(TEXT("x"), SerializeRichCurveKeys(Track.CurveVector->FloatCurves[0]));
				KeysObj->SetArrayField(TEXT("y"), SerializeRichCurveKeys(Track.CurveVector->FloatCurves[1]));
				KeysObj->SetArrayField(TEXT("z"), SerializeRichCurveKeys(Track.CurveVector->FloatCurves[2]));
				TrackObj->SetObjectField(TEXT("keys"), KeysObj);
			}
			VectorTracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
		}
		TimelineObj->SetArrayField(TEXT("vector_tracks"), VectorTracksArray);

		TArray<TSharedPtr<FJsonValue>> LinearColorTracksArray;
		for (const FTTLinearColorTrack& Track : Timeline->LinearColorTracks)
		{
			TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
			TrackObj->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
			TrackObj->SetStringField(TEXT("property_name"), Track.GetPropertyName().ToString());
			TrackObj->SetBoolField(TEXT("is_external_curve"), Track.bIsExternalCurve);
			if (Track.CurveLinearColor)
			{
				TSharedPtr<FJsonObject> KeysObj = MakeShared<FJsonObject>();
				KeysObj->SetArrayField(TEXT("r"), SerializeRichCurveKeys(Track.CurveLinearColor->FloatCurves[0]));
				KeysObj->SetArrayField(TEXT("g"), SerializeRichCurveKeys(Track.CurveLinearColor->FloatCurves[1]));
				KeysObj->SetArrayField(TEXT("b"), SerializeRichCurveKeys(Track.CurveLinearColor->FloatCurves[2]));
				KeysObj->SetArrayField(TEXT("a"), SerializeRichCurveKeys(Track.CurveLinearColor->FloatCurves[3]));
				TrackObj->SetObjectField(TEXT("keys"), KeysObj);
			}
			LinearColorTracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
		}
		TimelineObj->SetArrayField(TEXT("linear_color_tracks"), LinearColorTracksArray);

		TimelinesArray.Add(MakeShared<FJsonValueObject>(TimelineObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("timelines"), TimelinesArray);
	Data->SetNumberField(TEXT("count"), TimelinesArray.Num());

	return MakeResponse(true, Data);
}

static void SerializePinType(const FEdGraphPinType& PinType, TSharedPtr<FJsonObject>& OutObj)
{
	OutObj->SetStringField(TEXT("category"), PinType.PinCategory.ToString());
	OutObj->SetStringField(TEXT("subcategory"), PinType.PinSubCategory.ToString());
	if (PinType.PinSubCategoryObject.IsValid())
	{
		OutObj->SetStringField(TEXT("subcategory_object"), PinType.PinSubCategoryObject->GetName());
	}
	OutObj->SetBoolField(TEXT("is_array"), PinType.ContainerType == EPinContainerType::Array);
	OutObj->SetBoolField(TEXT("is_set"), PinType.ContainerType == EPinContainerType::Set);
	OutObj->SetBoolField(TEXT("is_map"), PinType.ContainerType == EPinContainerType::Map);
	OutObj->SetBoolField(TEXT("is_reference"), PinType.bIsReference);
	OutObj->SetBoolField(TEXT("is_const"), PinType.bIsConst);
}

FString FMCPServer::HandleReadInterface(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	const FString Path = Params->GetStringField(TEXT("path"));
	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Interface not found: %s"), *Path));
	}

	if (Blueprint->BlueprintType != BPTYPE_Interface)
	{
		return MakeError(TEXT("Blueprint is not an interface"));
	}

	TArray<TSharedPtr<FJsonValue>> FunctionsArray;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph) continue;

		UK2Node_FunctionEntry* EntryNode = nullptr;
		UK2Node_FunctionResult* ResultNode = nullptr;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!EntryNode)
			{
				EntryNode = Cast<UK2Node_FunctionEntry>(Node);
			}
			if (!ResultNode)
			{
				ResultNode = Cast<UK2Node_FunctionResult>(Node);
			}
		}

		TSharedPtr<FJsonObject> FuncObj = MakeShared<FJsonObject>();
		FuncObj->SetStringField(TEXT("name"), Graph->GetName());

		TArray<TSharedPtr<FJsonValue>> InputsArray;
		if (EntryNode)
		{
			for (UEdGraphPin* Pin : EntryNode->Pins)
			{
				if (!Pin || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
				if (Pin->Direction != EGPD_Output) continue;

				TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
				PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
				SerializePinType(Pin->PinType, PinObj);
				InputsArray.Add(MakeShared<FJsonValueObject>(PinObj));
			}
		}
		FuncObj->SetArrayField(TEXT("inputs"), InputsArray);

		TArray<TSharedPtr<FJsonValue>> OutputsArray;
		if (ResultNode)
		{
			for (UEdGraphPin* Pin : ResultNode->Pins)
			{
				if (!Pin || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
				if (Pin->Direction != EGPD_Input) continue;

				TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
				PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
				SerializePinType(Pin->PinType, PinObj);
				OutputsArray.Add(MakeShared<FJsonValueObject>(PinObj));
			}
		}
		FuncObj->SetArrayField(TEXT("outputs"), OutputsArray);

		FunctionsArray.Add(MakeShared<FJsonValueObject>(FuncObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("functions"), FunctionsArray);
	Data->SetNumberField(TEXT("count"), FunctionsArray.Num());

	return MakeResponse(true, Data);
}

static void SerializeProperty(const FProperty* Prop, TSharedPtr<FJsonObject>& OutObj)
{
	OutObj->SetStringField(TEXT("name"), Prop->GetName());
	OutObj->SetStringField(TEXT("property_class"), Prop->GetClass()->GetName());

	bool bIsArray = false;
	if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
	{
		bIsArray = true;
		OutObj->SetStringField(TEXT("inner_property_class"), ArrayProp->Inner->GetClass()->GetName());
		if (const FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner))
		{
			OutObj->SetStringField(TEXT("inner_struct"), InnerStruct->Struct->GetName());
		}
		if (const FObjectPropertyBase* InnerObj = CastField<FObjectPropertyBase>(ArrayProp->Inner))
		{
			OutObj->SetStringField(TEXT("inner_object_class"), InnerObj->PropertyClass->GetName());
		}
	}
	OutObj->SetBoolField(TEXT("is_array"), bIsArray);

	if (const FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		OutObj->SetStringField(TEXT("struct_type"), StructProp->Struct->GetName());
	}
	else if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
	{
		OutObj->SetStringField(TEXT("object_class"), ObjProp->PropertyClass->GetName());
	}
	else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		OutObj->SetStringField(TEXT("enum_type"), EnumProp->GetEnum() ? EnumProp->GetEnum()->GetName() : TEXT(""));
	}
	else if (const FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
	{
		if (ByteProp->Enum)
		{
			OutObj->SetStringField(TEXT("enum_type"), ByteProp->Enum->GetName());
		}
	}
}

FString FMCPServer::HandleReadUserDefinedStruct(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	const FString Path = Params->GetStringField(TEXT("path"));
	UUserDefinedStruct* Struct = LoadObject<UUserDefinedStruct>(nullptr, *Path);
	if (!Struct)
	{
		return MakeError(FString::Printf(TEXT("Struct not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> FieldsArray;
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		const FProperty* Prop = *It;
		if (!Prop) continue;
		TSharedPtr<FJsonObject> FieldObj = MakeShared<FJsonObject>();
		SerializeProperty(Prop, FieldObj);
		FieldsArray.Add(MakeShared<FJsonValueObject>(FieldObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Struct->GetName());
	Data->SetStringField(TEXT("path"), Struct->GetPathName());
	Data->SetArrayField(TEXT("fields"), FieldsArray);
	Data->SetNumberField(TEXT("count"), FieldsArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadUserDefinedEnum(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	const FString Path = Params->GetStringField(TEXT("path"));
	UUserDefinedEnum* Enum = LoadObject<UUserDefinedEnum>(nullptr, *Path);
	if (!Enum)
	{
		return MakeError(FString::Printf(TEXT("Enum not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> EntriesArray;
	const int32 NumEnums = Enum->NumEnums();
	for (int32 Index = 0; Index < NumEnums; ++Index)
	{
		if (Enum->HasMetaData(TEXT("Hidden"), Index))
		{
			continue;
		}

		FString Name = Enum->GetNameStringByIndex(Index);
		if (Name.EndsWith(TEXT("_MAX")))
		{
			continue;
		}

		TSharedPtr<FJsonObject> EntryObj = MakeShared<FJsonObject>();
		EntryObj->SetStringField(TEXT("name"), Name);
		EntryObj->SetStringField(TEXT("display_name"), Enum->GetDisplayNameTextByIndex(Index).ToString());
		EntryObj->SetNumberField(TEXT("value"), Enum->GetValueByIndex(Index));

		EntriesArray.Add(MakeShared<FJsonValueObject>(EntryObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Enum->GetName());
	Data->SetStringField(TEXT("path"), Enum->GetPathName());
	Data->SetArrayField(TEXT("entries"), EntriesArray);
	Data->SetNumberField(TEXT("count"), EntriesArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleListActors(const TSharedPtr<FJsonObject>& Params)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeError(TEXT("No world available"));
	}

	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
		ActorObj->SetStringField(TEXT("name"), Actor->GetName());
		ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
		ActorObj->SetStringField(TEXT("label"), Actor->GetActorLabel());

		FVector Location = Actor->GetActorLocation();
		ActorObj->SetStringField(TEXT("location"), FString::Printf(TEXT("%.1f, %.1f, %.1f"), Location.X, Location.Y, Location.Z));

		ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("actors"), ActorsArray);
	Data->SetNumberField(TEXT("count"), ActorsArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadActorComponents(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("actor_name")))
	{
		return MakeError(TEXT("Missing required parameter: 'actor_name'"));
	}

	const FString ActorName = Params->GetStringField(TEXT("actor_name"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeError(TEXT("No world available"));
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetName() == ActorName)
		{
			FoundActor = Actor;
			break;
		}
	}

	if (!FoundActor)
	{
		return MakeError(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	TInlineComponentArray<UActorComponent*> Components;
	FoundActor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!Component) continue;

		TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
		CompObj->SetStringField(TEXT("name"), Component->GetName());
		CompObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());
		CompObj->SetBoolField(TEXT("active"), Component->IsActive());
		CompObj->SetBoolField(TEXT("registered"), Component->IsRegistered());
		CompObj->SetBoolField(TEXT("editor_only"), Component->IsEditorOnly());
		ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor_name"), ActorName);
	Data->SetStringField(TEXT("actor_class"), FoundActor->GetClass()->GetName());
	Data->SetArrayField(TEXT("components"), ComponentsArray);
	Data->SetNumberField(TEXT("count"), ComponentsArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadActorComponentProperties(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("actor_name")) || !Params->HasField(TEXT("component_name")))
	{
		return MakeError(TEXT("Missing required parameters: 'actor_name', 'component_name'"));
	}

	const FString ActorName = Params->GetStringField(TEXT("actor_name"));
	const FString ComponentName = Params->GetStringField(TEXT("component_name"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeError(TEXT("No world available"));
	}

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetName() == ActorName)
		{
			FoundActor = Actor;
			break;
		}
	}

	if (!FoundActor)
	{
		return MakeError(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	UActorComponent* TargetComponent = nullptr;
	TInlineComponentArray<UActorComponent*> Components;
	FoundActor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!Component) continue;
		const FString CompName = Component->GetName();
		if (CompName.Equals(ComponentName, ESearchCase::IgnoreCase))
		{
			TargetComponent = Component;
			break;
		}
	}

	if (!TargetComponent)
	{
		return MakeError(FString::Printf(TEXT("Component not found on actor '%s': %s"), *ActorName, *ComponentName));
	}

	TArray<TSharedPtr<FJsonValue>> PropsArray;
	UClass* ComponentClass = TargetComponent->GetClass();

	for (TFieldIterator<FProperty> PropIt(ComponentClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property) continue;

		if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			continue;
		}

		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), Property->GetName());
		PropObj->SetStringField(TEXT("type"), Property->GetClass()->GetName());

		FString Category = Property->GetMetaData(TEXT("Category"));
		if (!Category.IsEmpty())
		{
			PropObj->SetStringField(TEXT("category"), Category);
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetComponent);
		FString ValueStr;
		Property->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, TargetComponent, PPF_None);
		PropObj->SetStringField(TEXT("value"), ValueStr);

		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
		{
			UObject* ObjValue = ObjProp->GetObjectPropertyValue(ValuePtr);
			if (ObjValue)
			{
				PropObj->SetStringField(TEXT("object_value"), ObjValue->GetPathName());
				PropObj->SetStringField(TEXT("object_class"), ObjValue->GetClass()->GetName());
			}
		}
		else if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
		{
			UClass* ClassValue = Cast<UClass>(ClassProp->GetObjectPropertyValue(ValuePtr));
			if (ClassValue)
			{
				PropObj->SetStringField(TEXT("class_value"), ClassValue->GetPathName());
				PropObj->SetStringField(TEXT("class_name"), ClassValue->GetName());
			}
		}

		PropsArray.Add(MakeShared<FJsonValueObject>(PropObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor_name"), ActorName);
	Data->SetStringField(TEXT("component_name"), TargetComponent->GetName());
	Data->SetStringField(TEXT("component_class"), ComponentClass->GetName());
	Data->SetArrayField(TEXT("properties"), PropsArray);
	Data->SetNumberField(TEXT("count"), PropsArray.Num());

	return MakeResponse(true, Data);
}
