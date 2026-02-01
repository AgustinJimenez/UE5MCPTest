#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Common/TcpListener.h"

class FMCPServer
{
public:
	FMCPServer();
	~FMCPServer();

	bool Start(int32 Port = 9877);
	void Stop();

private:
	bool HandleConnection(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint);
	void HandleClient(FSocket* ClientSocket);
	FString ProcessCommand(const TSharedPtr<FJsonObject>& JsonCommand);

	// Blueprint reading commands
	FString HandleListBlueprints(const TSharedPtr<FJsonObject>& Params);
	FString HandleCheckAllBlueprints(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadBlueprint(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadVariables(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadClassDefaults(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadComponents(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadEventGraph(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadEventGraphDetailed(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadFunctionGraphs(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadTimelines(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadInterface(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadUserDefinedStruct(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadUserDefinedEnum(const TSharedPtr<FJsonObject>& Params);
	FString HandleListActors(const TSharedPtr<FJsonObject>& Params);
	FString HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params);
	FString HandleGetActorMaterialInfo(const TSharedPtr<FJsonObject>& Params);
	FString HandleGetSceneSummary(const TSharedPtr<FJsonObject>& Params);

	// Blueprint writing commands
	FString HandleAddComponent(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddInputMapping(const TSharedPtr<FJsonObject>& Params);
	FString HandleReparentBlueprint(const TSharedPtr<FJsonObject>& Params);
	FString HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params);
	FString HandleSaveAsset(const TSharedPtr<FJsonObject>& Params);
	FString HandleSaveAll(const TSharedPtr<FJsonObject>& Params);
	FString HandleDeleteInterfaceFunction(const TSharedPtr<FJsonObject>& Params);
	FString HandleModifyInterfaceFunctionParameter(const TSharedPtr<FJsonObject>& Params);
	FString HandleDeleteFunctionGraph(const TSharedPtr<FJsonObject>& Params);
	FString HandleClearEventGraph(const TSharedPtr<FJsonObject>& Params);
	FString HandleRefreshNodes(const TSharedPtr<FJsonObject>& Params);
	FString HandleBreakOrphanedPins(const TSharedPtr<FJsonObject>& Params);
	FString HandleDeleteUserDefinedStruct(const TSharedPtr<FJsonObject>& Params);
	FString HandleModifyStructField(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetBlueprintCompileSettings(const TSharedPtr<FJsonObject>& Params);
	FString HandleModifyFunctionMetadata(const TSharedPtr<FJsonObject>& Params);
	FString HandleCaptureScreenshot(const TSharedPtr<FJsonObject>& Params);
	FString HandleRemoveErrorNodes(const TSharedPtr<FJsonObject>& Params);
	FString HandleClearAnimationBlueprintTags(const TSharedPtr<FJsonObject>& Params);
	FString HandleClearAnimGraph(const TSharedPtr<FJsonObject>& Params);

	// Blueprint function creation commands (Sprint 1)
	FString HandleCreateBlueprintFunction(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddFunctionInput(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddFunctionOutput(const TSharedPtr<FJsonObject>& Params);
	FString HandleRenameBlueprintFunction(const TSharedPtr<FJsonObject>& Params);

	// Level actor property commands (Sprint 2)
	FString HandleReadActorProperties(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetActorProperties(const TSharedPtr<FJsonObject>& Params);

	// Helpers
	FString MakeResponse(bool bSuccess, const TSharedPtr<FJsonObject>& Data, const FString& Error = TEXT(""));
	FString MakeError(const FString& Error);
	class UBlueprint* LoadBlueprintFromPath(const FString& Path);

	FTcpListener* Listener = nullptr;
	bool bRunning = false;
	TArray<FSocket*> ClientSockets;
	FCriticalSection ClientSocketsLock;
};
