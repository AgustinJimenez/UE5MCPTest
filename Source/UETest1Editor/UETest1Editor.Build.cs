using UnrealBuildTool;

public class UETest1Editor : ModuleRules
{
	public UETest1Editor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"AnimationModifierLibrary",
			"AnimationModifiers",
			"AnimationBlueprintLibrary"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
