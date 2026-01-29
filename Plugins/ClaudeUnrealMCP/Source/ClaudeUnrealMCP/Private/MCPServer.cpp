#include "MCPServer.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintEditorLibrary.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#include "Engine/UserDefinedStruct.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/TimelineTemplate.h"
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
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(65536);

	while (bRunning)
	{
		uint32 PendingDataSize = 0;
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
					int32 BytesSent = 0;
					ClientSocket->Send((uint8*)Converter.Get(), Converter.Length(), BytesSent);
				}
			}
		}
		else
		{
			FPlatformProcess::Sleep(0.01f);
		}

		// Check if socket is still connected
		ESocketConnectionState State = ClientSocket->GetConnectionState();
		if (State != ESocketConnectionState::SCS_Connected)
		{
			break;
		}
	}

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
	else if (Command == TEXT("read_blueprint"))
	{
		return HandleReadBlueprint(Params);
	}
	else if (Command == TEXT("read_variables"))
	{
		return HandleReadVariables(Params);
	}
	else if (Command == TEXT("read_components"))
	{
		return HandleReadComponents(Params);
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
	else if (Command == TEXT("ping"))
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("message"), TEXT("pong"));
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
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None);

	bool bSuccess = (Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("compiled"), bSuccess);
	Data->SetStringField(TEXT("status"), bSuccess ? TEXT("Success") : TEXT("Failed"));

	if (!bSuccess)
	{
		return MakeError(TEXT("Blueprint compilation failed"));
	}

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
