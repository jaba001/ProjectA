#include "Misc/AutomationTest.h"
#include "AbilitySystemGlobals.h"
#include "GameplayAbilitiesDeveloperSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayCueEffectivePathsTest, "ProjectA.Configuration.GameplayCueEffectivePaths", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameplayCueEffectivePathsTest::RunTest(const FString& Parameters)
{
    const TArray<FString> DeveloperPaths = GetDefault<UGameplayAbilitiesDeveloperSettings>()->GameplayCueNotifyPaths;
    const TArray<FString> EffectivePaths = UAbilitySystemGlobals::Get().GetGameplayCueNotifyPaths();
    TestTrue(TEXT("The initialized ability system searches project-owned cue content."), EffectivePaths.Contains(TEXT("/Game/User_JeHoon")));
    TestFalse(TEXT("Cue initialization does not fall back to the entire game content tree."), EffectivePaths.Contains(TEXT("/Game")) || EffectivePaths.Contains(TEXT("/Game/")));
    for (const FString& DeveloperPath : DeveloperPaths)
    {
        TestTrue(FString::Printf(TEXT("Additional developer settings cue paths remain active: %s"), *DeveloperPath), EffectivePaths.Contains(DeveloperPath));
    }
    return true;
}

#endif
