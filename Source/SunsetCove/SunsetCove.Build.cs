// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO; // Required for Path.Combine

public class SunsetCove : ModuleRules
{
	public SunsetCove(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] 
		{ 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput", 
			"GameplayTags",
			"Niagara", 
			"UMG", 
			"NavigationSystem", 
			"AIModule", 
			"GameplayTasks",
			"CoreFramework" 
		});

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Expose custom subfolders so headers can be found cleanly
		PublicIncludePaths.AddRange(new string[] 
		{
			Path.Combine(ModuleDirectory, "InteractionSystem"),
			Path.Combine(ModuleDirectory, "InventorySystem"),
			Path.Combine(ModuleDirectory, "Gameplay"),
			Path.Combine(ModuleDirectory, "Core")
		});
	}
}