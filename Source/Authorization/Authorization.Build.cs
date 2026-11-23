using UnrealBuildTool;

public class Authorization : ModuleRules
{
    public Authorization(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "EngineSettings", "OnlineBase", "OnlineSubsystem" });
    }
}
