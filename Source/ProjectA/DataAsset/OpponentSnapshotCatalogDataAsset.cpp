#include "DataAsset/OpponentSnapshotCatalogDataAsset.h"

#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "Unit/EnemyUnit.h"
#include "Unit/CharacterAppearanceComponent.h"
#include "DataAsset/CharacterAppearanceCatalog.h"

bool UOpponentSnapshotCatalogDataAsset::MatchesSavedUnitClass(const FPartySnapshotMember& Member, const FSoftObjectPath& SavedClass) const
{
    const TSubclassOf<AEnemyUnit> CurrentClass = EnemyClasses.FindRef(Member.ClassId);
    if (!CurrentClass || CurrentClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) return false;
    if (SavedClass == FSoftObjectPath(CurrentClass.Get())) return true;
    if (!Member.Appearance.IsEmpty()) return false;
    const TSubclassOf<AEnemyUnit> LegacyClass = LegacyEnemyClasses.FindRef(Member.ClassId);
    return LegacyClass && !LegacyClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) && SavedClass == FSoftObjectPath(LegacyClass.Get());
}

bool UOpponentSnapshotCatalogDataAsset::ResolveSkills(const FPartySnapshotMember& Member, TArray<TObjectPtr<USkillDefinitionDataAsset>>& OutSkills, FText& OutError) const
{
    TArray<TObjectPtr<USkillDefinitionDataAsset>> Resolved;
    TSet<FPrimaryAssetId> AssetIds;
    if (Member.SkillIds.IsEmpty() || Member.SkillIds.Num() > 5)
    {
        OutError = NSLOCTEXT("Snapshot", "SkillCount", "Snapshot requires 1–5 ordered skills. / 스킬은 순서가 있는 1~5개여야 합니다.");
        return false;
    }
    for (FName SkillId : Member.SkillIds)
    {
        USkillDefinitionDataAsset* Skill = Skills.FindRef(SkillId);
        if (!IsValid(Skill))
        {
            OutError = FText::Format(NSLOCTEXT("Snapshot", "UnknownSkill", "Unsupported or duplicate skill: {0}. / 미지원 또는 중복 스킬: {0}."), FText::FromName(SkillId));
            return false;
        }
        FCombatRoundSkill RoundSkill;
        if (!Skill->ResolveRoundSkill(RoundSkill, OutError)) return false;
        const FPrimaryAssetId AssetId = Skill->GetPrimaryAssetId();
        if (AssetIds.Contains(AssetId))
        {
            OutError = FText::Format(NSLOCTEXT("Snapshot", "DuplicateSkillAsset", "Catalog identifiers resolve to the same skill asset: {0}. / 카탈로그 식별자가 동일한 스킬 에셋을 가리킵니다: {0}."), FText::FromName(SkillId));
            return false;
        }
        // Snapshot aliases cannot duplicate one skill, while distinct round data may share a legacy ability class.
        // Snapshot 별칭은 같은 스킬을 중복하지 못하며 서로 다른 라운드 데이터는 기존 어빌리티 클래스를 공유할 수 있습니다.
        AssetIds.Add(AssetId);
        Resolved.Add(Skill);
    }
    OutSkills = MoveTemp(Resolved);
    OutError = FText::GetEmpty();
    return true;
}

bool UOpponentSnapshotCatalogDataAsset::ValidateForEncounter(const FPartySnapshot& Snapshot, int32 FormationSlotCount, FText& OutError) const
{
    if (!UPartySnapshotLibrary::ValidateSnapshot(Snapshot, OutError))
    {
        return false;
    }
    if (ContentVersion <= 0 || Snapshot.ContentVersion != ContentVersion)
    {
        OutError = NSLOCTEXT("Snapshot", "ContentVersion", "Snapshot content version is incompatible. / Snapshot 콘텐츠 버전이 호환되지 않습니다.");
        return false;
    }
    for (const FPartySnapshotMember& Member : Snapshot.Members)
    {
        const TSubclassOf<AEnemyUnit> EnemyClass = EnemyClasses.FindRef(Member.ClassId);
        if (!EnemyClass || EnemyClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            OutError = FText::Format(NSLOCTEXT("Snapshot", "UnknownClass", "Unknown opponent class: {0}. / 알 수 없는 상대 직업: {0}."), FText::FromName(Member.ClassId));
            return false;
        }
        const AEnemyUnit* Defaults = EnemyClass->GetDefaultObject<AEnemyUnit>();
        const UCharacterAppearanceCatalog* AppearanceCatalog = Defaults->CharacterAppearance ? Defaults->CharacterAppearance->AppearanceCatalog.Get() : nullptr;
        if (AppearanceCatalog)
        {
            if (!AppearanceCatalog->ValidateSelection(Member.Appearance, OutError)) return false;
        }
        else if (!Member.Appearance.ItemIds.IsEmpty())
        {
            OutError = NSLOCTEXT("Snapshot", "UnsupportedAppearance", "상대 직업에서 사용할 수 없는 의상 선택입니다.");
            return false;
        }
        // Future equipment and tactics must not silently produce a different opponent build.
        // 후속 장비와 전술을 조용히 무시해 다른 상대 빌드가 생성되지 않도록 합니다.
        if (!Member.EquipmentIds.IsEmpty() || !Member.TacticsId.IsNone())
        {
            OutError = NSLOCTEXT("Snapshot", "UnsupportedBuild", "Equipment and custom tactics are not supported yet. / 장비와 사용자 전술은 아직 지원하지 않습니다.");
            return false;
        }
        if (Member.Stats.CurrentHP <= 0.0f || Member.FormationSlot < 0 || Member.FormationSlot >= FormationSlotCount)
        {
            OutError = NSLOCTEXT("Snapshot", "InvalidPlacement", "Opponent requires positive HP and a valid arena formation slot. / 상대에게 양수 HP와 유효한 아레나 배치 슬롯이 필요합니다.");
            return false;
        }
        TArray<TObjectPtr<USkillDefinitionDataAsset>> Resolved;
        if (!ResolveSkills(Member, Resolved, OutError))
        {
            return false;
        }
    }
    OutError = FText::GetEmpty();
    return true;
}
