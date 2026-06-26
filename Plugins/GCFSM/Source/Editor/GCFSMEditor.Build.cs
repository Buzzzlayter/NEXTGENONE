// Copyright (c) 2018 Gamecentric, All rights reserved

namespace UnrealBuildTool.Rules
{
	public class GCFSMEditor : ModuleRules
	{
		public GCFSMEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

            ShadowVariableWarningLevel = WarningLevel.Off;

            PrivateIncludePaths.Add("Editor/Private");

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"UnrealEd",
					"Slate",
					"SlateCore",
					"BlueprintGraph",
					"Kismet",
					"KismetCompiler",
					"GraphEditor",
					"GCFSM",
					"Projects",
					"PropertyEditor",
					"EditorStyle",
					"InputCore",
                    "ToolMenus",
                });
		}
	}
}