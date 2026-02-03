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

FMCPServer::FMCPServer()
{
}

FMCPServer::~FMCPServer()
{
	Stop();
}

bool FMCPServer::Start(int32 Port)
{
	FIPv4Endpoint Endpoint(FIPv4Address::Any, Port);

	Listener = new FTcpListener(Endpoint);
	Listener->OnConnectionAccepted().BindRaw(this, &FMCPServer::HandleConnection);

	if (Listener->Init())
	{
		bRunning = true;
		return true;
	}

	delete Listener;
	Listener = nullptr;
	return false;
}

void FMCPServer::Stop()
{
	bRunning = false;

	if (Listener)
	{
		Listener->Stop();
		delete Listener;
		Listener = nullptr;
	}

	FScopeLock Lock(&ClientSocketsLock);
	for (FSocket* Socket : ClientSockets)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
	}
	ClientSockets.Empty();
}

bool FMCPServer::HandleConnection(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint)
{
	if (!bRunning) return false;

	UE_LOG(LogTemp, Log, TEXT("ClaudeUnrealMCP: Client connected from %s"), *ClientEndpoint.ToString());

	{
		FScopeLock Lock(&ClientSocketsLock);
		ClientSockets.Add(ClientSocket);
	}

	// Handle client in async task
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, ClientSocket]()
	{
		HandleClient(ClientSocket);
	});

	return true;
}

void FMCPServer::HandleClient(FSocket* ClientSocket)
{
	// Enable TCP settings for better connection handling
	ClientSocket->SetLinger(false, 0);      // Don't wait on close
	ClientSocket->SetNoDelay(true);         // Disable Nagle's algorithm for low latency
	ClientSocket->SetNonBlocking(false);     // Use blocking mode for simpler logic

	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(65536);

	// Handle ONE request per connection, then close
	// This prevents stale connection accumulation
	uint32 PendingDataSize = 0;
	const double StartTime = FPlatformTime::Seconds();
	const double MaxWaitTime = 5.0; // Wait max 5 seconds for data

	while (bRunning && (FPlatformTime::Seconds() - StartTime) < MaxWaitTime)
	{
		// Check socket state first
		ESocketConnectionState State = ClientSocket->GetConnectionState();
		if (State != ESocketConnectionState::SCS_Connected)
		{
			UE_LOG(LogTemp, Warning, TEXT("ClaudeUnrealMCP: Client disconnected before sending data"));
			break;
		}

		if (ClientSocket->HasPendingData(PendingDataSize) && PendingDataSize > 0)
		{
			int32 BytesRead = 0;
			if (ClientSocket->Recv(Buffer.GetData(), Buffer.Num() - 1, BytesRead))
			{
				if (BytesRead > 0)
				{
					Buffer[BytesRead] = 0;
					FString Request = UTF8_TO_TCHAR(Buffer.GetData());

					// Parse JSON command
					TSharedPtr<FJsonObject> JsonObject;
					TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Request);

					FString Response;
					if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
					{
						// Process on game thread for UE API safety
						FString* ResponsePtr = &Response;
						FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool(false);

						AsyncTask(ENamedThreads::GameThread, [this, JsonObject, ResponsePtr, DoneEvent]()
						{
							*ResponsePtr = ProcessCommand(JsonObject);
							DoneEvent->Trigger();
						});

						DoneEvent->Wait();
						FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
					}
					else
					{
						Response = MakeError(TEXT("Invalid JSON"));
					}

					// Send response
					Response += TEXT("\n");
					FTCHARToUTF8 Converter(*Response);

					// Send all data, handling partial sends
					int32 TotalBytesSent = 0;
					int32 DataLength = Converter.Length();
					while (TotalBytesSent < DataLength)
					{
						int32 BytesSent = 0;
						if (!ClientSocket->Send((uint8*)Converter.Get() + TotalBytesSent, DataLength - TotalBytesSent, BytesSent))
						{
							UE_LOG(LogTemp, Warning, TEXT("ClaudeUnrealMCP: Failed to send response to client"));
							break;
						}
						TotalBytesSent += BytesSent;

						if (BytesSent == 0)
						{
							// No progress, avoid infinite loop
							UE_LOG(LogTemp, Warning, TEXT("ClaudeUnrealMCP: Socket send returned 0 bytes, aborting"));
							break;
						}
					}

					if (TotalBytesSent == DataLength)
					{
						UE_LOG(LogTemp, Log, TEXT("ClaudeUnrealMCP: Successfully sent %d bytes"), DataLength);
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("ClaudeUnrealMCP: Partial send - sent %d of %d bytes"), TotalBytesSent, DataLength);
					}

					// Close connection after handling ONE request
					// This prevents connection reuse issues and stale connections
					break;
				}
				else
				{
					// Zero bytes read means connection closed
					break;
				}
			}
			else
			{
				// Recv failed, connection likely dead
				UE_LOG(LogTemp, Warning, TEXT("ClaudeUnrealMCP: Socket recv failed"));
				break;
			}
		}
		else
		{
			// No data yet, sleep briefly
			FPlatformProcess::Sleep(0.01f);
		}
	}

	// Clean up connection
	ClientSocket->Close();

	{
		FScopeLock Lock(&ClientSocketsLock);
		ClientSockets.Remove(ClientSocket);
	}

	ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
}

FString FMCPServer::ProcessCommand(const TSharedPtr<FJsonObject>& JsonCommand)
{
	FString Command = JsonCommand->GetStringField(TEXT("command"));
	TSharedPtr<FJsonObject> Params = JsonCommand->GetObjectField(TEXT("params"));

	if (Command == TEXT("list_blueprints"))
	{
		return HandleListBlueprints(Params);
	}
	else if (Command == TEXT("check_all_blueprints"))
	{
		return HandleCheckAllBlueprints(Params);
	}
	else if (Command == TEXT("read_blueprint"))
	{
		return HandleReadBlueprint(Params);
	}
	else if (Command == TEXT("read_variables"))
	{
		return HandleReadVariables(Params);
	}
	else if (Command == TEXT("read_class_defaults"))
	{
		return HandleReadClassDefaults(Params);
	}
	else if (Command == TEXT("read_components"))
	{
		return HandleReadComponents(Params);
	}
	else if (Command == TEXT("read_component_properties"))
	{
		return HandleReadComponentProperties(Params);
	}
	else if (Command == TEXT("read_event_graph"))
	{
		return HandleReadEventGraph(Params);
	}
	else if (Command == TEXT("read_event_graph_detailed"))
	{
		return HandleReadEventGraphDetailed(Params);
	}
	else if (Command == TEXT("read_function_graphs"))
	{
		return HandleReadFunctionGraphs(Params);
	}
	else if (Command == TEXT("read_timelines"))
	{
		return HandleReadTimelines(Params);
	}
	else if (Command == TEXT("read_interface"))
	{
		return HandleReadInterface(Params);
	}
	else if (Command == TEXT("read_user_defined_struct"))
	{
		return HandleReadUserDefinedStruct(Params);
	}
	else if (Command == TEXT("read_user_defined_enum"))
	{
		return HandleReadUserDefinedEnum(Params);
	}
	else if (Command == TEXT("list_actors"))
	{
		return HandleListActors(Params);
	}
	else if (Command == TEXT("find_actors_by_name"))
	{
		return HandleFindActorsByName(Params);
	}
	else if (Command == TEXT("get_actor_material_info"))
	{
		return HandleGetActorMaterialInfo(Params);
	}
	else if (Command == TEXT("get_scene_summary"))
	{
		return HandleGetSceneSummary(Params);
	}
	else if (Command == TEXT("ping"))
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("message"), TEXT("pong"));
		return MakeResponse(true, Data);
	}
	else if (Command == TEXT("list_structs"))
	{
		// Debug command to list all registered UScriptStruct objects matching a pattern
		FString Pattern = TEXT("FS_");
		if (Params.IsValid() && Params->HasField(TEXT("pattern")))
		{
			Pattern = Params->GetStringField(TEXT("pattern"));
		}

		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> StructsArray;

		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			UScriptStruct* Struct = *It;
			if (Struct->GetName().Contains(Pattern))
			{
				TSharedPtr<FJsonObject> StructInfo = MakeShared<FJsonObject>();
				StructInfo->SetStringField(TEXT("name"), Struct->GetName());
				StructInfo->SetStringField(TEXT("path"), Struct->GetPathName());
				StructsArray.Add(MakeShared<FJsonValueObject>(StructInfo));
			}
		}

		Data->SetArrayField(TEXT("structs"), StructsArray);
		Data->SetNumberField(TEXT("count"), StructsArray.Num());
		return MakeResponse(true, Data);
	}
	// Write commands
	else if (Command == TEXT("add_component"))
	{
		return HandleAddComponent(Params);
	}
	else if (Command == TEXT("set_component_property"))
	{
		return HandleSetComponentProperty(Params);
	}
	else if (Command == TEXT("set_blueprint_cdo_class_reference"))
	{
		return HandleSetBlueprintCDOClassReference(Params);
	}
	else if (Command == TEXT("replace_component_map_value"))
	{
		return HandleReplaceComponentMapValue(Params);
	}
	else if (Command == TEXT("replace_blueprint_array_value"))
	{
		return HandleReplaceBlueprintArrayValue(Params);
	}
	else if (Command == TEXT("add_input_mapping"))
	{
		return HandleAddInputMapping(Params);
	}
	else if (Command == TEXT("reparent_blueprint"))
	{
		return HandleReparentBlueprint(Params);
	}
	else if (Command == TEXT("compile_blueprint"))
	{
		return HandleCompileBlueprint(Params);
	}
	else if (Command == TEXT("save_asset"))
	{
		return HandleSaveAsset(Params);
	}
	else if (Command == TEXT("save_all"))
	{
		return HandleSaveAll(Params);
	}
	else if (Command == TEXT("delete_interface_function"))
	{
		return HandleDeleteInterfaceFunction(Params);
	}
	else if (Command == TEXT("modify_interface_function_parameter"))
	{
		return HandleModifyInterfaceFunctionParameter(Params);
	}
	else if (Command == TEXT("delete_function_graph"))
	{
		return HandleDeleteFunctionGraph(Params);
	}
	else if (Command == TEXT("clear_event_graph"))
	{
		return HandleClearEventGraph(Params);
	}
	else if (Command == TEXT("empty_graph"))
	{
		return HandleClearEventGraph(Params);
	}
	else if (Command == TEXT("refresh_nodes"))
	{
		return HandleRefreshNodes(Params);
	}
	else if (Command == TEXT("break_orphaned_pins"))
	{
		return HandleBreakOrphanedPins(Params);
	}
	else if (Command == TEXT("delete_user_defined_struct"))
	{
		return HandleDeleteUserDefinedStruct(Params);
	}
	else if (Command == TEXT("modify_struct_field"))
	{
		return HandleModifyStructField(Params);
	}
	else if (Command == TEXT("set_blueprint_compile_settings"))
	{
		return HandleSetBlueprintCompileSettings(Params);
	}
	else if (Command == TEXT("modify_function_metadata"))
	{
		return HandleModifyFunctionMetadata(Params);
	}
	else if (Command == TEXT("capture_screenshot"))
	{
		return HandleCaptureScreenshot(Params);
	}
	else if (Command == TEXT("remove_error_nodes"))
	{
		return HandleRemoveErrorNodes(Params);
	}
	else if (Command == TEXT("clear_animation_blueprint_tags"))
	{
		return HandleClearAnimationBlueprintTags(Params);
	}
	else if (Command == TEXT("clear_anim_graph"))
	{
		return HandleClearAnimGraph(Params);
	}
	// Blueprint function creation commands (Sprint 1)
	else if (Command == TEXT("create_blueprint_function"))
	{
		return HandleCreateBlueprintFunction(Params);
	}
	else if (Command == TEXT("add_function_input"))
	{
		return HandleAddFunctionInput(Params);
	}
	else if (Command == TEXT("add_function_output"))
	{
		return HandleAddFunctionOutput(Params);
	}
	else if (Command == TEXT("rename_blueprint_function"))
	{
		return HandleRenameBlueprintFunction(Params);
	}
	else if (Command == TEXT("read_actor_properties"))
	{
		return HandleReadActorProperties(Params);
	}
	else if (Command == TEXT("set_actor_properties"))
	{
		return HandleSetActorProperties(Params);
	}
	else if (Command == TEXT("reconstruct_actor"))
	{
		return HandleReconstructActor(Params);
	}
	else if (Command == TEXT("clear_component_map_value_array"))
	{
		return HandleClearComponentMapValueArray(Params);
	}
	else if (Command == TEXT("replace_component_class"))
	{
		return HandleReplaceComponentClass(Params);
	}

	return MakeError(FString::Printf(TEXT("Unknown command: %s"), *Command));
}

UBlueprint* FMCPServer::LoadBlueprintFromPath(const FString& Path)
{
	FString FullPath = Path;
	if (!FullPath.StartsWith(TEXT("/")))
	{
		FullPath = TEXT("/Game/") + Path;
	}
	if (!FullPath.EndsWith(TEXT(".") + FPackageName::GetAssetPackageExtension()))
	{
		// Just load by object path
	}

	return LoadObject<UBlueprint>(nullptr, *FullPath);
}

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

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
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

FString FMCPServer::MakeResponse(bool bSuccess, const TSharedPtr<FJsonObject>& Data, const FString& Error)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), bSuccess);

	if (Data.IsValid())
	{
		Response->SetObjectField(TEXT("data"), Data);
	}

	if (!Error.IsEmpty())
	{
		Response->SetStringField(TEXT("error"), Error);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);

	return OutputString;
}

FString FMCPServer::MakeError(const FString& Error)
{
	return MakeResponse(false, nullptr, Error);
}

void FMCPServer::ReconstructLevelVisuals()
{
	// Update LevelVisuals actors to refresh material colors after blueprint operations
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	// Use the C++ class directly instead of RerunConstructionScripts
	// This avoids OnConstruction side effects and just updates materials
	for (TActorIterator<ALevelVisuals> It(World); It; ++It)
	{
		ALevelVisuals* LevelVisuals = *It;
		if (LevelVisuals)
		{
			LevelVisuals->UpdateLevelVisuals();
			UE_LOG(LogTemp, Log, TEXT("ClaudeUnrealMCP: Updated materials on %s"), *LevelVisuals->GetName());
		}
	}
}

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
		}

		// Try to find the struct by iterating over all registered UScriptStruct objects
		// This works even for C++ structs as long as they've been registered at startup
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			UScriptStruct* Struct = *It;
			if (Struct->GetName() == TypeNameToFind)
			{
				TypeObject = Struct;
				break;
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

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

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

	// Instead of removing the extension (which causes crashes),
	// we'll try multiple approaches to clear tag references
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
		Data->SetStringField(TEXT("message"), TEXT("No tag mappings found to clear."));
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
