#include "UI/Gameplay/RunItemPresentation.h"

#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunSkillBalance.h"
#include "GAS/CombatGameplayTags.h"
#include "UI/ProjectALocalization.h"

namespace RunItemPresentationInternal
{
    FText SlotName(FGameplayTag Slot)
    {
        const TArray<FGameplayTag> Slots = URunEquipmentCatalog::GetSlotTags();
        const TArray<FText> Labels = {NSLOCTEXT("Equipment", "MainHand", "주 무기"), NSLOCTEXT("Equipment", "OffHand", "보조 무기"), NSLOCTEXT("Equipment", "Head", "투구"), NSLOCTEXT("Equipment", "Hands", "장갑"), NSLOCTEXT("Equipment", "Feet", "신발"), NSLOCTEXT("Equipment", "Body", "갑옷"), NSLOCTEXT("Equipment", "Neck", "목걸이"), NSLOCTEXT("Equipment", "RingOne", "반지 1"), NSLOCTEXT("Equipment", "RingTwo", "반지 2")};
        const int32 Index = Slots.IndexOfByKey(Slot);
        return Labels.IsValidIndex(Index) ? Labels[Index] : NSLOCTEXT("RunItem", "UnknownSlot", "기타 장비 슬롯");
    }

    FLinearColor SkillColor(FGameplayTag Rarity)
    {
        const TCHAR* Names[] = {TEXT("흰색"), TEXT("초록색"), TEXT("파란색"), TEXT("보라색"), TEXT("주황색")};
        const FLinearColor Colors[] = {FLinearColor::White, FLinearColor(0.15f, 0.85f, 0.25f), FLinearColor(0.15f, 0.45f, 1.0f), FLinearColor(0.7f, 0.25f, 1.0f), FLinearColor(1.0f, 0.5f, 0.1f)};
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index) if (Rarity == RunSkillBalance::ResolveRarityTag(Names[Index])) return Colors[Index];
        return FLinearColor::White;
    }

    FText TargetDescription(ESkillTargetRule Target)
    {
        switch (Target)
        {
        case ESkillTargetRule::EnemyUnit: return NSLOCTEXT("RunItem", "EnemyUnitTarget", "대상: 적 유닛");
        case ESkillTargetRule::AllyUnit: return NSLOCTEXT("RunItem", "AllyUnitTarget", "대상: 아군 유닛");
        case ESkillTargetRule::AnyUnit: return NSLOCTEXT("RunItem", "AnyUnitTarget", "대상: 아군 또는 적 유닛");
        case ESkillTargetRule::EnemyTile: return NSLOCTEXT("RunItem", "EnemyTileTarget", "대상: 적 진영 타일");
        case ESkillTargetRule::AllyTile: return NSLOCTEXT("RunItem", "AllyTileTarget", "대상: 아군 진영 타일");
        case ESkillTargetRule::AnyTile: return NSLOCTEXT("RunItem", "AnyTileTarget", "대상: 타일");
        default: return NSLOCTEXT("RunItem", "UnknownTarget", "대상 정보를 확인할 수 없습니다.");
        }
    }

    FText EffectDescription(const FCombatRoundSkill& Skill)
    {
        TArray<FText> Parts;
        Parts.Add(TargetDescription(Skill.TargetRule));
        if (Skill.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Heal)) Parts.Add(NSLOCTEXT("RunItem", "HealEffect", "살아 있는 대상의 HP를 회복합니다."));
        else if (Skill.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Shield)) Parts.Add(NSLOCTEXT("RunItem", "ShieldEffect", "살아 있는 대상에게 피해를 흡수하는 보호막을 부여합니다. 남은 보호막은 라운드 종료 시 사라집니다."));
        else if (Skill.EffectTags.HasTag(ProjectACombatTags::Skill_Effect_Damage)) Parts.Add(NSLOCTEXT("RunItem", "DamageEffect", "적중한 대상에게 피해를 줍니다."));
        else Parts.Add(NSLOCTEXT("RunItem", "OtherEffect", "대상에게 스킬 효과를 적용합니다."));
        if (CombatRoundRules::UsesChain(Skill) && Skill.Chain.MaxTargets > 1) Parts.Add(NSLOCTEXT("RunItem", "ChainEffect", "적중 후 주변 적에게 연쇄됩니다."));
        else if (Skill.bUseEffectCollision && Skill.bTargetOnly) Parts.Add(NSLOCTEXT("RunItem", "TargetOnlyEffect", "선택한 대상에게만 효과를 적용합니다."));
        else if (Skill.EffectTags.HasTag(ProjectACombatTags::Skill_Shape_Projectile)) Parts.Add(NSLOCTEXT("RunItem", "ProjectileEffect", "투사체를 발사합니다."));
        else if (Skill.EffectTags.HasTag(ProjectACombatTags::Skill_Shape_Beam)) Parts.Add(NSLOCTEXT("RunItem", "BeamEffect", "전방 직선 범위에 적용합니다."));
        else if (Skill.EffectTags.HasTag(ProjectACombatTags::Skill_Shape_Slash)) Parts.Add(NSLOCTEXT("RunItem", "SlashEffect", "전방 참격 범위에 적용합니다."));
        else if (Skill.EffectTags.HasTag(ProjectACombatTags::Skill_Shape_Area)) Parts.Add(NSLOCTEXT("RunItem", "AreaEffect", "지정 범위 안의 대상에게 적용합니다."));
        if (Skill.Approach != ECombatRoundApproach::None) Parts.Add(NSLOCTEXT("RunItem", "ApproachEffect", "대상에 접근한 뒤 사용합니다."));
        return FText::Join(FText::FromString(TEXT("\n")), Parts);
    }

    FText Stats(float Power, int32 AP, int32 SAP, float Windup)
    {
        return FText::Format(NSLOCTEXT("RunItem", "DetailedSkillStats", "위력 {0} · AP {1} / SAP {2} · 선딜 {3}초"), FText::AsNumber(Power), FText::AsNumber(AP), FText::AsNumber(SAP), FText::AsNumber(Windup));
    }
}

const FRunWeaponRarityRule* RunItemPresentation::FindRarity(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities)
{
    if (Item.GenerationVersion == 0) return nullptr;
    return Rarities.FindByPredicate([&Item](const FRunWeaponRarityRule& Rule) { return Rule.RarityTag == Item.RarityTag; });
}

FText RunItemPresentation::Name(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities)
{
    const FText Name = ProjectALocalization::AssetName(Item.Asset, Item.DisplayName.IsEmpty() ? FText::FromString(Item.Asset.GetAssetName()) : Item.DisplayName);
    const FRunWeaponRarityRule* Rarity = FindRarity(Item, Rarities);
    return Rarity ? FText::Format(NSLOCTEXT("RunItem", "RarityName", "[{0}] {1}"), ProjectALocalization::Content(TEXT("ItemRarity.") + Rarity->RarityTag.ToString() + TEXT(".Name"), Rarity->DisplayName), Name) : Name;
}

FText RunItemPresentation::SkillName(const USkillDefinitionDataAsset* Skill)
{
    if (!Skill) return NSLOCTEXT("RunItem", "MissingSkill", "스킬 정보를 불러올 수 없습니다.");
    const FText Source = Skill->SkillName.IsEmpty() ? FText::FromName(Skill->SkillId) : Skill->SkillName;
    return ProjectALocalization::SkillName(FName(*Skill->GetPrimaryAssetId().ToString()), Source);
}

FText RunItemPresentation::SkillDescription(const USkillDefinitionDataAsset* Skill)
{
    return Skill ? ProjectALocalization::SkillDescription(FName(*Skill->GetPrimaryAssetId().ToString()), Skill->SkillDescription) : FText::GetEmpty();
}

FText RunItemPresentation::GrantedSkills(const FRunItemDefinition& Item, bool bIncludeStats)
{
    if (Item.GenerationVersion == 0 || Item.GrantedSkills.IsEmpty()) return FText::GetEmpty();
    TArray<FText> Names;
    for (int32 Index = 0; Index < Item.GrantedSkills.Num(); ++Index)
    {
        const FSoftObjectPath& Path = Item.GrantedSkills[Index];
        const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Path.TryLoad());
        FText Name = SkillName(Skill);
        if (Item.SkillBalanceVersion == 1 && Item.GrantedSkillBalances.IsValidIndex(Index))
        {
            const FRunSkillBalance& Balance = Item.GrantedSkillBalances[Index];
            Name = bIncludeStats ? FText::Format(NSLOCTEXT("RunItem", "BalancedSkill", "[{0}] {1} · 위력 {2} · AP {3} / SAP {4}"), RunSkillBalance::RarityName(Balance.RarityTag), Name, FText::AsNumber(Balance.Power), FText::AsNumber(Balance.ActionPointCost), FText::AsNumber(Balance.SubActionPointCost)) : FText::Format(NSLOCTEXT("RunItem", "DetailedSkillName", "[{0}] {1}"), RunSkillBalance::RarityName(Balance.RarityTag), Name);
        }
        Names.Add(Name);
    }
    return FText::Format(NSLOCTEXT("RunItem", "GrantedSkills", "장착 스킬: {0}"), FText::Join(FText::FromString(TEXT(" · ")), Names));
}

FText RunItemPresentation::Tooltip(const FRunItemDefinition& Item, const TArray<FRunWeaponRarityRule>& Rarities)
{
    const FText Skills = GrantedSkills(Item);
    return Skills.IsEmpty() ? Name(Item, Rarities) : FText::Format(NSLOCTEXT("RunItem", "SkillTooltip", "{0}\n{1}"), Name(Item, Rarities), Skills);
}

FText RunItemPresentation::EquipmentDescription(const FRunItemDefinition& Item)
{
    const FRunEquipmentProfile* Profile = URunEquipmentCatalog::Get().ResolveProfile(Item);
    if (!Profile) return NSLOCTEXT("RunItem", "UnsupportedEquipment", "현재 장착을 지원하지 않는 아이템입니다.");
    TArray<FGameplayTag> ResolvedSlots;
    for (FGameplayTag RequestedSlot : URunEquipmentCatalog::GetSlotTags())
    {
        const FGameplayTag Slot = URunEquipmentCatalog::ResolveSlot(*Profile, RequestedSlot);
        if (Slot.IsValid()) ResolvedSlots.AddUnique(Slot);
    }
    if (ResolvedSlots.IsEmpty()) return NSLOCTEXT("RunItem", "MissingEquipmentSlot", "장착 위치를 확인할 수 없습니다.");
    TArray<FText> Labels;
    bool bTwoHanded = false;
    for (FGameplayTag Slot : ResolvedSlots)
    {
        Labels.Add(RunItemPresentationInternal::SlotName(Slot));
        const FGameplayTagContainer Occupied = URunEquipmentCatalog::GetOccupiedSlots(*Profile, Slot);
        bTwoHanded |= Occupied.HasTagExact(URunEquipmentCatalog::GetWeaponSlot(0)) && Occupied.HasTagExact(URunEquipmentCatalog::GetWeaponSlot(1));
    }
    return FText::Format(NSLOCTEXT("RunItem", "EquipmentDescription", "장착 위치: {0}{1}"), FText::Join(FText::FromString(TEXT(" · ")), Labels), bTwoHanded ? NSLOCTEXT("RunItem", "OccupiesBothHands", " · 양손 슬롯 사용") : FText::GetEmpty());
}

TArray<RunItemPresentation::FItemSkillDetails> RunItemPresentation::SkillDetails(const FRunItemDefinition& Item)
{
    TArray<FItemSkillDetails> Details;
    if (Item.GenerationVersion == 0) return Details;
    // Preserve every saved skill rather than assuming the current single-skill generation policy.
    // 현재의 스킬 한 개 생성 정책을 가정하지 않고 저장된 모든 스킬을 표시합니다.
    Details.Reserve(Item.GrantedSkills.Num());
    for (int32 Index = 0; Index < Item.GrantedSkills.Num(); ++Index)
    {
        FItemSkillDetails& Detail = Details.AddDefaulted_GetRef();
        const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Item.GrantedSkills[Index].TryLoad());
        Detail.Name = SkillName(Skill);
        FCombatRoundSkill Resolved;
        FText Error;
        const bool bResolved = Skill && Skill->ResolveRoundSkill(Resolved, Error);
        Detail.Description = bResolved ? RunItemPresentationInternal::EffectDescription(Resolved) : NSLOCTEXT("RunItem", "MissingSkillEffect", "스킬의 효과 정보를 확인할 수 없습니다.");
        if (Item.SkillBalanceVersion == 1)
        {
            // Frozen values remain authoritative even when an asset cannot be loaded; never substitute current asset tuning.
            // 에셋을 불러오지 못해도 저장된 수치가 기준이며 현재 에셋 수치로 대체하지 않습니다.
            const FRunSkillBalance* Balance = Item.GrantedSkillBalances.Num() == Item.GrantedSkills.Num() ? &Item.GrantedSkillBalances[Index] : nullptr;
            if (!Balance || !RunSkillBalance::IsValid(*Balance) || (bResolved && !RunSkillBalance::Apply(*Balance, Resolved, Error)))
            {
                Detail.Stats = NSLOCTEXT("RunItem", "MissingFrozenSkillStats", "저장된 스킬 등급·수치 정보를 확인할 수 없습니다.");
                continue;
            }
            Detail.Name = FText::Format(NSLOCTEXT("RunItem", "DetailedSkillName", "[{0}] {1}"), RunSkillBalance::RarityName(Balance->RarityTag), Detail.Name);
            Detail.Color = RunItemPresentationInternal::SkillColor(Balance->RarityTag);
            Detail.Stats = RunItemPresentationInternal::Stats(Balance->Power, Balance->ActionPointCost, Balance->SubActionPointCost, Balance->WindupSeconds);
        }
        else if (Item.SkillBalanceVersion == 0 && Item.GrantedSkillBalances.IsEmpty() && bResolved)
        {
            Detail.Stats = RunItemPresentationInternal::Stats(Resolved.Power, Resolved.ActionPointCost, Resolved.SubActionPointCost, Resolved.WindupSeconds);
        }
        else Detail.Stats = NSLOCTEXT("RunItem", "MissingSkillStats", "스킬 수치 정보를 확인할 수 없습니다.");
    }
    return Details;
}
