// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.IO;

public class UETest1 : ModuleRules
{
	public UETest1(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayTags", "AIModule", "Mover", "MotionWarping", "PoseSearch", "StateTreeModule", "GameplayStateTreeModule", "GameplayInteractionsModule", "SmartObjectsModule", "GameplayTasks", "NavigationSystem", "DrawDebugLibrary", "Landscape", "AnimGraphRuntime" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Plugins/Runtime/GameplayInteractions/Source/GameplayInteractionsModule/Private"));
		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Plugins/Runtime/GameplayInteractions/Source/GameplayInteractionsModule/Private/AI"));

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
