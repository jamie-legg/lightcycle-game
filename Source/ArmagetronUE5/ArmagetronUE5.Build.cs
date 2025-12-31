// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ArmagetronUE5 : ModuleRules
{
	public ArmagetronUE5(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// Add the module's root directory to include paths
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"Niagara",
			"ProceduralMeshComponent",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");
	}
}

