using UnrealBuildTool;

public class CommonLoadingScreen : ModuleRules
{
	public CommonLoadingScreen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "DeveloperSettings", "UMG" });
		PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "PreLoadScreen" });
	}
}
