// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectAEditor : ModuleRules
{
	public ProjectAEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"ProjectA"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AssetRegistry",
			"AssetTools",
			"BlueprintGraph",
			"CommonUI",
			"Json",
			"JsonUtilities",
			"Kismet",
			"KismetCompiler",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UMG",
			"UMGEditor",
			"UnrealEd"
		});
	}
}
