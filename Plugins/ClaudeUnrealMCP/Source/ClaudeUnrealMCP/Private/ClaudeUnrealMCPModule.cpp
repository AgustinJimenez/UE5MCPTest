#include "ClaudeUnrealMCPModule.h"
#include "MCPServer.h"

#define LOCTEXT_NAMESPACE "FClaudeUnrealMCPModule"

void FClaudeUnrealMCPModule::StartupModule()
{
	Server = new FMCPServer();
	if (Server->Start(9877))
	{
		UE_LOG(LogTemp, Log, TEXT("ClaudeUnrealMCP: Server started on port 9877"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ClaudeUnrealMCP: Failed to start server"));
	}
}

void FClaudeUnrealMCPModule::ShutdownModule()
{
	if (Server)
	{
		Server->Stop();
		delete Server;
		Server = nullptr;
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FClaudeUnrealMCPModule, ClaudeUnrealMCP)
