#include "DataAsset/RunWeaponSkillRulesDataAsset.h"

#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/RunWeaponSkillRules.h"
#include "Game/Run/RunSkillBalance.h"
#include "GAS/CombatGameplayTags.h"
#include "Misc/PackageName.h"
#include "NativeGameplayTags.h"
#include <initializer_list>

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WeaponRarityWhite, "Item.Rarity.White");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WeaponRarityGreen, "Item.Rarity.Green");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WeaponRarityBlue, "Item.Rarity.Blue");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WeaponRarityPurple, "Item.Rarity.Purple");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WeaponRarityOrange, "Item.Rarity.Orange");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WeaponPoolWhite, "Selection.WeaponSkill.Test.White");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WeaponPoolGreen, "Selection.WeaponSkill.Test.Green");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WeaponPoolBlue, "Selection.WeaponSkill.Test.Blue");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WeaponPoolPurple, "Selection.WeaponSkill.Test.Purple");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_WeaponPoolOrange, "Selection.WeaponSkill.Test.Orange");

namespace
{
    FGameplayTagQuery MakeItemQuery(std::initializer_list<const TCHAR*> Names)
    {
        FGameplayTagContainer Tags;
        for (const TCHAR* Name : Names) Tags.AddTag(FGameplayTag::RequestGameplayTag(FName(Name)));
        return FGameplayTagQuery::MakeQuery_MatchAnyTags(Tags);
    }

    void AddCandidate(FRunWeaponSkillRulesState& State, const FString& PackagePath, const FGameplayTagQuery& ItemQuery, const FGameplayTagContainer& SelectionTags)
    {
        const FSoftObjectPath Path(PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath));
        FRunWeaponSkillCandidate* Existing = State.Candidates.FindByPredicate([&Path](const FRunWeaponSkillCandidate& Candidate) { return Candidate.Skill == Path; });
        if (Existing)
        {
            Existing->SelectionTags.AppendTags(SelectionTags);
            return;
        }
        FRunWeaponSkillCandidate& Candidate = State.Candidates.AddDefaulted_GetRef();
        Candidate.Skill = Path;
        Candidate.AllowedItemQuery = ItemQuery;
        Candidate.SelectionTags = SelectionTags;
        Candidate.BaseWeight = 1.0f;
    }

    void AddPackCandidates(FRunWeaponSkillRulesState& State, const TCHAR* Pack, std::initializer_list<const TCHAR*> Names, const FGameplayTagQuery& ItemQuery, FGameplayTag PoolTag)
    {
        const FGameplayTagContainer SelectionTags(PoolTag);
        for (const TCHAR* Name : Names) AddCandidate(State, FString::Printf(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/%s/%s"), Pack, Name), ItemQuery, SelectionTags);
    }

    void AddRarity(FRunWeaponSkillRulesState& State, FGameplayTag RarityTag, const TCHAR* Name, const FLinearColor& Color, FGameplayTag PoolTag)
    {
        FRunWeaponRarityRule& Rarity = State.Rarities.AddDefaulted_GetRef();
        Rarity.RarityTag = RarityTag;
        Rarity.DisplayName = FText::FromString(Name);
        Rarity.Color = Color;
        Rarity.BaseWeight = 1.0f;
        Rarity.SkillQuery = FGameplayTagQuery::MakeQuery_MatchTag(PoolTag);
    }
}

URunWeaponSkillRulesDataAsset::URunWeaponSkillRulesDataAsset()
{
    Rules.SchemaVersion = 1;
    Rules.SkillCount = 1;
    Rules.WeaponQuery = MakeItemQuery({TEXT("Item.Weapon.Sword"), TEXT("Item.Weapon.Dagger"), TEXT("Item.Weapon.Axe"), TEXT("Item.Weapon.Hammer"), TEXT("Item.Weapon.MaceClub"), TEXT("Item.Weapon.Spear"), TEXT("Item.Weapon.Scythe"), TEXT("Item.Weapon.Gauntlet"), TEXT("Item.Weapon.Bow"), TEXT("Item.Weapon.Crossbow"), TEXT("Item.Weapon.StaffWand"), TEXT("Item.Weapon.Spellbook"), TEXT("Item.Weapon.Shield")});
    const FGameplayTagQuery CloseWeaponQuery = MakeItemQuery({TEXT("Item.Weapon.Sword"), TEXT("Item.Weapon.Dagger"), TEXT("Item.Weapon.Axe"), TEXT("Item.Weapon.Hammer"), TEXT("Item.Weapon.MaceClub"), TEXT("Item.Weapon.Spear"), TEXT("Item.Weapon.Scythe"), TEXT("Item.Weapon.Gauntlet")});
    const FGameplayTagQuery BowQuery = MakeItemQuery({TEXT("Item.Weapon.Bow")});
    const FGameplayTagQuery CrossbowQuery = MakeItemQuery({TEXT("Item.Weapon.Crossbow")});
    const FGameplayTagQuery MagicWeaponQuery = MakeItemQuery({TEXT("Item.Weapon.StaffWand"), TEXT("Item.Weapon.Spellbook")});
    FRunWeaponSkillItemQueryOverride& ShieldOverride = ItemQueryOverrides.AddDefaulted_GetRef();
    ShieldOverride.SkillQuery = FGameplayTagQuery::MakeQuery_MatchTag(ProjectACombatTags::Skill_Effect_Shield);
    ShieldOverride.AllowedItemQuery = MakeItemQuery({TEXT("Item.Weapon.Shield")});
    FGameplayTagContainer AllPools;
    const FGameplayTag PoolTags[] = {TAG_WeaponPoolWhite, TAG_WeaponPoolGreen, TAG_WeaponPoolBlue, TAG_WeaponPoolPurple, TAG_WeaponPoolOrange};
    for (FGameplayTag Tag : PoolTags) AllPools.AddTag(Tag);

    // Keep legacy pool metadata; BuildState adds the CSV rarity policy only to newly created Runs.
    // 기존 풀 메타데이터를 유지하며 BuildState는 새 Run에만 CSV 등급 정책을 추가합니다.
    AddCandidate(Rules, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/Weapons/DA_MeleeAttack"), CloseWeaponQuery, AllPools);
    AddCandidate(Rules, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/Weapons/DA_CrossbowAttack"), CrossbowQuery, AllPools);
    AddCandidate(Rules, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/ProjectileHitVFX/DA_DrGame_ProjectileHitVFX_Arrow"), BowQuery, AllPools);
    AddCandidate(Rules, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/ProjectileHitVFX/DA_DrGame_ProjectileHitVFX_FireBall"), MagicWeaponQuery, AllPools);
    AddCandidate(Rules, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/__AoeVFX/DA_DrGame_AoeVFX_AOE_Guardian"), MagicWeaponQuery, AllPools);

    AddPackCandidates(Rules, TEXT("SlashHitVFX"), {TEXT("DA_DrGame_SlashHitVFX_Slash_Axe"), TEXT("DA_DrGame_SlashHitVFX_Slash_CurvedSword"), TEXT("DA_DrGame_SlashHitVFX_Slash_Katana"), TEXT("DA_DrGame_SlashHitVFX_Slash_Reaper")}, CloseWeaponQuery, TAG_WeaponPoolGreen);
    AddPackCandidates(Rules, TEXT("ProjectileHitVFX"), {TEXT("DA_DrGame_ProjectileHitVFX_AerolithCurved"), TEXT("DA_DrGame_ProjectileHitVFX_Comet"), TEXT("DA_DrGame_ProjectileHitVFX_FireBall"), TEXT("DA_DrGame_ProjectileHitVFX_HolyEnergy"), TEXT("DA_DrGame_ProjectileHitVFX_IceCrystal"), TEXT("DA_DrGame_ProjectileHitVFX_Iceicle3D"), TEXT("DA_DrGame_ProjectileHitVFX_MagicLanceShuriken"), TEXT("DA_DrGame_ProjectileHitVFX_MeteoriteStraight"), TEXT("DA_DrGame_ProjectileHitVFX_PoisonSkullFish"), TEXT("DA_DrGame_ProjectileHitVFX_RockSling"), TEXT("DA_DrGame_ProjectileHitVFX_SonicBlade"), TEXT("DA_DrGame_ProjectileHitVFX_ThunderBolt"), TEXT("DA_DrGame_ProjectileHitVFX_VenomDart")}, MagicWeaponQuery, TAG_WeaponPoolGreen);
    AddPackCandidates(Rules, TEXT("__AoeVFX"), {TEXT("DA_DrGame_AoeVFX_AOE_FireArrow")}, BowQuery, TAG_WeaponPoolBlue);
    AddPackCandidates(Rules, TEXT("__AoeVFX"), {TEXT("DA_DrGame_AoeVFX_AOE_BlazeBlast"), TEXT("DA_DrGame_AoeVFX_AOE_Blizzard"), TEXT("DA_DrGame_AoeVFX_AOE_CrystalFlower"), TEXT("DA_DrGame_AoeVFX_AOE_Hail"), TEXT("DA_DrGame_AoeVFX_AOE_IceSpiral"), TEXT("DA_DrGame_AoeVFX_AOE_LavaLotus"), TEXT("DA_DrGame_AoeVFX_AOE_LightningHoop"), TEXT("DA_DrGame_AoeVFX_AOE_Meteoroid_lite"), TEXT("DA_DrGame_AoeVFX_AOE_PoisonCarousel")}, MagicWeaponQuery, TAG_WeaponPoolBlue);
    AddPackCandidates(Rules, TEXT("__GroundAttackVFX"), {TEXT("DA_DrGame_GroundAttackVFX_AberrateObelisk"), TEXT("DA_DrGame_GroundAttackVFX_BrambleTusk"), TEXT("DA_DrGame_GroundAttackVFX_DashMagma"), TEXT("DA_DrGame_GroundAttackVFX_FeudFang"), TEXT("DA_DrGame_GroundAttackVFX_FreezeCrystal_Ice"), TEXT("DA_DrGame_GroundAttackVFX_HuntingSharkFin"), TEXT("DA_DrGame_GroundAttackVFX_IceSpell"), TEXT("DA_DrGame_GroundAttackVFX_IceSpike"), TEXT("DA_DrGame_GroundAttackVFX_IceSprout"), TEXT("DA_DrGame_GroundAttackVFX_LavaBurst"), TEXT("DA_DrGame_GroundAttackVFX_Lazurite"), TEXT("DA_DrGame_GroundAttackVFX_Line_Lava"), TEXT("DA_DrGame_GroundAttackVFX_PoisonCannibalPlant"), TEXT("DA_DrGame_GroundAttackVFX_StoneRush")}, MagicWeaponQuery, TAG_WeaponPoolBlue);
    AddPackCandidates(Rules, TEXT("___LinkChainVFX"), {TEXT("DA_DrGame_LinkChainVFX_Link_Bramble"), TEXT("DA_DrGame_LinkChainVFX_Link_Electric"), TEXT("DA_DrGame_LinkChainVFX_Link_Energy"), TEXT("DA_DrGame_LinkChainVFX_Link_Fire"), TEXT("DA_DrGame_LinkChainVFX_Link_Magic")}, MagicWeaponQuery, TAG_WeaponPoolPurple);
    AddPackCandidates(Rules, TEXT("__AoeVFX"), {TEXT("DA_DrGame_AoeVFX_AOE_TimeSpell")}, MagicWeaponQuery, TAG_WeaponPoolOrange);
    AddPackCandidates(Rules, TEXT("_LevelUpSpawn"), {TEXT("DA_DrGame_LevelUpSpawn_LevelUp_Ascend_Root"), TEXT("DA_DrGame_LevelUpSpawn_LevelUp_Burst_Pivot"), TEXT("DA_DrGame_LevelUpSpawn_LevelUp_Descend_Root"), TEXT("DA_DrGame_LevelUpSpawn_LevelUp_Lift_root"), TEXT("DA_DrGame_LevelUpSpawn_LevelUp_Twist_Root"), TEXT("DA_DrGame_LevelUpSpawn_Spawn_Magic_Root"), TEXT("DA_DrGame_LevelUpSpawn_Spawn_Teleport_Root")}, MagicWeaponQuery, TAG_WeaponPoolOrange);
    AddPackCandidates(Rules, TEXT("_LevelUpSpawn"), {TEXT("DA_DrGame_LevelUpSpawn_Spawn_Down_Root"), TEXT("DA_DrGame_LevelUpSpawn_Spawn_Ground_Root"), TEXT("DA_DrGame_LevelUpSpawn_Spawn_Ninja_Root"), TEXT("DA_DrGame_LevelUpSpawn_Spawn_Up_Root")}, MagicWeaponQuery, TAG_WeaponPoolOrange);

    AddRarity(Rules, TAG_WeaponRarityWhite, TEXT("흰색"), FLinearColor::White, TAG_WeaponPoolWhite);
    AddRarity(Rules, TAG_WeaponRarityGreen, TEXT("초록색"), FLinearColor(0.15f, 0.85f, 0.25f), TAG_WeaponPoolGreen);
    AddRarity(Rules, TAG_WeaponRarityBlue, TEXT("파란색"), FLinearColor(0.15f, 0.45f, 1.0f), TAG_WeaponPoolBlue);
    AddRarity(Rules, TAG_WeaponRarityPurple, TEXT("보라색"), FLinearColor(0.7f, 0.25f, 1.0f), TAG_WeaponPoolPurple);
    AddRarity(Rules, TAG_WeaponRarityOrange, TEXT("주황색"), FLinearColor(1.0f, 0.5f, 0.1f), TAG_WeaponPoolOrange);
}

bool URunWeaponSkillRulesDataAsset::BuildState(FRunWeaponSkillRulesState& OutState, FText& OutError) const
{
    OutError = NSLOCTEXT("RunWeaponSkills", "InvalidRules", "무기 스킬 생성 규칙의 버전·개수·태그·후보·등급이 올바르지 않습니다.");
    if (Rules.WeaponQuery.IsEmpty()) return false;
    for (const FRunWeaponSkillItemQueryOverride& Override : ItemQueryOverrides)
    {
        if (Override.SkillQuery.IsEmpty() || Override.AllowedItemQuery.IsEmpty()) return false;
    }
    FRunWeaponSkillRulesState State = Rules;
    FGameplayTagQueryExpression ExistingWeaponExpression;
    State.WeaponQuery.GetQueryExpr(ExistingWeaponExpression);
    FGameplayTagQueryExpression WeaponExpression;
    WeaponExpression.AnyExprMatch().AddExpr(ExistingWeaponExpression);
    TSet<int32> AppliedOverrides;
    for (FRunWeaponSkillCandidate& Candidate : State.Candidates)
    {
        const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Candidate.Skill.TryLoad());
        if (!Skill)
        {
            OutError = FText::Format(NSLOCTEXT("RunWeaponSkills", "MissingDefaultCandidate", "Weapon skill candidate is unavailable: {0}. / 무기 스킬 후보를 불러올 수 없습니다: {0}."), FText::FromString(Candidate.Skill.ToString()));
            return false;
        }
        FCombatRoundSkill Definition;
        if (!Skill->ResolveRoundSkill(Definition, OutError)) return false;
        Candidate.Tags = Definition.EffectTags;
        // The first matching authored override freezes item eligibility with the resolved execution tags.
        // 처음 일치한 작성 규칙으로 해석된 실행 태그에 맞는 아이템 적합성을 고정합니다.
        for (int32 OverrideIndex = 0; OverrideIndex < ItemQueryOverrides.Num(); ++OverrideIndex)
        {
            const FRunWeaponSkillItemQueryOverride& Override = ItemQueryOverrides[OverrideIndex];
            if (!Override.SkillQuery.Matches(Candidate.Tags)) continue;
            Candidate.AllowedItemQuery = Override.AllowedItemQuery;
            if (!AppliedOverrides.Contains(OverrideIndex))
            {
                // Preserve serialized item eligibility while including each applied override once.
                // 직렬화된 아이템 적합성을 유지하면서 실제 적용한 규칙을 한 번씩 포함합니다.
                FGameplayTagQueryExpression OverrideExpression;
                Override.AllowedItemQuery.GetQueryExpr(OverrideExpression);
                WeaponExpression.AddExpr(OverrideExpression);
                AppliedOverrides.Add(OverrideIndex);
            }
            break;
        }
    }
    if (!AppliedOverrides.IsEmpty()) State.WeaponQuery = FGameplayTagQuery::BuildQuery(WeaponExpression);
    if (bUseCsvBalance && !RunSkillBalance::Load(State, OutError)) return false;
    if (!RunWeaponSkillRules::Validate(State, OutError)) return false;
    OutState = MoveTemp(State);
    OutError = FText::GetEmpty();
    return true;
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult URunWeaponSkillRulesDataAsset::IsDataValid(FDataValidationContext& Context) const
{
    const EDataValidationResult ParentResult = Super::IsDataValid(Context);
    FRunWeaponSkillRulesState State;
    FText Error;
    if (!BuildState(State, Error))
    {
        Context.AddError(Error);
        return EDataValidationResult::Invalid;
    }
    return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif
