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
