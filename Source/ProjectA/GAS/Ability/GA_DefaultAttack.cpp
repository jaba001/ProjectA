#include "GAS/Ability/GA_DefaultAttack.h"
#include "Unit/UnitBase.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"

UGA_DefaultAttack::UGA_DefaultAttack()
{
}

bool UGA_DefaultAttack::CacheAttackContext()
{
    if (!CachedOwnerUnit)
    {
        return false;
    }

    CachedTargetUnit = CachedOwnerUnit->PendingTargetUnit;

    if (!CachedTargetUnit || !CachedTargetUnit->IsUnitAlive())
    {
        return false;
    }

    return true;
}

bool UGA_DefaultAttack::ValidateAttackContext() const
{
    if (!CachedTargetUnit)
    {
        return false;
    }

    if (!CachedTargetUnit->IsUnitAlive())
    {
        return false;
    }

    return true;
}

void UGA_DefaultAttack::ApplyAttackEffect()
{
    ApplyDamageEffectToTarget();
}

void UGA_DefaultAttack::ClearCachedAttackContext()
{
    CachedTargetUnit = nullptr;
}

void UGA_DefaultAttack::ApplyDamageEffectToTarget()
{
    if (!CachedOwnerUnit || !CachedTargetUnit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ApplyDamageEffectToTarget Failed | Invalid Owner or Target"));
        return;
    }

    if (!CachedTargetUnit->IsUnitAlive())
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ApplyDamageEffectToTarget Failed | Target Dead | Target=%s"), *GetNameSafe(CachedTargetUnit));
        return;
    }

    UAbilitySystemComponent* SourceASC = CachedOwnerUnit->GetAbilitySystemComponent();
    UAbilitySystemComponent* TargetASC = CachedTargetUnit->GetAbilitySystemComponent();

    if (!SourceASC || !TargetASC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ApplyDamageEffectToTarget Failed | Invalid ASC | Source=%s | Target=%s"), *GetNameSafe(CachedOwnerUnit), *GetNameSafe(CachedTargetUnit));
        return;
    }

    if (!DamageEffectClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ApplyDamageEffectToTarget Failed | DamageEffectClass is null"));
        return;
    }

    FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
    EffectContext.AddSourceObject(CachedOwnerUnit);

    FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, EffectContext);

    if (!SpecHandle.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ApplyDamageEffectToTarget Failed | Invalid SpecHandle"));
        return;
    }

    const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage"));

    SpecHandle.Data->SetSetByCallerMagnitude(DamageTag, -DamageAmount);

    const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

    const UGameplayEffect* DamageEffectCDO = DamageEffectClass->GetDefaultObject<UGameplayEffect>();
    const bool bIsInstantEffect = DamageEffectCDO && DamageEffectCDO->DurationPolicy == EGameplayEffectDurationType::Instant;

    if (!AppliedHandle.IsValid() && !bIsInstantEffect)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ApplyDamageEffectToTarget Failed | Invalid AppliedHandle | Source=%s | Target=%s"),
            *GetNameSafe(CachedOwnerUnit),
            *GetNameSafe(CachedTargetUnit));
        return;
    }
}