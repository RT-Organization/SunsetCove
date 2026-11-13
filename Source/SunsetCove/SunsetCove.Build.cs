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
			"PhysicsCore",      // physical materials / surfaces (footsteps choose the sound by the ground)
			"CoreFramework"
		});

		// AssetRegistry: only the editor helper "Populate From Naming Convention" of the arms animation data asset uses it.
		PrivateDependencyModuleNames.AddRange(new string[] { "AssetRegistry" });

		// UI System / HUD: Slate drawing of the crosshair and the tool wheel, the CommonUI parents of the CoreFramework widgets,
		// and the Project Settings page (UDeveloperSettings).
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "CommonUI", "DeveloperSettings" });

		// Settings menu: the controller icon family (Xbox / PlayStation) is set on CommonUI's input subsystem.
		PrivateDependencyModuleNames.AddRange(new string[] { "CommonInput" });

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