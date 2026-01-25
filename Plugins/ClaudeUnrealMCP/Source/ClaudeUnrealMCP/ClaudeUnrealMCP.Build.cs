using UnrealBuildTool;

public class ClaudeUnrealMCP : ModuleRules
{
	public ClaudeUnrealMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Sockets",
			"Networking"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"BlueprintGraph",
			"Kismet",
			"AssetRegistry",
			"Json",
			"JsonUtilities",
			"EnhancedInput",
			"InputCore"
		});
	}
}
