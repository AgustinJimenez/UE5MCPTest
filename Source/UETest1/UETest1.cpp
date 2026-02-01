// Fill out your copyright notice in the Description page of Project Settings.

#include "UETest1.h"
#include "Modules/ModuleManager.h"

// Include struct headers to force registration via StaticStruct()
#include "CharacterPropertiesStructs.h"

void FUETest1Module::StartupModule()
{
    // Force struct registration by calling StaticStruct() on startup
    // This ensures these C++ structs are discoverable via reflection
    UE_LOG(LogTemp, Log, TEXT("UETest1 Module: Registering C++ structs for MCP discovery..."));

    // CharacterPropertiesStructs.h - FS_ prefix structs
    FS_PlayerInputState::StaticStruct();
    FS_CharacterPropertiesForAnimation::StaticStruct();
    FS_CharacterPropertiesForCamera::StaticStruct();
    FS_CharacterPropertiesForTraversal::StaticStruct();

    UE_LOG(LogTemp, Log, TEXT("UETest1 Module: C++ struct registration complete."));
}

IMPLEMENT_PRIMARY_GAME_MODULE(FUETest1Module, UETest1, "UETest1");
