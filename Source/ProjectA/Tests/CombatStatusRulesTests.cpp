#include "Misc/AutomationTest.h"
#include "Combat/Round/CombatStatusRules.h"
#include "GAS/CombatGameplayTags.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatStatusRulesTest, "ProjectA.Combat.StatusRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatStatusRulesTest::RunTest(const FString& Parameters)
{
    using namespace ProjectACombatTags;
    CombatStatusRules::FTuning Tuning;

    // SE-01·SE-02: damage the current burn first, then decay with either formula.
    // SE-01·SE-02: 현재 화상만큼 먼저 피해를 주고 각 공식으로 감소합니다.
    const auto BurnSequence = [&Tuning](ECombatBurnDecay Decay)
    {
        CombatStatusRules::FTuning Local = Tuning;
        Local.BurnDecay = Decay;
        TArray<FCombatStatusEntry> Entries;
        CombatStatusRules::AddValueStatus(Entries, Status_Burning, 9, Local);
        TArray<int32> Damage;
        for (int32 Round = 0; Round < 10 && CombatStatusRules::Has(Entries, Status_Burning); ++Round)
        {
            for (const CombatStatusRules::FRoundEndDamage& Tick : CombatStatusRules::TickRoundEnd(Entries, Local)) Damage.Add(Tick.Amount);
        }
        return Damage;
    };
    TestTrue(TEXT("Burn A deals 9, 4, 2, 1."), BurnSequence(ECombatBurnDecay::HalfFloor) == TArray<int32>{9, 4, 2, 1});
    TestTrue(TEXT("Burn B deals 9, 3."), BurnSequence(ECombatBurnDecay::HalfFloorMinusOne) == TArray<int32>{9, 3});
    TestEqual(TEXT("Burn 1 still deals its damage before removal."), CombatStatusRules::NextBurn(1, ECombatBurnDecay::HalfFloorMinusOne), 0);

    // SE-04: poison 3 deals 3, 2, 1 and is removed.
    // SE-04: 중독 3은 3, 2, 1 피해 후 제거됩니다.
    TArray<FCombatStatusEntry> Poison;
    CombatStatusRules::AddValueStatus(Poison, Status_Poisoned, 3, Tuning);
    TArray<int32> PoisonDamage;
    for (int32 Round = 0; Round < 5; ++Round)
    {
        for (const CombatStatusRules::FRoundEndDamage& Tick : CombatStatusRules::TickRoundEnd(Poison, Tuning)) PoisonDamage.Add(Tick.Amount);
    }
    TestTrue(TEXT("Poison deals 3, 2, 1."), PoisonDamage == TArray<int32>{3, 2, 1});
    TestTrue(TEXT("Poison is removed at zero."), Poison.IsEmpty());

    // Reapplying a value status adds the value; duration statuses refresh their rounds.
    // 수치형 상태 재부여는 합산하며 지속형 상태는 라운드를 갱신합니다.
    TArray<FCombatStatusEntry> Stacked;
    CombatStatusRules::AddValueStatus(Stacked, Status_Burning, 2, Tuning);
    CombatStatusRules::AddValueStatus(Stacked, Status_Burning, 3, Tuning);
    TestEqual(TEXT("Burn stacks by addition."), CombatStatusRules::GetValue(Stacked, Status_Burning), 5);
    CombatStatusRules::AddValueStatus(Stacked, Status_Poisoned, 0, Tuning);
    TestFalse(TEXT("Zero or negative values are ignored."), CombatStatusRules::Has(Stacked, Status_Poisoned));

    // Chill lowers speed for two planning boundaries, then expires; it never raises a slower unit.
    // 냉각은 두 계획 경계 동안 속도를 낮춘 뒤 만료되며 더 느린 유닛을 올리지 않습니다.
    TArray<FCombatStatusEntry> Chill;
    CombatStatusRules::AddValueStatus(Chill, Status_Chilled, 3, Tuning);
    CombatStatusRules::AdvancePlanningBoundary(Chill);
    TestEqual(TEXT("Chill 3 reduces speed 10 to 7."), CombatStatusRules::ChilledSpeed(10.f, Chill, Tuning), 7.f);
    TestEqual(TEXT("Chill keeps the minimum speed."), CombatStatusRules::ChilledSpeed(2.f, Chill, Tuning), 1.f);
    TestEqual(TEXT("Chill never raises a slower unit."), CombatStatusRules::ChilledSpeed(0.5f, Chill, Tuning), 0.5f);
    CombatStatusRules::AdvancePlanningBoundary(Chill);
    TestTrue(TEXT("Chill is still active on the second boundary."), CombatStatusRules::Has(Chill, Status_Chilled));
    CombatStatusRules::AdvancePlanningBoundary(Chill);
    TestFalse(TEXT("Chill expires on the third boundary."), CombatStatusRules::Has(Chill, Status_Chilled));

    // Freeze and confusion reserve exactly the next round and lock only that planning round.
    // 빙결과 혼란은 다음 한 라운드만 예약하고 그 계획 라운드만 잠급니다.
    TArray<FCombatStatusEntry> Control;
    CombatStatusRules::ReserveNextRound(Control, Status_Frozen);
    CombatStatusRules::ReserveNextRound(Control, Status_Frozen);
    TestFalse(TEXT("A reservation does not lock the current round."), CombatStatusRules::LocksPlan(Control));
    CombatStatusRules::AdvancePlanningBoundary(Control);
    TestTrue(TEXT("The reserved round is locked."), CombatStatusRules::IsActiveThisRound(Control, Status_Frozen));
    CombatStatusRules::AdvancePlanningBoundary(Control);
    TestFalse(TEXT("Repeated freeze does not extend to a second round."), CombatStatusRules::LocksPlan(Control));

    FCombatStatusEntry Stun;
    Stun.Status = Status_Stunned;
    Stun.Value = 1;
    TestFalse(TEXT("Stun never persists as an entry."), CombatStatusRules::IsValidEntry(Stun));
    TestTrue(TEXT("Every harmful status tag is supported."), CombatStatusRules::IsSupportedStatus(Status_Shocked) && CombatStatusRules::IsSupportedStatus(Status_Bleeding));
    return true;
}

#endif
