// Exclusive publisher - Forward Gateway, LLC
using UnrealBuildTool;

public class UIModule : ModuleRules
{
    public UIModule(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnableExceptions = true;

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "PropertyEditor" });
        }

        PrivateIncludePaths.AddRange(
            new string[]
            {
                ModuleDirectory
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "GameplayTags",
				"InputCore",
				"EnhancedInput",
                "GCFSM",
                "UMG",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "DeveloperSettings",
                "DeveloperSettings",
            }
        );
    }
}
