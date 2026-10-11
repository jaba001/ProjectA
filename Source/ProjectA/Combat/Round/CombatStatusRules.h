#pragma once

#include "CoreMinimal.h"
#include "Combat/Round/CombatStatusTypes.h"
#include "Combat/Round/CombatStatusSettings.h"

class AUnitBase;

// Pure status bookkeeping shared by the round coordinator, checkpoints and tests.
// 라운드 조정자·체크포인트·테스트가 함께 쓰는 순수 상태 계산입니다.
namespace CombatStatusRules
{
    // Tunable values copied from project settings so tests can supply their own.
    // 테스트가 직접 지정할 수 있도록 프로젝트 설정에서 복사한 조정값입니다.
    struct PROJECTA_API FTuning
    {
        ECombatBurnDecay BurnDecay = ECombatBurnDecay::HalfFloorMinusOne;
        int32 BleedingRounds = 2;
        int32 ChillRounds = 2;
        int32 ShockRounds = 2;
        float ChillSpeedPerPoint = 1.f;
        float MinimumChilledSpeed = 1.f;

        static FTuning FromSettings();
    };

    // Damage produced by one round-end tick for a single unit, in application order.
    // 한 유닛의 라운드 종료 처리에서 적용 순서대로 발생한 피해입니다.
    struct PROJECTA_API FRoundEndDamage
    {
        FGameplayTag Status;
        int32 Amount = 0;
    };

    PROJECTA_API bool IsSupportedStatus(const FGameplayTag& Status);
    PROJECTA_API bool IsValidEntry(const FCombatStatusEntry& Entry);
    PROJECTA_API int32 GetValue(const TArray<FCombatStatusEntry>& Entries, const FGameplayTag& Status);
    PROJECTA_API bool Has(const TArray<FCombatStatusEntry>& Entries, const FGameplayTag& Status);
    PROJECTA_API void Remove(TArray<FCombatStatusEntry>& Entries, const FGameplayTag& Status);

    // Stack a value status by addition; freeze and confusion reserve the next round instead.
    // 수치형 상태는 합산하고 빙결·혼란은 다음 라운드를 예약합니다.
    PROJECTA_API void AddValueStatus(TArray<FCombatStatusEntry>& Entries, const FGameplayTag& Status, int32 Value, const FTuning& Tuning);
    PROJECTA_API void ReserveNextRound(TArray<FCombatStatusEntry>& Entries, const FGameplayTag& Status);

    PROJECTA_API int32 NextBurn(int32 Burn, ECombatBurnDecay Decay);

    // Burn then poison: damage the current value first, then decay; zero values never damage.
    // 화상 다음 중독 순서로 현재 수치만큼 먼저 피해를 주고 감소시키며 0은 피해가 없습니다.
    PROJECTA_API TArray<FRoundEndDamage> TickRoundEnd(TArray<FCombatStatusEntry>& Entries, const FTuning& Tuning);

    // Expire finished durations and activate next-round reservations at a planning boundary.
    // 계획 경계에서 끝난 지속시간을 만료하고 다음 라운드 예약을 활성화합니다.
    PROJECTA_API void AdvancePlanningBoundary(TArray<FCombatStatusEntry>& Entries);

    // Active in this planning round: a frozen or confused entry whose reservation reached zero.
    // 이번 계획 라운드에 유효한 상태는 예약이 0에 도달한 빙결 또는 혼란입니다.
    PROJECTA_API bool IsActiveThisRound(const TArray<FCombatStatusEntry>& Entries, const FGameplayTag& Status);
    PROJECTA_API bool LocksPlan(const TArray<FCombatStatusEntry>& Entries);

    PROJECTA_API float ChilledSpeed(float BaseSpeed, const TArray<FCombatStatusEntry>& Entries, const FTuning& Tuning);

    // Mirror statuses as loose tags so existing GameplayTagQuery conditions can read them.
    // 기존 GameplayTagQuery 조건이 읽을 수 있도록 상태를 Loose 태그로 반영합니다.
    PROJECTA_API void SyncOwnedTags(AUnitBase* Unit, const TArray<FCombatStatusEntry>& Entries);

    PROJECTA_API FText Summary(const TArray<FCombatStatusEntry>& Entries);
}
