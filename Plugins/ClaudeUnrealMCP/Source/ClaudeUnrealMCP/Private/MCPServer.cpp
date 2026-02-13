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
#include "K2Node_CustomEvent.h"
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
#include "K2Node_BreakStruct.h"
#include "StructUtils/InstancedStruct.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_CastByteToEnum.h"
#include "K2Node_Select.h"
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


FString FMCPServer::HandleAddComponent(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString ComponentClass = Params->GetStringField(TEXT("component_class"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));

	if (BlueprintPath.IsEmpty() || ComponentClass.IsEmpty())
	{
		return MakeError(TEXT("Missing blueprint_path or component_class"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Find the component class
	UClass* CompClass = FindFirstObject<UClass>(*ComponentClass, EFindFirstObjectOptions::ExactClass);
	if (!CompClass)
	{
		// Try with _C suffix for blueprint classes
		CompClass = FindFirstObject<UClass>(*(ComponentClass + TEXT("_C")), EFindFirstObjectOptions::ExactClass);
	}
	if (!CompClass)
	{
		// Try loading it
		CompClass = LoadClass<UActorComponent>(nullptr, *ComponentClass);
	}
	if (!CompClass)
	{
		return MakeError(FString::Printf(TEXT("Component class not found: %s"), *ComponentClass));
	}

	if (!Blueprint->SimpleConstructionScript)
	{
		return MakeError(TEXT("Blueprint has no SimpleConstructionScript"));
	}

	// Generate name if not provided
	if (ComponentName.IsEmpty())
	{
		ComponentName = CompClass->GetName();
		ComponentName.RemoveFromEnd(TEXT("Component"));
	}

	// Create the new node
	USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(CompClass, *ComponentName);
	if (!NewNode)
	{
		return MakeError(TEXT("Failed to create component node"));
	}

	// Add to root or default scene root
	USCS_Node* RootNode = Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode();
	if (RootNode)
	{
		RootNode->AddChildNode(NewNode);
	}
	else
	{
		Blueprint->SimpleConstructionScript->AddNode(NewNode);
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("component_name"), NewNode->GetVariableName().ToString());
	Data->SetStringField(TEXT("message"), TEXT("Component added successfully"));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	FString PropertyValue = Params->GetStringField(TEXT("property_value"));

	if (BlueprintPath.IsEmpty() || ComponentName.IsEmpty() || PropertyName.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
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

	// Find the property
	FProperty* Property = ComponentTemplate->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		return MakeError(FString::Printf(TEXT("Property not found: %s"), *PropertyName));
	}

	// Handle object reference properties (like UInputAction*)
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		// Load the referenced object
		UObject* ReferencedObject = LoadObject<UObject>(nullptr, *PropertyValue);
		if (!ReferencedObject)
		{
			return MakeError(FString::Printf(TEXT("Could not load object: %s"), *PropertyValue));
		}

		ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(ComponentTemplate), ReferencedObject);
	}
	else
	{
		// Use generic property import for other types
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ComponentTemplate);
		if (!Property->ImportText_Direct(*PropertyValue, ValuePtr, ComponentTemplate, PPF_None))
		{
			return MakeError(FString::Printf(TEXT("Failed to set property value: %s"), *PropertyValue));
		}
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Property %s set successfully"), *PropertyName));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetBlueprintCDOClassReference(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	FString ClassName = Params->GetStringField(TEXT("class_name"));

	if (BlueprintPath.IsEmpty() || ComponentName.IsEmpty() || PropertyName.IsEmpty() || ClassName.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
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

	// Find the property
	FProperty* Property = ComponentTemplate->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		return MakeError(FString::Printf(TEXT("Property not found: %s"), *PropertyName));
	}

	// Handle class properties (FClassProperty)
	if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
	{
		// Try to find the class by name
		UClass* TargetClass = FindObject<UClass>(nullptr, *ClassName);
		if (!TargetClass)
		{
			// Try loading as a full path
			TargetClass = LoadObject<UClass>(nullptr, *ClassName);
		}

		if (!TargetClass)
		{
			return MakeError(FString::Printf(TEXT("Class not found: %s"), *ClassName));
		}

		// Verify the class is compatible with the property's metaclass
		if (!TargetClass->IsChildOf(ClassProp->MetaClass))
		{
			return MakeError(FString::Printf(TEXT("Class %s is not compatible with property metaclass %s"),
				*TargetClass->GetName(), *ClassProp->MetaClass->GetName()));
		}

		// Set the class reference
		ClassProp->SetObjectPropertyValue(ClassProp->ContainerPtrToValuePtr<void>(ComponentTemplate), TargetClass);
	}
	// Handle soft class properties (FSoftClassProperty)
	else if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
	{
		// Try to find the class by name
		UClass* TargetClass = FindObject<UClass>(nullptr, *ClassName);
		if (!TargetClass)
		{
			// Try loading as a full path
			TargetClass = LoadObject<UClass>(nullptr, *ClassName);
		}

		if (!TargetClass)
		{
			return MakeError(FString::Printf(TEXT("Class not found: %s"), *ClassName));
		}

		// Create a soft object ptr to the class
		FSoftObjectPtr SoftPtr(TargetClass);
		void* ValuePtr = SoftClassProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
		SoftClassProp->SetPropertyValue(ValuePtr, SoftPtr);
	}
	else
	{
		return MakeError(FString::Printf(TEXT("Property %s is not a class property (type: %s)"),
			*PropertyName, *Property->GetClass()->GetName()));
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Class reference %s set successfully to %s"), *PropertyName, *ClassName));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReplaceComponentMapValue(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	FString MapKey = Params->GetStringField(TEXT("map_key"));
	FString TargetClassName = Params->GetStringField(TEXT("target_class"));

	if (BlueprintPath.IsEmpty() || ComponentName.IsEmpty() || PropertyName.IsEmpty() || MapKey.IsEmpty() || TargetClassName.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
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

	// Find the map property
	FMapProperty* MapProp = FindFProperty<FMapProperty>(ComponentTemplate->GetClass(), *PropertyName);
	if (!MapProp)
	{
		return MakeError(FString::Printf(TEXT("Map property not found: %s"), *PropertyName));
	}

	// Get the map helper
	void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
	FScriptMapHelper MapHelper(MapProp, MapPtr);

	// Find the key in the map
	int32 FoundIndex = INDEX_NONE;
	FString KeyToFind = MapKey;

	for (int32 i = 0; i < MapHelper.Num(); ++i)
	{
		if (!MapHelper.IsValidIndex(i)) continue;

		// Get the key
		void* KeyPtr = MapHelper.GetKeyPtr(i);
		FString KeyStr;
		MapProp->KeyProp->ExportTextItem_Direct(KeyStr, KeyPtr, nullptr, nullptr, PPF_None);

		// Remove quotes if present
		KeyStr = KeyStr.TrimQuotes();

		if (KeyStr == KeyToFind)
		{
			FoundIndex = i;
			break;
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		return MakeError(FString::Printf(TEXT("Map key not found: %s"), *MapKey));
	}

	// Get the current value
	void* ValuePtr = MapHelper.GetValuePtr(FoundIndex);
	FObjectProperty* ValueProp = CastField<FObjectProperty>(MapProp->ValueProp);

	if (!ValueProp)
	{
		return MakeError(TEXT("Map value is not an object property"));
	}

	UObject* CurrentValue = ValueProp->GetObjectPropertyValue(ValuePtr);
	if (!CurrentValue)
	{
		return MakeError(TEXT("Current map value is null"));
	}

	// Find or load the target class
	UClass* TargetClass = FindObject<UClass>(nullptr, *TargetClassName);
	if (!TargetClass)
	{
		TargetClass = LoadObject<UClass>(nullptr, *TargetClassName);
	}

	if (!TargetClass)
	{
		return MakeError(FString::Printf(TEXT("Target class not found: %s"), *TargetClassName));
	}

	// Create a new instance of the target class
	UObject* NewInstance = NewObject<UObject>(ComponentTemplate, TargetClass, NAME_None, RF_Transactional);
	if (!NewInstance)
	{
		return MakeError(FString::Printf(TEXT("Failed to create instance of class: %s"), *TargetClassName));
	}

	// Copy properties from old instance to new instance if they're compatible
	if (CurrentValue->GetClass()->IsChildOf(TargetClass) || TargetClass->IsChildOf(CurrentValue->GetClass()))
	{
		for (TFieldIterator<FProperty> PropIt(TargetClass); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
			{
				continue;
			}

			// Try to find matching property in source
			FProperty* SourceProperty = CurrentValue->GetClass()->FindPropertyByName(Property->GetFName());
			if (SourceProperty && SourceProperty->SameType(Property))
			{
				void* SourceValuePtr = SourceProperty->ContainerPtrToValuePtr<void>(CurrentValue);
				void* DestValuePtr = Property->ContainerPtrToValuePtr<void>(NewInstance);
				Property->CopyCompleteValue(DestValuePtr, SourceValuePtr);
			}
		}
	}

	// Replace the map value
	ValueProp->SetObjectPropertyValue(ValuePtr, NewInstance);

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Replaced map entry '%s' with instance of %s"), *MapKey, *TargetClassName));
	Data->SetStringField(TEXT("old_class"), CurrentValue->GetClass()->GetName());
	Data->SetStringField(TEXT("new_class"), TargetClass->GetName());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReplaceBlueprintArrayValue(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	int32 ArrayIndex = Params->GetIntegerField(TEXT("array_index"));
	FString TargetClassName = Params->GetStringField(TEXT("target_class"));

	if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty() || TargetClassName.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	UClass* GeneratedClass = Blueprint->GeneratedClass;
	if (!GeneratedClass)
	{
		return MakeError(TEXT("Blueprint has no generated class"));
	}

	UObject* CDO = GeneratedClass->GetDefaultObject();
	if (!CDO)
	{
		return MakeError(TEXT("Could not get Class Default Object"));
	}

	// Find the array property
	FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(CDO->GetClass(), *PropertyName);
	if (!ArrayProp)
	{
		return MakeError(FString::Printf(TEXT("Array property not found: %s"), *PropertyName));
	}

	// Get the array helper
	void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(CDO);
	FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);

	// Check array bounds
	if (ArrayIndex < 0 || ArrayIndex >= ArrayHelper.Num())
	{
		return MakeError(FString::Printf(TEXT("Array index %d out of bounds (array size: %d)"), ArrayIndex, ArrayHelper.Num()));
	}

	// Get the element property
	FObjectProperty* ElementProp = CastField<FObjectProperty>(ArrayProp->Inner);
	if (!ElementProp)
	{
		return MakeError(TEXT("Array element is not an object property"));
	}

	// Get current value
	void* ElementPtr = ArrayHelper.GetRawPtr(ArrayIndex);
	UObject* CurrentValue = ElementProp->GetObjectPropertyValue(ElementPtr);
	if (!CurrentValue)
	{
		return MakeError(TEXT("Current array element is null"));
	}

	// Find or load the target class
	UClass* TargetClass = FindObject<UClass>(nullptr, *TargetClassName);
	if (!TargetClass)
	{
		TargetClass = LoadObject<UClass>(nullptr, *TargetClassName);
	}

	if (!TargetClass)
	{
		return MakeError(FString::Printf(TEXT("Target class not found: %s"), *TargetClassName));
	}

	// Create a new instance of the target class
	UObject* NewInstance = NewObject<UObject>(CDO, TargetClass, NAME_None, RF_Transactional | RF_ArchetypeObject | RF_Public);
	if (!NewInstance)
	{
		return MakeError(FString::Printf(TEXT("Failed to create instance of class: %s"), *TargetClassName));
	}

	// Copy properties from old instance to new instance if they're compatible
	if (CurrentValue->GetClass()->IsChildOf(TargetClass) || TargetClass->IsChildOf(CurrentValue->GetClass()))
	{
		for (TFieldIterator<FProperty> PropIt(TargetClass); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
			{
				continue;
			}

			// Try to find matching property in source
			FProperty* SourceProperty = CurrentValue->GetClass()->FindPropertyByName(Property->GetFName());
			if (SourceProperty && SourceProperty->SameType(Property))
			{
				void* SourceValuePtr = SourceProperty->ContainerPtrToValuePtr<void>(CurrentValue);
				void* DestValuePtr = Property->ContainerPtrToValuePtr<void>(NewInstance);
				Property->CopyCompleteValue(DestValuePtr, SourceValuePtr);
			}
		}
	}

	// Replace the array element
	ElementProp->SetObjectPropertyValue(ElementPtr, NewInstance);

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Replaced array[%d] with instance of %s"), ArrayIndex, *TargetClassName));
	Data->SetStringField(TEXT("old_class"), CurrentValue->GetClass()->GetName());
	Data->SetStringField(TEXT("new_class"), TargetClass->GetName());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleAddInputMapping(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString ContextPath = Params->GetStringField(TEXT("context_path"));
	FString ActionPath = Params->GetStringField(TEXT("action_path"));
	FString KeyName = Params->GetStringField(TEXT("key"));

	if (ContextPath.IsEmpty() || ActionPath.IsEmpty() || KeyName.IsEmpty())
	{
		return MakeError(TEXT("Missing context_path, action_path, or key"));
	}

	// Load the input mapping context
	UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!Context)
	{
		return MakeError(FString::Printf(TEXT("Input mapping context not found: %s"), *ContextPath));
	}

	// Load the input action
	UInputAction* Action = LoadObject<UInputAction>(nullptr, *ActionPath);
	if (!Action)
	{
		return MakeError(FString::Printf(TEXT("Input action not found: %s"), *ActionPath));
	}

	// Parse the key
	FKey Key(*KeyName);
	if (!Key.IsValid())
	{
		return MakeError(FString::Printf(TEXT("Invalid key: %s"), *KeyName));
	}

	// Add the mapping
	FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);

	// Mark as modified
	Context->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Mapped %s to %s in %s"), *KeyName, *Action->GetName(), *Context->GetName()));

	return MakeResponse(true, Data);
}

static UClass* ResolveParentClass(const FString& ParentClassPath)
{
	if (ParentClassPath.IsEmpty())
	{
		return nullptr;
	}

	UClass* NewParent = FindFirstObject<UClass>(*ParentClassPath, EFindFirstObjectOptions::ExactClass);
	if (!NewParent && !ParentClassPath.EndsWith(TEXT("_C")))
	{
		NewParent = FindFirstObject<UClass>(*(ParentClassPath + TEXT("_C")), EFindFirstObjectOptions::ExactClass);
	}

	if (!NewParent)
	{
		NewParent = LoadObject<UClass>(nullptr, *ParentClassPath);
	}
	if (!NewParent)
	{
		NewParent = LoadClass<UObject>(nullptr, *ParentClassPath);
	}
	if (!NewParent && !ParentClassPath.EndsWith(TEXT("_C")))
	{
		NewParent = LoadObject<UClass>(nullptr, *(ParentClassPath + TEXT("_C")));
		if (!NewParent)
		{
			NewParent = LoadClass<UObject>(nullptr, *(ParentClassPath + TEXT("_C")));
		}
	}

	if (!NewParent)
	{
		if (UBlueprint* ParentBP = LoadObject<UBlueprint>(nullptr, *ParentClassPath))
		{
			NewParent = ParentBP->GeneratedClass;
		}
	}

	return NewParent;
}

FString FMCPServer::HandleReparentBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) || !Params->HasField(TEXT("parent_class")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' or 'parent_class' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString ParentClassPath = Params->GetStringField(TEXT("parent_class"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	UClass* NewParent = ResolveParentClass(ParentClassPath);
	if (!NewParent)
	{
		return MakeError(FString::Printf(TEXT("Parent class not found: %s"), *ParentClassPath));
	}

	UClass* OldParent = Blueprint->ParentClass;
	if (OldParent == NewParent)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("message"), TEXT("Blueprint already uses specified parent class"));
		Data->SetStringField(TEXT("parent_class"), NewParent->GetName());
		return MakeResponse(true, Data);
	}

	UBlueprintEditorLibrary::ReparentBlueprint(Blueprint, NewParent);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	// Auto-reconstruct LevelVisuals to refresh material colors
	ReconstructLevelVisuals();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Blueprint reparented successfully"));
	Data->SetStringField(TEXT("old_parent"), OldParent ? OldParent->GetName() : TEXT("None"));
	Data->SetStringField(TEXT("new_parent"), NewParent->GetName());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params)
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

	// Compile the blueprint
	FCompilerResultsLog CompileLog;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog);

	bool bSuccess = (Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("compiled"), bSuccess);

	// Get blueprint status string
	FString StatusString;
	switch (Blueprint->Status)
	{
	case BS_Unknown: StatusString = TEXT("Unknown"); break;
	case BS_Dirty: StatusString = TEXT("Dirty"); break;
	case BS_Error: StatusString = TEXT("Error"); break;
	case BS_UpToDate: StatusString = TEXT("UpToDate"); break;
	case BS_BeingCreated: StatusString = TEXT("BeingCreated"); break;
	case BS_UpToDateWithWarnings: StatusString = TEXT("UpToDateWithWarnings"); break;
	default: StatusString = TEXT("Unknown");
	}
	Data->SetStringField(TEXT("status"), StatusString);

	// Capture compilation errors and warnings
	TArray<TSharedPtr<FJsonValue>> ErrorsArray;
	TArray<TSharedPtr<FJsonValue>> WarningsArray;

	for (const TSharedRef<FTokenizedMessage>& Message : CompileLog.Messages)
	{
		TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
		MsgObj->SetStringField(TEXT("message"), Message->ToText().ToString());
		MsgObj->SetStringField(TEXT("severity"), FString::FromInt((int32)Message->GetSeverity()));

		if (Message->GetSeverity() == EMessageSeverity::Error)
		{
			ErrorsArray.Add(MakeShared<FJsonValueObject>(MsgObj));
		}
		else if (Message->GetSeverity() == EMessageSeverity::Warning)
		{
			WarningsArray.Add(MakeShared<FJsonValueObject>(MsgObj));
		}
	}

	Data->SetArrayField(TEXT("errors"), ErrorsArray);
	Data->SetArrayField(TEXT("warnings"), WarningsArray);
	Data->SetNumberField(TEXT("error_count"), ErrorsArray.Num());
	Data->SetNumberField(TEXT("warning_count"), WarningsArray.Num());

	// Auto-reconstruct LevelVisuals to refresh material colors
	ReconstructLevelVisuals();

	// Always return success=true so error details are visible
	// The 'compiled' field indicates actual compilation status
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSaveAsset(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));

	// Try to load as blueprint first
	UObject* Asset = LoadObject<UBlueprint>(nullptr, *Path);
	if (!Asset)
	{
		// Try as generic object
		Asset = LoadObject<UObject>(nullptr, *Path);
	}

	if (!Asset)
	{
		return MakeError(FString::Printf(TEXT("Asset not found: %s"), *Path));
	}

	UPackage* Package = Asset->GetOutermost();
	if (!Package)
	{
		return MakeError(TEXT("Could not get package"));
	}

	FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Standalone;

	bool bSaved = UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("saved"), bSaved);
	Data->SetStringField(TEXT("message"), bSaved ? TEXT("Asset saved successfully") : TEXT("Failed to save asset"));

	if (!bSaved)
	{
		return MakeError(TEXT("Failed to save asset"));
	}

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSaveAll(const TSharedPtr<FJsonObject>& Params)
{
	// Save all dirty packages
	TArray<UPackage*> DirtyPackages;

	// Get all dirty packages
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		UPackage* Package = *It;
		if (Package && Package->IsDirty() && !Package->HasAnyFlags(RF_Transient))
		{
			DirtyPackages.Add(Package);
		}
	}

	int32 SavedCount = 0;
	int32 FailedCount = 0;
	TArray<FString> FailedPackages;

	for (UPackage* Package : DirtyPackages)
	{
		FString PackageName = Package->GetName();

		// Skip packages that don't have a valid path (e.g., temp packages)
		if (!FPackageName::IsValidLongPackageName(PackageName))
		{
			continue;
		}

		FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;

		if (UPackage::SavePackage(Package, nullptr, *PackageFileName, SaveArgs))
		{
			SavedCount++;
		}
		else
		{
			FailedCount++;
			FailedPackages.Add(PackageName);
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("saved_count"), SavedCount);
	Data->SetNumberField(TEXT("failed_count"), FailedCount);
	Data->SetNumberField(TEXT("total_dirty"), DirtyPackages.Num());

	if (FailedPackages.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> FailedArray;
		for (const FString& FailedPkg : FailedPackages)
		{
			FailedArray.Add(MakeShared<FJsonValueString>(FailedPkg));
		}
		Data->SetArrayField(TEXT("failed_packages"), FailedArray);
	}

	FString Message = FString::Printf(TEXT("Saved %d package(s), %d failed"), SavedCount, FailedCount);
	Data->SetStringField(TEXT("message"), Message);

	// Auto-reconstruct LevelVisuals to refresh material colors
	ReconstructLevelVisuals();

	if (FailedCount > 0)
	{
		return MakeResponse(false, Data, Message);
	}

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleDeleteInterfaceFunction(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("interface_path")) || !Params->HasField(TEXT("function_name")))
	{
		return MakeError(TEXT("Missing 'interface_path' or 'function_name' parameter"));
	}

	const FString InterfacePath = Params->GetStringField(TEXT("interface_path"));
	const FString FunctionName = Params->GetStringField(TEXT("function_name"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(InterfacePath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Interface not found: %s"), *InterfacePath));
	}

	if (Blueprint->BlueprintType != BPTYPE_Interface)
	{
		return MakeError(TEXT("Blueprint is not an interface"));
	}

	// Find the function graph
	UEdGraph* GraphToDelete = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			GraphToDelete = Graph;
			break;
		}
	}

	if (!GraphToDelete)
	{
		return MakeError(FString::Printf(TEXT("Function '%s' not found in interface"), *FunctionName));
	}

	// Remove the function graph
	Blueprint->FunctionGraphs.Remove(GraphToDelete);

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Function deleted successfully"));
	Data->SetStringField(TEXT("function_name"), FunctionName);
	Data->SetNumberField(TEXT("remaining_functions"), Blueprint->FunctionGraphs.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleModifyInterfaceFunctionParameter(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("interface_path")) ||
		!Params->HasField(TEXT("function_name")) || !Params->HasField(TEXT("parameter_name")) ||
		!Params->HasField(TEXT("new_type")))
	{
		return MakeError(TEXT("Missing required parameters: 'interface_path', 'function_name', 'parameter_name', 'new_type'"));
	}

	const FString InterfacePath = Params->GetStringField(TEXT("interface_path"));
	const FString FunctionName = Params->GetStringField(TEXT("function_name"));
	const FString ParameterName = Params->GetStringField(TEXT("parameter_name"));
	const FString NewType = Params->GetStringField(TEXT("new_type"));
	const bool bIsOutput = Params->HasField(TEXT("is_output")) ? Params->GetBoolField(TEXT("is_output")) : false;

	UBlueprint* Blueprint = LoadBlueprintFromPath(InterfacePath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Interface not found: %s"), *InterfacePath));
	}

	if (Blueprint->BlueprintType != BPTYPE_Interface)
	{
		return MakeError(TEXT("Blueprint is not an interface"));
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return MakeError(FString::Printf(TEXT("Function '%s' not found in interface"), *FunctionName));
	}

	// Parse the new type
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	FEdGraphPinType NewPinType;

	// Handle common types
	if (NewType == TEXT("int") || NewType == TEXT("int32"))
	{
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (NewType == TEXT("float") || NewType == TEXT("double"))
	{
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		NewPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (NewType == TEXT("bool") || NewType == TEXT("boolean"))
	{
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (NewType == TEXT("string") || NewType == TEXT("FString"))
	{
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (NewType == TEXT("FVector"))
	{
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		NewPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (NewType == TEXT("FRotator"))
	{
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		NewPinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (NewType == TEXT("FTransform"))
	{
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		NewPinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else if (NewType == TEXT("struct") && Params->HasField(TEXT("struct_type")))
	{
		// When new_type is "struct", use struct_type parameter to find the specific struct
		FString StructTypePath = Params->GetStringField(TEXT("struct_type"));
		FString StructNameToFind = StructTypePath;
		UScriptStruct* FoundStruct = nullptr;
		if (StructTypePath.StartsWith(TEXT("/Script/")))
		{
			FString Remainder = StructTypePath.RightChop(8);
			FString ModuleName;
			Remainder.Split(TEXT("."), &ModuleName, &StructNameToFind);
			// Use FindObject first to resolve exact C++ type (avoids BP name collisions)
			FoundStruct = FindObject<UScriptStruct>(nullptr, *StructTypePath);
		}
		if (!FoundStruct)
		{
			for (TObjectIterator<UScriptStruct> It; It; ++It)
			{
				if (It->GetName() == StructNameToFind)
				{
					FoundStruct = *It;
					break;
				}
			}
		}
		if (!FoundStruct)
		{
			return MakeError(FString::Printf(TEXT("Struct type not found: %s"), *StructTypePath));
		}
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		NewPinType.PinSubCategoryObject = FoundStruct;
	}
	else if (NewType == TEXT("enum") && Params->HasField(TEXT("enum_type")))
	{
		// When new_type is "enum", use enum_type parameter to find the specific enum
		FString EnumTypePath = Params->GetStringField(TEXT("enum_type"));
		FString EnumNameToFind = EnumTypePath;
		if (EnumTypePath.StartsWith(TEXT("/Script/")))
		{
			FString Remainder = EnumTypePath.RightChop(8);
			FString ModuleName;
			Remainder.Split(TEXT("."), &ModuleName, &EnumNameToFind);
		}
		UEnum* FoundEnum = nullptr;
		for (TObjectIterator<UEnum> It; It; ++It)
		{
			if (It->GetName() == EnumNameToFind)
			{
				FoundEnum = *It;
				break;
			}
		}
		if (!FoundEnum)
		{
			return MakeError(FString::Printf(TEXT("Enum type not found: %s"), *EnumTypePath));
		}
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		NewPinType.PinSubCategoryObject = FoundEnum;
	}
	else
	{
		// Try to load as object type or struct
		UObject* TypeObject = nullptr;
		FString TypeNameToFind = NewType;

		// Extract just the type name from /Script/ModuleName.TypeName format
		if (NewType.StartsWith(TEXT("/Script/")))
		{
			FString PathWithoutScript = NewType.RightChop(8);
			FString ModuleName;
			PathWithoutScript.Split(TEXT("."), &ModuleName, &TypeNameToFind);

			// For /Script/ paths, use FindObject first to resolve the exact C++ type
			// This avoids ambiguity when BP UserDefinedStructs share the same name
			TypeObject = FindObject<UScriptStruct>(nullptr, *NewType);
			if (!TypeObject)
			{
				TypeObject = FindObject<UClass>(nullptr, *NewType);
			}
		}

		// Fall back to TObjectIterator name search
		if (!TypeObject)
		{
			for (TObjectIterator<UScriptStruct> It; It; ++It)
			{
				UScriptStruct* Struct = *It;
				if (Struct->GetName() == TypeNameToFind)
				{
					TypeObject = Struct;
					break;
				}
			}
		}

		// If not found as struct, try classes
		if (!TypeObject)
		{
			for (TObjectIterator<UClass> It; It; ++It)
			{
				UClass* Class = *It;
				if (Class->GetName() == TypeNameToFind)
				{
					TypeObject = Class;
					break;
				}
			}
		}

		// Fallback to standard FindObject/LoadObject with full path
		if (!TypeObject)
		{
			TypeObject = FindObject<UObject>(nullptr, *NewType);
		}
		if (!TypeObject)
		{
			TypeObject = LoadObject<UObject>(nullptr, *NewType);
		}

		if (UClass* Class = Cast<UClass>(TypeObject))
		{
			NewPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			NewPinType.PinSubCategoryObject = Class;
		}
		else if (UScriptStruct* Struct = Cast<UScriptStruct>(TypeObject))
		{
			NewPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			NewPinType.PinSubCategoryObject = Struct;
		}
		else
		{
			return MakeError(FString::Printf(TEXT("Unknown type: %s. For C++ structs, ensure the struct has been used in the project (instantiated). C++ USTRUCT types may not be discoverable via reflection until they are actually used."), *NewType));
		}
	}

	// Find the entry or result node based on whether it's input or output
	UK2Node_EditablePinBase* TargetNode = nullptr;
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (bIsOutput)
		{
			UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node);
			if (ResultNode)
			{
				TargetNode = ResultNode;
				break;
			}
		}
		else
		{
			UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node);
			if (EntryNode)
			{
				TargetNode = EntryNode;
				break;
			}
		}
	}

	if (!TargetNode)
	{
		return MakeError(TEXT("Could not find entry/result node in function graph"));
	}

	// Find the existing pin and remove it
	bool bFoundPin = false;
	TArray<TSharedPtr<FUserPinInfo>> PinsToKeep;

	for (const TSharedPtr<FUserPinInfo>& PinInfo : TargetNode->UserDefinedPins)
	{
		if (PinInfo.IsValid() && PinInfo->PinName.ToString() == ParameterName)
		{
			bFoundPin = true;
			// Create a new pin info with the new type
			TSharedPtr<FUserPinInfo> NewPinInfo = MakeShared<FUserPinInfo>();
			NewPinInfo->PinName = PinInfo->PinName;
			NewPinInfo->PinType = NewPinType;
			NewPinInfo->DesiredPinDirection = PinInfo->DesiredPinDirection;
			PinsToKeep.Add(NewPinInfo);
		}
		else
		{
			PinsToKeep.Add(PinInfo);
		}
	}

	if (!bFoundPin)
	{
		return MakeError(FString::Printf(TEXT("Parameter '%s' not found in function"), *ParameterName));
	}

	// Replace the user defined pins
	TargetNode->UserDefinedPins = PinsToKeep;

	// Reconstruct the node to reflect the changes
	TargetNode->ReconstructNode();

	// Mark the blueprint as modified and compile to update the generated class
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Interface function parameter modified successfully"));
	Data->SetStringField(TEXT("function_name"), FunctionName);
	Data->SetStringField(TEXT("parameter_name"), ParameterName);
	Data->SetStringField(TEXT("new_type"), NewType);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleDeleteFunctionGraph(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) || !Params->HasField(TEXT("function_name")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' or 'function_name' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString FunctionName = Params->GetStringField(TEXT("function_name"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Find the function graph
	UEdGraph* GraphToDelete = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			GraphToDelete = Graph;
			break;
		}
	}

	if (!GraphToDelete)
	{
		return MakeError(FString::Printf(TEXT("Function '%s' not found in blueprint"), *FunctionName));
	}

	// Remove the function graph
	Blueprint->FunctionGraphs.Remove(GraphToDelete);

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Function graph deleted successfully"));
	Data->SetStringField(TEXT("function_name"), FunctionName);
	Data->SetNumberField(TEXT("remaining_functions"), Blueprint->FunctionGraphs.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleClearEventGraph(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Get the event graph (typically UbergraphPages[0])
	if (Blueprint->UbergraphPages.Num() == 0)
	{
		return MakeError(TEXT("Blueprint has no event graph"));
	}

	UEdGraph* EventGraph = Blueprint->UbergraphPages[0];
	if (!EventGraph)
	{
		return MakeError(TEXT("Event graph is null"));
	}

	int32 NodesCleared = EventGraph->Nodes.Num();

	// Remove all nodes from the event graph
	EventGraph->Nodes.Empty();

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Event graph cleared successfully"));
	Data->SetNumberField(TEXT("nodes_cleared"), NodesCleared);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleRefreshNodes(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	int32 NodesRefreshed = 0;

	// First, try the built-in RefreshAllNodes which may handle orphaned pins better
	FBlueprintEditorUtils::RefreshAllNodes(Blueprint);

	// Refresh nodes in all graphs (UbergraphPages, FunctionGraphs, etc.)
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				// First, break links to orphaned/stale pins before reconstructing
				// This handles the case where pins have been renamed or removed in C++ function signatures
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (!Pin) continue;

					// Make a copy of linked pins since we'll be modifying the array
					TArray<UEdGraphPin*> LinkedToCopy = Pin->LinkedTo;
					for (UEdGraphPin* OtherPin : LinkedToCopy)
					{
						// If we are linked to a pin that its owner doesn't know about, break that link
						if (!OtherPin || !OtherPin->GetOwningNodeUnchecked() ||
							!OtherPin->GetOwningNode()->Pins.Contains(OtherPin))
						{
							Pin->BreakLinkTo(OtherPin);
						}
					}
				}

				// Reconstruct the node to get the updated pin configuration
				Node->ReconstructNode();

				// CRITICAL: After reconstruction, clean up stale incoming links from OTHER nodes
				// This fixes the "In use pin X no longer exists" errors
				for (UEdGraphNode* OtherNode : Graph->Nodes)
				{
					if (OtherNode && OtherNode != Node)
					{
						for (UEdGraphPin* OtherPin : OtherNode->Pins)
						{
							if (!OtherPin) continue;

							// Check if this pin is linked to any pins that no longer exist on the reconstructed node
							TArray<UEdGraphPin*> LinkedToCopy = OtherPin->LinkedTo;
							for (UEdGraphPin* LinkedPin : LinkedToCopy)
							{
								// If linked to a pin on the reconstructed node that no longer exists in its Pins array
								if (LinkedPin && LinkedPin->GetOwningNode() == Node &&
									!Node->Pins.Contains(LinkedPin))
								{
									OtherPin->BreakLinkTo(LinkedPin);
								}
							}
						}
					}
				}

				NodesRefreshed++;
			}
		}
	}

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Nodes refreshed successfully"));
	Data->SetNumberField(TEXT("nodes_refreshed"), NodesRefreshed);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleBreakOrphanedPins(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	int32 PinsBroken = 0;
	int32 PinsRemoved = 0;

	// Get all graphs
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			// Track pins to remove (can't remove while iterating)
			TArray<UEdGraphPin*> PinsToRemove;

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;

				// Check if this pin is orphaned - it has the bOrphanedPin flag or has invalid connections
				bool bIsOrphaned = Pin->bOrphanedPin;

				// Also check if the pin name suggests it's orphaned (contains "DEPRECATED", "TRASH", or similar markers)
				if (Pin->PinName.ToString().Contains(TEXT("TRASH")) ||
					Pin->PinName.ToString().Contains(TEXT("DEPRECATED")))
				{
					bIsOrphaned = true;
				}

				// Break all connections to/from orphaned pins
				if (bIsOrphaned && Pin->LinkedTo.Num() > 0)
				{
					Pin->BreakAllPinLinks();
					PinsBroken += Pin->LinkedTo.Num();
				}

				// Mark orphaned pins for removal
				if (bIsOrphaned)
				{
					PinsToRemove.Add(Pin);
				}
			}

			// Remove orphaned pins from the node
			for (UEdGraphPin* PinToRemove : PinsToRemove)
			{
				Node->Pins.Remove(PinToRemove);
				PinsRemoved++;
			}
		}
	}

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Orphaned pins cleaned up successfully"));
	Data->SetNumberField(TEXT("pins_broken"), PinsBroken);
	Data->SetNumberField(TEXT("pins_removed"), PinsRemoved);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReconstructNode(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString NodeGuidStr = Params->HasField(TEXT("node_guid")) ? Params->GetStringField(TEXT("node_guid")) : TEXT("");
	const FString VariableFilter = Params->HasField(TEXT("variable_name")) ? Params->GetStringField(TEXT("variable_name")) : TEXT("");

	if (NodeGuidStr.IsEmpty() && VariableFilter.IsEmpty())
	{
		return MakeError(TEXT("Must provide 'node_guid' or 'variable_name' parameter"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint) return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));

	// Collect all graphs including sub-graphs
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	TArray<UEdGraph*> ToProcess = AllGraphs;
	while (ToProcess.Num() > 0)
	{
		UEdGraph* G = ToProcess.Pop();
		for (UEdGraphNode* N : G->Nodes)
		{
			if (!N) continue;
			for (UEdGraph* Sub : N->GetSubGraphs())
			{
				if (Sub && !AllGraphs.Contains(Sub))
				{
					AllGraphs.Add(Sub);
					ToProcess.Add(Sub);
				}
			}
		}
	}

	// Find nodes to reconstruct
	TArray<UEdGraphNode*> NodesToReconstruct;
	if (!NodeGuidStr.IsEmpty())
	{
		FGuid NodeGuid;
		if (!FGuid::Parse(NodeGuidStr, NodeGuid))
		{
			return MakeError(FString::Printf(TEXT("Invalid GUID: %s"), *NodeGuidStr));
		}
		for (UEdGraph* Graph : AllGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node && Node->NodeGuid == NodeGuid)
				{
					NodesToReconstruct.Add(Node);
					break;
				}
			}
			if (NodesToReconstruct.Num() > 0) break;
		}
	}
	else if (!VariableFilter.IsEmpty())
	{
		// Find all VariableGet/VariableSet nodes for the specified variable that have sub-pins (expanded)
		for (UEdGraph* Graph : AllGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node) continue;
				// Check if this is a VariableGet or VariableSet node
				UK2Node_VariableGet* VarGet = Cast<UK2Node_VariableGet>(Node);
				UK2Node_VariableSet* VarSet = Cast<UK2Node_VariableSet>(Node);
				if (!VarGet && !VarSet) continue;

				// Check variable name
				FName VarName = VarGet ? VarGet->GetVarName() : VarSet->GetVarName();
				if (VarName.ToString() != VariableFilter) continue;

				// Check if any pin has sub-pins (expanded struct)
				bool bHasSubPins = false;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin && Pin->SubPins.Num() > 0)
					{
						bHasSubPins = true;
						break;
					}
				}
				if (bHasSubPins)
				{
					NodesToReconstruct.Add(Node);
				}
			}
		}
	}

	if (NodesToReconstruct.Num() == 0)
	{
		return MakeError(TEXT("No matching nodes found"));
	}

	// Reconstruct each node
	int32 TotalRestored = 0;
	int32 TotalFailed = 0;
	TArray<TSharedPtr<FJsonValue>> NodeReports;

	struct FSavedConnection
	{
		FName PinName;
		EEdGraphPinDirection Direction;
		FGuid RemoteNodeGuid;
		FName RemotePinName;
	};

	for (UEdGraphNode* TargetNode : NodesToReconstruct)
	{
		TArray<FSavedConnection> SavedConnections;
		for (UEdGraphPin* Pin : TargetNode->Pins)
		{
			if (!Pin) continue;
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked && Linked->GetOwningNode())
				{
					SavedConnections.Add({Pin->PinName, Pin->Direction, Linked->GetOwningNode()->NodeGuid, Linked->PinName});
				}
			}
		}

		// Find which graph this node is in
		FString GraphName;
		for (UEdGraph* Graph : AllGraphs)
		{
			if (Graph->Nodes.Contains(TargetNode))
			{
				GraphName = Graph->GetName();
				break;
			}
		}

		TargetNode->ReconstructNode();

		int32 Restored = 0;
		int32 Failed = 0;
		for (const FSavedConnection& Conn : SavedConnections)
		{
			UEdGraphPin* OurPin = TargetNode->FindPin(Conn.PinName, Conn.Direction);
			if (!OurPin) { Failed++; continue; }

			UEdGraphNode* RemoteNode = nullptr;
			for (UEdGraph* Graph : AllGraphs)
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					if (Node && Node->NodeGuid == Conn.RemoteNodeGuid)
					{
						RemoteNode = Node;
						break;
					}
				}
				if (RemoteNode) break;
			}
			if (!RemoteNode) { Failed++; continue; }

			EEdGraphPinDirection RemoteDir = (Conn.Direction == EGPD_Input) ? EGPD_Output : EGPD_Input;
			UEdGraphPin* RemotePin = RemoteNode->FindPin(Conn.RemotePinName, RemoteDir);
			if (!RemotePin) { Failed++; continue; }

			OurPin->MakeLinkTo(RemotePin);
			Restored++;
		}
		TotalRestored += Restored;
		TotalFailed += Failed;

		TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetStringField(TEXT("node_guid"), TargetNode->NodeGuid.ToString());
		Report->SetStringField(TEXT("node_class"), TargetNode->GetClass()->GetName());
		Report->SetStringField(TEXT("graph"), GraphName);
		Report->SetNumberField(TEXT("pins_after"), TargetNode->Pins.Num());
		Report->SetNumberField(TEXT("connections_restored"), Restored);
		Report->SetNumberField(TEXT("connections_failed"), Failed);
		NodeReports.Add(MakeShared<FJsonValueObject>(Report));
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Reconstructed %d node(s)"), NodesToReconstruct.Num()));
	Data->SetNumberField(TEXT("nodes_reconstructed"), NodesToReconstruct.Num());
	Data->SetNumberField(TEXT("total_connections_restored"), TotalRestored);
	Data->SetNumberField(TEXT("total_connections_failed"), TotalFailed);
	Data->SetArrayField(TEXT("nodes"), NodeReports);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetPinDefault(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));

	FString BPPath = Params->GetStringField(TEXT("blueprint_path"));
	FString GraphName = Params->GetStringField(TEXT("graph_name"));
	FString NodeGuid = Params->GetStringField(TEXT("node_guid"));
	FString PinName = Params->GetStringField(TEXT("pin_name"));
	FString NewDefault = Params->GetStringField(TEXT("new_default"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BPPath);
	if (!Blueprint) return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	// Search all graphs for the node
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!GraphName.IsEmpty() && Graph->GetName() != GraphName) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			FString GuidStr = Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces);
			// Also try without braces
			FString GuidNoBraces = Node->NodeGuid.ToString(EGuidFormats::Digits);

			if (GuidStr != NodeGuid && GuidNoBraces != NodeGuid && Node->NodeGuid.ToString() != NodeGuid)
				continue;

			// Found the node, find the pin
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->PinName.ToString() == PinName)
				{
					FString OldDefault = Pin->DefaultValue;
					Pin->DefaultValue = NewDefault;

					FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

					TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
					Data->SetStringField(TEXT("graph"), Graph->GetName());
					Data->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
					Data->SetStringField(TEXT("pin"), PinName);
					Data->SetStringField(TEXT("old_default"), OldDefault);
					Data->SetStringField(TEXT("new_default"), NewDefault);
					return MakeResponse(true, Data);
				}
			}
			return MakeError(FString::Printf(TEXT("Pin '%s' not found on node"), *PinName));
		}
	}
	return MakeError(FString::Printf(TEXT("Node with GUID '%s' not found"), *NodeGuid));
}

FString FMCPServer::HandleDeleteUserDefinedStruct(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("struct_path")))
	{
		return MakeError(TEXT("Missing 'struct_path' parameter"));
	}

	const FString StructPath = Params->GetStringField(TEXT("struct_path"));

	// Load the struct asset
	UUserDefinedStruct* Struct = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
	if (!Struct)
	{
		return MakeError(FString::Printf(TEXT("Struct not found: %s"), *StructPath));
	}

	// Delete the asset
	TArray<UObject*> ObjectsToDelete;
	ObjectsToDelete.Add(Struct);

	int32 NumDeleted = ObjectTools::DeleteObjects(ObjectsToDelete, false);

	if (NumDeleted == 0)
	{
		return MakeError(TEXT("Failed to delete struct asset"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Struct deleted successfully"));
	Data->SetStringField(TEXT("struct_path"), StructPath);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleModifyStructField(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("struct_path")) ||
		!Params->HasField(TEXT("field_name")) || !Params->HasField(TEXT("new_type")))
	{
		return MakeError(TEXT("Missing required parameters: struct_path, field_name, new_type"));
	}

	const FString StructPath = Params->GetStringField(TEXT("struct_path"));
	const FString FieldName = Params->GetStringField(TEXT("field_name"));
	const FString NewType = Params->GetStringField(TEXT("new_type"));

	// Load the struct asset
	UUserDefinedStruct* Struct = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
	if (!Struct)
	{
		return MakeError(FString::Printf(TEXT("Struct not found: %s"), *StructPath));
	}

	// Get the struct's variable descriptions
	TArray<FStructVariableDescription>& Variables = const_cast<TArray<FStructVariableDescription>&>(
		FStructureEditorUtils::GetVarDesc(Struct)
	);

	// Find the field to modify
	bool bFieldFound = false;
	for (FStructVariableDescription& Variable : Variables)
	{
		if (Variable.VarName.ToString() == FieldName)
		{
			bFieldFound = true;

			// Determine the new type based on the input
			// For struct types, we need to load the struct and set it as the SubCategoryObject
			if (NewType.StartsWith(TEXT("F")) || NewType.StartsWith(TEXT("S_")))
			{
				// This is a struct type
				FString StructTypePath = NewType;
				if (!StructTypePath.Contains(TEXT(".")))
				{
					// Try to find the struct - could be C++ or blueprint
					// For C++ structs, they're in /Script/UETest1.StructName format
					// For blueprint structs, they're in /Game/Path/StructName.StructName format

					if (NewType.StartsWith(TEXT("FS_")))
					{
						// C++ struct - construct the path
						StructTypePath = FString::Printf(TEXT("/Script/UETest1.%s"), *NewType);
					}
					else if (NewType.StartsWith(TEXT("S_")))
					{
						// Blueprint struct - try common locations
						StructTypePath = FString::Printf(TEXT("/Game/Levels/LevelPrototyping/Data/%s.%s"), *NewType, *NewType);
					}
				}

				// Try to load as UScriptStruct (C++ struct) - try multiple search patterns
				UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, *StructTypePath);

				// If not found, try searching by struct name alone (for C++ structs)
				if (!ScriptStruct && NewType.StartsWith(TEXT("FS_")))
				{
					// For C++ structs, use FindPackage to get the module package
					UPackage* Package = FindPackage(nullptr, TEXT("/Script/UETest1"));
					if (Package)
					{
						ScriptStruct = FindObject<UScriptStruct>(Package, *NewType);
					}

					// If still not found, try LoadObject with full path
					if (!ScriptStruct)
					{
						FString PackagePath = FString::Printf(TEXT("/Script/UETest1.%s"), *NewType);
						ScriptStruct = LoadObject<UScriptStruct>(nullptr, *PackagePath);
					}

					// Last resort: iterate through all UScriptStruct objects
					if (!ScriptStruct)
					{
						TArray<FString> FoundStructNames;
						for (TObjectIterator<UScriptStruct> It; It; ++It)
						{
							FString StructName = It->GetName();
							// Collect all struct names that start with "FS_" or "S_" for debugging
							if (StructName.StartsWith(TEXT("FS_")) || StructName.StartsWith(TEXT("S_")))
							{
								FoundStructNames.Add(FString::Printf(TEXT("%s (%s)"), *StructName, *It->GetPathName()));
							}

							if (StructName == NewType)
							{
								ScriptStruct = *It;
								break;
							}
						}

						// If not found, include debug info about similar structs
						if (!ScriptStruct && FoundStructNames.Num() > 0)
						{
							// Limit to first 20 structs for debugging
							TArray<FString> LimitedList;
							for (int32 i = 0; i < FMath::Min(20, FoundStructNames.Num()); i++)
							{
								LimitedList.Add(FoundStructNames[i]);
							}
							FString DebugInfo = FString::Printf(TEXT("Found %d structs (showing max 20): %s"),
								FoundStructNames.Num(),
								*FString::Join(LimitedList, TEXT(", ")));
							UE_LOG(LogTemp, Warning, TEXT("Struct not found. %s"), *DebugInfo);
						}
					}
				}

				if (!ScriptStruct)
				{
					// Try to load as UserDefinedStruct (blueprint struct)
					UUserDefinedStruct* TargetStruct = LoadObject<UUserDefinedStruct>(nullptr, *StructTypePath);
					if (TargetStruct)
					{
						ScriptStruct = TargetStruct;
					}
				}

				if (ScriptStruct)
				{
					Variable.Category = UEdGraphSchema_K2::PC_Struct;
					Variable.SubCategoryObject = ScriptStruct;
					Variable.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Struct;
					Variable.PinValueType.TerminalSubCategoryObject = ScriptStruct;
				}
				else
				{
					return MakeError(FString::Printf(TEXT("Could not find struct type: %s (tried path: %s)"), *NewType, *StructTypePath));
				}
			}

			break;
		}
	}

	if (!bFieldFound)
	{
		return MakeError(FString::Printf(TEXT("Field not found: %s"), *FieldName));
	}

	// Recompile the struct
	FStructureEditorUtils::CompileStructure(Struct);

	// Mark as modified
	Struct->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Struct field modified successfully"));
	Data->SetStringField(TEXT("struct_path"), StructPath);
	Data->SetStringField(TEXT("field_name"), FieldName);
	Data->SetStringField(TEXT("new_type"), NewType);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetBlueprintCompileSettings(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	bool bModified = false;

	// Handle bRunConstructionScriptOnDrag setting
	if (Params->HasField(TEXT("run_construction_script_on_drag")))
	{
		bool bValue = Params->GetBoolField(TEXT("run_construction_script_on_drag"));
		Blueprint->bRunConstructionScriptOnDrag = bValue;
		bModified = true;
	}

	// Handle bGenerateConstClass setting
	if (Params->HasField(TEXT("generate_const_class")))
	{
		bool bValue = Params->GetBoolField(TEXT("generate_const_class"));
		Blueprint->bGenerateConstClass = bValue;
		bModified = true;
	}

	// Handle bForceFullEditor setting
	if (Params->HasField(TEXT("force_full_editor")))
	{
		bool bValue = Params->GetBoolField(TEXT("force_full_editor"));
		Blueprint->bForceFullEditor = bValue;
		bModified = true;
	}

	if (!bModified)
	{
		return MakeError(TEXT("No valid settings provided. Available settings: run_construction_script_on_drag, generate_const_class, force_full_editor"));
	}

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Blueprint compile settings updated successfully"));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleModifyFunctionMetadata(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) || !Params->HasField(TEXT("function_name")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' or 'function_name' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString FunctionName = Params->GetStringField(TEXT("function_name"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return MakeError(FString::Printf(TEXT("Function '%s' not found in blueprint"), *FunctionName));
	}

	// Find the function entry node
	UK2Node_FunctionEntry* EntryNode = nullptr;
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		EntryNode = Cast<UK2Node_FunctionEntry>(Node);
		if (EntryNode) break;
	}

	if (!EntryNode)
	{
		return MakeError(TEXT("Function entry node not found"));
	}

	bool bModified = false;

	// Handle BlueprintThreadSafe metadata
	if (Params->HasField(TEXT("blueprint_thread_safe")))
	{
		bool bValue = Params->GetBoolField(TEXT("blueprint_thread_safe"));
		if (bValue)
		{
			EntryNode->MetaData.bThreadSafe = true;
		}
		else
		{
			EntryNode->MetaData.bThreadSafe = false;
		}
		bModified = true;
	}

	// Handle BlueprintPure metadata
	if (Params->HasField(TEXT("blueprint_pure")))
	{
		bool bValue = Params->GetBoolField(TEXT("blueprint_pure"));
		EntryNode->MetaData.bCallInEditor = bValue;
		bModified = true;
	}

	if (!bModified)
	{
		return MakeError(TEXT("No valid metadata provided. Available metadata: blueprint_thread_safe, blueprint_pure"));
	}

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Function metadata updated successfully"));
	Data->SetStringField(TEXT("function_name"), FunctionName);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleCaptureScreenshot(const TSharedPtr<FJsonObject>& Params)
{
	// Get optional filename parameter
	FString Filename = TEXT("MCP_Screenshot");
	if (Params.IsValid() && Params->HasField(TEXT("filename")))
	{
		Filename = Params->GetStringField(TEXT("filename"));
		// Remove extension if provided - we'll add .png
		Filename.RemoveFromEnd(TEXT(".png"));
		Filename.RemoveFromEnd(TEXT(".jpg"));
	}

	// Add timestamp to ensure uniqueness
	FDateTime Now = FDateTime::Now();
	FString TimestampedFilename = FString::Printf(TEXT("%s_%s"),
		*Filename,
		*Now.ToString(TEXT("%Y%m%d_%H%M%S")));

	// Construct full path: ProjectDir/Saved/Screenshots/
	FString ProjectDir = FPaths::ProjectDir();
	FString ScreenshotDir = FPaths::Combine(ProjectDir, TEXT("Saved"), TEXT("Screenshots"));

	// Ensure directory exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*ScreenshotDir))
	{
		if (!PlatformFile.CreateDirectoryTree(*ScreenshotDir))
		{
			return MakeError(FString::Printf(TEXT("Failed to create screenshot directory: %s"), *ScreenshotDir));
		}
	}

	// Construct full file path
	FString FullPath = FPaths::Combine(ScreenshotDir, TimestampedFilename + TEXT(".png"));

	// Request screenshot - this captures the active viewport
	FScreenshotRequest::RequestScreenshot(FullPath, false, false);

	// Note: Screenshot is captured asynchronously, but the file should be written very quickly
	// We'll return immediately with the expected path

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetStringField(TEXT("message"), TEXT("Screenshot captured successfully"));
	ResponseData->SetStringField(TEXT("path"), FullPath);
	ResponseData->SetStringField(TEXT("filename"), TimestampedFilename + TEXT(".png"));

	return MakeResponse(true, ResponseData);
}

FString FMCPServer::HandleRemoveErrorNodes(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	bool bAutoRewire = Params->HasField(TEXT("auto_rewire")) ? Params->GetBoolField(TEXT("auto_rewire")) : true;

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// First, compile to identify error nodes
	FCompilerResultsLog CompileLog;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog);

	// Track nodes to remove
	TArray<UEdGraphNode*> NodesToRemove;
	int32 NodesRemoved = 0;
	int32 ConnectionsRewired = 0;

	// Get all graphs
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			bool bIsErrorNode = false;

			// Check for bad cast nodes (invalid target type)
			if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
			{
				// If the target class is null or invalid, this is a bad cast
				if (CastNode->TargetType == nullptr)
				{
					bIsErrorNode = true;
				}
			}

			// Check for function call nodes calling non-existent functions
			if (UK2Node_CallFunction* FunctionNode = Cast<UK2Node_CallFunction>(Node))
			{
				// Check if the function reference is valid
				UFunction* TargetFunction = FunctionNode->GetTargetFunction();
				if (!TargetFunction && FunctionNode->FunctionReference.GetMemberName() != NAME_None)
				{
					// Function is referenced but doesn't exist
					bIsErrorNode = true;
				}
			}

			// Check node's own error state
			if (Node->bHasCompilerMessage && Node->ErrorType >= EMessageSeverity::Error)
			{
				bIsErrorNode = true;
			}

			if (bIsErrorNode)
			{
				// Try to rewire execution flow around this node if requested
				if (bAutoRewire)
				{
					UEdGraphPin* ExecInputPin = nullptr;
					UEdGraphPin* ExecOutputPin = nullptr;

					// Find execution pins
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
						{
							if (Pin->Direction == EGPD_Input)
							{
								ExecInputPin = Pin;
							}
							else if (Pin->Direction == EGPD_Output)
							{
								ExecOutputPin = Pin;
							}
						}
					}

					// If we have both input and output exec pins, try to rewire
					if (ExecInputPin && ExecOutputPin && ExecInputPin->LinkedTo.Num() > 0 && ExecOutputPin->LinkedTo.Num() > 0)
					{
						UEdGraphPin* InputSource = ExecInputPin->LinkedTo[0];
						UEdGraphPin* OutputTarget = ExecOutputPin->LinkedTo[0];

						if (InputSource && OutputTarget)
						{
							InputSource->MakeLinkTo(OutputTarget);
							ConnectionsRewired++;
						}
					}
				}

				NodesToRemove.Add(Node);
			}
		}
	}

	// Remove the bad nodes
	for (UEdGraphNode* NodeToRemove : NodesToRemove)
	{
		if (NodeToRemove && NodeToRemove->GetGraph())
		{
			NodeToRemove->GetGraph()->RemoveNode(NodeToRemove);
			NodesRemoved++;
		}
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	// Recompile to verify fixes
	FCompilerResultsLog PostCompileLog;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &PostCompileLog);

	// For animation blueprints with tag errors, try removing disconnected nodes
	int32 DisconnectedNodesRemoved = 0;
	bool bHasTagErrors = false;
	for (const TSharedRef<FTokenizedMessage>& Message : PostCompileLog.Messages)
	{
		if (Message->GetSeverity() == EMessageSeverity::Error)
		{
			FString ErrorText = Message->ToText().ToString();
			if (ErrorText.Contains(TEXT("cannot find referenced node with tag")))
			{
				bHasTagErrors = true;
				break;
			}
		}
	}

	if (bHasTagErrors && Blueprint->IsA<UAnimBlueprint>())
	{
		// Find and remove disconnected nodes in animation graphs
		TArray<UEdGraph*> AnimGraphs;
		Blueprint->GetAllGraphs(AnimGraphs);

		for (UEdGraph* Graph : AnimGraphs)
		{
			if (!Graph || !Graph->GetName().Contains(TEXT("AnimGraph"))) continue;

			// Find all nodes that are disconnected (no output connections)
			TArray<UEdGraphNode*> DisconnectedNodes;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node) continue;

				// Skip the root/output node
				if (Node->GetClass()->GetName().Contains(TEXT("AnimGraphNode_Root"))) continue;

				// Check if this node has any output pose connections
				bool bHasOutputConnection = false;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
					{
						// Check if it's a pose link (not just exec or data)
						if (Pin->PinType.PinCategory == TEXT("struct") &&
							Pin->PinType.PinSubCategoryObject.IsValid() &&
							Pin->PinType.PinSubCategoryObject->GetName().Contains(TEXT("PoseLink")))
						{
							bHasOutputConnection = true;
							break;
						}
					}
				}

				// If no output pose connection, it's disconnected
				if (!bHasOutputConnection)
				{
					DisconnectedNodes.Add(Node);
				}
			}

			// Remove disconnected nodes
			for (UEdGraphNode* NodeToRemove : DisconnectedNodes)
			{
				Graph->RemoveNode(NodeToRemove);
				DisconnectedNodesRemoved++;
			}
		}

		// Recompile after removing disconnected nodes
		if (DisconnectedNodesRemoved > 0)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			FCompilerResultsLog FinalLog;
			FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &FinalLog);
			PostCompileLog = FinalLog;
		}
	}

	bool bSuccess = (Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Error nodes removed successfully"));
	Data->SetNumberField(TEXT("nodes_removed"), NodesRemoved);
	Data->SetNumberField(TEXT("connections_rewired"), ConnectionsRewired);
	Data->SetNumberField(TEXT("disconnected_nodes_removed"), DisconnectedNodesRemoved);
	Data->SetBoolField(TEXT("compiled_successfully"), bSuccess);

	// Get remaining error count
	int32 RemainingErrors = 0;
	for (const TSharedRef<FTokenizedMessage>& Message : PostCompileLog.Messages)
	{
		if (Message->GetSeverity() == EMessageSeverity::Error)
		{
			RemainingErrors++;
		}
	}
	Data->SetNumberField(TEXT("remaining_errors"), RemainingErrors);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleClearAnimationBlueprintTags(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	bool bRemoveExtension = Params->HasField(TEXT("remove_extension")) ? Params->GetBoolField(TEXT("remove_extension")) : false;

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
	if (!AnimBP)
	{
		return MakeError(TEXT("Blueprint is not an Animation Blueprint"));
	}

	TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
	int32 RemovedCount = 0;
	int32 ExtensionsRemoved = 0;

	// Find AnimBlueprintExtension_Tag objects
	TArray<UBlueprintExtension*> ExtensionsToRemove;
	TArray<FString> AllExtensions;

	for (UBlueprintExtension* Extension : AnimBP->GetExtensions())
	{
		if (Extension)
		{
			FString ClassName = Extension->GetClass()->GetName();
			AllExtensions.Add(ClassName);
			if (ClassName.Contains(TEXT("Tag")))
			{
				ExtensionsToRemove.Add(Extension);
			}
		}
	}

	// Output all extension names for debugging
	TArray<TSharedPtr<FJsonValue>> ExtArray;
	for (const FString& ExtName : AllExtensions)
	{
		ExtArray.Add(MakeShared<FJsonValueString>(ExtName));
	}
	Data->SetArrayField(TEXT("all_extensions"), ExtArray);
	Data->SetNumberField(TEXT("tag_extensions_found"), ExtensionsToRemove.Num());

	// If remove_extension is true, actually remove the extension from the blueprint
	if (bRemoveExtension && ExtensionsToRemove.Num() > 0)
	{
		AnimBP->Modify();

		// Get direct access to the extensions array via reflection
		FArrayProperty* ExtensionsProp = FindFProperty<FArrayProperty>(UBlueprint::StaticClass(), TEXT("Extensions"));
		if (ExtensionsProp)
		{
			void* ArrayPtr = ExtensionsProp->ContainerPtrToValuePtr<void>(AnimBP);
			FScriptArrayHelper ArrayHelper(ExtensionsProp, ArrayPtr);

			// Remove extensions in reverse order to maintain indices
			for (int32 i = ArrayHelper.Num() - 1; i >= 0; i--)
			{
				if (ArrayHelper.IsValidIndex(i))
				{
					FObjectProperty* InnerProp = CastField<FObjectProperty>(ExtensionsProp->Inner);
					if (InnerProp)
					{
						UObject* ExtObj = InnerProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(i));
						if (ExtObj && ExtObj->GetClass()->GetName().Contains(TEXT("Tag")))
						{
							ArrayHelper.RemoveValues(i, 1);
							ExtensionsRemoved++;
						}
					}
				}
			}
		}

		Data->SetNumberField(TEXT("extensions_removed"), ExtensionsRemoved);

		if (ExtensionsRemoved > 0)
		{
			AnimBP->MarkPackageDirty();

			// Save immediately
			UPackage* Package = AnimBP->GetOutermost();
			FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			bool bSaved = UPackage::SavePackage(Package, AnimBP, *PackageFilename, SaveArgs);

			if (bSaved)
			{
				Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Removed %d tag extension(s) and saved. Restart editor to reload."), ExtensionsRemoved));
			}
			else
			{
				Data->SetStringField(TEXT("message"), TEXT("Extensions removed but save failed."));
			}

			return MakeResponse(true, Data);
		}
	}

	// Otherwise, just clear the data inside the extensions
	TArray<FString> FoundProperties;

	for (UBlueprintExtension* Extension : ExtensionsToRemove)
	{
		// Iterate all properties to find tag-related data
		for (TFieldIterator<FProperty> PropIt(Extension->GetClass()); PropIt; ++PropIt)
		{
			FProperty* Prop = *PropIt;
			FString PropInfo = FString::Printf(TEXT("%s (%s)"), *Prop->GetName(), *Prop->GetClass()->GetName());
			FoundProperties.Add(PropInfo);

			// Try map properties
			if (FMapProperty* MapProp = CastField<FMapProperty>(Prop))
			{
				void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(Extension);
				FScriptMapHelper MapHelper(MapProp, MapPtr);
				int32 NumEntries = MapHelper.Num();
				if (NumEntries > 0)
				{
					MapHelper.EmptyValues();
					RemovedCount += NumEntries;
				}
			}
			// Try array properties
			else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
			{
				void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(Extension);
				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);
				int32 NumEntries = ArrayHelper.Num();
				if (NumEntries > 0)
				{
					ArrayHelper.EmptyValues();
					RemovedCount += NumEntries;
				}
			}
			// Try struct properties - dig into them to find maps/arrays
			else if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				FoundProperties.Add(FString::Printf(TEXT("  -> Struct: %s"), *StructProp->Struct->GetName()));

				// Get pointer to the struct data
				void* StructPtr = StructProp->ContainerPtrToValuePtr<void>(Extension);

				// Iterate properties inside the struct
				for (TFieldIterator<FProperty> StructPropIt(StructProp->Struct); StructPropIt; ++StructPropIt)
				{
					FProperty* InnerProp = *StructPropIt;
					FString InnerPropInfo = FString::Printf(TEXT("    -> %s (%s)"), *InnerProp->GetName(), *InnerProp->GetClass()->GetName());
					FoundProperties.Add(InnerPropInfo);

					// Check for maps inside struct
					if (FMapProperty* InnerMapProp = CastField<FMapProperty>(InnerProp))
					{
						void* MapPtr = InnerMapProp->ContainerPtrToValuePtr<void>(StructPtr);
						FScriptMapHelper MapHelper(InnerMapProp, MapPtr);
						int32 NumEntries = MapHelper.Num();
						FoundProperties.Add(FString::Printf(TEXT("      Map entries: %d"), NumEntries));
						if (NumEntries > 0)
						{
							MapHelper.EmptyValues();
							RemovedCount += NumEntries;
						}
					}
					// Check for arrays inside struct
					else if (FArrayProperty* InnerArrayProp = CastField<FArrayProperty>(InnerProp))
					{
						void* ArrayPtr = InnerArrayProp->ContainerPtrToValuePtr<void>(StructPtr);
						FScriptArrayHelper ArrayHelper(InnerArrayProp, ArrayPtr);
						int32 NumEntries = ArrayHelper.Num();
						FoundProperties.Add(FString::Printf(TEXT("      Array entries: %d"), NumEntries));
						if (NumEntries > 0)
						{
							ArrayHelper.EmptyValues();
							RemovedCount += NumEntries;
						}
					}
				}
			}
		}
	}

	// Store found properties for debugging
	TArray<TSharedPtr<FJsonValue>> PropsArray;
	for (const FString& PropInfo : FoundProperties)
	{
		PropsArray.Add(MakeShared<FJsonValueString>(PropInfo));
	}
	Data->SetArrayField(TEXT("found_properties"), PropsArray);

	if (RemovedCount > 0)
	{
		AnimBP->MarkPackageDirty();

		// Save immediately to persist changes before any compilation attempt
		UPackage* Package = AnimBP->GetOutermost();
		FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		bool bSaved = UPackage::SavePackage(Package, AnimBP, *PackageFilename, SaveArgs);

		if (bSaved)
		{
			Data->SetStringField(TEXT("message"), TEXT("Tag mappings cleared and saved. Restart the editor to reload cleanly."));
		}
		else
		{
			Data->SetStringField(TEXT("message"), TEXT("Tag mappings cleared but save failed. Try saving manually."));
		}
	}
	else
	{
		Data->SetStringField(TEXT("message"), TEXT("No tag mappings found to clear. Try with remove_extension: true to fully remove the tag extension."));
	}

	Data->SetNumberField(TEXT("removed_count"), RemovedCount);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleClearAnimGraph(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
	if (!AnimBP)
	{
		return MakeError(TEXT("Blueprint is not an Animation Blueprint"));
	}

	// Find the AnimGraph - check both FunctionGraphs and UbergraphPages
	UEdGraph* AnimGraph = nullptr;

	// First check FunctionGraphs (where AnimGraph is typically stored)
	for (UEdGraph* Graph : AnimBP->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == TEXT("AnimGraph"))
		{
			AnimGraph = Graph;
			break;
		}
	}

	// If not found, check UbergraphPages
	if (!AnimGraph)
	{
		for (UEdGraph* Graph : AnimBP->UbergraphPages)
		{
			if (Graph && Graph->GetName() == TEXT("AnimGraph"))
			{
				AnimGraph = Graph;
				break;
			}
		}
	}

	if (!AnimGraph)
	{
		return MakeError(TEXT("AnimGraph not found in animation blueprint"));
	}

	TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
	int32 DeletedCount = 0;

	// Collect all nodes except the root output node
	TArray<UEdGraphNode*> NodesToDelete;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (Node && !Node->GetClass()->GetName().Contains(TEXT("AnimGraphNode_Root")))
		{
			NodesToDelete.Add(Node);
		}
	}

	// Delete the nodes
	for (UEdGraphNode* Node : NodesToDelete)
	{
		AnimGraph->RemoveNode(Node);
		DeletedCount++;
	}

	// Also clear all nested/sub-graphs (state machines, transitions, etc.)
	// This prevents crashes from orphaned nodes in nested graphs
	TArray<UEdGraph*> AllGraphs;
	AllGraphs.Append(AnimBP->FunctionGraphs);
	AllGraphs.Append(AnimBP->UbergraphPages);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph && Graph != AnimGraph)
		{
			// Clear all nodes from this sub-graph
			TArray<UEdGraphNode*> SubGraphNodesToDelete;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node)
				{
					SubGraphNodesToDelete.Add(Node);
				}
			}

			for (UEdGraphNode* Node : SubGraphNodesToDelete)
			{
				Graph->RemoveNode(Node);
				DeletedCount++;
			}
		}
	}

	if (DeletedCount > 0)
	{
		AnimBP->MarkPackageDirty();
		Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Deleted %d nodes from AnimGraph and all nested graphs (state machines, transitions, etc.). Blueprint marked dirty. Compile manually in the editor."), DeletedCount));
	}
	else
	{
		Data->SetStringField(TEXT("message"), TEXT("No nodes to delete (only root output node found)."));
	}

	Data->SetNumberField(TEXT("deleted_count"), DeletedCount);

	return MakeResponse(true, Data);
}

// ==================================================
// Sprint 1: Blueprint Function Creation Commands
// ==================================================

FString FMCPServer::HandleCreateBlueprintFunction(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) || !Params->HasField(TEXT("function_name")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' or 'function_name' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString FunctionName = Params->GetStringField(TEXT("function_name"));

	// Optional parameters
	const bool bIsPure = Params->HasField(TEXT("is_pure")) ? Params->GetBoolField(TEXT("is_pure")) : false;
	const bool bIsThreadSafe = Params->HasField(TEXT("is_thread_safe")) ? Params->GetBoolField(TEXT("is_thread_safe")) : false;
	const bool bIsConst = Params->HasField(TEXT("is_const")) ? Params->GetBoolField(TEXT("is_const")) : false;

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Check if function already exists
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			return MakeError(FString::Printf(TEXT("Function '%s' already exists in blueprint"), *FunctionName));
		}
	}

	// Create new function graph using the blueprint editor utilities
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (!NewGraph)
	{
		return MakeError(TEXT("Failed to create function graph"));
	}

	// Initialize the graph schema
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	K2Schema->CreateDefaultNodesForGraph(*NewGraph);

	// Add to blueprint's function graphs array
	Blueprint->FunctionGraphs.Add(NewGraph);

	// Find the function entry node to set metadata
	UK2Node_FunctionEntry* EntryNode = nullptr;
	for (UEdGraphNode* Node : NewGraph->Nodes)
	{
		EntryNode = Cast<UK2Node_FunctionEntry>(Node);
		if (EntryNode)
		{
			break;
		}
	}

	if (EntryNode)
	{
		// Set function flags
		if (bIsPure)
		{
			EntryNode->AddExtraFlags(FUNC_BlueprintPure);
		}
		if (bIsThreadSafe)
		{
			EntryNode->MetaData.bThreadSafe = true;
		}
		if (bIsConst)
		{
			EntryNode->AddExtraFlags(FUNC_Const);
		}
	}

	// Mark blueprint as structurally modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Function created successfully"));
	Data->SetStringField(TEXT("function_name"), FunctionName);
	Data->SetBoolField(TEXT("is_pure"), bIsPure);
	Data->SetBoolField(TEXT("is_thread_safe"), bIsThreadSafe);
	Data->SetBoolField(TEXT("is_const"), bIsConst);
	Data->SetNumberField(TEXT("total_functions"), Blueprint->FunctionGraphs.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleAddFunctionInput(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("function_name")) || !Params->HasField(TEXT("parameter_name")) ||
		!Params->HasField(TEXT("parameter_type")))
	{
		return MakeError(TEXT("Missing required parameters: 'blueprint_path', 'function_name', 'parameter_name', 'parameter_type'"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString FunctionName = Params->GetStringField(TEXT("function_name"));
	const FString ParameterName = Params->GetStringField(TEXT("parameter_name"));
	const FString ParameterType = Params->GetStringField(TEXT("parameter_type"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return MakeError(FString::Printf(TEXT("Function '%s' not found in blueprint"), *FunctionName));
	}

	// Find the function entry node
	UK2Node_FunctionEntry* EntryNode = nullptr;
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		EntryNode = Cast<UK2Node_FunctionEntry>(Node);
		if (EntryNode)
		{
			break;
		}
	}

	if (!EntryNode)
	{
		return MakeError(TEXT("Function entry node not found"));
	}

	// Parse parameter type and create pin
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	FEdGraphPinType PinType;

	// Simple type parsing (can be expanded)
	if (ParameterType == TEXT("int") || ParameterType == TEXT("int32"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (ParameterType == TEXT("float") || ParameterType == TEXT("double"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (ParameterType == TEXT("bool") || ParameterType == TEXT("boolean"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (ParameterType == TEXT("string") || ParameterType == TEXT("FString"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (ParameterType == TEXT("FVector"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (ParameterType == TEXT("FRotator"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (ParameterType == TEXT("FTransform"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else
	{
		// Try to load as object type or struct
		UObject* TypeObject = FindObject<UObject>(nullptr, *ParameterType);
		if (!TypeObject)
		{
			TypeObject = LoadObject<UObject>(nullptr, *ParameterType);
		}

		if (UClass* Class = Cast<UClass>(TypeObject))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			PinType.PinSubCategoryObject = Class;
		}
		else if (UScriptStruct* Struct = Cast<UScriptStruct>(TypeObject))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			PinType.PinSubCategoryObject = Struct;
		}
		else
		{
			return MakeError(FString::Printf(TEXT("Unknown parameter type: %s"), *ParameterType));
		}
	}

	// Create the new user-defined pin
	EntryNode->CreateUserDefinedPin(FName(*ParameterName), PinType, EGPD_Output);

	// Mark blueprint as structurally modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Function input parameter added successfully"));
	Data->SetStringField(TEXT("function_name"), FunctionName);
	Data->SetStringField(TEXT("parameter_name"), ParameterName);
	Data->SetStringField(TEXT("parameter_type"), ParameterType);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleAddFunctionOutput(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("function_name")) || !Params->HasField(TEXT("parameter_name")) ||
		!Params->HasField(TEXT("parameter_type")))
	{
		return MakeError(TEXT("Missing required parameters: 'blueprint_path', 'function_name', 'parameter_name', 'parameter_type'"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString FunctionName = Params->GetStringField(TEXT("function_name"));
	const FString ParameterName = Params->GetStringField(TEXT("parameter_name"));
	const FString ParameterType = Params->GetStringField(TEXT("parameter_type"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return MakeError(FString::Printf(TEXT("Function '%s' not found in blueprint"), *FunctionName));
	}

	// Find the function result node
	UK2Node_FunctionResult* ResultNode = nullptr;
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		ResultNode = Cast<UK2Node_FunctionResult>(Node);
		if (ResultNode)
		{
			break;
		}
	}

	// If no result node exists, create one
	if (!ResultNode)
	{
		ResultNode = NewObject<UK2Node_FunctionResult>(FunctionGraph);
		ResultNode->CreateNewGuid();
		ResultNode->PostPlacedNewNode();
		ResultNode->AllocateDefaultPins();
		FunctionGraph->AddNode(ResultNode);
	}

	// Parse parameter type and create pin (same logic as input)
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	FEdGraphPinType PinType;

	if (ParameterType == TEXT("int") || ParameterType == TEXT("int32"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (ParameterType == TEXT("float") || ParameterType == TEXT("double"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (ParameterType == TEXT("bool") || ParameterType == TEXT("boolean"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (ParameterType == TEXT("string") || ParameterType == TEXT("FString"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (ParameterType == TEXT("FVector"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (ParameterType == TEXT("FRotator"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (ParameterType == TEXT("FTransform"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else
	{
		UObject* TypeObject = FindObject<UObject>(nullptr, *ParameterType);
		if (!TypeObject)
		{
			TypeObject = LoadObject<UObject>(nullptr, *ParameterType);
		}

		if (UClass* Class = Cast<UClass>(TypeObject))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			PinType.PinSubCategoryObject = Class;
		}
		else if (UScriptStruct* Struct = Cast<UScriptStruct>(TypeObject))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			PinType.PinSubCategoryObject = Struct;
		}
		else
		{
			return MakeError(FString::Printf(TEXT("Unknown parameter type: %s"), *ParameterType));
		}
	}

	// Create the new user-defined pin for output
	ResultNode->CreateUserDefinedPin(FName(*ParameterName), PinType, EGPD_Input);

	// Mark blueprint as structurally modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Function output parameter added successfully"));
	Data->SetStringField(TEXT("function_name"), FunctionName);
	Data->SetStringField(TEXT("parameter_name"), ParameterName);
	Data->SetStringField(TEXT("parameter_type"), ParameterType);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleRenameBlueprintFunction(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("old_function_name")) || !Params->HasField(TEXT("new_function_name")))
	{
		return MakeError(TEXT("Missing required parameters: 'blueprint_path', 'old_function_name', 'new_function_name'"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString OldFunctionName = Params->GetStringField(TEXT("old_function_name"));
	const FString NewFunctionName = Params->GetStringField(TEXT("new_function_name"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Find the function graph to rename
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == OldFunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return MakeError(FString::Printf(TEXT("Function '%s' not found in blueprint"), *OldFunctionName));
	}

	// Check if new name already exists
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph != FunctionGraph && Graph->GetName() == NewFunctionName)
		{
			return MakeError(FString::Printf(TEXT("Function '%s' already exists in blueprint"), *NewFunctionName));
		}
	}

	// Rename the graph
	FBlueprintEditorUtils::RenameGraph(FunctionGraph, NewFunctionName);

	// Mark blueprint as structurally modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Function renamed successfully"));
	Data->SetStringField(TEXT("old_name"), OldFunctionName);
	Data->SetStringField(TEXT("new_name"), NewFunctionName);

	return MakeResponse(true, Data);
}

/**
 * HandleReadActorProperties - Read actual property values from a level actor instance
 *
 * RECOMMENDED METHOD (2026-02-03):
 * This is the CORRECT way to read actual working values from a Blueprint actor in a level.
 *
 * WHY USE THIS INSTEAD OF read_class_defaults:
 * - Reads property overrides set in the Details panel (stored in level .umap file)
 * - Returns ACTUAL gameplay-tested values, not class defaults
 * - Gets the real working behavior from a reference project
 * - Essential for accurate Blueprint-to-C++ conversion
 *
 * WORKFLOW FOR COPYING REFERENCE DATA:
 * When you need to copy correct values from a working project (e.g., GameAnimationSample):
 * 1. Copy the level file (.umap) from the working project to your project
 * 2. Open the level in Unreal Editor
 * 3. Use this command to read the actor instance
 * 4. Extract the property values from the response
 * 5. Use those values in your C++ implementation
 *
 * EXAMPLE - LevelVisuals Style Values:
 * - Blueprint CDO (read_class_defaults): FogColor = (0.79, 0.75, 1.0) ❌ Wrong
 * - Level Instance (this function): FogColor = (0.261, 0.261, 0.302) ✅ Correct
 *
 * The level instance values match the actual gameplay behavior seen in the reference project.
 *
 * USE CASES:
 * - Preserving actor configuration before blueprint reparenting
 * - Getting reference values from a working project for C++ conversion
 * - Debugging property override issues
 * - Comparing class defaults vs instance values
 */
FString FMCPServer::HandleReadActorProperties(const TSharedPtr<FJsonObject>& Params)
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

	// Find the actor by name in the current level
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

	// Serialize actor properties to JSON
	TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
	UClass* ActorClass = FoundActor->GetClass();

	for (TFieldIterator<FProperty> PropIt(ActorClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		// Only export EditAnywhere properties to preserve instance data
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}

		FString PropertyName = Property->GetName();
		FString PropertyValue;
		Property->ExportTextItem_Direct(PropertyValue, Property->ContainerPtrToValuePtr<void>(FoundActor), nullptr, FoundActor, PPF_None);

		PropertiesObj->SetStringField(PropertyName, PropertyValue);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("actor_name"), ActorName);
	Data->SetStringField(TEXT("actor_class"), ActorClass->GetName());
	Data->SetStringField(TEXT("actor_label"), FoundActor->GetActorLabel());
	Data->SetObjectField(TEXT("properties"), PropertiesObj);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("actor_name")) || !Params->HasField(TEXT("properties")))
	{
		return MakeError(TEXT("Missing required parameters: 'actor_name', 'properties'"));
	}

	const FString ActorName = Params->GetStringField(TEXT("actor_name"));
	const TSharedPtr<FJsonObject> PropertiesObj = Params->GetObjectField(TEXT("properties"));

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeError(TEXT("No world available"));
	}

	// Find the actor by name
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

	// Mark the actor as modified
	FoundActor->Modify();

	// Deserialize properties from JSON back to actor
	UClass* ActorClass = FoundActor->GetClass();
	int32 PropertiesSet = 0;

	for (auto& Pair : PropertiesObj->Values)
	{
		FString PropertyName = Pair.Key;
		FString PropertyValue = Pair.Value->AsString();

		FProperty* Property = ActorClass->FindPropertyByName(FName(*PropertyName));
		if (!Property)
		{
			UE_LOG(LogTemp, Warning, TEXT("Property not found: %s"), *PropertyName);
			continue;
		}

		// Only set EditAnywhere properties
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			UE_LOG(LogTemp, Warning, TEXT("Property not editable: %s"), *PropertyName);
			continue;
		}

		// Import the property value
		Property->ImportText_Direct(*PropertyValue, Property->ContainerPtrToValuePtr<void>(FoundActor), FoundActor, PPF_None);
		PropertiesSet++;
	}

	// Mark the actor for saving
	FoundActor->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Actor properties set successfully"));
	Data->SetStringField(TEXT("actor_name"), ActorName);
	Data->SetNumberField(TEXT("properties_set"), PropertiesSet);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetActorComponentProperty(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("actor_name")) || !Params->HasField(TEXT("component_name")) ||
		!Params->HasField(TEXT("property_name")) || !Params->HasField(TEXT("property_value")))
	{
		return MakeError(TEXT("Missing required parameters: 'actor_name', 'component_name', 'property_name', 'property_value'"));
	}

	const FString ActorName = Params->GetStringField(TEXT("actor_name"));
	const FString ComponentName = Params->GetStringField(TEXT("component_name"));
	const FString PropertyName = Params->GetStringField(TEXT("property_name"));
	const FString PropertyValue = Params->GetStringField(TEXT("property_value"));

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
		if (Component->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
		{
			TargetComponent = Component;
			break;
		}
	}

	if (!TargetComponent)
	{
		return MakeError(FString::Printf(TEXT("Component not found on actor '%s': %s"), *ActorName, *ComponentName));
	}

	TargetComponent->Modify();

	bool bSet = false;
	if (PropertyName.Equals(TEXT("CollisionProfileName"), ESearchCase::IgnoreCase))
	{
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(TargetComponent))
		{
			Prim->SetCollisionProfileName(FName(*PropertyValue));
			bSet = true;
		}
	}

	if (!bSet)
	{
		FProperty* Property = TargetComponent->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!Property)
		{
			return MakeError(FString::Printf(TEXT("Property not found: %s"), *PropertyName));
		}

		if (!Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			return MakeError(FString::Printf(TEXT("Property not editable: %s"), *PropertyName));
		}

		if (!Property->ImportText_Direct(*PropertyValue, Property->ContainerPtrToValuePtr<void>(TargetComponent), TargetComponent, PPF_None))
		{
			return MakeError(FString::Printf(TEXT("Failed to set property value: %s"), *PropertyValue));
		}
	}

	if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(TargetComponent))
	{
		Prim->MarkRenderStateDirty();
	}

	FoundActor->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Actor component property set successfully"));
	Data->SetStringField(TEXT("actor_name"), ActorName);
	Data->SetStringField(TEXT("component_name"), TargetComponent->GetName());
	Data->SetStringField(TEXT("property_name"), PropertyName);
	Data->SetStringField(TEXT("property_value"), PropertyValue);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReconstructActor(const TSharedPtr<FJsonObject>& Params)
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

	// Find the actor by name
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

	// Mark the actor as modified
	FoundActor->Modify();

	// Rerun construction scripts to trigger OnConstruction
	FoundActor->RerunConstructionScripts();

	// Mark the actor for saving
	FoundActor->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Actor reconstructed successfully"));
	Data->SetStringField(TEXT("actor_name"), ActorName);
	Data->SetStringField(TEXT("actor_class"), FoundActor->GetClass()->GetName());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleClearComponentMapValueArray(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Invalid parameters"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString ComponentName = Params->GetStringField(TEXT("component_name"));
	const FString MapPropertyName = Params->GetStringField(TEXT("map_property_name"));
	const FString MapKey = Params->GetStringField(TEXT("map_key"));
	const FString ArrayPropertyName = Params->GetStringField(TEXT("array_property_name"));

	if (BlueprintPath.IsEmpty() || ComponentName.IsEmpty() || MapPropertyName.IsEmpty() ||
		MapKey.IsEmpty() || ArrayPropertyName.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters"));
	}

	// Load the blueprint
	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	// Get the SimpleConstructionScript
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		return MakeError(TEXT("Blueprint has no SimpleConstructionScript"));
	}

	// Find the component node
	USCS_Node* ComponentNode = nullptr;
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			ComponentNode = Node;
			break;
		}
	}

	if (!ComponentNode)
	{
		return MakeError(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}

	// Get the component template
	UObject* ComponentTemplate = ComponentNode->ComponentTemplate;
	if (!ComponentTemplate)
	{
		return MakeError(TEXT("Component template is null"));
	}

	// Find the map property
	FMapProperty* MapProperty = FindFProperty<FMapProperty>(ComponentTemplate->GetClass(), *MapPropertyName);
	if (!MapProperty)
	{
		return MakeError(FString::Printf(TEXT("Map property not found: %s"), *MapPropertyName));
	}

	// Get the map helper
	void* MapPtr = MapProperty->ContainerPtrToValuePtr<void>(ComponentTemplate);
	FScriptMapHelper MapHelper(MapProperty, MapPtr);

	// Find the key in the map
	int32 FoundIndex = -1;
	FProperty* KeyProp = MapProperty->KeyProp;

	// Create a temporary key to search with
	void* TempKey = FMemory::Malloc(KeyProp->GetSize());
	KeyProp->InitializeValue(TempKey);

	if (FNameProperty* NameKeyProp = CastField<FNameProperty>(KeyProp))
	{
		FName KeyName(*MapKey);
		NameKeyProp->SetPropertyValue(TempKey, KeyName);
	}
	else if (FStrProperty* StrKeyProp = CastField<FStrProperty>(KeyProp))
	{
		StrKeyProp->SetPropertyValue(TempKey, MapKey);
	}
	else
	{
		KeyProp->DestroyValue(TempKey);
		FMemory::Free(TempKey);
		return MakeError(TEXT("Unsupported key type (only FName and FString supported)"));
	}

	// Find the key in the map
	for (int32 i = 0; i < MapHelper.Num(); i++)
	{
		if (MapHelper.IsValidIndex(i))
		{
			const void* KeyPtr = MapHelper.GetKeyPtr(i);
			if (KeyProp->Identical(KeyPtr, TempKey))
			{
				FoundIndex = i;
				break;
			}
		}
	}

	KeyProp->DestroyValue(TempKey);
	FMemory::Free(TempKey);

	if (FoundIndex == -1)
	{
		return MakeError(FString::Printf(TEXT("Key not found in map: %s"), *MapKey));
	}

	// Get the value object at this index
	uint8* ValuePtr = MapHelper.GetValuePtr(FoundIndex);
	FObjectProperty* ValueProp = CastField<FObjectProperty>(MapProperty->ValueProp);
	if (!ValueProp)
	{
		return MakeError(TEXT("Map value is not an object property"));
	}

	UObject* ValueObject = ValueProp->GetObjectPropertyValue(ValuePtr);
	if (!ValueObject)
	{
		return MakeError(TEXT("Map value object is null"));
	}

	// Find the array property in the value object
	FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(ValueObject->GetClass(), *ArrayPropertyName);
	if (!ArrayProp)
	{
		return MakeError(FString::Printf(TEXT("Array property not found in value object: %s"), *ArrayPropertyName));
	}

	// Mark for modification
	Blueprint->Modify();
	ComponentTemplate->Modify();
	ValueObject->Modify();

	// Get the array helper and clear it
	void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(ValueObject);
	FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);

	int32 OldSize = ArrayHelper.Num();
	ArrayHelper.EmptyValues();

	// Mark as dirty
	Blueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Array cleared successfully"));
	Data->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Data->SetStringField(TEXT("component_name"), ComponentName);
	Data->SetStringField(TEXT("map_property"), MapPropertyName);
	Data->SetStringField(TEXT("map_key"), MapKey);
	Data->SetStringField(TEXT("array_property"), ArrayPropertyName);
	Data->SetNumberField(TEXT("elements_cleared"), OldSize);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReplaceComponentClass(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString ComponentName = Params->GetStringField(TEXT("component_name"));
	FString NewComponentClass = Params->GetStringField(TEXT("new_class"));

	if (BlueprintPath.IsEmpty() || ComponentName.IsEmpty() || NewComponentClass.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, component_name, or new_class"));
	}

	// Load the blueprint
	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	if (!Blueprint->SimpleConstructionScript)
	{
		return MakeError(TEXT("Blueprint has no SimpleConstructionScript"));
	}

	// Find the component node
	USCS_Node* ComponentNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			ComponentNode = Node;
			break;
		}
	}

	if (!ComponentNode)
	{
		return MakeError(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
	}

	// Find the new component class
	UClass* NewClass = FindFirstObject<UClass>(*NewComponentClass, EFindFirstObjectOptions::ExactClass);
	if (!NewClass)
	{
		// Try with _C suffix for blueprint classes
		NewClass = FindFirstObject<UClass>(*(NewComponentClass + TEXT("_C")), EFindFirstObjectOptions::ExactClass);
	}
	if (!NewClass)
	{
		// Try loading as path
		NewClass = LoadClass<UActorComponent>(nullptr, *NewComponentClass);
	}
	if (!NewClass)
	{
		return MakeError(FString::Printf(TEXT("Component class not found: %s"), *NewComponentClass));
	}

	// Verify it's an ActorComponent
	if (!NewClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return MakeError(FString::Printf(TEXT("Class is not an ActorComponent: %s"), *NewComponentClass));
	}

	FString OldClassName = ComponentNode->ComponentClass ? ComponentNode->ComponentClass->GetName() : TEXT("None");

	// Mark for modification
	Blueprint->Modify();
	if (ComponentNode->ComponentTemplate)
	{
		ComponentNode->ComponentTemplate->Modify();
	}

	// Replace the component class
	ComponentNode->ComponentClass = NewClass;

	// Recreate the component template with the new class
	if (ComponentNode->ComponentTemplate)
	{
		// Destroy old template
		ComponentNode->ComponentTemplate = nullptr;
	}

	// Create new template
	ComponentNode->ComponentTemplate = NewObject<UActorComponent>(
		Blueprint->SimpleConstructionScript,
		NewClass,
		*ComponentName,
		RF_ArchetypeObject | RF_Public | RF_Transactional
	);

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Blueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Component class replaced successfully"));
	Data->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Data->SetStringField(TEXT("component_name"), ComponentName);
	Data->SetStringField(TEXT("old_class"), OldClassName);
	Data->SetStringField(TEXT("new_class"), NewClass->GetName());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetBlueprintCDOProperty(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString PropertyName = Params->GetStringField(TEXT("property_name"));
	FString PropertyValue = Params->GetStringField(TEXT("property_value"));

	if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path or property_name"));
	}

	// Load the blueprint
	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Get the generated class and its CDO
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

	// Find the property (search through entire class hierarchy including inherited properties)
	FProperty* Property = nullptr;
	for (TFieldIterator<FProperty> PropIt(GeneratedClass); PropIt; ++PropIt)
	{
		if (PropIt->GetName() == PropertyName)
		{
			Property = *PropIt;
			break;
		}
	}

	if (!Property)
	{
		// Also try FindPropertyByName as fallback
		Property = GeneratedClass->FindPropertyByName(*PropertyName);
	}

	if (!Property)
	{
		return MakeError(FString::Printf(TEXT("Property not found: %s"), *PropertyName));
	}

	// Mark for modification
	Blueprint->Modify();
	CDO->Modify();

	// Handle different property types
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(CDO);

	// Handle object property (TObjectPtr<>, UObject*, etc.)
	if (FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
	{
		if (PropertyValue.IsEmpty() || PropertyValue.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			// Clear the property
			ObjectProp->SetObjectPropertyValue(ValuePtr, nullptr);
		}
		else
		{
			// Load the object from the path
			UObject* ObjectValue = LoadObject<UObject>(nullptr, *PropertyValue);
			if (!ObjectValue)
			{
				// Try adding the asset name suffix (e.g., /Game/Input/IA_Sprint -> /Game/Input/IA_Sprint.IA_Sprint)
				FString AssetPath = PropertyValue;
				if (!AssetPath.Contains(TEXT(".")))
				{
					FString AssetName = FPaths::GetBaseFilename(AssetPath);
					AssetPath = AssetPath + TEXT(".") + AssetName;
				}
				ObjectValue = LoadObject<UObject>(nullptr, *AssetPath);
			}

			if (!ObjectValue)
			{
				return MakeError(FString::Printf(TEXT("Could not load object: %s"), *PropertyValue));
			}

			// Verify type compatibility
			if (!ObjectValue->IsA(ObjectProp->PropertyClass))
			{
				return MakeError(FString::Printf(TEXT("Object %s is not compatible with property type %s"),
					*ObjectValue->GetClass()->GetName(), *ObjectProp->PropertyClass->GetName()));
			}

			ObjectProp->SetObjectPropertyValue(ValuePtr, ObjectValue);
		}
	}
	// Handle soft object property (TSoftObjectPtr<>)
	else if (FSoftObjectProperty* SoftObjectProp = CastField<FSoftObjectProperty>(Property))
	{
		if (PropertyValue.IsEmpty() || PropertyValue.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			FSoftObjectPtr NullPtr;
			SoftObjectProp->SetPropertyValue(ValuePtr, NullPtr);
		}
		else
		{
			FSoftObjectPath SoftPath(PropertyValue);
			FSoftObjectPtr SoftPtr(SoftPath);
			SoftObjectProp->SetPropertyValue(ValuePtr, SoftPtr);
		}
	}
	// Handle class property (TSubclassOf<>)
	else if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
	{
		if (PropertyValue.IsEmpty() || PropertyValue.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			ClassProp->SetObjectPropertyValue(ValuePtr, nullptr);
		}
		else
		{
			UClass* ClassValue = LoadClass<UObject>(nullptr, *PropertyValue);
			if (!ClassValue)
			{
				return MakeError(FString::Printf(TEXT("Could not load class: %s"), *PropertyValue));
			}

			if (!ClassValue->IsChildOf(ClassProp->MetaClass))
			{
				return MakeError(FString::Printf(TEXT("Class %s is not compatible with metaclass %s"),
					*ClassValue->GetName(), *ClassProp->MetaClass->GetName()));
			}

			ClassProp->SetObjectPropertyValue(ValuePtr, ClassValue);
		}
	}
	// Handle basic types through text import
	else
	{
		const TCHAR* ImportBuffer = *PropertyValue;
		Property->ImportText_Direct(ImportBuffer, ValuePtr, CDO, PPF_None);
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	Blueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Property %s set successfully"), *PropertyName));
	Data->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Data->SetStringField(TEXT("property_name"), PropertyName);
	Data->SetStringField(TEXT("property_value"), PropertyValue);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleRemoveImplementedInterface(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString InterfaceName = Params->GetStringField(TEXT("interface_name"));

	if (BlueprintPath.IsEmpty() || InterfaceName.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path or interface_name"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Find and remove the implemented interface
	int32 RemovedCount = 0;
	for (int32 i = Blueprint->ImplementedInterfaces.Num() - 1; i >= 0; i--)
	{
		FBPInterfaceDescription& Interface = Blueprint->ImplementedInterfaces[i];
		if (Interface.Interface)
		{
			FString ClassName = Interface.Interface->GetName();
			if (ClassName.Contains(InterfaceName))
			{
				// Remove any graphs associated with this interface
				for (UEdGraph* Graph : Interface.Graphs)
				{
					if (Graph)
					{
						FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
					}
				}
				Blueprint->ImplementedInterfaces.RemoveAt(i);
				RemovedCount++;
			}
		}
	}

	if (RemovedCount == 0)
	{
		return MakeError(FString::Printf(TEXT("Interface '%s' not found in blueprint"), *InterfaceName));
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Blueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Removed %d interface(s) matching '%s'"), RemovedCount, *InterfaceName));
	Data->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Data->SetNumberField(TEXT("interfaces_removed"), RemovedCount);
	Data->SetNumberField(TEXT("remaining_interfaces"), Blueprint->ImplementedInterfaces.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
	FString NamePattern = Params->GetStringField(TEXT("name_pattern"));
	FString ActorClass = Params->HasField(TEXT("actor_class")) ? Params->GetStringField(TEXT("actor_class")) : TEXT("");

	if (NamePattern.IsEmpty())
	{
		return MakeError(TEXT("name_pattern parameter is required"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeError(TEXT("No world available"));
	}

	// Convert wildcard pattern to regex-like matching
	// * = any characters, ? = single character
	FString Pattern = NamePattern;
	Pattern.ReplaceInline(TEXT("*"), TEXT(".*"));
	Pattern.ReplaceInline(TEXT("?"), TEXT("."));

	TArray<TSharedPtr<FJsonValue>> MatchedActors;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		// Filter by class if specified
		if (!ActorClass.IsEmpty() && Actor->GetClass()->GetName() != ActorClass)
		{
			continue;
		}

		// Check if name matches pattern (simple wildcard matching)
		FString ActorName = Actor->GetName();
		bool bMatches = false;

		if (NamePattern.Contains(TEXT("*")) || NamePattern.Contains(TEXT("?")))
		{
			// Simple wildcard matching - check if pattern matches
			FString Remaining = ActorName;
			TArray<FString> Parts;
			NamePattern.ParseIntoArray(Parts, TEXT("*"), true);

			if (Parts.Num() == 0)
			{
				bMatches = true; // Pattern is just "*"
			}
			else
			{
				bMatches = true;
				int32 SearchStart = 0;

				for (int32 i = 0; i < Parts.Num(); i++)
				{
					if (Parts[i].IsEmpty()) continue;

					int32 FoundIndex = Remaining.Find(Parts[i], ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchStart);
					if (FoundIndex == INDEX_NONE || (i == 0 && !NamePattern.StartsWith(TEXT("*")) && FoundIndex != 0))
					{
						bMatches = false;
						break;
					}
					SearchStart = FoundIndex + Parts[i].Len();
				}

				// Check end match
				if (bMatches && !NamePattern.EndsWith(TEXT("*")) && Parts.Num() > 0)
				{
					if (!Remaining.EndsWith(Parts.Last()))
					{
						bMatches = false;
					}
				}
			}
		}
		else
		{
			// Exact match or contains
			bMatches = ActorName.Contains(NamePattern);
		}

		if (bMatches)
		{
			TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
			ActorObj->SetStringField(TEXT("name"), Actor->GetName());
			ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
			ActorObj->SetStringField(TEXT("label"), Actor->GetActorLabel());

			FVector Location = Actor->GetActorLocation();
			ActorObj->SetStringField(TEXT("location"), FString::Printf(TEXT("%.1f, %.1f, %.1f"), Location.X, Location.Y, Location.Z));

			MatchedActors.Add(MakeShared<FJsonValueObject>(ActorObj));
		}
	}

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetArrayField(TEXT("actors"), MatchedActors);
	ResponseData->SetNumberField(TEXT("count"), MatchedActors.Num());
	ResponseData->SetStringField(TEXT("pattern"), NamePattern);

	return MakeResponse(true, ResponseData);
}

FString FMCPServer::HandleGetActorMaterialInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString ActorName = Params->GetStringField(TEXT("actor_name"));

	if (ActorName.IsEmpty())
	{
		return MakeError(TEXT("actor_name parameter is required"));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeError(TEXT("No world available"));
	}

	// Find the actor
	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetName() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		return MakeError(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
	}

	// Collect material information from all components
	TArray<TSharedPtr<FJsonValue>> MaterialsArray;

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	FoundActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimComp : PrimitiveComponents)
	{
		if (!PrimComp) continue;

		int32 NumMaterials = PrimComp->GetNumMaterials();
		for (int32 i = 0; i < NumMaterials; i++)
		{
			UMaterialInterface* Material = PrimComp->GetMaterial(i);
			if (!Material) continue;

			TSharedPtr<FJsonObject> MaterialObj = MakeShared<FJsonObject>();
			MaterialObj->SetStringField(TEXT("component"), PrimComp->GetName());
			MaterialObj->SetNumberField(TEXT("slot"), i);
			MaterialObj->SetStringField(TEXT("material_name"), Material->GetName());
			MaterialObj->SetStringField(TEXT("material_path"), Material->GetPathName());
			MaterialObj->SetStringField(TEXT("material_class"), Material->GetClass()->GetName());

			// Check if it's a dynamic material instance
			if (UMaterialInstanceDynamic* DynMaterial = Cast<UMaterialInstanceDynamic>(Material))
			{
				MaterialObj->SetBoolField(TEXT("is_dynamic"), true);
				if (DynMaterial->Parent)
				{
					MaterialObj->SetStringField(TEXT("parent_material"), DynMaterial->Parent->GetPathName());
				}
			}
			else
			{
				MaterialObj->SetBoolField(TEXT("is_dynamic"), false);
			}

			MaterialsArray.Add(MakeShared<FJsonValueObject>(MaterialObj));
		}
	}

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetStringField(TEXT("actor_name"), ActorName);
	ResponseData->SetStringField(TEXT("actor_class"), FoundActor->GetClass()->GetName());
	ResponseData->SetArrayField(TEXT("materials"), MaterialsArray);
	ResponseData->SetNumberField(TEXT("material_count"), MaterialsArray.Num());

	return MakeResponse(true, ResponseData);
}

FString FMCPServer::HandleGetSceneSummary(const TSharedPtr<FJsonObject>& Params)
{
	bool bIncludeDetails = Params->HasField(TEXT("include_details")) ? Params->GetBoolField(TEXT("include_details")) : true;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return MakeError(TEXT("No world available"));
	}

	// Count actors by class
	TMap<FString, int32> ActorCountByClass;
	int32 TotalActors = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		FString ClassName = Actor->GetClass()->GetName();
		ActorCountByClass.FindOrAdd(ClassName)++;
		TotalActors++;
	}

	// Build response
	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetStringField(TEXT("level_name"), World->GetName());
	ResponseData->SetNumberField(TEXT("total_actors"), TotalActors);
	ResponseData->SetNumberField(TEXT("unique_actor_classes"), ActorCountByClass.Num());

	if (bIncludeDetails)
	{
		TSharedPtr<FJsonObject> ClassBreakdown = MakeShared<FJsonObject>();

		// Sort by count (highest first)
		TArray<TPair<FString, int32>> SortedCounts;
		for (const auto& Pair : ActorCountByClass)
		{
			SortedCounts.Add(Pair);
		}
		SortedCounts.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) {
			return A.Value > B.Value;
		});

		for (const auto& Pair : SortedCounts)
		{
			ClassBreakdown->SetNumberField(Pair.Key, Pair.Value);
		}

		ResponseData->SetObjectField(TEXT("actor_classes"), ClassBreakdown);
	}

	return MakeResponse(true, ResponseData);
}

// ==================================================
// Sprint 5: Blueprint Node Manipulation Commands
// ==================================================

FString FMCPServer::HandleConnectNodes(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("source_node_id")) || !Params->HasField(TEXT("source_pin")) ||
		!Params->HasField(TEXT("target_node_id")) || !Params->HasField(TEXT("target_pin")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, source_node_id, source_pin, target_node_id, target_pin"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString SourceNodeId = Params->GetStringField(TEXT("source_node_id"));
	FString SourcePinName = Params->GetStringField(TEXT("source_pin"));
	FString TargetNodeId = Params->GetStringField(TEXT("target_node_id"));
	FString TargetPinName = Params->GetStringField(TEXT("target_pin"));
	FString GraphName = Params->HasField(TEXT("graph_name")) ? Params->GetStringField(TEXT("graph_name")) : TEXT("EventGraph");

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	// Find the target graph
	UEdGraph* TargetGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			TargetGraph = Graph;
			break;
		}
	}

	// Also check function graphs if not found in ubergraph
	if (!TargetGraph)
	{
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == GraphName)
			{
				TargetGraph = Graph;
				break;
			}
		}
	}

	if (!TargetGraph)
	{
		return MakeError(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
	}

	// Find source and target nodes by GUID
	UEdGraphNode* SourceNode = nullptr;
	UEdGraphNode* TargetNode = nullptr;
	FGuid SourceGuid, TargetGuid;

	if (!FGuid::Parse(SourceNodeId, SourceGuid))
	{
		return MakeError(FString::Printf(TEXT("Invalid source node ID format: %s"), *SourceNodeId));
	}
	if (!FGuid::Parse(TargetNodeId, TargetGuid))
	{
		return MakeError(FString::Printf(TEXT("Invalid target node ID format: %s"), *TargetNodeId));
	}

	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (Node)
		{
			if (Node->NodeGuid == SourceGuid)
			{
				SourceNode = Node;
			}
			if (Node->NodeGuid == TargetGuid)
			{
				TargetNode = Node;
			}
		}
	}

	if (!SourceNode)
	{
		return MakeError(FString::Printf(TEXT("Source node not found with ID: %s"), *SourceNodeId));
	}
	if (!TargetNode)
	{
		return MakeError(FString::Printf(TEXT("Target node not found with ID: %s"), *TargetNodeId));
	}

	// Find source and target pins
	UEdGraphPin* SourcePin = nullptr;
	UEdGraphPin* TargetPin = nullptr;

	for (UEdGraphPin* Pin : SourceNode->Pins)
	{
		if (Pin && Pin->PinName.ToString() == SourcePinName && Pin->Direction == EGPD_Output)
		{
			SourcePin = Pin;
			break;
		}
	}

	for (UEdGraphPin* Pin : TargetNode->Pins)
	{
		if (Pin && Pin->PinName.ToString() == TargetPinName && Pin->Direction == EGPD_Input)
		{
			TargetPin = Pin;
			break;
		}
	}

	if (!SourcePin)
	{
		// List available output pins for debugging
		TArray<FString> AvailablePins;
		for (UEdGraphPin* Pin : SourceNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output)
			{
				AvailablePins.Add(Pin->PinName.ToString());
			}
		}
		return MakeError(FString::Printf(TEXT("Source output pin not found: %s. Available output pins: %s"),
			*SourcePinName, *FString::Join(AvailablePins, TEXT(", "))));
	}

	if (!TargetPin)
	{
		// List available input pins for debugging
		TArray<FString> AvailablePins;
		for (UEdGraphPin* Pin : TargetNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input)
			{
				AvailablePins.Add(Pin->PinName.ToString());
			}
		}
		return MakeError(FString::Printf(TEXT("Target input pin not found: %s. Available input pins: %s"),
			*TargetPinName, *FString::Join(AvailablePins, TEXT(", "))));
	}

	// Check if connection can be made using the schema
	const UEdGraphSchema* Schema = TargetGraph->GetSchema();
	if (Schema)
	{
		FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, TargetPin);
		if (Response.Response == CONNECT_RESPONSE_DISALLOW)
		{
			return MakeError(FString::Printf(TEXT("Cannot create connection: %s"), *Response.Message.ToString()));
		}
	}

	// Check if already connected
	if (SourcePin->LinkedTo.Contains(TargetPin))
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("message"), TEXT("Pins are already connected"));
		Data->SetBoolField(TEXT("already_connected"), true);
		return MakeResponse(true, Data);
	}

	// Create the connection
	bool bModified = Schema->TryCreateConnection(SourcePin, TargetPin);

	if (!bModified)
	{
		// Fallback to direct link
		SourcePin->MakeLinkTo(TargetPin);
		bModified = true;
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	// Check for compilation errors
	bool bCompiledSuccessfully = (Blueprint->Status != BS_Error);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Nodes connected successfully"));
	Data->SetStringField(TEXT("source_node"), SourceNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Data->SetStringField(TEXT("source_pin"), SourcePinName);
	Data->SetStringField(TEXT("target_node"), TargetNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Data->SetStringField(TEXT("target_pin"), TargetPinName);
	Data->SetBoolField(TEXT("compiled_successfully"), bCompiledSuccessfully);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleDisconnectPin(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("node_id")) || !Params->HasField(TEXT("pin_name")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, node_id, pin_name"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString NodeId = Params->GetStringField(TEXT("node_id"));
	FString PinName = Params->GetStringField(TEXT("pin_name"));
	FString GraphName = Params->HasField(TEXT("graph_name")) ? Params->GetStringField(TEXT("graph_name")) : TEXT("EventGraph");

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	// Find the target graph
	UEdGraph* TargetGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			TargetGraph = Graph;
			break;
		}
	}

	if (!TargetGraph)
	{
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == GraphName)
			{
				TargetGraph = Graph;
				break;
			}
		}
	}

	if (!TargetGraph)
	{
		return MakeError(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
	}

	// Find node by GUID
	FGuid NodeGuid;
	if (!FGuid::Parse(NodeId, NodeGuid))
	{
		return MakeError(FString::Printf(TEXT("Invalid node ID format: %s"), *NodeId));
	}

	UEdGraphNode* Node = nullptr;
	for (UEdGraphNode* N : TargetGraph->Nodes)
	{
		if (N && N->NodeGuid == NodeGuid)
		{
			Node = N;
			break;
		}
	}

	if (!Node)
	{
		return MakeError(FString::Printf(TEXT("Node not found with ID: %s"), *NodeId));
	}

	// Find pin
	UEdGraphPin* Pin = nullptr;
	for (UEdGraphPin* P : Node->Pins)
	{
		if (P && P->PinName.ToString() == PinName)
		{
			Pin = P;
			break;
		}
	}

	if (!Pin)
	{
		TArray<FString> AvailablePins;
		for (UEdGraphPin* P : Node->Pins)
		{
			if (P)
			{
				AvailablePins.Add(P->PinName.ToString());
			}
		}
		return MakeError(FString::Printf(TEXT("Pin not found: %s. Available pins: %s"),
			*PinName, *FString::Join(AvailablePins, TEXT(", "))));
	}

	// Break all links
	int32 LinksCount = Pin->LinkedTo.Num();
	Pin->BreakAllPinLinks();

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Disconnected %d links from pin"), LinksCount));
	Data->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Data->SetStringField(TEXT("pin"), PinName);
	Data->SetNumberField(TEXT("links_broken"), LinksCount);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleAddSetStructNode(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("struct_type")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, struct_type"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString StructType = Params->GetStringField(TEXT("struct_type"));
	FString GraphName = Params->HasField(TEXT("graph_name")) ? Params->GetStringField(TEXT("graph_name")) : TEXT("EventGraph");
	int32 NodeX = Params->HasField(TEXT("x")) ? Params->GetIntegerField(TEXT("x")) : 0;
	int32 NodeY = Params->HasField(TEXT("y")) ? Params->GetIntegerField(TEXT("y")) : 0;

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	// Find the target graph
	UEdGraph* TargetGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			TargetGraph = Graph;
			break;
		}
	}

	if (!TargetGraph)
	{
		return MakeError(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
	}

	// Find the struct type
	UScriptStruct* Struct = nullptr;

	// Try loading from path first (works for blueprint structs)
	Struct = LoadObject<UScriptStruct>(nullptr, *StructType);

	if (!Struct)
	{
		// Try to find in any package by iterating
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			if (It->GetName() == StructType)
			{
				Struct = *It;
				break;
			}
		}
	}

	if (!Struct)
	{
		return MakeError(FString::Printf(TEXT("Struct type not found: %s"), *StructType));
	}

	// Get fields to expose (optional)
	TArray<FString> FieldsToExpose;
	if (Params->HasField(TEXT("fields")))
	{
		const TArray<TSharedPtr<FJsonValue>>* FieldsArray;
		if (Params->TryGetArrayField(TEXT("fields"), FieldsArray))
		{
			for (const auto& Field : *FieldsArray)
			{
				FieldsToExpose.Add(Field->AsString());
			}
		}
	}

	// Create the node
	UK2Node_SetFieldsInStruct* NewNode = NewObject<UK2Node_SetFieldsInStruct>(TargetGraph);
	NewNode->StructType = Struct;
	NewNode->NodePosX = NodeX;
	NewNode->NodePosY = NodeY;
	NewNode->CreateNewGuid();

	// Allocate default pins first (this populates ShowPinForProperties with all fields hidden by default)
	NewNode->AllocateDefaultPins();

	// Use RestoreAllPins() to show all struct field pins if any fields were requested
	// This is the official way to expose pins on K2Node_SetFieldsInStruct
	if (FieldsToExpose.Num() > 0)
	{
		// RestoreAllPins() will show all struct member pins
		NewNode->RestoreAllPins();
	}

	TargetGraph->AddNode(NewNode, false, false);

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Set struct node created successfully"));
	Data->SetStringField(TEXT("node_id"), NewNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
	Data->SetStringField(TEXT("struct_type"), Struct->GetName());

	// List available pins
	TArray<TSharedPtr<FJsonValue>> PinsArray;
	for (UEdGraphPin* Pin : NewNode->Pins)
	{
		if (Pin)
		{
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
			PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
			PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
		}
	}
	Data->SetArrayField(TEXT("pins"), PinsArray);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleDeleteNode(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("node_id")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, node_id"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString NodeId = Params->GetStringField(TEXT("node_id"));
	FString GraphName = Params->HasField(TEXT("graph_name")) ? Params->GetStringField(TEXT("graph_name")) : TEXT("EventGraph");

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Failed to load blueprint: %s"), *BlueprintPath));
	}

	// Find the target graph
	UEdGraph* TargetGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			TargetGraph = Graph;
			break;
		}
	}

	if (!TargetGraph)
	{
		return MakeError(FString::Printf(TEXT("Graph not found: %s"), *GraphName));
	}

	// Find the node by GUID
	UEdGraphNode* NodeToDelete = nullptr;
	FString NodeTitle;
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (Node)
		{
			FString NodeGuidStr = Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensLower);
			FString NodeGuidStrUpper = Node->NodeGuid.ToString(EGuidFormats::Digits);
			if (NodeGuidStr.Equals(NodeId, ESearchCase::IgnoreCase) ||
				NodeGuidStrUpper.Equals(NodeId, ESearchCase::IgnoreCase))
			{
				NodeToDelete = Node;
				NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
				break;
			}
		}
	}

	if (!NodeToDelete)
	{
		return MakeError(FString::Printf(TEXT("Node not found with ID: %s"), *NodeId));
	}

	// Remove the node
	TargetGraph->RemoveNode(NodeToDelete);

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	// Compile the blueprint
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Node deleted successfully"));
	Data->SetStringField(TEXT("node_id"), NodeId);
	Data->SetStringField(TEXT("node_title"), NodeTitle);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadInputMappingContext(const TSharedPtr<FJsonObject>& Params)
{
	FString Path = Params->GetStringField(TEXT("path"));
	if (Path.IsEmpty())
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	// Normalize the path
	FString FullPath = Path;
	if (!FullPath.StartsWith(TEXT("/")))
	{
		FullPath = TEXT("/Game/") + Path;
	}

	// Load the Input Mapping Context
	UInputMappingContext* IMC = LoadObject<UInputMappingContext>(nullptr, *FullPath);
	if (!IMC)
	{
		// Try with .IMC_Sandbox suffix
		FString AssetName = FPaths::GetBaseFilename(FullPath);
		FString TryPath = FullPath + TEXT(".") + AssetName;
		IMC = LoadObject<UInputMappingContext>(nullptr, *TryPath);
	}

	if (!IMC)
	{
		return MakeError(FString::Printf(TEXT("Input Mapping Context not found: %s"), *Path));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), IMC->GetName());
	Data->SetStringField(TEXT("path"), IMC->GetPathName());

	// Get the mappings
	TArray<TSharedPtr<FJsonValue>> MappingsArray;
	const TArray<FEnhancedActionKeyMapping>& Mappings = IMC->GetMappings();

	for (const FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		TSharedPtr<FJsonObject> MappingObj = MakeShared<FJsonObject>();

		// Input Action info
		if (Mapping.Action)
		{
			MappingObj->SetStringField(TEXT("action_name"), Mapping.Action->GetName());
			MappingObj->SetStringField(TEXT("action_path"), Mapping.Action->GetPathName());
		}
		else
		{
			MappingObj->SetStringField(TEXT("action_name"), TEXT("(None)"));
		}

		// Key info
		MappingObj->SetStringField(TEXT("key"), Mapping.Key.GetFName().ToString());
		MappingObj->SetStringField(TEXT("key_display"), Mapping.Key.GetDisplayName().ToString());

		// Modifiers
		TArray<TSharedPtr<FJsonValue>> ModifiersArray;
		for (UInputModifier* Modifier : Mapping.Modifiers)
		{
			if (Modifier)
			{
				TSharedPtr<FJsonObject> ModObj = MakeShared<FJsonObject>();
				ModObj->SetStringField(TEXT("class"), Modifier->GetClass()->GetName());
				ModifiersArray.Add(MakeShared<FJsonValueObject>(ModObj));
			}
		}
		MappingObj->SetArrayField(TEXT("modifiers"), ModifiersArray);

		// Triggers
		TArray<TSharedPtr<FJsonValue>> TriggersArray;
		for (UInputTrigger* Trigger : Mapping.Triggers)
		{
			if (Trigger)
			{
				TSharedPtr<FJsonObject> TrigObj = MakeShared<FJsonObject>();
				TrigObj->SetStringField(TEXT("class"), Trigger->GetClass()->GetName());
				TriggersArray.Add(MakeShared<FJsonValueObject>(TrigObj));
			}
		}
		MappingObj->SetArrayField(TEXT("triggers"), TriggersArray);

		MappingsArray.Add(MakeShared<FJsonValueObject>(MappingObj));
	}

	Data->SetArrayField(TEXT("mappings"), MappingsArray);

	return MakeResponse(true, Data);
}

// ===== Sprint 7: Struct Migration =====

struct FSavedPinConnection
{
	FName PinName;
	EEdGraphPinDirection Direction;
	FGuid RemoteNodeGuid;
	FName RemotePinName;
};

static void SaveNodeConnections(UEdGraphNode* Node, TArray<FSavedPinConnection>& OutConnections)
{
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin) continue;
		for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			if (!LinkedPin || !LinkedPin->GetOwningNodeUnchecked()) continue;
			FSavedPinConnection Conn;
			Conn.PinName = Pin->PinName;
			Conn.Direction = Pin->Direction;
			Conn.RemoteNodeGuid = LinkedPin->GetOwningNode()->NodeGuid;
			Conn.RemotePinName = LinkedPin->PinName;
			OutConnections.Add(Conn);
		}
	}
}

static void RestoreNodeConnections(
	UEdGraphNode* Node,
	const TArray<FSavedPinConnection>& SavedConnections,
	const TMap<FName, FName>& FieldNameMap,
	UEdGraph* Graph)
{
	for (const FSavedPinConnection& Conn : SavedConnections)
	{
		// Map old GUID-suffixed pin name to clean C++ name
		FName MappedPinName = Conn.PinName;
		if (const FName* NewName = FieldNameMap.Find(Conn.PinName))
		{
			MappedPinName = *NewName;
		}

		// Find our pin by mapped name
		UEdGraphPin* OurPin = Node->FindPin(MappedPinName, Conn.Direction);
		if (!OurPin)
		{
			// Try exact old name (for non-struct pins like exec, struct input/output)
			OurPin = Node->FindPin(Conn.PinName, Conn.Direction);
		}
		if (!OurPin) continue;

		// Find the remote node and pin
		for (UEdGraphNode* OtherNode : Graph->Nodes)
		{
			if (OtherNode && OtherNode->NodeGuid == Conn.RemoteNodeGuid)
			{
				EEdGraphPinDirection RemoteDir = (Conn.Direction == EGPD_Input) ? EGPD_Output : EGPD_Input;
				// Try exact remote pin name first
				UEdGraphPin* RemotePin = OtherNode->FindPin(Conn.RemotePinName, RemoteDir);
				if (!RemotePin)
				{
					// Remote pin may also have been remapped
					FName MappedRemoteName = Conn.RemotePinName;
					if (const FName* NewRemoteName = FieldNameMap.Find(Conn.RemotePinName))
					{
						MappedRemoteName = *NewRemoteName;
					}
					RemotePin = OtherNode->FindPin(MappedRemoteName, RemoteDir);
				}
				if (RemotePin && !OurPin->LinkedTo.Contains(RemotePin))
				{
					OurPin->MakeLinkTo(RemotePin);
				}
				break;
			}
		}
	}
}

static bool DoesBlueprintReferenceStruct(UBlueprint* Blueprint, UUserDefinedStruct* OldStruct)
{
	// Check variables
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
			Var.VarType.PinSubCategoryObject.Get() == OldStruct)
		{
			return true;
		}
	}

	// Check graph nodes
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
					Pin->PinType.PinSubCategoryObject.Get() == OldStruct)
				{
					return true;
				}
			}
		}
	}

	return false;
}

FString FMCPServer::HandleMigrateStructReferences(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("source_struct_path")) || !Params->HasField(TEXT("target_struct_path")))
	{
		return MakeError(TEXT("Missing 'source_struct_path' or 'target_struct_path' parameter"));
	}

	FString SourceStructPath = Params->GetStringField(TEXT("source_struct_path"));
	FString TargetStructPath = Params->GetStringField(TEXT("target_struct_path"));
	bool bDryRun = Params->HasField(TEXT("dry_run")) ? Params->GetBoolField(TEXT("dry_run")) : false;

	// Load old BP struct
	FString FullSourcePath = SourceStructPath;
	if (!FullSourcePath.EndsWith(FPaths::GetCleanFilename(FullSourcePath)))
	{
		FullSourcePath = SourceStructPath + TEXT(".") + FPaths::GetCleanFilename(SourceStructPath);
	}
	UUserDefinedStruct* OldStruct = LoadObject<UUserDefinedStruct>(nullptr, *FullSourcePath);
	if (!OldStruct)
	{
		OldStruct = LoadObject<UUserDefinedStruct>(nullptr, *SourceStructPath);
	}
	if (!OldStruct)
	{
		return MakeError(FString::Printf(TEXT("Source UserDefinedStruct not found at '%s'"), *SourceStructPath));
	}

	// Load new C++ struct
	UScriptStruct* NewStruct = nullptr;
	FString TypeNameToFind = TargetStructPath;
	if (TargetStructPath.StartsWith(TEXT("/Script/")))
	{
		FString Remainder = TargetStructPath;
		Remainder.RemoveFromStart(TEXT("/Script/"));
		FString ModuleName;
		Remainder.Split(TEXT("."), &ModuleName, &TypeNameToFind);
	}
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		if (It->GetName() == TypeNameToFind)
		{
			NewStruct = *It;
			break;
		}
	}
	if (!NewStruct)
	{
		return MakeError(FString::Printf(TEXT("Target C++ struct '%s' not found"), *TargetStructPath));
	}

	// Build GUID → clean name field mapping
	TMap<FName, FName> FieldNameMap;
	TArray<TSharedPtr<FJsonValue>> MappingsJsonArray;

	const TArray<FStructVariableDescription>& VarDescs = FStructureEditorUtils::GetVarDesc(OldStruct);
	for (const FStructVariableDescription& Desc : VarDescs)
	{
		FName OldName = Desc.VarName;
		FName CleanName = FName(*Desc.FriendlyName);
		FieldNameMap.Add(OldName, CleanName);

		TSharedPtr<FJsonObject> MapObj = MakeShared<FJsonObject>();
		MapObj->SetStringField(TEXT("old_name"), OldName.ToString());
		MapObj->SetStringField(TEXT("new_name"), CleanName.ToString());
		MappingsJsonArray.Add(MakeShared<FJsonValueObject>(MapObj));
	}

	// Find all blueprint assets
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AllBPAssets;
	AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), AllBPAssets);
	TArray<FAssetData> AnimBPAssets;
	AssetRegistry.GetAssetsByClass(UAnimBlueprint::StaticClass()->GetClassPathName(), AnimBPAssets);
	AllBPAssets.Append(AnimBPAssets);

	// Find affected blueprints
	TArray<UBlueprint*> AffectedBlueprints;
	for (const FAssetData& Asset : AllBPAssets)
	{
		FString AssetPath = Asset.GetObjectPathString();
		if (!AssetPath.StartsWith(TEXT("/Game/"))) continue;

		UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AssetPath);
		if (!BP) continue;

		if (DoesBlueprintReferenceStruct(BP, OldStruct))
		{
			AffectedBlueprints.Add(BP);
		}
	}

	// Migrate each affected blueprint
	TArray<TSharedPtr<FJsonValue>> BlueprintReportsArray;
	int32 TotalVariablesMigrated = 0;
	int32 TotalNodesMigrated = 0;
	int32 TotalConnectionsRestored = 0;
	int32 TotalConnectionsFailed = 0;

	for (UBlueprint* Blueprint : AffectedBlueprints)
	{
		TSharedPtr<FJsonObject> BPReport = MakeShared<FJsonObject>();
		BPReport->SetStringField(TEXT("path"), Blueprint->GetPathName());
		BPReport->SetStringField(TEXT("name"), Blueprint->GetName());

		int32 VarCount = 0;
		int32 NodeCount = 0;

		// --- Update member variables ---
		for (FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			if (Var.VarType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
				Var.VarType.PinSubCategoryObject.Get() == OldStruct)
			{
				VarCount++;
				if (!bDryRun)
				{
					Var.VarType.PinSubCategoryObject = NewStruct;
				}
			}
		}

		// --- Update function-local variables (stored on FunctionEntry nodes) ---
		{
			TArray<UEdGraph*> TempGraphs;
			Blueprint->GetAllGraphs(TempGraphs);
			for (UEdGraph* Graph : TempGraphs)
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node);
					if (!EntryNode) continue;
					for (FBPVariableDescription& LocalVar : EntryNode->LocalVariables)
					{
						if (LocalVar.VarType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
							LocalVar.VarType.PinSubCategoryObject.Get() == OldStruct)
						{
							VarCount++;
							if (!bDryRun)
							{
								LocalVar.VarType.PinSubCategoryObject = NewStruct;
							}
						}
					}
				}
			}
		}

		// --- Collect all affected nodes across all graphs ---
		struct FNodeMigrationInfo
		{
			UEdGraphNode* Node;
			UEdGraph* Graph;
			bool bIsStructNode; // Break/Make/Set struct node (needs ReconstructNode)
			TArray<FSavedPinConnection> SavedConnections;
		};
		TArray<FNodeMigrationInfo> NodesToMigrate;

		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);

		// Add sub-graphs recursively (GetAllGraphs may not include node sub-graphs)
		{
			TArray<UEdGraph*> GraphsToProcess = AllGraphs;
			while (GraphsToProcess.Num() > 0)
			{
				UEdGraph* CurrentGraph = GraphsToProcess.Pop();
				for (UEdGraphNode* GraphNode : CurrentGraph->Nodes)
				{
					if (!GraphNode) continue;
					for (UEdGraph* SubGraph : GraphNode->GetSubGraphs())
					{
						if (SubGraph && !AllGraphs.Contains(SubGraph))
						{
							AllGraphs.Add(SubGraph);
							GraphsToProcess.Add(SubGraph);
						}
					}
				}
			}
		}

		for (UEdGraph* Graph : AllGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node) continue;
				bool bNodeAffected = false;
				bool bIsStructNode = false;

				// Check Break/Make/Set struct nodes
				if (UK2Node_BreakStruct* BreakNode = Cast<UK2Node_BreakStruct>(Node))
				{
					if (BreakNode->StructType == OldStruct) { bNodeAffected = true; bIsStructNode = true; }
				}
				else if (UK2Node_MakeStruct* MakeNode = Cast<UK2Node_MakeStruct>(Node))
				{
					if (MakeNode->StructType == OldStruct) { bNodeAffected = true; bIsStructNode = true; }
				}
				else if (UK2Node_SetFieldsInStruct* SetNode = Cast<UK2Node_SetFieldsInStruct>(Node))
				{
					if (SetNode->StructType == OldStruct) { bNodeAffected = true; bIsStructNode = true; }
				}
				else
				{
					// Generic: check any pin referencing old struct
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
							Pin->PinType.PinSubCategoryObject.Get() == OldStruct)
						{
							bNodeAffected = true;
							break;
						}
					}
				}

				if (bNodeAffected)
				{
					FNodeMigrationInfo Info;
					Info.Node = Node;
					Info.Graph = Graph;
					Info.bIsStructNode = bIsStructNode;
					SaveNodeConnections(Node, Info.SavedConnections);
					NodesToMigrate.Add(MoveTemp(Info));
				}
			}
		}

		NodeCount = NodesToMigrate.Num();

		// --- Migrate nodes ---
		if (!bDryRun)
		{
			// Pass 1: Update struct references
			for (FNodeMigrationInfo& Info : NodesToMigrate)
			{
				UEdGraphNode* Node = Info.Node;

				if (Info.bIsStructNode)
				{
					// Break/Make/Set nodes: need ReconstructNode to rebuild pins with new field names
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin) Pin->BreakAllPinLinks();
					}

					if (UK2Node_BreakStruct* BreakNode = Cast<UK2Node_BreakStruct>(Node))
					{
						BreakNode->StructType = NewStruct;
					}
					else if (UK2Node_MakeStruct* MakeNode = Cast<UK2Node_MakeStruct>(Node))
					{
						MakeNode->StructType = NewStruct;
					}
					else if (UK2Node_SetFieldsInStruct* SetNode = Cast<UK2Node_SetFieldsInStruct>(Node))
					{
						SetNode->StructType = NewStruct;
					}
					Node->ReconstructNode();
				}
				else
				{
					// Generic nodes: update pin types in-place WITHOUT ReconstructNode
					// ReconstructNode on generic nodes rebuilds from internal state and reverts our changes
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
							Pin->PinType.PinSubCategoryObject.Get() == OldStruct)
						{
							Pin->PinType.PinSubCategoryObject = NewStruct;
						}
					}

					// Also update K2Node_CustomEvent::UserDefinedPins so ReconstructNode won't revert
					if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
					{
						for (TSharedPtr<FUserPinInfo>& PinInfo : CustomEvent->UserDefinedPins)
						{
							if (PinInfo.IsValid() &&
								PinInfo->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
								PinInfo->PinType.PinSubCategoryObject.Get() == OldStruct)
							{
								PinInfo->PinType.PinSubCategoryObject = NewStruct;
							}
						}
					}
				}
			}

			// Pass 2: Restore connections on struct nodes (generic nodes kept their connections)
			for (FNodeMigrationInfo& Info : NodesToMigrate)
			{
				if (Info.bIsStructNode)
				{
					int32 ConnectionsBefore = Info.SavedConnections.Num();
					RestoreNodeConnections(Info.Node, Info.SavedConnections, FieldNameMap, Info.Graph);

					int32 RestoredCount = 0;
					for (UEdGraphPin* Pin : Info.Node->Pins)
					{
						if (Pin) RestoredCount += Pin->LinkedTo.Num();
					}
					TotalConnectionsRestored += RestoredCount;
					int32 FailedCount = ConnectionsBefore - RestoredCount;
					if (FailedCount > 0) TotalConnectionsFailed += FailedCount;
				}
				else
				{
					// Generic nodes kept connections — count them as restored
					for (UEdGraphPin* Pin : Info.Node->Pins)
					{
						if (Pin) TotalConnectionsRestored += Pin->LinkedTo.Num();
					}
				}
			}

			// Finalize
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}

		TotalVariablesMigrated += VarCount;
		TotalNodesMigrated += NodeCount;

		BPReport->SetNumberField(TEXT("variables_migrated"), VarCount);
		BPReport->SetNumberField(TEXT("nodes_migrated"), NodeCount);
		BlueprintReportsArray.Add(MakeShared<FJsonValueObject>(BPReport));
	}

	// Build response
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("dry_run"), bDryRun);
	Data->SetStringField(TEXT("source_struct"), OldStruct->GetPathName());
	Data->SetStringField(TEXT("target_struct"), NewStruct->GetPathName());
	Data->SetNumberField(TEXT("field_mappings_count"), FieldNameMap.Num());
	Data->SetArrayField(TEXT("field_name_mapping"), MappingsJsonArray);
	Data->SetNumberField(TEXT("blueprints_affected"), AffectedBlueprints.Num());
	Data->SetArrayField(TEXT("affected_blueprints"), BlueprintReportsArray);
	Data->SetNumberField(TEXT("total_variables_migrated"), TotalVariablesMigrated);
	Data->SetNumberField(TEXT("total_nodes_migrated"), TotalNodesMigrated);
	if (!bDryRun)
	{
		Data->SetNumberField(TEXT("connections_restored"), TotalConnectionsRestored);
		Data->SetNumberField(TEXT("connections_failed"), TotalConnectionsFailed);
	}
	Data->SetStringField(TEXT("message"),
		bDryRun ? TEXT("Dry run complete - no changes made") : TEXT("Struct migration complete"));

	return MakeResponse(true, Data);
}

// Helper: Check if a blueprint references a given enum anywhere in its graphs
static bool DoesBlueprintReferenceEnum(UBlueprint* Blueprint, UEnum* EnumToFind)
{
	// Check variables
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarType.PinSubCategoryObject.Get() == EnumToFind)
			return true;
	}
	// Check all graphs including sub-graphs
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	TArray<UEdGraph*> GraphsToProcess = AllGraphs;
	while (GraphsToProcess.Num() > 0)
	{
		UEdGraph* Graph = GraphsToProcess.Pop();
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->PinType.PinSubCategoryObject.Get() == EnumToFind)
					return true;
			}
			for (UEdGraph* SubGraph : Node->GetSubGraphs())
			{
				if (SubGraph && !AllGraphs.Contains(SubGraph))
				{
					AllGraphs.Add(SubGraph);
					GraphsToProcess.Add(SubGraph);
				}
			}
		}
	}
	return false;
}

FString FMCPServer::HandleMigrateEnumReferences(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString SourceEnumPath = Params->GetStringField(TEXT("source_enum_path"));
	FString TargetEnumPath = Params->GetStringField(TEXT("target_enum_path"));
	bool bDryRun = Params->HasField(TEXT("dry_run")) ? Params->GetBoolField(TEXT("dry_run")) : false;

	if (SourceEnumPath.IsEmpty() || TargetEnumPath.IsEmpty())
	{
		return MakeError(TEXT("Missing source_enum_path or target_enum_path"));
	}

	// Load old enum (UserDefinedEnum)
	UUserDefinedEnum* OldEnum = LoadObject<UUserDefinedEnum>(nullptr, *SourceEnumPath);
	if (!OldEnum)
	{
		return MakeError(FString::Printf(TEXT("Could not load source UserDefinedEnum: %s"), *SourceEnumPath));
	}

	// Find new enum (C++ UEnum)
	UEnum* NewEnum = nullptr;
	for (TObjectIterator<UEnum> It; It; ++It)
	{
		if (It->GetPathName() == TargetEnumPath)
		{
			NewEnum = *It;
			break;
		}
	}
	if (!NewEnum)
	{
		return MakeError(FString::Printf(TEXT("Could not find target UEnum: %s"), *TargetEnumPath));
	}

	// Build value name mapping: BP enum display names -> C++ enum names
	// BP UserDefinedEnum values have display names like "Walk" and internal names like "E_Gait::NewEnumerator0"
	// C++ enum has values like "E_Gait::Walk"
	TMap<FName, FName> ValueNameMap;          // OldInternalName -> NewInternalName
	TMap<FString, FString> DisplayToNewValue; // DisplayName -> NewInternalName (for default value remapping)
	TArray<TSharedPtr<FJsonValue>> ValueMappingsArray;
	for (int32 i = 0; i < OldEnum->NumEnums() - 1; ++i) // -1 to skip _MAX
	{
		FText DisplayText = OldEnum->GetDisplayNameTextByIndex(i);
		FString DisplayName = DisplayText.ToString();
		FName OldValueName = FName(*OldEnum->GetNameStringByIndex(i));

		// Find matching C++ enum value by display name
		for (int32 j = 0; j < NewEnum->NumEnums() - 1; ++j)
		{
			FString NewDisplayName = NewEnum->GetDisplayNameTextByIndex(j).ToString();
			if (NewDisplayName == DisplayName)
			{
				FName NewValueName = FName(*NewEnum->GetNameStringByIndex(j));
				ValueNameMap.Add(OldValueName, NewValueName);
				DisplayToNewValue.Add(DisplayName, NewValueName.ToString());

				TSharedPtr<FJsonObject> Mapping = MakeShared<FJsonObject>();
				Mapping->SetStringField(TEXT("display_name"), DisplayName);
				Mapping->SetStringField(TEXT("old_value"), OldValueName.ToString());
				Mapping->SetStringField(TEXT("new_value"), NewValueName.ToString());
				ValueMappingsArray.Add(MakeShared<FJsonValueObject>(Mapping));
				break;
			}
		}
	}

	// --- Phase 0: Update UserDefinedStruct field definitions ---
	// BP structs may have fields typed as BP enums. Break/Set/Make nodes derive
	// sub-pin types from the struct definition, so we must update the source.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	int32 TotalStructFieldsFixed = 0;
	TArray<TSharedPtr<FJsonValue>> StructFieldReportsArray;
	TArray<TSharedPtr<FJsonValue>> StructDiagArray;
	{
		TArray<FAssetData> AllStructAssets;
		AssetRegistry.GetAssetsByClass(UUserDefinedStruct::StaticClass()->GetClassPathName(), AllStructAssets, true);

		FString OldEnumName = OldEnum->GetName();

		for (const FAssetData& StructAssetData : AllStructAssets)
		{
			UUserDefinedStruct* UDStruct = Cast<UUserDefinedStruct>(StructAssetData.GetAsset());
			if (!UDStruct) continue;

			TArray<FStructVariableDescription>& Variables = const_cast<TArray<FStructVariableDescription>&>(
				FStructureEditorUtils::GetVarDesc(UDStruct)
			);

			bool bStructModified = false;
			for (FStructVariableDescription& Variable : Variables)
			{
				// Diagnostic: dump ALL fields that have any SubCategoryObject referencing enum name
				FString SubCatPath = Variable.SubCategoryObject.ToSoftObjectPath().ToString();
				bool bSubCatMatchesName = SubCatPath.Contains(OldEnumName);

				// Also check the compiled FProperty for this field
				FString CompiledEnumPath;
				if (FProperty* Prop = UDStruct->FindPropertyByName(Variable.VarName))
				{
					if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
					{
						if (ByteProp->Enum)
						{
							CompiledEnumPath = ByteProp->Enum->GetPathName();
							if (!bSubCatMatchesName && ByteProp->Enum->GetName() == OldEnumName)
							{
								bSubCatMatchesName = true; // compiled property references this enum
							}
						}
					}
					else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
					{
						if (EnumProp->GetEnum())
						{
							CompiledEnumPath = EnumProp->GetEnum()->GetPathName();
							if (!bSubCatMatchesName && EnumProp->GetEnum()->GetName() == OldEnumName)
							{
								bSubCatMatchesName = true;
							}
						}
					}
				}

				if (bSubCatMatchesName)
				{
					TSharedPtr<FJsonObject> Diag = MakeShared<FJsonObject>();
					Diag->SetStringField(TEXT("struct"), UDStruct->GetName());
					Diag->SetStringField(TEXT("field"), Variable.FriendlyName);
					Diag->SetStringField(TEXT("var_name"), Variable.VarName.ToString());
					Diag->SetStringField(TEXT("category"), Variable.Category.ToString());
					Diag->SetStringField(TEXT("sub_category"), Variable.SubCategory.ToString());
					Diag->SetStringField(TEXT("sub_category_object_path"), SubCatPath);
					Diag->SetStringField(TEXT("compiled_enum_path"), CompiledEnumPath);

					UObject* ResolvedObj = Variable.SubCategoryObject.LoadSynchronous();
					Diag->SetStringField(TEXT("resolved_obj"), ResolvedObj ? ResolvedObj->GetPathName() : TEXT("null"));
					Diag->SetStringField(TEXT("old_enum_path"), OldEnum->GetPathName());
					Diag->SetBoolField(TEXT("matches_old_enum"), ResolvedObj == OldEnum);
					Diag->SetStringField(TEXT("resolved_class"), ResolvedObj ? ResolvedObj->GetClass()->GetName() : TEXT("null"));
					Diag->SetBoolField(TEXT("subcat_is_null"), SubCatPath.IsEmpty());

					StructDiagArray.Add(MakeShared<FJsonValueObject>(Diag));
				}

				// Try multiple matching strategies for the actual fix
				UObject* ResolvedObj = Variable.SubCategoryObject.LoadSynchronous();
				bool bMatchesOld = (ResolvedObj == OldEnum);

				// Also match by compiled property enum pointer
				if (!bMatchesOld && !CompiledEnumPath.IsEmpty())
				{
					if (FProperty* Prop = UDStruct->FindPropertyByName(Variable.VarName))
					{
						UEnum* CompiledEnum = nullptr;
						if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
							CompiledEnum = ByteProp->Enum;
						else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
							CompiledEnum = EnumProp->GetEnum();

						if (CompiledEnum == OldEnum)
							bMatchesOld = true;
					}
				}

				if (bMatchesOld)
				{
					if (!bDryRun)
					{
						Variable.SubCategoryObject = TSoftObjectPtr<UObject>(NewEnum);

						// Also directly update the COMPILED FByteProperty::Enum pointer
						// This is critical because Break/Make/SetFieldsInStruct nodes derive
						// pin types from the compiled property, not from SubCategoryObject
						if (FProperty* Prop = UDStruct->FindPropertyByName(Variable.VarName))
						{
							if (FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
							{
								ByteProp->Enum = NewEnum;
							}
							else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
							{
								// EnumProperty doesn't have a simple setter, but we can
								// use the internal pointer if available
							}
						}

						// Remap default value if set
						if (!Variable.DefaultValue.IsEmpty())
						{
							FName OldVal = FName(*Variable.DefaultValue);
							if (const FName* NewVal = ValueNameMap.Find(OldVal))
							{
								Variable.DefaultValue = NewVal->ToString();
							}
							else if (const FString* NewValStr = DisplayToNewValue.Find(Variable.DefaultValue))
							{
								Variable.DefaultValue = *NewValStr;
							}
						}
					}

					bStructModified = true;
					TotalStructFieldsFixed++;

					TSharedPtr<FJsonObject> FieldReport = MakeShared<FJsonObject>();
					FieldReport->SetStringField(TEXT("struct"), UDStruct->GetName());
					FieldReport->SetStringField(TEXT("field"), Variable.FriendlyName);
					StructFieldReportsArray.Add(MakeShared<FJsonValueObject>(FieldReport));
				}
			}

			if (bStructModified && !bDryRun)
			{
				UDStruct->MarkPackageDirty();
			}
		}
	}

	// Find all affected blueprints
	TArray<FAssetData> AllBlueprintAssets;
	AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), AllBlueprintAssets, true);

	TArray<UBlueprint*> AffectedBlueprints;
	for (const FAssetData& AssetData : AllBlueprintAssets)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
		if (!Blueprint) continue;
		if (DoesBlueprintReferenceEnum(Blueprint, OldEnum))
		{
			AffectedBlueprints.Add(Blueprint);
		}
	}

	TArray<TSharedPtr<FJsonValue>> BlueprintReportsArray;
	int32 TotalPinsMigrated = 0;
	int32 TotalVariablesMigrated = 0;

	for (UBlueprint* Blueprint : AffectedBlueprints)
	{
		TSharedPtr<FJsonObject> BPReport = MakeShared<FJsonObject>();
		BPReport->SetStringField(TEXT("path"), Blueprint->GetPathName());
		BPReport->SetStringField(TEXT("name"), Blueprint->GetName());

		int32 PinCount = 0;
		int32 VarCount = 0;

		if (!bDryRun)
		{
			// Migrate variables
			for (FBPVariableDescription& Var : Blueprint->NewVariables)
			{
				if (Var.VarType.PinSubCategoryObject.Get() == OldEnum)
				{
					Var.VarType.PinSubCategoryObject = NewEnum;
					VarCount++;
				}
			}

			// Collect all graphs (use GetAllGraphs like struct migration)
			TArray<UEdGraph*> AllGraphs;
			Blueprint->GetAllGraphs(AllGraphs);

			// Add sub-graphs recursively (GetAllGraphs may miss node sub-graphs)
			TArray<UEdGraph*> GraphsToProcess = AllGraphs;
			while (GraphsToProcess.Num() > 0)
			{
				UEdGraph* Graph = GraphsToProcess.Pop();
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					for (UEdGraph* SubGraph : Node->GetSubGraphs())
					{
						if (SubGraph && !AllGraphs.Contains(SubGraph))
						{
							AllGraphs.Add(SubGraph);
							GraphsToProcess.Add(SubGraph);
						}
					}
				}
			}

			// Migrate all pins in all graphs (NO ReconstructNode — pin names don't change for enums)
			for (UEdGraph* Graph : AllGraphs)
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					// Handle Switch on Enum nodes — update internal Enum reference
					// Do NOT call SetEnum() as it rebuilds pins and breaks connections
					if (UK2Node_SwitchEnum* SwitchNode = Cast<UK2Node_SwitchEnum>(Node))
					{
						if (SwitchNode->Enum == OldEnum)
						{
							// Build pin FName rename map: OldEnum::OldInternalName → NewEnum::NewValueName
							// Pin FNames use qualified format "EnumName::ValueName"
							FString OldEnumPrefix = OldEnum->GetName() + TEXT("::");
							FString NewEnumPrefix = NewEnum->GetName() + TEXT("::");

							// Remap EnumEntries from old internal names to new names
							for (FName& Entry : SwitchNode->EnumEntries)
							{
								if (const FName* NewVal = ValueNameMap.Find(Entry))
								{
									Entry = *NewVal;
								}
							}

							// Rename Switch node pin FNames from BP-style to C++-style
							// so they match what the C++ enum generates on reload
							for (UEdGraphPin* Pin : SwitchNode->Pins)
							{
								if (!Pin || Pin->Direction != EGPD_Output) continue;
								FString PinNameStr = Pin->PinName.ToString();
								// Check if pin name starts with the enum prefix (qualified name)
								if (PinNameStr.StartsWith(OldEnumPrefix))
								{
									FString OldValuePart = PinNameStr.RightChop(OldEnumPrefix.Len());
									FName OldValueFName(*OldValuePart);
									if (const FName* NewVal = ValueNameMap.Find(OldValueFName))
									{
										Pin->PinName = FName(*(NewEnumPrefix + NewVal->ToString()));
									}
								}
								else
								{
									// Try bare name (without prefix) in case pin uses display name
									FName BareName = Pin->PinName;
									if (const FName* NewVal = ValueNameMap.Find(BareName))
									{
										Pin->PinName = FName(*(NewEnumPrefix + NewVal->ToString()));
									}
								}
							}

							// Directly update enum pointer without reconstructing
							SwitchNode->Enum = NewEnum;
							PinCount++;
							// Fall through to also update pin types below
						}
					}

					// Handle Byte to Enum conversion nodes — update Enum field and reconstruct
					if (UK2Node_CastByteToEnum* CastNode = Cast<UK2Node_CastByteToEnum>(Node))
					{
						if (CastNode->Enum == OldEnum)
						{
							// Save connections (simple: just byte input and enum output)
							struct FCastPinConn { FName PinName; EEdGraphPinDirection Dir; FGuid RemoteGuid; FName RemotePin; };
							TArray<FCastPinConn> SavedConns;
							for (UEdGraphPin* Pin : CastNode->Pins)
							{
								if (!Pin) continue;
								for (UEdGraphPin* Linked : Pin->LinkedTo)
								{
									if (Linked && Linked->GetOwningNode())
										SavedConns.Add({Pin->PinName, Pin->Direction, Linked->GetOwningNode()->NodeGuid, Linked->PinName});
								}
								Pin->BreakAllPinLinks();
							}

							CastNode->Enum = NewEnum;
							CastNode->ReconstructNode();

							// Restore connections
							for (const FCastPinConn& C : SavedConns)
							{
								UEdGraphPin* OurPin = nullptr;
								for (UEdGraphPin* Pin : CastNode->Pins)
								{
									if (Pin && Pin->PinName == C.PinName && Pin->Direction == C.Dir) { OurPin = Pin; break; }
								}
								if (!OurPin)
								{
									// Try matching by direction only (pin names might change)
									for (UEdGraphPin* Pin : CastNode->Pins)
									{
										if (Pin && Pin->Direction == C.Dir && Pin->LinkedTo.Num() == 0) { OurPin = Pin; break; }
									}
								}
								if (OurPin)
								{
									for (UEdGraphNode* SN : Graph->Nodes)
									{
										if (SN && SN->NodeGuid == C.RemoteGuid)
										{
											for (UEdGraphPin* RP : SN->Pins)
											{
												if (RP && RP->PinName == C.RemotePin) { OurPin->MakeLinkTo(RP); break; }
											}
											break;
										}
									}
								}
							}
							PinCount++;
							continue; // Skip generic pin loop — reconstruction handled pins
						}
					}

					// Handle Select nodes — update private fields via reflection (no SetEnum/ReconstructNode)
					if (UK2Node_Select* SelectNode = Cast<UK2Node_Select>(Node))
					{
						if (SelectNode->GetEnum() == OldEnum)
						{
							UClass* SelectClass = SelectNode->GetClass();

							// 1. Update private Enum field
							FObjectProperty* EnumProp = CastField<FObjectProperty>(SelectClass->FindPropertyByName(TEXT("Enum")));
							if (EnumProp)
							{
								void** EnumPtr = EnumProp->ContainerPtrToValuePtr<void*>(SelectNode);
								*EnumPtr = NewEnum;
							}

							// 2. Update private IndexPinType.PinSubCategoryObject
							FStructProperty* IndexPinTypeProp = CastField<FStructProperty>(SelectClass->FindPropertyByName(TEXT("IndexPinType")));
							if (IndexPinTypeProp)
							{
								FEdGraphPinType* IndexPinTypePtr = IndexPinTypeProp->ContainerPtrToValuePtr<FEdGraphPinType>(SelectNode);
								IndexPinTypePtr->PinSubCategoryObject = NewEnum;
							}

							// 3. Build new EnumEntries from NewEnum and update private field
							TArray<FName> NewEnumEntries;
							for (int32 i = 0; i < NewEnum->NumEnums() - 1; ++i)
							{
								bool bHidden = NewEnum->HasMetaData(TEXT("Hidden"), i) || NewEnum->HasMetaData(TEXT("Spacer"), i);
								if (!bHidden)
								{
									NewEnumEntries.Add(FName(*NewEnum->GetNameStringByIndex(i)));
								}
							}

							FArrayProperty* EnumEntriesProp = CastField<FArrayProperty>(SelectClass->FindPropertyByName(TEXT("EnumEntries")));
							if (EnumEntriesProp)
							{
								TArray<FName>* EntriesPtr = EnumEntriesProp->ContainerPtrToValuePtr<TArray<FName>>(SelectNode);

								// 4. Rename option pins: map old entry names to new entry names
								// Old EnumEntries has the BP enum internal names (NewEnumerator0, etc.)
								TArray<FName> OldEntries = *EntriesPtr;
								for (int32 i = 0; i < OldEntries.Num() && i < NewEnumEntries.Num(); ++i)
								{
									FName OldPinName = OldEntries[i];
									FName NewPinName = NewEnumEntries[i];
									for (UEdGraphPin* Pin : SelectNode->Pins)
									{
										if (Pin && Pin->PinName == OldPinName)
										{
											Pin->PinName = NewPinName;
											// Also update default value if it matches old enum value
											if (!Pin->DefaultValue.IsEmpty())
											{
												FName OldVal = FName(*Pin->DefaultValue);
												if (const FName* NewVal = ValueNameMap.Find(OldVal))
												{
													Pin->DefaultValue = NewVal->ToString();
												}
												else if (const FString* NewValStr = DisplayToNewValue.Find(Pin->DefaultValue))
												{
													Pin->DefaultValue = *NewValStr;
												}
											}
											break;
										}
									}
								}

								// Write new EnumEntries
								*EntriesPtr = NewEnumEntries;
							}

							// 5. Update Index pin's PinSubCategoryObject
							for (UEdGraphPin* Pin : SelectNode->Pins)
							{
								if (Pin && Pin->PinName == TEXT("Index"))
								{
									Pin->PinType.PinSubCategoryObject = NewEnum;
									// Remap default value
									if (!Pin->DefaultValue.IsEmpty())
									{
										FName OldVal = FName(*Pin->DefaultValue);
										if (const FName* NewVal = ValueNameMap.Find(OldVal))
										{
											Pin->DefaultValue = NewVal->ToString();
										}
										else if (const FString* NewValStr = DisplayToNewValue.Find(Pin->DefaultValue))
										{
											Pin->DefaultValue = *NewValStr;
										}
									}
									break;
								}
							}

							PinCount++;
							continue; // Skip generic pin loop — Select fully handled
						}
					}

					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin->PinType.PinSubCategoryObject.Get() == OldEnum)
						{
							Pin->PinType.PinSubCategoryObject = NewEnum;

							// Remap default value: try internal name first, then display name
							if (!Pin->DefaultValue.IsEmpty())
							{
								FName OldVal = FName(*Pin->DefaultValue);
								if (const FName* NewVal = ValueNameMap.Find(OldVal))
								{
									Pin->DefaultValue = NewVal->ToString();
								}
								else if (const FString* NewValStr = DisplayToNewValue.Find(Pin->DefaultValue))
								{
									Pin->DefaultValue = *NewValStr;
								}
							}

							PinCount++;
						}
						// Cleanup pass: fix pins already typed as NewEnum but with stale NewEnumerator* defaults
						// (happens when struct reconstruction set C++ enum type but preserved old default values)
						else if (Pin->PinType.PinSubCategoryObject.Get() == NewEnum && !Pin->DefaultValue.IsEmpty())
						{
							FName OldVal = FName(*Pin->DefaultValue);
							if (const FName* NewVal = ValueNameMap.Find(OldVal))
							{
								Pin->DefaultValue = NewVal->ToString();
								PinCount++;
							}
							else if (const FString* NewValStr = DisplayToNewValue.Find(Pin->DefaultValue))
							{
								Pin->DefaultValue = *NewValStr;
								PinCount++;
							}
						}
					}
				}
			}

			// Finalize — no RefreshAllNodes to avoid breaking connections
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
		else
		{
			// Dry run: just count
			for (const FBPVariableDescription& Var : Blueprint->NewVariables)
			{
				if (Var.VarType.PinSubCategoryObject.Get() == OldEnum)
					VarCount++;
			}
			TArray<UEdGraph*> AllGraphs;
			AllGraphs.Append(Blueprint->UbergraphPages);
			AllGraphs.Append(Blueprint->FunctionGraphs);
			TArray<UEdGraph*> GraphsToProcess = AllGraphs;
			while (GraphsToProcess.Num() > 0)
			{
				UEdGraph* Graph = GraphsToProcess.Pop();
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					for (UEdGraph* SubGraph : Node->GetSubGraphs())
					{
						if (SubGraph && !AllGraphs.Contains(SubGraph))
						{
							AllGraphs.Add(SubGraph);
							GraphsToProcess.Add(SubGraph);
						}
					}
				}
			}
			for (UEdGraph* Graph : AllGraphs)
			{
				for (UEdGraphNode* Node : Graph->Nodes)
				{
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin->PinType.PinSubCategoryObject.Get() == OldEnum)
							PinCount++;
					}
				}
			}
		}

		TotalPinsMigrated += PinCount;
		TotalVariablesMigrated += VarCount;

		BPReport->SetNumberField(TEXT("pins_migrated"), PinCount);
		BPReport->SetNumberField(TEXT("variables_migrated"), VarCount);
		BlueprintReportsArray.Add(MakeShared<FJsonValueObject>(BPReport));
	}

	// Build response
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("dry_run"), bDryRun);
	Data->SetStringField(TEXT("source_enum"), OldEnum->GetPathName());
	Data->SetStringField(TEXT("target_enum"), NewEnum->GetPathName());
	Data->SetNumberField(TEXT("value_mappings_count"), ValueNameMap.Num());
	Data->SetArrayField(TEXT("value_name_mapping"), ValueMappingsArray);
	Data->SetNumberField(TEXT("blueprints_affected"), AffectedBlueprints.Num());
	Data->SetArrayField(TEXT("affected_blueprints"), BlueprintReportsArray);
	Data->SetNumberField(TEXT("total_pins_migrated"), TotalPinsMigrated);
	Data->SetNumberField(TEXT("total_variables_migrated"), TotalVariablesMigrated);
	Data->SetNumberField(TEXT("struct_fields_fixed"), TotalStructFieldsFixed);
	if (StructFieldReportsArray.Num() > 0)
	{
		Data->SetArrayField(TEXT("struct_fields"), StructFieldReportsArray);
	}
	if (StructDiagArray.Num() > 0)
	{
		Data->SetArrayField(TEXT("struct_diagnostics"), StructDiagArray);
	}
	Data->SetStringField(TEXT("message"),
		bDryRun ? TEXT("Dry run complete - no changes made") : TEXT("Enum migration complete"));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleFixPropertyAccessPaths(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("source_struct_path")) || !Params->HasField(TEXT("target_struct_path")))
	{
		return MakeError(TEXT("Missing 'source_struct_path' or 'target_struct_path' parameter"));
	}

	FString SourceStructPath = Params->GetStringField(TEXT("source_struct_path"));
	FString TargetStructPath = Params->GetStringField(TEXT("target_struct_path"));
	bool bDryRun = Params->HasField(TEXT("dry_run")) ? Params->GetBoolField(TEXT("dry_run")) : false;
	FString BlueprintFilter = Params->HasField(TEXT("blueprint_path")) ? Params->GetStringField(TEXT("blueprint_path")) : TEXT("");

	// Load old BP struct for field name mapping
	FString FullSourcePath = SourceStructPath;
	if (!FullSourcePath.EndsWith(FPaths::GetCleanFilename(FullSourcePath)))
	{
		FullSourcePath = SourceStructPath + TEXT(".") + FPaths::GetCleanFilename(SourceStructPath);
	}
	UUserDefinedStruct* OldStruct = LoadObject<UUserDefinedStruct>(nullptr, *FullSourcePath);
	if (!OldStruct)
	{
		OldStruct = LoadObject<UUserDefinedStruct>(nullptr, *SourceStructPath);
	}
	if (!OldStruct)
	{
		return MakeError(FString::Printf(TEXT("Source UserDefinedStruct not found at '%s'"), *SourceStructPath));
	}

	// Load new C++ struct (to verify it exists)
	UScriptStruct* NewStruct = nullptr;
	FString TypeNameToFind = TargetStructPath;
	if (TargetStructPath.StartsWith(TEXT("/Script/")))
	{
		FString Remainder = TargetStructPath;
		Remainder.RemoveFromStart(TEXT("/Script/"));
		FString ModuleName;
		Remainder.Split(TEXT("."), &ModuleName, &TypeNameToFind);
	}
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		if (It->GetName() == TypeNameToFind)
		{
			NewStruct = *It;
			break;
		}
	}
	if (!NewStruct)
	{
		return MakeError(FString::Printf(TEXT("Target C++ struct '%s' not found"), *TargetStructPath));
	}

	// Build GUID-suffixed → clean field name mapping
	TMap<FString, FString> FieldNameMap; // String-based for path segment matching
	TArray<TSharedPtr<FJsonValue>> MappingsJsonArray;

	const TArray<FStructVariableDescription>& VarDescs = FStructureEditorUtils::GetVarDesc(OldStruct);
	for (const FStructVariableDescription& Desc : VarDescs)
	{
		FString OldName = Desc.VarName.ToString();
		FString CleanName = Desc.FriendlyName;
		FieldNameMap.Add(OldName, CleanName);

		TSharedPtr<FJsonObject> MapObj = MakeShared<FJsonObject>();
		MapObj->SetStringField(TEXT("old_name"), OldName);
		MapObj->SetStringField(TEXT("new_name"), CleanName);
		MappingsJsonArray.Add(MakeShared<FJsonValueObject>(MapObj));
	}

	// Find the K2Node_PropertyAccess class via reflection (it's in a Private header)
	UClass* PropertyAccessNodeClass = nullptr;
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		if (ClassIt->GetName() == TEXT("K2Node_PropertyAccess"))
		{
			PropertyAccessNodeClass = *ClassIt;
			break;
		}
	}
	if (!PropertyAccessNodeClass)
	{
		return MakeError(TEXT("K2Node_PropertyAccess class not found - PropertyAccessNode plugin may not be loaded"));
	}

	// Find the Path property via reflection
	FArrayProperty* PathProperty = CastField<FArrayProperty>(PropertyAccessNodeClass->FindPropertyByName(TEXT("Path")));
	if (!PathProperty)
	{
		return MakeError(TEXT("Could not find 'Path' property on K2Node_PropertyAccess via reflection"));
	}

	// Find blueprints to scan
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AllBPAssets;
	AssetRegistry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), AllBPAssets);
	TArray<FAssetData> AnimBPAssets;
	AssetRegistry.GetAssetsByClass(UAnimBlueprint::StaticClass()->GetClassPathName(), AnimBPAssets);
	AllBPAssets.Append(AnimBPAssets);

	TArray<TSharedPtr<FJsonValue>> BlueprintReportsArray;
	int32 TotalNodesFixed = 0;
	int32 TotalPathSegmentsUpdated = 0;

	for (const FAssetData& Asset : AllBPAssets)
	{
		FString AssetPath = Asset.GetObjectPathString();
		if (!AssetPath.StartsWith(TEXT("/Game/"))) continue;
		if (!BlueprintFilter.IsEmpty() && !AssetPath.Contains(BlueprintFilter)) continue;

		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
		if (!Blueprint) continue;

		// Collect all graphs including sub-graphs
		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);
		{
			TArray<UEdGraph*> GraphsToProcess = AllGraphs;
			while (GraphsToProcess.Num() > 0)
			{
				UEdGraph* CurrentGraph = GraphsToProcess.Pop();
				for (UEdGraphNode* GraphNode : CurrentGraph->Nodes)
				{
					if (!GraphNode) continue;
					for (UEdGraph* SubGraph : GraphNode->GetSubGraphs())
					{
						if (SubGraph && !AllGraphs.Contains(SubGraph))
						{
							AllGraphs.Add(SubGraph);
							GraphsToProcess.Add(SubGraph);
						}
					}
				}
			}
		}

		int32 NodesFixed = 0;
		int32 SegmentsUpdated = 0;
		TArray<TSharedPtr<FJsonValue>> NodeDetailsArray;

		for (UEdGraph* Graph : AllGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node || !Node->IsA(PropertyAccessNodeClass)) continue;

				// Read Path via reflection
				TArray<FString>* PathPtr = PathProperty->ContainerPtrToValuePtr<TArray<FString>>(Node);
				if (!PathPtr || PathPtr->Num() == 0) continue;

				// Check if any segment needs remapping
				bool bNeedsUpdate = false;
				TArray<FString> NewPath = *PathPtr;
				TSharedPtr<FJsonObject> NodeDetail = MakeShared<FJsonObject>();
				NodeDetail->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString());
				NodeDetail->SetStringField(TEXT("graph"), Graph->GetName());

				TArray<TSharedPtr<FJsonValue>> OldPathJson;
				for (const FString& Seg : *PathPtr)
				{
					OldPathJson.Add(MakeShared<FJsonValueString>(Seg));
				}
				NodeDetail->SetArrayField(TEXT("old_path"), OldPathJson);

				for (int32 i = 0; i < NewPath.Num(); i++)
				{
					if (FString* CleanName = FieldNameMap.Find(NewPath[i]))
					{
						NewPath[i] = *CleanName;
						bNeedsUpdate = true;
						SegmentsUpdated++;
					}
				}

				if (bNeedsUpdate)
				{
					TArray<TSharedPtr<FJsonValue>> NewPathJson;
					for (const FString& Seg : NewPath)
					{
						NewPathJson.Add(MakeShared<FJsonValueString>(Seg));
					}
					NodeDetail->SetArrayField(TEXT("new_path"), NewPathJson);

					if (!bDryRun)
					{
						// Save connections from the output pin
						struct FPAConnection
						{
							FGuid RemoteNodeGuid;
							FName RemotePinName;
						};
						TArray<FPAConnection> SavedConnections;

						UEdGraphPin* OutputPin = Node->FindPin(TEXT("Value"), EGPD_Output);
						if (OutputPin)
						{
							for (UEdGraphPin* Linked : OutputPin->LinkedTo)
							{
								if (Linked && Linked->GetOwningNode())
								{
									SavedConnections.Add({Linked->GetOwningNode()->NodeGuid, Linked->PinName});
								}
							}
							OutputPin->BreakAllPinLinks();
						}

						// Update Path via reflection
						*PathPtr = NewPath;

						// Reconstruct node - this calls AllocatePins which resolves
						// the property and updates TextPath + ResolvedPinType
						Node->ReconstructNode();

						// Restore connections
						UEdGraphPin* NewOutputPin = Node->FindPin(TEXT("Value"), EGPD_Output);
						if (NewOutputPin)
						{
							for (const FPAConnection& Conn : SavedConnections)
							{
								for (UEdGraphNode* SearchNode : Graph->Nodes)
								{
									if (SearchNode && SearchNode->NodeGuid == Conn.RemoteNodeGuid)
									{
										for (UEdGraphPin* RemotePin : SearchNode->Pins)
										{
											if (RemotePin && RemotePin->PinName == Conn.RemotePinName)
											{
												NewOutputPin->MakeLinkTo(RemotePin);
												break;
											}
										}
										break;
									}
								}
							}
						}

						int32 RestoredCount = NewOutputPin ? NewOutputPin->LinkedTo.Num() : 0;
						NodeDetail->SetNumberField(TEXT("connections_restored"), RestoredCount);
						NodeDetail->SetNumberField(TEXT("connections_expected"), SavedConnections.Num());
					}

					NodeDetailsArray.Add(MakeShared<FJsonValueObject>(NodeDetail));
					NodesFixed++;
				}
			}
		}

		if (NodesFixed > 0)
		{
			if (!bDryRun)
			{
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			}

			TSharedPtr<FJsonObject> BPReport = MakeShared<FJsonObject>();
			BPReport->SetStringField(TEXT("path"), Blueprint->GetPathName());
			BPReport->SetStringField(TEXT("name"), Blueprint->GetName());
			BPReport->SetNumberField(TEXT("nodes_fixed"), NodesFixed);
			BPReport->SetNumberField(TEXT("segments_updated"), SegmentsUpdated);
			BPReport->SetArrayField(TEXT("nodes"), NodeDetailsArray);
			BlueprintReportsArray.Add(MakeShared<FJsonValueObject>(BPReport));

			TotalNodesFixed += NodesFixed;
			TotalPathSegmentsUpdated += SegmentsUpdated;
		}
	}

	// Build response
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("dry_run"), bDryRun);
	Data->SetStringField(TEXT("source_struct"), OldStruct->GetPathName());
	Data->SetStringField(TEXT("target_struct"), NewStruct->GetPathName());
	Data->SetNumberField(TEXT("field_mappings_count"), FieldNameMap.Num());
	Data->SetArrayField(TEXT("field_name_mapping"), MappingsJsonArray);
	Data->SetNumberField(TEXT("blueprints_affected"), BlueprintReportsArray.Num());
	Data->SetArrayField(TEXT("affected_blueprints"), BlueprintReportsArray);
	Data->SetNumberField(TEXT("total_nodes_fixed"), TotalNodesFixed);
	Data->SetNumberField(TEXT("total_segments_updated"), TotalPathSegmentsUpdated);
	Data->SetStringField(TEXT("message"),
		bDryRun ? TEXT("Dry run complete - no changes made") : TEXT("PropertyAccess paths updated"));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleFixStructSubPins(const TSharedPtr<FJsonObject>& Params)
{
	FString SourceStructPath = Params->GetStringField(TEXT("source_struct_path"));
	FString TargetStructPath = Params->GetStringField(TEXT("target_struct_path"));
	bool bDryRun = Params->HasField(TEXT("dry_run")) ? Params->GetBoolField(TEXT("dry_run")) : false;
	FString SpecificBPPath = Params->HasField(TEXT("blueprint_path")) ? Params->GetStringField(TEXT("blueprint_path")) : TEXT("");
	bool bReconstructEvents = Params->HasField(TEXT("reconstruct_events")) ? Params->GetBoolField(TEXT("reconstruct_events")) : false;

	// Load old BP struct
	UUserDefinedStruct* OldStruct = Cast<UUserDefinedStruct>(
		StaticLoadObject(UScriptStruct::StaticClass(), nullptr, *SourceStructPath));
	if (!OldStruct)
	{
		return MakeError(FString::Printf(TEXT("Source BP struct not found: %s"), *SourceStructPath));
	}

	// Find new C++ struct
	FString StructNameToFind = TargetStructPath;
	if (TargetStructPath.StartsWith(TEXT("/Script/")))
	{
		FString Remainder = TargetStructPath.RightChop(8);
		FString ModuleName;
		Remainder.Split(TEXT("."), &ModuleName, &StructNameToFind);
	}
	UScriptStruct* NewStruct = nullptr;
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		if (It->GetName() == StructNameToFind) { NewStruct = *It; break; }
	}
	if (!NewStruct)
	{
		return MakeError(FString::Printf(TEXT("Target C++ struct not found: %s"), *TargetStructPath));
	}

	// Build FriendlyName → C++ property name mapping
	TMap<FName, FName> FriendlyToPropertyMap;
	// Build VarName (GUID-suffixed) → C++ property name mapping for sub-pin renaming
	TMap<FString, FString> GUIDToCleanMap;
	const TArray<FStructVariableDescription>& VarDescs = FStructureEditorUtils::GetVarDesc(OldStruct);
	for (const FStructVariableDescription& Desc : VarDescs)
	{
		// Get the clean name from VarName (strip GUID suffix)
		FString VarNameStr = Desc.VarName.ToString();
		// Format: "FieldName_Number_GUID" — find the matching C++ property
		for (TFieldIterator<FProperty> PropIt(NewStruct); PropIt; ++PropIt)
		{
			FProperty* Prop = *PropIt;
			// Check if VarNameStr starts with the property name
			if (VarNameStr.StartsWith(Prop->GetName()))
			{
				FriendlyToPropertyMap.Add(FName(*Desc.FriendlyName), FName(*Prop->GetName()));
				GUIDToCleanMap.Add(VarNameStr, Prop->GetName());
				break;
			}
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("dry_run"), bDryRun);

	// Build mappings array for report
	TArray<TSharedPtr<FJsonValue>> MappingsArray;
	for (const auto& Pair : FriendlyToPropertyMap)
	{
		TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
		M->SetStringField(TEXT("friendly_name"), Pair.Key.ToString());
		M->SetStringField(TEXT("property_name"), Pair.Value.ToString());
		MappingsArray.Add(MakeShared<FJsonValueObject>(M));
	}
	Data->SetArrayField(TEXT("field_mappings"), MappingsArray);

	// Find affected blueprints
	TArray<UBlueprint*> BlueprintsToProcess;
	if (!SpecificBPPath.IsEmpty())
	{
		UBlueprint* BP = LoadBlueprintFromPath(SpecificBPPath);
		if (BP) BlueprintsToProcess.Add(BP);
	}
	else
	{
		FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> AllBPs;
		ARM.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), AllBPs, true);
		for (const FAssetData& AD : AllBPs)
		{
			UBlueprint* BP = Cast<UBlueprint>(AD.GetAsset());
			if (BP) BlueprintsToProcess.Add(BP);
		}
	}

	int32 TotalPinsRenamed = 0;
	int32 TotalEventsReconstructed = 0;
	TArray<TSharedPtr<FJsonValue>> BPReportsArray;

	for (UBlueprint* Blueprint : BlueprintsToProcess)
	{
		int32 BPPinsRenamed = 0;
		int32 BPEventsReconstructed = 0;

		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);
		// Recursively add sub-graphs
		TArray<UEdGraph*> ToProcess = AllGraphs;
		while (ToProcess.Num() > 0)
		{
			UEdGraph* G = ToProcess.Pop();
			for (UEdGraphNode* N : G->Nodes)
			{
				if (!N) continue;
				for (UEdGraph* Sub : N->GetSubGraphs())
				{
					if (Sub && !AllGraphs.Contains(Sub))
					{
						AllGraphs.Add(Sub);
						ToProcess.Add(Sub);
					}
				}
			}
		}

		for (UEdGraph* Graph : AllGraphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node) continue;

				// Rename struct sub-pins that use old friendly names or GUID-suffixed names
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (!Pin) continue;

					// Check if this pin is struct-typed with the new C++ struct
					bool bIsTargetStruct = (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
						Pin->PinType.PinSubCategoryObject.Get() == NewStruct);
					// Check if this pin is a sub-pin of a struct parent (has ParentPin with struct type)
					bool bIsSubPinOfStruct = (Pin->ParentPin != nullptr &&
						Pin->ParentPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
						Pin->ParentPin->PinType.PinSubCategoryObject.Get() == NewStruct);

					// If this is a sub-pin of a C++ struct parent, try GUID-suffixed rename
					if (bIsSubPinOfStruct)
					{
						FString ParentPrefix = Pin->ParentPin->PinName.ToString() + TEXT("_");
						FString PinNameStr = Pin->PinName.ToString();
						if (PinNameStr.StartsWith(ParentPrefix))
						{
							FString FieldPart = PinNameStr.RightChop(ParentPrefix.Len());
							if (FString* CleanName = GUIDToCleanMap.Find(FieldPart))
							{
								FString NewPinName = ParentPrefix + *CleanName;
								if (!bDryRun)
								{
									Pin->PinName = FName(*NewPinName);
								}
								BPPinsRenamed++;
							}
						}
					}

					// Also check if the pin itself or its parent references the struct
					bool bPinNameMatches = FriendlyToPropertyMap.Contains(Pin->PinName);

					if (bPinNameMatches)
					{
						const FName* NewName = FriendlyToPropertyMap.Find(Pin->PinName);
						if (NewName && !bDryRun)
						{
							Pin->PinName = *NewName;
							BPPinsRenamed++;
						}
						else if (NewName)
						{
							BPPinsRenamed++;
						}
					}

					// For struct-typed pins with sub-pins, rename GUID-suffixed sub-pin names
					if (bIsTargetStruct && Pin->SubPins.Num() > 0)
					{
						FString ParentPrefix = Pin->PinName.ToString() + TEXT("_");
						for (UEdGraphPin* SubPin : Pin->SubPins)
						{
							if (!SubPin) continue;
							FString SubPinName = SubPin->PinName.ToString();
							if (SubPinName.StartsWith(ParentPrefix))
							{
								FString FieldPart = SubPinName.RightChop(ParentPrefix.Len());
								// Check GUID-suffixed map first
								if (FString* CleanName = GUIDToCleanMap.Find(FieldPart))
								{
									FString NewPinName = ParentPrefix + *CleanName;
									if (!bDryRun)
									{
										SubPin->PinName = FName(*NewPinName);
									}
									BPPinsRenamed++;
								}
								// Also check FriendlyName map (for non-GUID names)
								else if (FriendlyToPropertyMap.Contains(FName(*FieldPart)))
								{
									const FName* NewName = FriendlyToPropertyMap.Find(FName(*FieldPart));
									if (NewName)
									{
										FString NewPinName = ParentPrefix + NewName->ToString();
										if (!bDryRun)
										{
											SubPin->PinName = FName(*NewPinName);
										}
										BPPinsRenamed++;
									}
								}
							}
						}
					}
					// Legacy: also check sub-pins by exact name match
					else
					{
						for (UEdGraphPin* SubPin : Pin->SubPins)
						{
							if (SubPin && FriendlyToPropertyMap.Contains(SubPin->PinName))
							{
								const FName* NewName = FriendlyToPropertyMap.Find(SubPin->PinName);
								if (NewName && !bDryRun)
								{
									SubPin->PinName = *NewName;
									BPPinsRenamed++;
								}
								else if (NewName)
								{
									BPPinsRenamed++;
								}
							}
						}
					}
				}

				// Reconstruct event nodes that reference the struct
				if (bReconstructEvents && !bDryRun)
				{
					if (Node->IsA<UK2Node_CustomEvent>() || Node->IsA<UK2Node_Event>())
					{
						bool bHasStructPin = false;
						for (UEdGraphPin* Pin : Node->Pins)
						{
							if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
								Pin->PinType.PinSubCategoryObject.Get() == NewStruct)
							{
								bHasStructPin = true;
								break;
							}
						}
						if (bHasStructPin)
						{
							Node->ReconstructNode();
							BPEventsReconstructed++;
						}
					}
				}
			}
		}

		if (BPPinsRenamed > 0 || BPEventsReconstructed > 0)
		{
			if (!bDryRun)
			{
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			}

			TSharedPtr<FJsonObject> BPReport = MakeShared<FJsonObject>();
			BPReport->SetStringField(TEXT("path"), Blueprint->GetPathName());
			BPReport->SetStringField(TEXT("name"), Blueprint->GetName());
			BPReport->SetNumberField(TEXT("pins_renamed"), BPPinsRenamed);
			BPReport->SetNumberField(TEXT("events_reconstructed"), BPEventsReconstructed);
			BPReportsArray.Add(MakeShared<FJsonValueObject>(BPReport));

			TotalPinsRenamed += BPPinsRenamed;
			TotalEventsReconstructed += BPEventsReconstructed;
		}
	}

	Data->SetNumberField(TEXT("blueprints_affected"), BPReportsArray.Num());
	Data->SetArrayField(TEXT("affected_blueprints"), BPReportsArray);
	Data->SetNumberField(TEXT("total_pins_renamed"), TotalPinsRenamed);
	Data->SetNumberField(TEXT("total_events_reconstructed"), TotalEventsReconstructed);
	Data->SetStringField(TEXT("message"), bDryRun ? TEXT("Dry run complete") : TEXT("Struct sub-pins fixed"));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleRenameLocalVariable(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) || !Params->HasField(TEXT("function_name")) ||
		!Params->HasField(TEXT("old_name")) || !Params->HasField(TEXT("new_name")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, function_name, old_name, new_name"));
	}

	FString BPPath = Params->GetStringField(TEXT("blueprint_path"));
	FString FunctionName = Params->GetStringField(TEXT("function_name"));
	FString OldName = Params->GetStringField(TEXT("old_name"));
	FString NewName = Params->GetStringField(TEXT("new_name"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BPPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return MakeError(FString::Printf(TEXT("Function graph '%s' not found"), *FunctionName));
	}

	// Find the FunctionEntry node
	UK2Node_FunctionEntry* EntryNode = nullptr;
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		EntryNode = Cast<UK2Node_FunctionEntry>(Node);
		if (EntryNode) break;
	}

	if (!EntryNode)
	{
		return MakeError(TEXT("FunctionEntry node not found in function graph"));
	}

	// Rename in LocalVariables
	int32 VarsRenamed = 0;
	FName OldFName(*OldName);
	FName NewFName(*NewName);

	for (FBPVariableDescription& LocalVar : EntryNode->LocalVariables)
	{
		if (LocalVar.VarName == OldFName)
		{
			LocalVar.VarName = NewFName;
			LocalVar.FriendlyName = NewName;
			VarsRenamed++;
		}
	}

	// Also update any VariableGet/Set nodes that reference the old name
	int32 NodesUpdated = 0;
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (!Node) continue;

		// Check if this is a variable reference node
		if (UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node))
		{
			if (VarNode->GetVarName() == OldFName)
			{
				VarNode->VariableReference.SetSelfMember(NewFName);
				NodesUpdated++;

				// Also rename the pin
				for (UEdGraphPin* Pin : VarNode->Pins)
				{
					if (Pin && Pin->PinName == OldFName)
					{
						Pin->PinName = NewFName;
					}
				}
			}
		}
	}

	if (VarsRenamed > 0 || NodesUpdated > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprint"), Blueprint->GetName());
	Data->SetStringField(TEXT("function"), FunctionName);
	Data->SetNumberField(TEXT("variables_renamed"), VarsRenamed);
	Data->SetNumberField(TEXT("nodes_updated"), NodesUpdated);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Renamed '%s' to '%s'"), *OldName, *NewName));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleFixPinEnumType(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("wrong_enum_path")) || !Params->HasField(TEXT("correct_enum_path")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, wrong_enum_path, correct_enum_path"));
	}

	FString BPPath = Params->GetStringField(TEXT("blueprint_path"));
	FString WrongEnumPath = Params->GetStringField(TEXT("wrong_enum_path"));
	FString CorrectEnumPath = Params->GetStringField(TEXT("correct_enum_path"));
	FString TargetDefaultValue = Params->HasField(TEXT("default_value")) ? Params->GetStringField(TEXT("default_value")) : TEXT("");
	FString TargetNodeGuid = Params->HasField(TEXT("node_guid")) ? Params->GetStringField(TEXT("node_guid")) : TEXT("");

	UBlueprint* Blueprint = LoadBlueprintFromPath(BPPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));
	}

	// Find both enums
	UEnum* WrongEnum = nullptr;
	UEnum* CorrectEnum = nullptr;

	for (TObjectIterator<UEnum> It; It; ++It)
	{
		FString EnumPath = It->GetPathName();
		if (EnumPath == WrongEnumPath || It->GetName() == FPaths::GetCleanFilename(WrongEnumPath))
		{
			WrongEnum = *It;
		}
		if (EnumPath == CorrectEnumPath || It->GetName() == FPaths::GetCleanFilename(CorrectEnumPath))
		{
			CorrectEnum = *It;
		}
	}

	if (!WrongEnum) return MakeError(FString::Printf(TEXT("Wrong enum not found: %s"), *WrongEnumPath));
	if (!CorrectEnum) return MakeError(FString::Printf(TEXT("Correct enum not found: %s"), *CorrectEnumPath));

	// Iterate all graphs including function graphs and subgraphs
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	{
		TArray<UEdGraph*> GraphsToProcess = AllGraphs;
		while (GraphsToProcess.Num() > 0)
		{
			UEdGraph* CurrentGraph = GraphsToProcess.Pop();
			for (UEdGraphNode* GraphNode : CurrentGraph->Nodes)
			{
				if (!GraphNode) continue;
				for (UEdGraph* SubGraph : GraphNode->GetSubGraphs())
				{
					if (SubGraph && !AllGraphs.Contains(SubGraph))
					{
						AllGraphs.Add(SubGraph);
						GraphsToProcess.Add(SubGraph);
					}
				}
			}
		}
	}

	int32 PinsFixed = 0;
	TArray<TSharedPtr<FJsonValue>> FixedPinsArray;

	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			// If a target node GUID is specified, only fix that node
			if (!TargetNodeGuid.IsEmpty() && Node->NodeGuid.ToString() != TargetNodeGuid) continue;

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;
				if (Pin->PinType.PinSubCategoryObject.Get() != WrongEnum) continue;

				// If a target default value is specified, only fix pins with matching default
				if (!TargetDefaultValue.IsEmpty() && Pin->DefaultValue != TargetDefaultValue) continue;

				Pin->PinType.PinSubCategoryObject = CorrectEnum;
				PinsFixed++;

				TSharedPtr<FJsonObject> PinReport = MakeShared<FJsonObject>();
				PinReport->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
				PinReport->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString());
				PinReport->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
				PinReport->SetStringField(TEXT("default_value"), Pin->DefaultValue);
				PinReport->SetStringField(TEXT("graph"), Graph->GetName());
				FixedPinsArray.Add(MakeShared<FJsonValueObject>(PinReport));
			}

			// Also fix K2Node_CustomEvent::UserDefinedPins
			if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
			{
				for (TSharedPtr<FUserPinInfo>& PinInfo : CustomEvent->UserDefinedPins)
				{
					if (PinInfo.IsValid() && PinInfo->PinType.PinSubCategoryObject.Get() == WrongEnum)
					{
						PinInfo->PinType.PinSubCategoryObject = CorrectEnum;
					}
				}
			}
		}
	}

	if (PinsFixed > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprint"), Blueprint->GetName());
	Data->SetNumberField(TEXT("pins_fixed"), PinsFixed);
	Data->SetArrayField(TEXT("fixed_pins"), FixedPinsArray);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Fixed %d pins from %s to %s"), PinsFixed, *WrongEnum->GetName(), *CorrectEnum->GetName()));

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleFixEnumDefaults(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) || !Params->HasField(TEXT("enum_path")))
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path, enum_path"));
	}

	FString BPPath = Params->GetStringField(TEXT("blueprint_path"));
	FString EnumPath = Params->GetStringField(TEXT("enum_path"));
	FString OldEnumPath = Params->HasField(TEXT("old_enum_path")) ? Params->GetStringField(TEXT("old_enum_path")) : TEXT("");
	bool bDryRun = Params->HasField(TEXT("dry_run")) && Params->GetBoolField(TEXT("dry_run"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BPPath);
	if (!Blueprint) return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));

	UEnum* TargetEnum = nullptr;
	for (TObjectIterator<UEnum> It; It; ++It)
	{
		if (It->GetPathName() == EnumPath)
		{
			TargetEnum = *It;
			break;
		}
	}
	if (!TargetEnum) return MakeError(FString::Printf(TEXT("Enum not found: %s"), *EnumPath));

	// Load old enum if specified (for mapping old internal names)
	UUserDefinedEnum* OldEnum = nullptr;
	if (!OldEnumPath.IsEmpty())
	{
		OldEnum = Cast<UUserDefinedEnum>(StaticLoadObject(UUserDefinedEnum::StaticClass(), nullptr, *OldEnumPath));
	}

	// Build map of short/old name -> valid default value for target enum
	FString EnumPrefix = TargetEnum->GetName() + TEXT("::");
	TMap<FString, FString> ValueFixMap; // Maps invalid default values to valid ones
	for (int32 i = 0; i < TargetEnum->NumEnums() - 1; ++i)
	{
		FString FullName = TargetEnum->GetNameStringByIndex(i);
		FString DisplayName = TargetEnum->GetDisplayNameTextByIndex(i).ToString();

		// Determine the valid default value format
		// Use the name form that GetIndexByNameString accepts
		FString ValidForm = FullName;

		// Check if GetNameStringByIndex returns short or qualified form
		FString ShortName = FullName.Contains(TEXT("::")) ? FullName.RightChop(FullName.Find(TEXT("::")) + 2) : FullName;
		FString QualifiedName = FullName.Contains(TEXT("::")) ? FullName : EnumPrefix + FullName;

		// Map various possible default values to the valid form
		if (ShortName != ValidForm) ValueFixMap.Add(ShortName, ValidForm);
		if (QualifiedName != ValidForm) ValueFixMap.Add(QualifiedName, ValidForm);
		if (DisplayName != ValidForm && DisplayName != ShortName) ValueFixMap.Add(DisplayName, ValidForm);

		// If old enum provided, also map old internal names
		if (OldEnum)
		{
			FText OldDisplayText = OldEnum->GetDisplayNameTextByIndex(i);
			if (OldDisplayText.ToString() == DisplayName && i < OldEnum->NumEnums() - 1)
			{
				FString OldInternalName = OldEnum->GetNameStringByIndex(i);
				if (OldInternalName != ValidForm)
				{
					ValueFixMap.Add(OldInternalName, ValidForm);
				}
			}
		}
	}

	// Also build old enum internal name mapping by iterating old enum directly
	if (OldEnum)
	{
		for (int32 i = 0; i < OldEnum->NumEnums() - 1; ++i)
		{
			FString OldInternalName = OldEnum->GetNameStringByIndex(i);
			FString OldDisplayName = OldEnum->GetDisplayNameTextByIndex(i).ToString();
			// Find matching new enum value by display name
			for (int32 j = 0; j < TargetEnum->NumEnums() - 1; ++j)
			{
				FString NewDisplayName = TargetEnum->GetDisplayNameTextByIndex(j).ToString();
				if (NewDisplayName == OldDisplayName)
				{
					FString ValidForm = TargetEnum->GetNameStringByIndex(j);
					ValueFixMap.Add(OldInternalName, ValidForm);
					break;
				}
			}
		}
	}

	// Log the mapping for debugging
	TArray<TSharedPtr<FJsonValue>> MappingsArray;
	for (auto& Pair : ValueFixMap)
	{
		TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
		M->SetStringField(TEXT("from"), Pair.Key);
		M->SetStringField(TEXT("to"), Pair.Value);
		MappingsArray.Add(MakeShared<FJsonValueObject>(M));
	}

	// Collect all graphs
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	{
		TArray<UEdGraph*> GraphsToProcess = AllGraphs;
		while (GraphsToProcess.Num() > 0)
		{
			UEdGraph* G = GraphsToProcess.Pop();
			for (UEdGraphNode* N : G->Nodes)
			{
				if (!N) continue;
				for (UEdGraph* Sub : N->GetSubGraphs())
				{
					if (Sub && !AllGraphs.Contains(Sub))
					{
						AllGraphs.Add(Sub);
						GraphsToProcess.Add(Sub);
					}
				}
			}
		}
	}

	int32 PinsFixed = 0;
	TArray<TSharedPtr<FJsonValue>> FixedPinsArray;
	TArray<TSharedPtr<FJsonValue>> DiagnosticArray;

	// Also find ALL enum objects named the same (to catch BP/C++ duplicates)
	FString TargetEnumName = TargetEnum->GetName();
	TArray<UEnum*> AllSameNameEnums;
	for (TObjectIterator<UEnum> It; It; ++It)
	{
		if (It->GetName() == TargetEnumName)
		{
			AllSameNameEnums.Add(*It);
		}
	}

	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			// Fix UK2Node_CastByteToEnum node-level Enum reference
			if (UK2Node_CastByteToEnum* CastNode = Cast<UK2Node_CastByteToEnum>(Node))
			{
				if (CastNode->Enum)
				{
					bool bOldEnum = false;
					for (UEnum* E : AllSameNameEnums)
					{
						if (CastNode->Enum == E && CastNode->Enum != TargetEnum) { bOldEnum = true; break; }
					}
					if (bOldEnum && !bDryRun)
					{
						CastNode->Enum = TargetEnum;
						PinsFixed++;
					}
				}
			}

			// Fix UK2Node_SwitchEnum node-level Enum reference
			if (UK2Node_SwitchEnum* SwitchNode = Cast<UK2Node_SwitchEnum>(Node))
			{
				if (SwitchNode->Enum)
				{
					bool bOldEnum = false;
					for (UEnum* E : AllSameNameEnums)
					{
						if (SwitchNode->Enum == E && SwitchNode->Enum != TargetEnum) { bOldEnum = true; break; }
					}
					if (bOldEnum && !bDryRun)
					{
						SwitchNode->Enum = TargetEnum;
						PinsFixed++;
					}
				}
			}

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;
				UObject* PinEnumObj = Pin->PinType.PinSubCategoryObject.Get();
				if (!PinEnumObj) continue;

				// Check if this pin references ANY enum with the same name
				bool bMatchesTarget = (PinEnumObj == TargetEnum);
				bool bMatchesSameName = false;
				for (UEnum* E : AllSameNameEnums)
				{
					if (PinEnumObj == E) { bMatchesSameName = true; break; }
				}

				if (!bMatchesSameName) continue;

				if (!bMatchesTarget)
				{
					// Wrong enum object — fix PinSubCategoryObject to target
					if (!bDryRun)
					{
						Pin->PinType.PinSubCategoryObject = TargetEnum;
					}
					PinsFixed++;
				}

				if (Pin->DefaultValue.IsEmpty()) continue;

				// Report for diagnostic (only pins with non-empty defaults)
				TSharedPtr<FJsonObject> Diag = MakeShared<FJsonObject>();
				Diag->SetStringField(TEXT("graph"), Graph->GetName());
				Diag->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
				Diag->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
				Diag->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString());
				Diag->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
				Diag->SetStringField(TEXT("default_value"), Pin->DefaultValue);
				Diag->SetStringField(TEXT("enum_path"), PinEnumObj->GetPathName());
				Diag->SetBoolField(TEXT("is_target_enum"), bMatchesTarget);
				DiagnosticArray.Add(MakeShared<FJsonValueObject>(Diag));

				// Check if default value is valid for the TARGET enum
				int32 ValIndex = TargetEnum->GetIndexByNameString(Pin->DefaultValue);
				if (ValIndex != INDEX_NONE) continue; // Already valid on target enum

				// Try to fix: look up in ValueFixMap
				const FString* QualifiedPtr = ValueFixMap.Find(Pin->DefaultValue);
				if (QualifiedPtr)
				{
					TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
					Report->SetStringField(TEXT("graph"), Graph->GetName());
					Report->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
					Report->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString());
					Report->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
					Report->SetStringField(TEXT("old_value"), Pin->DefaultValue);
					Report->SetStringField(TEXT("new_value"), *QualifiedPtr);
					FixedPinsArray.Add(MakeShared<FJsonValueObject>(Report));

					if (!bDryRun)
					{
						Pin->DefaultValue = *QualifiedPtr;
					}
					PinsFixed++;
				}
			}

			// Also fix UserDefinedPins on FunctionEntry and FunctionResult nodes
			// These define function signatures and override pin types
			if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
			{
				for (TSharedPtr<FUserPinInfo>& PinInfo : EntryNode->UserDefinedPins)
				{
					if (PinInfo.IsValid())
					{
						UObject* PinInfoEnum = PinInfo->PinType.PinSubCategoryObject.Get();
						bool bInfoMatch = false;
						for (UEnum* E : AllSameNameEnums)
						{
							if (PinInfoEnum == E && PinInfoEnum != TargetEnum) { bInfoMatch = true; break; }
						}
						if (bInfoMatch && !bDryRun)
						{
							PinInfo->PinType.PinSubCategoryObject = TargetEnum;
							PinsFixed++;
						}
					}
				}
			}
			if (UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node))
			{
				for (TSharedPtr<FUserPinInfo>& PinInfo : ResultNode->UserDefinedPins)
				{
					if (PinInfo.IsValid())
					{
						UObject* PinInfoEnum = PinInfo->PinType.PinSubCategoryObject.Get();
						bool bInfoMatch = false;
						for (UEnum* E : AllSameNameEnums)
						{
							if (PinInfoEnum == E && PinInfoEnum != TargetEnum) { bInfoMatch = true; break; }
						}
						if (bInfoMatch && !bDryRun)
						{
							PinInfo->PinType.PinSubCategoryObject = TargetEnum;
							PinsFixed++;
						}
					}
				}
			}
			// Also fix K2Node_CustomEvent UserDefinedPins
			if (UK2Node_CustomEvent* EventNode = Cast<UK2Node_CustomEvent>(Node))
			{
				for (TSharedPtr<FUserPinInfo>& PinInfo : EventNode->UserDefinedPins)
				{
					if (PinInfo.IsValid())
					{
						UObject* PinInfoEnum = PinInfo->PinType.PinSubCategoryObject.Get();
						bool bInfoMatch = false;
						for (UEnum* E : AllSameNameEnums)
						{
							if (PinInfoEnum == E && PinInfoEnum != TargetEnum) { bInfoMatch = true; break; }
						}
						if (bInfoMatch && !bDryRun)
						{
							PinInfo->PinType.PinSubCategoryObject = TargetEnum;
							PinsFixed++;
						}
					}
				}
			}
		}
	}

	// Also fix Blueprint variables
	for (FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		UObject* VarEnum = Var.VarType.PinSubCategoryObject.Get();
		bool bVarMatch = false;
		for (UEnum* E : AllSameNameEnums)
		{
			if (VarEnum == E && VarEnum != TargetEnum) { bVarMatch = true; break; }
		}
		if (bVarMatch && !bDryRun)
		{
			Var.VarType.PinSubCategoryObject = TargetEnum;
			PinsFixed++;
		}
	}

	if ((PinsFixed > 0 || DiagnosticArray.Num() > 0) && !bDryRun)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("blueprint"), Blueprint->GetName());
	Data->SetStringField(TEXT("enum"), TargetEnum->GetName());
	Data->SetStringField(TEXT("enum_path"), TargetEnum->GetPathName());
	Data->SetBoolField(TEXT("dry_run"), bDryRun);
	Data->SetNumberField(TEXT("pins_fixed"), PinsFixed);
	Data->SetNumberField(TEXT("same_name_enums"), AllSameNameEnums.Num());
	Data->SetNumberField(TEXT("graphs_searched"), AllGraphs.Num());
	Data->SetArrayField(TEXT("mappings"), MappingsArray);
	Data->SetArrayField(TEXT("diagnostic"), DiagnosticArray);
	Data->SetArrayField(TEXT("fixed_pins"), FixedPinsArray);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleFixAssetStructReference(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("asset_path")) ||
		!Params->HasField(TEXT("old_struct_path")) || !Params->HasField(TEXT("new_struct_path")))
	{
		return MakeError(TEXT("Missing required parameters: asset_path, old_struct_path, new_struct_path"));
	}

	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FString OldStructPath = Params->GetStringField(TEXT("old_struct_path"));
	FString NewStructPath = Params->GetStringField(TEXT("new_struct_path"));

	// Load old struct
	FString FullOldPath = OldStructPath;
	if (!FullOldPath.Contains(TEXT(".")))
	{
		FullOldPath = OldStructPath + TEXT(".") + FPaths::GetCleanFilename(OldStructPath);
	}
	UScriptStruct* OldStruct = LoadObject<UScriptStruct>(nullptr, *FullOldPath);
	if (!OldStruct)
	{
		OldStruct = LoadObject<UScriptStruct>(nullptr, *OldStructPath);
	}
	if (!OldStruct)
	{
		return MakeError(FString::Printf(TEXT("Old struct not found: %s"), *OldStructPath));
	}

	// Load new struct
	UScriptStruct* NewStruct = nullptr;
	FString TypeNameToFind = NewStructPath;
	if (NewStructPath.StartsWith(TEXT("/Script/")))
	{
		FString Remainder = NewStructPath;
		Remainder.RemoveFromStart(TEXT("/Script/"));
		FString ModuleName;
		Remainder.Split(TEXT("."), &ModuleName, &TypeNameToFind);
	}
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		if (It->GetName() == TypeNameToFind)
		{
			NewStruct = *It;
			break;
		}
	}
	if (!NewStruct)
	{
		return MakeError(FString::Printf(TEXT("New struct not found: %s"), *NewStructPath));
	}

	// Load the asset
	FString FullAssetPath = AssetPath;
	if (!FullAssetPath.Contains(TEXT(".")))
	{
		FullAssetPath = AssetPath + TEXT(".") + FPaths::GetCleanFilename(AssetPath);
	}
	UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *FullAssetPath);
	if (!Asset)
	{
		Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	}
	if (!Asset)
	{
		return MakeError(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
	}

	// Deep property traversal to find struct references (including inside FInstancedStruct)
	int32 PropertiesFixed = 0;
	TArray<TSharedPtr<FJsonValue>> FixedArray;

	// Get FInstancedStruct type for special handling
	UScriptStruct* InstancedStructType = TBaseStructure<FInstancedStruct>::Get();

	// Recursive lambda to walk struct data
	TSet<void*> VisitedPtrs;
	TFunction<void(void*, const UStruct*, const FString&)> WalkStructData;
	WalkStructData = [&](void* DataPtr, const UStruct* StructType, const FString& Prefix)
	{
		if (!DataPtr || !StructType) return;
		if (VisitedPtrs.Contains(DataPtr)) return;
		VisitedPtrs.Add(DataPtr);

		for (TFieldIterator<FProperty> PropIt(StructType); PropIt; ++PropIt)
		{
			FProperty* Prop = *PropIt;
			FString PropPath = Prefix + Prop->GetName();

			// Check FObjectPropertyBase values (TObjectPtr<UScriptStruct>, etc.)
			if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
			{
				void* ValuePtr = ObjProp->ContainerPtrToValuePtr<void>(DataPtr);
				UObject* ObjValue = ObjProp->GetObjectPropertyValue(ValuePtr);
				if (ObjValue == OldStruct)
				{
					ObjProp->SetObjectPropertyValue(ValuePtr, NewStruct);
					PropertiesFixed++;

					TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
					Report->SetStringField(TEXT("property"), PropPath);
					Report->SetStringField(TEXT("type"), TEXT("ObjectProperty"));
					FixedArray.Add(MakeShared<FJsonValueObject>(Report));
				}
			}

			// Recurse into struct properties
			if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				void* StructDataPtr = StructProp->ContainerPtrToValuePtr<void>(DataPtr);

				// Special handling for FInstancedStruct
				if (StructProp->Struct == InstancedStructType)
				{
					FInstancedStruct* Instanced = static_cast<FInstancedStruct*>(StructDataPtr);
					if (Instanced && Instanced->IsValid())
					{
						const UScriptStruct* InnerType = Instanced->GetScriptStruct();
						if (InnerType == OldStruct)
						{
							// The FInstancedStruct itself holds the old struct type — reinit with new type
							Instanced->InitializeAs(NewStruct);
							PropertiesFixed++;

							TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
							Report->SetStringField(TEXT("property"), PropPath + TEXT(" (FInstancedStruct::ScriptStruct)"));
							Report->SetStringField(TEXT("type"), TEXT("FInstancedStruct type"));
							FixedArray.Add(MakeShared<FJsonValueObject>(Report));
						}
						else if (InnerType)
						{
							// Recurse into the FInstancedStruct's data to find nested references
							void* InnerData = Instanced->GetMutableMemory();
							if (InnerData)
							{
								WalkStructData(InnerData, InnerType, PropPath + TEXT("."));
							}
						}
					}
				}
				else
				{
					// Regular struct — recurse
					WalkStructData(StructDataPtr, StructProp->Struct, PropPath + TEXT("."));
				}
			}

			// Recurse into arrays
			if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
			{
				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(DataPtr));
				for (int32 i = 0; i < ArrayHelper.Num(); i++)
				{
					FString ElemPath = FString::Printf(TEXT("%s[%d]."), *PropPath, i);
					void* ElemPtr = ArrayHelper.GetRawPtr(i);

					if (FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner))
					{
						// Special handling for TArray<FInstancedStruct>
						if (InnerStructProp->Struct == InstancedStructType)
						{
							FInstancedStruct* Instanced = static_cast<FInstancedStruct*>(ElemPtr);
							if (Instanced && Instanced->IsValid())
							{
								const UScriptStruct* InnerType = Instanced->GetScriptStruct();
								if (InnerType == OldStruct)
								{
									Instanced->InitializeAs(NewStruct);
									PropertiesFixed++;

									TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
									Report->SetStringField(TEXT("property"), ElemPath + TEXT("(FInstancedStruct::ScriptStruct)"));
									Report->SetStringField(TEXT("type"), TEXT("FInstancedStruct type in array"));
									FixedArray.Add(MakeShared<FJsonValueObject>(Report));
								}
								else if (InnerType)
								{
									void* InnerData = Instanced->GetMutableMemory();
									if (InnerData)
									{
										WalkStructData(InnerData, InnerType, ElemPath);
									}
								}
							}
						}
						else
						{
							// Regular struct array element — recurse
							WalkStructData(ElemPtr, InnerStructProp->Struct, ElemPath);
						}
					}

					// Object array elements
					if (FObjectPropertyBase* InnerObjProp = CastField<FObjectPropertyBase>(ArrayProp->Inner))
					{
						UObject* ObjValue = InnerObjProp->GetObjectPropertyValue(ElemPtr);
						if (ObjValue == OldStruct)
						{
							InnerObjProp->SetObjectPropertyValue(ElemPtr, NewStruct);
							PropertiesFixed++;

							TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
							Report->SetStringField(TEXT("property"), ElemPath);
							Report->SetStringField(TEXT("type"), TEXT("ObjectProperty in array"));
							FixedArray.Add(MakeShared<FJsonValueObject>(Report));
						}
					}
				}
			}
		}
	};

	// Walk the main asset object and all subobjects
	TSet<UObject*> VisitedObjs;
	TArray<UObject*> ObjQueue;
	ObjQueue.Add(Asset);
	TArray<UObject*> SubObjects;
	GetObjectsWithOuter(Asset, SubObjects, true);
	ObjQueue.Append(SubObjects);

	for (UObject* Obj : ObjQueue)
	{
		if (!Obj || VisitedObjs.Contains(Obj)) continue;
		VisitedObjs.Add(Obj);
		WalkStructData(Obj, Obj->GetClass(), Obj == Asset ? TEXT("") : Obj->GetName() + TEXT("."));
	}

	if (PropertiesFixed > 0)
	{
		Asset->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset"), Asset->GetPathName());
	Data->SetStringField(TEXT("asset_class"), Asset->GetClass()->GetName());
	Data->SetNumberField(TEXT("properties_fixed"), PropertiesFixed);
	Data->SetArrayField(TEXT("fixed_properties"), FixedArray);
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Fixed %d struct references in %s"), PropertiesFixed, *Asset->GetName()));

	return MakeResponse(true, Data);
}
