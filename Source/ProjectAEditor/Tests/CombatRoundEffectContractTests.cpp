#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CombatRoundEffectTestTypes.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "GAS/Effect/GE_Damage.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundEffectDurationContractTest, "ProjectA.Combat.Round.CheckpointEffectDurationContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundEffectDurationContractTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<USkillDefinitionDataAsset> Definition(NewObject<USkillDefinitionDataAsset>());
    Definition->bUseRoundDefinition = true;
    Definition->RoundDefinition.SkillId = TEXT("DurationContract");
    FCombatRoundSkill Resolved;
    FText Error;
    TestTrue(TEXT("Empty effect preserves the existing Instant damage fallback"), Definition->ResolveRoundSkill(Resolved, Error));
    Definition->RoundDefinition.EffectClass = UGE_Damage::StaticClass();
    TestTrue(TEXT("An authored Instant effect remains valid"), Definition->ResolveRoundSkill(Resolved, Error));
    TestEqual(TEXT("Instant effect keeps its authored class"), Resolved.EffectClass.Get(), UGE_Damage::StaticClass());
    const TArray<TSubclassOf<UGameplayEffect>> Unsupported = {UCombatRoundDurationTestEffect::StaticClass(), UCombatRoundInfiniteTestEffect::StaticClass()};
    for (const TSubclassOf<UGameplayEffect>& EffectClass : Unsupported)
    {
        Definition->RoundDefinition.EffectClass = EffectClass;
        TestFalse(TEXT("Runtime profile rejects effects whose lifetime is not saved"), CombatRoundRules::IsValidSkill(Definition->RoundDefinition));
        TestFalse(TEXT("Asset resolution rejects the same unsupported lifetime"), Definition->ResolveRoundSkill(Resolved, Error));
        TestTrue(TEXT("The failure identifies the unsupported duration"), Error.ToString().Contains(TEXT("지원하지 않는 효과 지속시간")));
        TestTrue(TEXT("Rejected duration effects are never silently replaced with damage"), Resolved.SkillId.IsNone() && !Resolved.EffectClass);
        TestEqual(TEXT("Rejected authored effect stays unchanged"), Definition->RoundDefinition.EffectClass.Get(), EffectClass.Get());
    }
    return true;
}

#endif
