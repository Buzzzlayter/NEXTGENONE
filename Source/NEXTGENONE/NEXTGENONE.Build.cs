// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NEXTGENONE : ModuleRules
{
	public NEXTGENONE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		// Chaos-разрушение (Geometry Collections + поля): пивот на Chaos для сносимых конструкций.
		PublicDependencyModuleNames.AddRange(new string[] {
			"GeometryCollectionEngine",   // UGeometryCollectionComponent, AGeometryCollectionActor
			"FieldSystemEngine",          // поля: RadialFalloff (strain) + RadialVector (kick) → взрыв
			"Chaos",
			"ChaosSolverEngine",
			"PhysicsCore",
			"UIModule",
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "NgxVoxel" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
