#include "GAS/Library/CombatEffectLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Unit/UnitBase.h"

bool UCombatEffectLibrary::ApplyDamageToUnit(AUnitBase* SourceUnit, AUnitBase* TargetUnit,
    TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount)
{
    if (!SourceUnit || !TargetUnit)
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

    if (!DamageEffectClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatEffectLibrary] ApplyDamageToUnit Failed | DamageEffectClass is null"));
        return false;
    }

    FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
    EffectContext.AddSourceObject(SourceUnit);

    FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);

    if (!SpecHandle.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatEffectLibrary] ApplyDamageToUnit Failed | Invalid SpecHandle"));
        return false;
    }

    const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));

    SpecHandle.Data->SetSetByCallerMagnitude(DamageTag, -DamageAmount);

    const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

    if (!AppliedHandle.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatEffectLibrary] ApplyDamageToUnit Failed | Apply Result Invalid | Source=%s | Target=%s"), *GetNameSafe(SourceUnit), *GetNameSafe(TargetUnit));
        return false;
    }

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