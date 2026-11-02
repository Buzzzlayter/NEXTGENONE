using UnrealBuildTool;

public class NEXTGENONEServerTarget : TargetRules
{
    public NEXTGENONEServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("NEXTGENONE");
        DisablePlugins.AddRange(new[] { "ModelContextProtocol", "Terminal", "EditorToolset", "ModelingToolsEditorMode" });
    }
}
