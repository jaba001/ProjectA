#include "Misc/AutomationTest.h"
#include "AbilitySystemGlobals.h"
#include "GameplayAbilitiesDeveloperSettings.h"
#include "Misc/ConfigCacheIni.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayCueEffectivePathsTest, "ProjectA.Configuration.GameplayCueEffectivePaths", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameplayCueEffectivePathsTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("The engine config cache is available."), GConfig)) return false;
    const UGameplayAbilitiesDeveloperSettings* Settings = GetDefault<UGameplayAbilitiesDeveloperSettings>();
    const FString Section = Settings->GetClass()->GetPathName();
    TArray<FString> ConfiguredPaths;
    GConfig->GetArray(*Section, TEXT("GameplayCueNotifyPaths"), ConfiguredPaths, GGameIni);
    const TArray<FString> DeveloperPaths = Settings->GameplayCueNotifyPaths;

    // Observe each source before querying Globals without reloading or changing the settings object.
    // 설정 객체를 다시 읽거나 변경하지 않고 Globals 조회 전에 각 입력을 관측합니다.
    AddInfo(FString::Printf(TEXT("GameplayCue Game config: File=%s Section=%s Paths=[%s]"), *GGameIni, *Section, *FString::Join(ConfiguredPaths, TEXT(", "))));
    AddInfo(FString::Printf(TEXT("GameplayCue DeveloperSettings CDO: Config=%s Paths=[%s]"), *Settings->GetClass()->ClassConfigName.ToString(), *FString::Join(DeveloperPaths, TEXT(", "))));
    TestTrue(TEXT("The merged Game config includes project-owned cue content."), ConfiguredPaths.Contains(TEXT("/Game/User_JeHoon")));
    TestTrue(TEXT("DeveloperSettings CDO cue paths match the merged Game config."), DeveloperPaths == ConfiguredPaths);

    const TArray<FString> EffectivePaths = UAbilitySystemGlobals::Get().GetGameplayCueNotifyPaths();
    AddInfo(FString::Printf(TEXT("GameplayCue effective Globals paths: [%s]"), *FString::Join(EffectivePaths, TEXT(", "))));
    TestTrue(TEXT("Effective cue paths include project-owned content."), EffectivePaths.Contains(TEXT("/Game/User_JeHoon")));
    TestFalse(TEXT("Cue initialization does not fall back to the entire game content tree."), EffectivePaths.Contains(TEXT("/Game")) || EffectivePaths.Contains(TEXT("/Game/")));
    for (const FString& DeveloperPath : DeveloperPaths)
    {
        TestTrue(FString::Printf(TEXT("Additional developer settings cue paths remain active: %s"), *DeveloperPath), EffectivePaths.Contains(DeveloperPath));
    }
    return true;
}

#endif
