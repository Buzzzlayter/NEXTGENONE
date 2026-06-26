// Exclusive publisher - Forward Gateway, LLC
using UnrealBuildTool;

public class UIModule : ModuleRules
{
    public UIModule(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnableExceptions = true;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "InputCore",
                "EnhancedInput",
                "UMG",
                "GameplayTags",
                "GCFSM",
                "DeveloperSettings",
                "DeveloperSettings", 
                "StructUtils"
            }
        );
    }
}
