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
			"AnimGraph",
			"AnimGraphRuntime",
			"AssetRegistry",
			"AssetTools",
			"BlueprintGraph",
			"CommonUI",
			"ControlRig",
			"ControlRigDeveloper",
			"GameplayAbilities",
			"GameplayTags",
			"InputCore",
			"IKRig",
			"IKRigEditor",
			"NavigationSystem",
			"NetCore",
			"Json",
			"JsonUtilities",
			"Kismet",
			"KismetCompiler",
			"MeshDescription",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UMG",
			"UMGEditor",
			"UnrealEd"
		});
	}
}
