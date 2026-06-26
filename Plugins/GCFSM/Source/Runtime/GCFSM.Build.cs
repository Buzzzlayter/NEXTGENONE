// Copyright (c) 2018 Gamecentric, All rights reserved

namespace UnrealBuildTool.Rules
{
	public class GCFSM : ModuleRules
	{
		public GCFSM(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

            ShadowVariableWarningLevel = WarningLevel.Off;

            PrivateIncludePaths.Add("Runtime/Private");

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"AIModule",
					"GameplayTasks",
					"UMG"
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
				});
		}
	}
}