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
#include "K2Node_Message.h"
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
