#include "GAS/Ability/GA_DefaultAttack.h"
#include "Unit/UnitBase.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "GAS/Attribute/AS_Unit.h"

UGA_DefaultAttack::UGA_DefaultAttack()
{
}

bool UGA_DefaultAttack::CacheAttackContext()
{
    if (!CachedOwnerUnit)
    {
        return false;
    }

    // 현재 공격 대상 유닛을 가져온다.
    CachedTargetUnit = CachedOwnerUnit->PendingTargetUnit;

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
    // 유효한 공격자/타겟이 없으면 종료
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

    const float BeforeHP = TargetASC->GetNumericAttribute(UAS_Unit::GetHPAttribute());

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

    const float AfterHP = TargetASC->GetNumericAttribute(UAS_Unit::GetHPAttribute());

    if (!AppliedHandle.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_DefaultAttack] ApplyDamageEffectToTarget Failed | Invalid AppliedHandle | Source=%s | Target=%s"), *GetNameSafe(CachedOwnerUnit), *GetNameSafe(CachedTargetUnit));
        return;
    }
}