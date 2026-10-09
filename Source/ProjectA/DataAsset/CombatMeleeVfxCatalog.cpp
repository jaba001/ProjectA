#include "DataAsset/CombatMeleeVfxCatalog.h"

#include "Animation/AnimMontage.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "GAS/CombatGameplayTags.h"
#include "NiagaraSystem.h"

UCombatMeleeVfxCatalog::UCombatMeleeVfxCatalog()
{
    FGameplayTagQueryExpression RequiredTags;
    RequiredTags.AllTagsMatch().AddTag(ProjectACombatTags::Skill_Shape_Slash).AddTag(FGameplayTag::RequestGameplayTag(TEXT("Attack.Close"))).AddTag(ProjectACombatTags::Skill_Effect_Damage);
    const FGameplayTagQuery Query = FGameplayTagQuery::BuildQuery(RequiredTags);
    const TArray<TSoftObjectPtr<UAnimMontage>> Montages =
    {
        TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/User_JeHoon/ParagonAnimationsRetargetedToManny/GreystoneManny/Attack/AM_SwordSkillCast.AM_SwordSkillCast"))),
        TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(TEXT("/Game/User_JeHoon/ParagonAnimationsRetargetedToManny/GreystoneManny/Attack/AM_SwordSkillCast_SkeletonGuard.AM_SwordSkillCast_SkeletonGuard")))
    };
    const auto AddRule = [this, &Query, &Montages](FName Id, const TCHAR* Path, bool bEnabled, float RotationSpeed)
    {
        FCombatMeleeVfxRule& Rule = Rules.AddDefaulted_GetRef();
        Rule.RuleId = Id;
        Rule.bEnabled = bEnabled;
        Rule.SkillQuery = Query;
        Rule.Niagara = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(Path));
        Rule.CastMontages = Montages;
        if (bEnabled) Rule.FloatOverrides.Add(TEXT("User.RotateSpeed"), RotationSpeed);
    };
    AddRule(TEXT("Axe"), TEXT("/Game/SlashHitVFX/NS/NS_Slash_Axe.NS_Slash_Axe"), true, -1.0f);
    AddRule(TEXT("CurvedSword"), TEXT("/Game/SlashHitVFX/NS/NS_Slash_CurvedSword.NS_Slash_CurvedSword"), true, -1.5f);
    AddRule(TEXT("Reaper"), TEXT("/Game/SlashHitVFX/NS/NS_Slash_Reaper.NS_Slash_Reaper"), true, -2.0f);
}

FCombatSkillVfx UCombatMeleeVfxCatalog::Resolve(const FCombatRoundSkill& Skill, const UAnimMontage* ResolvedCastMontage) const
{
    const FCombatSkillVfx& Original = Skill.Vfx;
    if (!IsValid(ResolvedCastMontage) || Skill.Kind != ECombatRoundSkillKind::Melee || !Skill.bUseEffectCollision || Original.Niagara.IsNull() || !Original.Cascade.IsNull()) return Original;
    if (Original.RelativeTransform.ContainsNaN() || !Original.RelativeTransform.GetRotation().Equals(FQuat::Identity, 0.0) || Original.RelativeTransform.GetScale3D().GetMin() <= 0.0) return Original;
    const FSoftObjectPath MontagePath(ResolvedCastMontage);
    const FCombatMeleeVfxRule* Matched = nullptr;
    for (const FCombatMeleeVfxRule& Rule : Rules)
    {
        if (!Rule.bEnabled || Rule.RuleId.IsNone() || Rule.SkillQuery.IsEmpty() || !Rule.SkillQuery.Matches(Skill.EffectTags) || Rule.Niagara.IsNull() || Rule.Niagara.ToSoftObjectPath() != Original.Niagara.ToSoftObjectPath()) continue;
        if (!Rule.CastMontages.ContainsByPredicate([&MontagePath](const TSoftObjectPtr<UAnimMontage>& Montage) { return Montage.ToSoftObjectPath() == MontagePath; })) continue;
        // Ambiguous matching rules never accumulate corrections or depend on insertion order.
        // 여러 규칙이 일치하면 보정을 누적하거나 배열 순서에 의존하지 않고 원본을 보존합니다.
        if (Matched) return Original;
        Matched = &Rule;
    }
    if (!Matched || Original.FloatParameters.Num() + Matched->FloatOverrides.Num() > 64) return Original;
    for (const TPair<FName, float>& Parameter : Matched->FloatOverrides)
    {
        // An existing authored override takes precedence, including a previously corrected visual copy.
        // 기존 작성 재정의는 이미 보정된 시각 사본을 포함하여 항상 우선합니다.
        if (Parameter.Key.IsNone() || !FMath::IsFinite(Parameter.Value) || Original.FloatParameters.Contains(Parameter.Key) || Original.BoolParameters.Contains(Parameter.Key) || Parameter.Key == Original.StartPositionParameter || Parameter.Key == Original.EndPositionParameter) return Original;
    }
    FCombatSkillVfx Result = Original;
    for (const TPair<FName, float>& Parameter : Matched->FloatOverrides) Result.FloatParameters.Add(Parameter.Key, Parameter.Value);
    return Result;
}
