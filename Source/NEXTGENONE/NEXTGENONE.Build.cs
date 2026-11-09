// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NEXTGENONE : ModuleRules
{
	public NEXTGENONE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Project headers use module-relative includes (Gameplay/...).
		PublicIncludePaths.Add(ModuleDirectory);
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "AIModule", "UMG", "InputCore", "EnhancedInput", "GameplayAbilities", "GameplayTags", "GameplayTasks", "NetCore", "ALSV4_CPP", "GameplayFoundation", "UIModule" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "Kismet", "BlueprintGraph", "AssetTools", "AssetRegistry", "UMGEditor" });
		}

		PrivateDependencyModuleNames.AddRange(new string[] { "GCFSM", "Slate", "SlateCore", "PhysicsCore", "Niagara" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
