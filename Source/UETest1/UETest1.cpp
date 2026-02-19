// Fill out your copyright notice in the Description page of Project Settings.

#include "UETest1.h"
#include "Modules/ModuleManager.h"

#include "CharacterPropertiesStructs.h"
#include "TraversalTypes.h"

void FUETest1Module::StartupModule()
{
    // Force struct registration for MCP discovery
    FS_PlayerInputState::StaticStruct();
    FS_CharacterPropertiesForAnimation::StaticStruct();
    FS_CharacterPropertiesForCamera::StaticStruct();
    FS_CharacterPropertiesForTraversal::StaticStruct();
    FS_TraversalCheckResult::StaticStruct();
    FS_TraversalChooserInputs::StaticStruct();
    FS_TraversalChooserOutputs::StaticStruct();
}

IMPLEMENT_PRIMARY_GAME_MODULE(FUETest1Module, UETest1, "UETest1");
