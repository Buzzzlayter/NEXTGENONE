// Copyright. NgxVoxel — reusable runtime voxel tech module.

using UnrealBuildTool;

public class NgxVoxel : ModuleRules
{
	public NgxVoxel(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
