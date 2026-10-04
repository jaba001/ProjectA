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
			"AudioMixer",
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
			"Niagara",
			"NiagaraEditor",
			"NetCore",
			"Json",
			"JsonUtilities",
			"Kismet",
			"KismetCompiler",
			"MeshDescription",
			"SkeletalMeshDescription",
			"StaticMeshDescription",
			"PhysicsUtilities",
			"RenderCore",
			"SlateNullRenderer",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UMG",
			"UMGEditor",
			"UnrealEd"
		});
	}
}
