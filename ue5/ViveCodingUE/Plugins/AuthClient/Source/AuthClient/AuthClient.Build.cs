using UnrealBuildTool;

public class AuthClient : ModuleRules
{
	public AuthClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTP",
			"Json",
			"JsonUtilities",
			"UMG",
			"Slate",
			"SlateCore"
		});
	}
}
