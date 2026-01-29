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
	FString HandleReadBlueprint(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadVariables(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadComponents(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadEventGraph(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadEventGraphDetailed(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadFunctionGraphs(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadTimelines(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadInterface(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadUserDefinedStruct(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadUserDefinedEnum(const TSharedPtr<FJsonObject>& Params);
	FString HandleListActors(const TSharedPtr<FJsonObject>& Params);

	// Blueprint writing commands
	FString HandleAddComponent(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddInputMapping(const TSharedPtr<FJsonObject>& Params);
	FString HandleReparentBlueprint(const TSharedPtr<FJsonObject>& Params);
	FString HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params);
	FString HandleSaveAsset(const TSharedPtr<FJsonObject>& Params);

	// Helpers
	FString MakeResponse(bool bSuccess, const TSharedPtr<FJsonObject>& Data, const FString& Error = TEXT(""));
	FString MakeError(const FString& Error);
	class UBlueprint* LoadBlueprintFromPath(const FString& Path);

	FTcpListener* Listener = nullptr;
	bool bRunning = false;
	TArray<FSocket*> ClientSockets;
	FCriticalSection ClientSocketsLock;
};
