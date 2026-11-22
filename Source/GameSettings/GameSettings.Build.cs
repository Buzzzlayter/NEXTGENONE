using UnrealBuildTool;

public class GameSettings : ModuleRules
{
	public GameSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GameplayTags", "InputCore", "UMG", "DeveloperSettings", "AudioMixer", "PropertyPath" });
		PrivateDependencyModuleNames.AddRange(new[] { "ApplicationCore", "AudioModulation", "EnhancedInput", "Slate", "SlateCore" });
	}
}
