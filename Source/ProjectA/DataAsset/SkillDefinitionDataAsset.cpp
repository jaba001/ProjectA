#include "DataAsset/SkillDefinitionDataAsset.h"
#include "GAS/Ability/GA_AttackBase.h"

bool USkillDefinitionDataAsset::ResolveRoundSkill(FCombatRoundSkill& OutSkill, FText& OutError) const
{
    OutSkill = FCombatRoundSkill();
    OutError = FText::GetEmpty();
    const auto Fail = [this, &OutError](const FText& Reason)
    {
        OutError = FText::Format(NSLOCTEXT("SkillRound", "AssetError", "{0}: {1}"), FText::FromString(GetPathName()), Reason);
        return false;
    };
    const FPrimaryAssetId AssetId = GetPrimaryAssetId();
    if (!AssetId.IsValid()) return Fail(NSLOCTEXT("SkillRound", "MissingId", "A valid primary asset ID is required. / 유효한 기본 에셋 식별자가 필요합니다."));

    FCombatRoundSkill Skill = RoundDefinition;
    if (!bUseRoundDefinition)
    {
        const bool bSupportedArea = AreaType == ESkillAreaType::Single || AreaType == ESkillAreaType::AroundTarget;
        if (TargetRule != ESkillTargetRule::EnemyUnit || !bSupportedArea || AreaRadius < 0 || ActionPointCost <= 0)
        {
            return Fail(NSLOCTEXT("SkillRound", "ExplicitProfileRequired", "Automatic migration supports EnemyUnit with Single or AroundTarget, a nonnegative radius and positive AP cost. Enable bUseRoundDefinition and author RoundDefinition for other semantics. / 자동 이행은 EnemyUnit의 Single·AroundTarget, 0 이상의 반경과 양수 AP 비용만 지원합니다. 그 외 의미는 bUseRoundDefinition을 켜고 RoundDefinition을 직접 작성하세요."));
        }
        const UGA_AttackBase* Attack = AbilityClass ? Cast<UGA_AttackBase>(AbilityClass->GetDefaultObject()) : nullptr;
        if (!Attack)
        {
            return Fail(NSLOCTEXT("SkillRound", "ExplicitAbilityProfileRequired", "Automatic migration requires an attack derived from UGA_AttackBase. Custom abilities need an explicit RoundDefinition. / 자동 이행에는 UGA_AttackBase를 상속한 공격이 필요합니다. 사용자 어빌리티는 RoundDefinition을 직접 작성하세요."));
        }

        // Only the authored damage value is reused; no legacy ability is activated.
        // 작성된 피해 수치만 재사용하며 기존 어빌리티를 활성화하지 않습니다.
        Skill = FCombatRoundSkill();
        Skill.ActionPointCost = ActionPointCost;
        Skill.Power = Attack->GetAuthoredDamageAmount();
        Skill.Kind = bMoveToTarget ? ECombatRoundSkillKind::Melee : ECombatRoundSkillKind::Projectile;
        Skill.Approach = bMoveToTarget ? ECombatRoundApproach::Unit : ECombatRoundApproach::None;
        Skill.TargetLoss = bMoveToTarget ? ECombatRoundTargetLoss::Cancel : ECombatRoundTargetLoss::KeepLocation;
        Skill.bTargetOnly = false;
        if (AreaType == ESkillAreaType::AroundTarget)
        {
            Skill.Kind = ECombatRoundSkillKind::GroundAttack;
            Skill.Approach = bMoveToTarget ? ECombatRoundApproach::Tile : ECombatRoundApproach::None;
            Skill.TargetLoss = ECombatRoundTargetLoss::KeepLocation;
            Skill.HitRange = FMath::Max(150.f, static_cast<float>(AreaRadius) * 200.f);
        }
    }
    Skill.SkillId = FName(*AssetId.ToString());
    Skill.Name = SkillName;
    if (!CombatRoundRules::IsValidSkill(Skill)) return Fail(NSLOCTEXT("SkillRound", "InvalidProfile", "RoundDefinition contains invalid timing, power, range, cost or approach settings. / RoundDefinition의 시간·위력·범위·비용·접근 설정이 유효하지 않습니다."));
    OutSkill = MoveTemp(Skill);
    return true;
}

FText USkillDefinitionDataAsset::GetActionPointCostText() const
{
    const int32 Cost = bUseRoundDefinition ? RoundDefinition.ActionPointCost : ActionPointCost;
    if (Cost < 0 || Cost > 100 || (!bUseRoundDefinition && Cost == 0))
    {
        return NSLOCTEXT("SkillCost", "Invalid", "Invalid AP / AP 오류");
    }

    return FText::Format(NSLOCTEXT("SkillCost", "Amount", "AP {0}"), FText::AsNumber(Cost));
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult USkillDefinitionDataAsset::IsDataValid(FDataValidationContext& Context) const
{
    const EDataValidationResult ParentResult = Super::IsDataValid(Context);
    FCombatRoundSkill Skill;
    FText Error;
    if (!ResolveRoundSkill(Skill, Error))
    {
        Context.AddError(Error);
        return EDataValidationResult::Invalid;
    }
    return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif
