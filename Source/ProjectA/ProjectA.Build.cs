// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectA : ModuleRules
{
	public ProjectA(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Stage the authored catalog at the same project-relative path for packaged reads.
        // 패키지에서도 같은 프로젝트 상대 경로로 읽도록 원본 카탈로그를 포함합니다.
        RuntimeDependencies.Add("$(ProjectDir)/Docs/WEAPON_ASSETS.csv", StagedFileType.UFS);

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
		    "CommonUI",
			"CommonInput",
            "Slate",
            "SlateCore",
            "Paper2D",
            "GameplayAbilities",
		    "GameplayTags",
			"GameplayTasks"
        });

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ProjectA",
		});


        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
