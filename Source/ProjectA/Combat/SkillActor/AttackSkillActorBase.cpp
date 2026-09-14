#include "Combat/SkillActor/AttackSkillActorBase.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "GameplayEffect.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/UnitBase.h"

AAttackSkillActorBase::AAttackSkillActorBase()
{
}

void AAttackSkillActorBase::InitializeAttackSkillActor(const FSkillActorInitData& InitData, TSubclassOf<UGameplayEffect> InDamageEffectClass, float InDamageAmount)
{
    DamageEffectClass = InDamageEffectClass;
    DamageAmount = InDamageAmount;

    InitializeSkillActor(InitData);

    UE_LOG(LogTemp, Log, TEXT("[AttackSkillActorBase] InitializeAttackSkillActor | Actor=%s | DamageEffect=%s | DamageAmount=%.2f"), *GetNameSafe(this), *GetNameSafe(DamageEffectClass), DamageAmount);
}

TSubclassOf<UGameplayEffect> AAttackSkillActorBase::GetDamageEffectClass() const
{
    return DamageEffectClass;
}

float AAttackSkillActorBase::GetDamageAmount() const
{
    return DamageAmount;
}

void AAttackSkillActorBase::HandleImpact()
{
    Super::HandleImpact();
}

void AAttackSkillActorBase::ApplyImpactEffect()
{
    // Compatibility actor has no damage authority; real-time projectiles report to the round coordinator.
    // 호환 액터에는 피해 권위가 없으며 실시간 투사체는 라운드 조정자에게 적중을 보고합니다.
}

TArray<AUnitBase*> AAttackSkillActorBase::ResolveImpactTargetUnits() const
{
    return {};
}

bool AAttackSkillActorBase::IsValidImpactTargetUnit(AUnitBase* TargetUnit) const
{
    return false;
}
