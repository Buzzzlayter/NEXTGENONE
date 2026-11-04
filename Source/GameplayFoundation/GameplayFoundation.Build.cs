using UnrealBuildTool;

public class GameplayFoundation : ModuleRules
{
    public GameplayFoundation(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "GameplayAbilities", "GameplayTags", "GameplayTasks", "EnhancedInput", "NetCore"
        });
    }
}
