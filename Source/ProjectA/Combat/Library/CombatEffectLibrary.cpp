#include "Combat/Library/CombatEffectLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "GAS/Attribute/AS_Unit.h"
#include "GAS/CombatGameplayTags.h"
#include "Unit/UnitBase.h"

bool UCombatEffectLibrary::ApplyDamageToUnit(AUnitBase* SourceUnit, AUnitBase* TargetUnit, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount)
{
    return ApplyTaggedEffectToUnit(SourceUnit, TargetUnit, DamageEffectClass, DamageAmount, FGameplayTagContainer());
}

bool UCombatEffectLibrary::ApplyTaggedEffectToUnit(AUnitBase* SourceUnit, AUnitBase* TargetUnit, TSubclassOf<UGameplayEffect> EffectClass, float Power, const FGameplayTagContainer& AssetTags)
{
    if (!IsValid(SourceUnit) || !IsValid(TargetUnit) || !SourceUnit->HasAuthority() || !TargetUnit->HasAuthority() || SourceUnit->GetWorld() != TargetUnit->GetWorld() || !FMath::IsFinite(Power) || Power < 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatEffectLibrary] ApplyDamageToUnit Failed | Invalid Source or Target"));
        return false;
    }

    if (!TargetUnit->IsUnitAlive())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatEffectLibrary] ApplyDamageToUnit Failed | Target Dead | Target=%s"), *GetNameSafe(TargetUnit));
        return false;
    }

    UAbilitySystemComponent* SourceASC = SourceUnit->GetAbilitySystemComponent();
    UAbilitySystemComponent* TargetASC = TargetUnit->GetAbilitySystemComponent();

    if (!SourceASC || !TargetASC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatEffectLibrary] ApplyDamageToUnit Failed | Invalid ASC | Source=%s | Target=%s"), *GetNameSafe(SourceUnit), *GetNameSafe(TargetUnit));
        return false;
    }

    if (!EffectClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatEffectLibrary] ApplyDamageToUnit Failed | DamageEffectClass is null"));
        return false;
    }

    FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
    EffectContext.AddSourceObject(SourceUnit);

    FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);

    if (!SpecHandle.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatEffectLibrary] ApplyDamageToUnit Failed | Invalid SpecHandle"));
        return false;
    }

    SpecHandle.Data->AppendDynamicAssetTags(AssetTags);
    FGameplayTagContainer EffectTags;
    SpecHandle.Data->GetAllAssetTags(EffectTags);
    const bool bHeal = EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Heal);
    const bool bShield = EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Shield);
    const bool bDamage = EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Damage);
    if ((bHeal && bShield) || (bDamage && (bHeal || bShield))) return false;
    if ((bHeal || bShield) && (!TargetUnit->GetAttributeSet() || TargetUnit->GetAttributeSet()->GetHP() <= 0.0f)) return false;

    // Damage retains its negative contract; positive support power follows authored effect tags.
    // 피해의 음수 계약을 유지하고 양수 지원 강도는 제작된 효과 태그를 따릅니다.
    SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Data.Damage")), -Power);
    SpecHandle.Data->SetSetByCallerMagnitude(ProjectACombatTags::Data_Heal, bHeal ? Power : 0.0f);
    SpecHandle.Data->SetSetByCallerMagnitude(ProjectACombatTags::Data_Shield, bShield ? Power : 0.0f);

    const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

    // Instant effects have no active handle; the application result still records rejection by GAS filters.
    // 즉시 효과에는 활성 핸들이 없지만 적용 결과에는 GAS 조건에 의한 거절 여부가 기록됩니다.
    if (!AppliedHandle.WasSuccessfullyApplied())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatEffectLibrary] ApplyDamageToUnit Failed | Apply Result Invalid | Source=%s | Target=%s"), *GetNameSafe(SourceUnit), *GetNameSafe(TargetUnit));
        return false;
    }

    return true;
}

bool UCombatEffectLibrary::ClearRoundShield(AUnitBase* Unit)
{
    if (!IsValid(Unit) || !Unit->HasAuthority() || !Unit->GetAttributeSet()) return false;
    UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
    if (!ASC) return false;
    ASC->SetNumericAttributeBase(UAS_Unit::GetShieldAttribute(), 0.0f);
    Unit->ForceNetUpdate();
    return true;
}

int32 UCombatEffectLibrary::ApplyDamageToUnits(AUnitBase* SourceUnit, const TArray<AUnitBase*>& TargetUnits, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount)
{
    if (!SourceUnit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatEffectLibrary] ApplyDamageToUnits Failed | SourceUnit is null"));
        return 0;
    }

    if (!DamageEffectClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatEffectLibrary] ApplyDamageToUnits Failed | DamageEffectClass is null"));
        return 0;
    }

    if (TargetUnits.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatEffectLibrary] ApplyDamageToUnits Skipped | TargetUnits Empty | Source=%s"), *GetNameSafe(SourceUnit));
        return 0;
    }

    int32 AppliedCount = 0;
    TSet<AUnitBase*> UniqueTargets;

    for (AUnitBase* TargetUnit : TargetUnits)
    {
        if (!TargetUnit)
        {
            continue;
        }

        if (TargetUnit == SourceUnit)
        {
            continue;
        }

        if (UniqueTargets.Contains(TargetUnit))
        {
            continue;
        }

        UniqueTargets.Add(TargetUnit);

        if (ApplyDamageToUnit(SourceUnit, TargetUnit, DamageEffectClass, DamageAmount))
        {
            ++AppliedCount;
        }
    }

    return AppliedCount;
}
