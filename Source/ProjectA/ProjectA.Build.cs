// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectA : ModuleRules
{
	public ProjectA(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Stage the authored catalog at the same project-relative path for packaged reads.
        // 패키지에서도 같은 프로젝트 상대 경로로 읽도록 원본 카탈로그를 포함합니다.
        RuntimeDependencies.Add("$(ProjectDir)/DataCatalogs/WEAPON_ASSETS.csv", StagedFileType.UFS);

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

		PrivateDependencyModuleNames.AddRange(new string[] { "AssetRegistry", "ApplicationCore" });
        PublicDependencyModuleNames.Add("OnlineSubsystem");
        bool bSteamDevelopment = Target.Platform == UnrealTargetPlatform.Win64 && Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test;
        PublicDefinitions.Add("PROJECTA_WITH_STEAM_DEV=" + (bSteamDevelopment ? "1" : "0"));
        if (bSteamDevelopment)
        {
            PrivateDependencyModuleNames.AddRange(new string[] { "OnlineSubsystemSteam", "SteamSockets" });
        }

		PublicIncludePaths.AddRange(new string[] {
			"ProjectA",
		});


        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

    }
}
