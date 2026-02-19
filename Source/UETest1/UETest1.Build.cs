// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class UETest1 : ModuleRules
{
	public UETest1(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "PoseSearch", "Mover", "MotionWarping", "GameplayTags", "StateTreeModule", "GameplayStateTreeModule", "AIModule", "SmartObjectsModule", "NavigationSystem", "GameplayTasks", "GameplayInteractionsModule", "Landscape", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
