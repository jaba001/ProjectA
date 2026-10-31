#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Game/Run/RunSkillBalance.h"
#include "GAS/CombatGameplayTags.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunSkillStatusEffectsTest, "ProjectA.Run.SkillStatusEffects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunSkillStatusEffectsTest::RunTest(const FString& Parameters)
{
    TMap<FString, TArray<FCombatStatusApplication>> Statuses;
    FText Error;
    const FString Header = TEXT("\"스킬 ID\",\"스킬 이름\",\"상태 태그\",\"수치\",\"비고\"\n");
    TestTrue(TEXT("Rows with supported tags parse."), RunSkillBalance::ParseStatusEffects(Header + TEXT("\"SkillDefinitionDataAsset:DA_A\",\"A\",\"Status.Burning\",\"3\",\"검사\"\n\"SkillDefinitionDataAsset:DA_A\",\"A\",\"Status.Frozen\",\"1\",\"검사\"\n"), Statuses, Error));
    const TArray<FCombatStatusApplication>* Parsed = Statuses.Find(TEXT("SkillDefinitionDataAsset:DA_A"));
    TestTrue(TEXT("Both statuses belong to the skill."), Parsed && Parsed->Num() == 2 && (*Parsed)[0].Status == ProjectACombatTags::Status_Burning && (*Parsed)[0].Value == 3);
    TestFalse(TEXT("Unknown tags are rejected."), RunSkillBalance::ParseStatusEffects(Header + TEXT("\"SkillDefinitionDataAsset:DA_A\",\"A\",\"Status.Unknown\",\"1\",\"검사\"\n"), Statuses, Error));
    TestFalse(TEXT("Zero values are rejected."), RunSkillBalance::ParseStatusEffects(Header + TEXT("\"SkillDefinitionDataAsset:DA_A\",\"A\",\"Status.Burning\",\"0\",\"검사\"\n"), Statuses, Error));
    TestFalse(TEXT("Duplicate statuses on one skill are rejected."), RunSkillBalance::ParseStatusEffects(Header + TEXT("\"SkillDefinitionDataAsset:DA_A\",\"A\",\"Status.Burning\",\"1\",\"검사\"\n\"SkillDefinitionDataAsset:DA_A\",\"A\",\"Status.Burning\",\"2\",\"검사\"\n"), Statuses, Error));

    // The production table must parse; balance loading separately rejects rows for unknown skills.
    // 실제 표는 읽혀야 하며 알 수 없는 스킬 행은 수치 로드에서 따로 거절됩니다.
    TestTrue(TEXT("The production status table parses."), RunSkillBalance::LoadStatusEffects(Statuses, Error) && !Statuses.IsEmpty());
    return true;
}

#endif
