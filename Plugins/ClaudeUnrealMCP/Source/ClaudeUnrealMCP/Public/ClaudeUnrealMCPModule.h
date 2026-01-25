#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMCPServer;

class FClaudeUnrealMCPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	FMCPServer* Server = nullptr;
};
