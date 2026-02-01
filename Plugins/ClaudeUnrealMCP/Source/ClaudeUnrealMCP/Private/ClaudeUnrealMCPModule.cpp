#include "ClaudeUnrealMCPModule.h"
#include "MCPServer.h"
#include "Editor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Framework/Application/SlateApplication.h"

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

	// Defer registration until editor is fully initialized
	FCoreDelegates::OnPostEngineInit.AddLambda([this]()
	{
		if (GEditor)
		{
			OnBlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &FClaudeUnrealMCPModule::OnBlueprintCompiled);
			UE_LOG(LogTemp, Log, TEXT("ClaudeUnrealMCP: Registered blueprint compile callback"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ClaudeUnrealMCP: GEditor not available, blueprint callback not registered"));
		}
	});
}

void FClaudeUnrealMCPModule::ShutdownModule()
{
	// Unregister blueprint compilation callback
	if (GEditor && OnBlueprintCompiledHandle.IsValid())
	{
		GEditor->OnBlueprintCompiled().Remove(OnBlueprintCompiledHandle);
	}

	if (Server)
	{
		Server->Stop();
		delete Server;
		Server = nullptr;
	}
}

void FClaudeUnrealMCPModule::OnBlueprintCompiled()
{
	// Auto-reconstruct LevelVisuals to refresh material colors after any blueprint compile
	if (Server)
	{
		Server->ReconstructLevelVisuals();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FClaudeUnrealMCPModule, ClaudeUnrealMCP)
