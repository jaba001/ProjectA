#include "Combat/Round/CombatStatusRules.h"
#include "AbilitySystemComponent.h"
#include "GAS/CombatGameplayTags.h"
#include "Unit/UnitBase.h"

namespace
{
    using namespace ProjectACombatTags;

    const TArray<FGameplayTag>& SupportedStatuses()
    {
        static const TArray<FGameplayTag> Tags = {Status_Burning, Status_Poisoned, Status_Bleeding, Status_Chilled, Status_Frozen, Status_Stunned, Status_Confused, Status_Shocked};
        return Tags;
    }

    bool UsesDuration(const FGameplayTag& Status)
    {
        return Status == Status_Bleeding || Status == Status_Chilled || Status == Status_Shocked || Status == Status_Frozen || Status == Status_Confused;
    }

    FCombatStatusEntry* Find(TArray<FCombatStatusEntry>& Entries, const FGameplayTag& Status)
    {
        return Entries.FindByPredicate([&Status](const FCombatStatusEntry& Entry) { return Entry.Status == Status; });
    }

    const FCombatStatusEntry* Find(const TArray<FCombatStatusEntry>& Entries, const FGameplayTag& Status)
    {
        return Entries.FindByPredicate([&Status](const FCombatStatusEntry& Entry) { return Entry.Status == Status; });
    }

    int32 SaturatingAdd(int32 A, int32 B)
    {
        return static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(A) + B, 0, MAX_int32));
    }
}

CombatStatusRules::FTuning CombatStatusRules::FTuning::FromSettings()
{
    FTuning Tuning;
    if (const UProjectAStatusEffectSettings* Settings = GetDefault<UProjectAStatusEffectSettings>())
    {
        Tuning.BurnDecay = Settings->BurnDecay;
        Tuning.BleedingRounds = FMath::Max(1, Settings->BleedingRounds);
        Tuning.ChillRounds = FMath::Max(1, Settings->ChillRounds);
        Tuning.ShockRounds = FMath::Max(1, Settings->ShockRounds);
        Tuning.ChillSpeedPerPoint = FMath::Max(0.f, Settings->ChillSpeedPerPoint);
        Tuning.MinimumChilledSpeed = FMath::Max(0.1f, Settings->MinimumChilledSpeed);
    }
    return Tuning;
}

bool CombatStatusRules::IsSupportedStatus(const FGameplayTag& Status)
{
    return SupportedStatuses().Contains(Status);
}

bool CombatStatusRules::IsValidEntry(const FCombatStatusEntry& Entry)
{
    // Stun is resolved instantly and never persists; other entries need non-negative bounded values.
    // 스턴은 즉시 처리되어 저장되지 않으며 나머지 항목은 음수가 아닌 제한된 값을 가져야 합니다.
    return IsSupportedStatus(Entry.Status) && Entry.Status != Status_Stunned && Entry.Value >= 0 && Entry.RemainingRounds >= 0 && Entry.RemainingRounds <= 100;
}

int32 CombatStatusRules::GetValue(const TArray<FCombatStatusEntry>& Entries, const FGameplayTag& Status)
{
    const FCombatStatusEntry* Entry = Find(Entries, Status);
    return Entry ? Entry->Value : 0;
}

bool CombatStatusRules::Has(const TArray<FCombatStatusEntry>& Entries, const FGameplayTag& Status)
{
    return Find(Entries, Status) != nullptr;
}

void CombatStatusRules::Remove(TArray<FCombatStatusEntry>& Entries, const FGameplayTag& Status)
{
    Entries.RemoveAll([&Status](const FCombatStatusEntry& Entry) { return Entry.Status == Status; });
}

void CombatStatusRules::AddValueStatus(TArray<FCombatStatusEntry>& Entries, const FGameplayTag& Status, int32 Value, const FTuning& Tuning)
{
    if (Value <= 0 || !IsSupportedStatus(Status) || Status == Status_Stunned || Status == Status_Frozen || Status == Status_Confused) return;
    FCombatStatusEntry* Entry = Find(Entries, Status);
    if (!Entry)
    {
        Entry = &Entries.AddDefaulted_GetRef();
        Entry->Status = Status;
    }
    Entry->Value = SaturatingAdd(Entry->Value, Value);
    // Reapplying a duration status adds its value and refreshes the duration.
    // 지속형 상태를 다시 부여하면 수치를 더하고 지속시간을 갱신합니다.
    if (Status == Status_Bleeding) Entry->RemainingRounds = Tuning.BleedingRounds;
    else if (Status == Status_Chilled) Entry->RemainingRounds = Tuning.ChillRounds;
    else if (Status == Status_Shocked) Entry->RemainingRounds = Tuning.ShockRounds;
}

void CombatStatusRules::ReserveNextRound(TArray<FCombatStatusEntry>& Entries, const FGameplayTag& Status)
{
    if (Status != Status_Frozen && Status != Status_Confused) return;
    FCombatStatusEntry* Entry = Find(Entries, Status);
    if (!Entry)
    {
        Entry = &Entries.AddDefaulted_GetRef();
        Entry->Status = Status;
    }
    // Repeated application reserves the single next round; it never stacks into extra rounds.
    // 반복 부여는 다음 한 라운드만 예약하며 추가 라운드로 누적되지 않습니다.
    Entry->Value = 1;
    Entry->RemainingRounds = 1;
}

int32 CombatStatusRules::NextBurn(int32 Burn, ECombatBurnDecay Decay)
{
    const int32 Half = FMath::Max(0, Burn) / 2;
    return Decay == ECombatBurnDecay::HalfFloor ? Half : FMath::Max(0, Half - 1);
}

TArray<CombatStatusRules::FRoundEndDamage> CombatStatusRules::TickRoundEnd(TArray<FCombatStatusEntry>& Entries, const FTuning& Tuning)
{
    TArray<FRoundEndDamage> Damage;
    if (FCombatStatusEntry* Burn = Find(Entries, Status_Burning); Burn && Burn->Value > 0)
    {
        Damage.Add({Status_Burning, Burn->Value});
        Burn->Value = NextBurn(Burn->Value, Tuning.BurnDecay);
    }
    if (FCombatStatusEntry* Poison = Find(Entries, Status_Poisoned); Poison && Poison->Value > 0)
    {
        Damage.Add({Status_Poisoned, Poison->Value});
        Poison->Value = FMath::Max(0, Poison->Value - 1);
    }
    Entries.RemoveAll([](const FCombatStatusEntry& Entry) { return (Entry.Status == Status_Burning || Entry.Status == Status_Poisoned) && Entry.Value <= 0; });
    return Damage;
}

void CombatStatusRules::AdvancePlanningBoundary(TArray<FCombatStatusEntry>& Entries)
{
    Entries.RemoveAll([](const FCombatStatusEntry& Entry) { return UsesDuration(Entry.Status) && Entry.RemainingRounds <= 0; });
    for (FCombatStatusEntry& Entry : Entries)
    {
        if (UsesDuration(Entry.Status)) --Entry.RemainingRounds;
    }
}

bool CombatStatusRules::IsActiveThisRound(const TArray<FCombatStatusEntry>& Entries, const FGameplayTag& Status)
{
    const FCombatStatusEntry* Entry = Find(Entries, Status);
    return Entry && (Status == Status_Frozen || Status == Status_Confused) && Entry->RemainingRounds == 0;
}

bool CombatStatusRules::LocksPlan(const TArray<FCombatStatusEntry>& Entries)
{
    return IsActiveThisRound(Entries, Status_Frozen) || IsActiveThisRound(Entries, Status_Confused);
}

float CombatStatusRules::ChilledSpeed(float BaseSpeed, const TArray<FCombatStatusEntry>& Entries, const FTuning& Tuning)
{
    const int32 Chill = GetValue(Entries, Status_Chilled);
    if (Chill <= 0 || !FMath::IsFinite(BaseSpeed)) return BaseSpeed;
    // Never raise an already slower unit to the floor value.
    // 이미 최소값보다 느린 유닛을 최소값까지 올리지 않습니다.
    return FMath::Min(BaseSpeed, FMath::Max(Tuning.MinimumChilledSpeed, BaseSpeed - Chill * Tuning.ChillSpeedPerPoint));
}

void CombatStatusRules::SyncOwnedTags(AUnitBase* Unit, const TArray<FCombatStatusEntry>& Entries)
{
    UAbilitySystemComponent* ASC = IsValid(Unit) && Unit->HasAuthority() ? Unit->GetAbilitySystemComponent() : nullptr;
    if (!ASC) return;
    for (const FGameplayTag& Status : SupportedStatuses()) ASC->SetLooseGameplayTagCount(Status, Has(Entries, Status) ? 1 : 0);
}

FText CombatStatusRules::Summary(const TArray<FCombatStatusEntry>& Entries)
{
    TArray<FText> Parts;
    for (const FCombatStatusEntry& Entry : Entries)
    {
        FText Name;
        bool bShowValue = true;
        if (Entry.Status == Status_Burning) Name = NSLOCTEXT("CombatStatus", "Burning", "화상");
        else if (Entry.Status == Status_Poisoned) Name = NSLOCTEXT("CombatStatus", "Poisoned", "중독");
        else if (Entry.Status == Status_Bleeding) Name = NSLOCTEXT("CombatStatus", "Bleeding", "출혈");
        else if (Entry.Status == Status_Chilled) Name = NSLOCTEXT("CombatStatus", "Chilled", "냉각");
        else if (Entry.Status == Status_Shocked) Name = NSLOCTEXT("CombatStatus", "Shocked", "감전");
        else if (Entry.Status == Status_Frozen)
        {
            Name = Entry.RemainingRounds == 0 ? NSLOCTEXT("CombatStatus", "FrozenNow", "빙결(행동 불가)") : NSLOCTEXT("CombatStatus", "FrozenNext", "빙결 예정");
            bShowValue = false;
        }
        else if (Entry.Status == Status_Confused)
        {
            Name = Entry.RemainingRounds == 0 ? NSLOCTEXT("CombatStatus", "ConfusedNow", "혼란(무작위 행동)") : NSLOCTEXT("CombatStatus", "ConfusedNext", "혼란 예정");
            bShowValue = false;
        }
        else continue;
        Parts.Add(bShowValue ? FText::Format(NSLOCTEXT("CombatStatus", "ValueFormat", "{0} {1}"), Name, FText::AsNumber(Entry.Value)) : Name);
    }
    return FText::Join(NSLOCTEXT("CombatStatus", "Separator", " · "), Parts);
}
