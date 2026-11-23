// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NEXTGENONE : ModuleRules
{
	public NEXTGENONE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Project headers use module-relative includes (Gameplay/...).
		PublicIncludePaths.Add(ModuleDirectory);
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "AIModule", "GCFSM", "UMG", "InputCore", "EnhancedInput", "GameplayAbilities", "GameplayTags", "GameplayTasks", "NetCore", "ALSV4_CPP", "GameplayFoundation", "UIModule", "GameSettings" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "Kismet", "BlueprintGraph", "AssetTools", "AssetRegistry", "UMGEditor" });
		}

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "PhysicsCore", "Niagara", "NavigationSystem", "GeometryCollectionEngine", "FieldSystemEngine", "Chaos", "CommonLoadingScreen" });

		
	}
}
