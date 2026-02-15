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

	if (Command == TEXT("ping"))
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

	using FCommandHandler = FString (FMCPServer::*)(const TSharedPtr<FJsonObject>&);
	static const TMap<FString, FCommandHandler> CommandHandlers = {
		{TEXT("list_blueprints"), &FMCPServer::HandleListBlueprints},
		{TEXT("check_all_blueprints"), &FMCPServer::HandleCheckAllBlueprints},
		{TEXT("read_blueprint"), &FMCPServer::HandleReadBlueprint},
		{TEXT("read_variables"), &FMCPServer::HandleReadVariables},
		{TEXT("read_class_defaults"), &FMCPServer::HandleReadClassDefaults},
		{TEXT("read_components"), &FMCPServer::HandleReadComponents},
		{TEXT("read_component_properties"), &FMCPServer::HandleReadComponentProperties},
		{TEXT("read_event_graph"), &FMCPServer::HandleReadEventGraph},
		{TEXT("read_event_graph_detailed"), &FMCPServer::HandleReadEventGraphDetailed},
		{TEXT("read_function_graphs"), &FMCPServer::HandleReadFunctionGraphs},
		{TEXT("read_timelines"), &FMCPServer::HandleReadTimelines},
		{TEXT("read_interface"), &FMCPServer::HandleReadInterface},
		{TEXT("read_user_defined_struct"), &FMCPServer::HandleReadUserDefinedStruct},
		{TEXT("read_user_defined_enum"), &FMCPServer::HandleReadUserDefinedEnum},
		{TEXT("list_actors"), &FMCPServer::HandleListActors},
		{TEXT("read_actor_components"), &FMCPServer::HandleReadActorComponents},
		{TEXT("read_actor_component_properties"), &FMCPServer::HandleReadActorComponentProperties},
		{TEXT("find_actors_by_name"), &FMCPServer::HandleFindActorsByName},
		{TEXT("get_actor_material_info"), &FMCPServer::HandleGetActorMaterialInfo},
		{TEXT("get_scene_summary"), &FMCPServer::HandleGetSceneSummary},
		{TEXT("add_component"), &FMCPServer::HandleAddComponent},
		{TEXT("set_component_property"), &FMCPServer::HandleSetComponentProperty},
		{TEXT("set_blueprint_cdo_class_reference"), &FMCPServer::HandleSetBlueprintCDOClassReference},
		{TEXT("replace_component_map_value"), &FMCPServer::HandleReplaceComponentMapValue},
		{TEXT("replace_blueprint_array_value"), &FMCPServer::HandleReplaceBlueprintArrayValue},
		{TEXT("add_input_mapping"), &FMCPServer::HandleAddInputMapping},
		{TEXT("reparent_blueprint"), &FMCPServer::HandleReparentBlueprint},
		{TEXT("compile_blueprint"), &FMCPServer::HandleCompileBlueprint},
		{TEXT("save_asset"), &FMCPServer::HandleSaveAsset},
		{TEXT("save_all"), &FMCPServer::HandleSaveAll},
		{TEXT("delete_interface_function"), &FMCPServer::HandleDeleteInterfaceFunction},
		{TEXT("modify_interface_function_parameter"), &FMCPServer::HandleModifyInterfaceFunctionParameter},
		{TEXT("delete_function_graph"), &FMCPServer::HandleDeleteFunctionGraph},
		{TEXT("clear_event_graph"), &FMCPServer::HandleClearEventGraph},
		{TEXT("empty_graph"), &FMCPServer::HandleClearEventGraph},
		{TEXT("refresh_nodes"), &FMCPServer::HandleRefreshNodes},
		{TEXT("break_orphaned_pins"), &FMCPServer::HandleBreakOrphanedPins},
		{TEXT("delete_user_defined_struct"), &FMCPServer::HandleDeleteUserDefinedStruct},
		{TEXT("modify_struct_field"), &FMCPServer::HandleModifyStructField},
		{TEXT("set_blueprint_compile_settings"), &FMCPServer::HandleSetBlueprintCompileSettings},
		{TEXT("modify_function_metadata"), &FMCPServer::HandleModifyFunctionMetadata},
		{TEXT("capture_screenshot"), &FMCPServer::HandleCaptureScreenshot},
		{TEXT("remove_error_nodes"), &FMCPServer::HandleRemoveErrorNodes},
		{TEXT("clear_animation_blueprint_tags"), &FMCPServer::HandleClearAnimationBlueprintTags},
		{TEXT("clear_anim_graph"), &FMCPServer::HandleClearAnimGraph},
		{TEXT("create_blueprint_function"), &FMCPServer::HandleCreateBlueprintFunction},
		{TEXT("add_function_input"), &FMCPServer::HandleAddFunctionInput},
		{TEXT("add_function_output"), &FMCPServer::HandleAddFunctionOutput},
		{TEXT("rename_blueprint_function"), &FMCPServer::HandleRenameBlueprintFunction},
		{TEXT("read_actor_properties"), &FMCPServer::HandleReadActorProperties},
		{TEXT("set_actor_properties"), &FMCPServer::HandleSetActorProperties},
		{TEXT("set_actor_component_property"), &FMCPServer::HandleSetActorComponentProperty},
		{TEXT("reconstruct_actor"), &FMCPServer::HandleReconstructActor},
		{TEXT("clear_component_map_value_array"), &FMCPServer::HandleClearComponentMapValueArray},
		{TEXT("replace_component_class"), &FMCPServer::HandleReplaceComponentClass},
		{TEXT("delete_component"), &FMCPServer::HandleDeleteComponent},
		{TEXT("set_blueprint_cdo_property"), &FMCPServer::HandleSetBlueprintCDOProperty},
		{TEXT("remove_implemented_interface"), &FMCPServer::HandleRemoveImplementedInterface},
		{TEXT("add_implemented_interface"), &FMCPServer::HandleAddImplementedInterface},
		{TEXT("migrate_interface_references"), &FMCPServer::HandleMigrateInterfaceReferences},
		{TEXT("connect_nodes"), &FMCPServer::HandleConnectNodes},
		{TEXT("disconnect_pin"), &FMCPServer::HandleDisconnectPin},
		{TEXT("add_set_struct_node"), &FMCPServer::HandleAddSetStructNode},
		{TEXT("delete_node"), &FMCPServer::HandleDeleteNode},
		{TEXT("read_input_mapping_context"), &FMCPServer::HandleReadInputMappingContext},
		{TEXT("migrate_struct_references"), &FMCPServer::HandleMigrateStructReferences},
		{TEXT("migrate_enum_references"), &FMCPServer::HandleMigrateEnumReferences},
		{TEXT("fix_property_access_paths"), &FMCPServer::HandleFixPropertyAccessPaths},
		{TEXT("fix_struct_sub_pins"), &FMCPServer::HandleFixStructSubPins},
		{TEXT("rename_local_variable"), &FMCPServer::HandleRenameLocalVariable},
		{TEXT("fix_pin_enum_type"), &FMCPServer::HandleFixPinEnumType},
		{TEXT("fix_enum_defaults"), &FMCPServer::HandleFixEnumDefaults},
		{TEXT("fix_asset_struct_reference"), &FMCPServer::HandleFixAssetStructReference},
		{TEXT("reconstruct_node"), &FMCPServer::HandleReconstructNode},
		{TEXT("set_pin_default"), &FMCPServer::HandleSetPinDefault}
	};

	if (const FCommandHandler* Handler = CommandHandlers.Find(Command))
	{
		return (this->**Handler)(Params);
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

