#include "SkillAttackTagLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "DataAsset/RunWeaponSkillRulesDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "FileHelpers.h"
#include "GAS/CombatGameplayTags.h"

bool USkillAttackTagLibrary::TagSkillAttackCategories(bool bSave, FString& OutReport)
{
    // Magic follows the authored weapon pool so bow skills with elements never gain Intelligence.
    // 마법은 작성된 무기 풀을 따르므로 속성이 있는 활 스킬은 지능을 받지 않습니다.
    FGameplayTagContainer StaffTags(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.StaffWand")));
    FGameplayTagContainer SpellbookTags(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Spellbook")));
    TSet<FName> MagicPackages;
    for (const FRunWeaponSkillCandidate& Candidate : GetDefault<URunWeaponSkillRulesDataAsset>()->Rules.Candidates)
    {
        if (Candidate.AllowedItemQuery.Matches(StaffTags) || Candidate.AllowedItemQuery.Matches(SpellbookTags)) MagicPackages.Add(FName(*Candidate.Skill.GetLongPackageName()));
    }

    TArray<FAssetData> Assets;
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.GetAssetsByPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills"), Assets, true);
    TArray<UPackage*> Packages;
    int32 Melee = 0;
    int32 Magic = 0;
    int32 Skipped = 0;
    for (const FAssetData& Data : Assets)
    {
        USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Data.GetAsset());
        if (!Skill) continue;
        if (!Skill->bUseRoundDefinition)
        {
            ++Skipped;
            continue;
        }
        FGameplayTagContainer& Tags = Skill->RoundDefinition.EffectTags;
        // Heal and shield skills such as consumables share the default melee kind but deal no direct damage.
        // 소모품 같은 회복·보호막 스킬은 기본 근접 실행 종류를 쓰지만 직접 피해가 없으므로 제외합니다.
        const bool bSupport = Tags.HasTag(ProjectACombatTags::Skill_Effect_Heal) || Tags.HasTag(ProjectACombatTags::Skill_Effect_Shield);
        const bool bMelee = !bSupport && Skill->RoundDefinition.Kind == ECombatRoundSkillKind::Melee;
        const bool bMagic = !bSupport && !bMelee && MagicPackages.Contains(Data.PackageName);
        const FGameplayTag Category = bMelee ? ProjectACombatTags::Skill_Attack_Melee : bMagic ? ProjectACombatTags::Skill_Attack_Magic : FGameplayTag();
        if (bMelee) ++Melee;
        if (bMagic) ++Magic;
        if (!Category.IsValid() || Tags.HasTagExact(Category)) continue;
        Skill->Modify();
        Tags.AddTag(Category);
        Skill->MarkPackageDirty();
        Packages.Add(Skill->GetOutermost());
    }
    OutReport = FString::Printf(TEXT("Melee=%d Magic=%d Changed=%d SkippedLegacy=%d"), Melee, Magic, Packages.Num(), Skipped);
    return !bSave || Packages.IsEmpty() || UEditorLoadingAndSavingUtils::SavePackages(Packages, false);
}
