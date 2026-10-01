#include "Game/Run/RunContentMigration.h"
#include "Game/Run/RunSaveGame.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Types/GameplayTagCandidateSelection.h"

bool RunContentMigration::IsRemovedSkill(const FSoftObjectPath& Path)
{
    return Path == FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_SweepingStrike.BPDA_SweepingStrike")) || Path == FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DA_SweepingStrike.DA_SweepingStrike")) || Path == FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_SweepingStrike.DA_SweepingStrike"));
}

bool RunContentMigration::IsRemovedSkillId(FName SkillId)
{
    return SkillId == TEXT("SweepingStrike") || SkillId == TEXT("SkillDefinitionDataAsset:DA_SweepingStrike") || SkillId == TEXT("SkillDefinitionDataAsset:BPDA_SweepingStrike");
}

bool RunContentMigration::RemoveDeletedSkills(URunSaveGame& Save, FText& OutError)
{
    for (FRunPartyMember& Member : Save.Party)
    {
        const int32 Removed = Member.Skills.RemoveAll([](const FSoftObjectPath& Path) { return IsRemovedSkill(Path); });
        if (Removed > 0 && Member.bHasSkillLoadout && Member.Skills.IsEmpty())
        {
            OutError = NSLOCTEXT("RunContentMigration", "EmptyLoadout", "휩쓸기 제거 후 보유 스킬이 없는 캐릭터는 현재 저장 규칙으로 재개할 수 없습니다. 원본 저장을 유지합니다.");
            return false;
        }
    }
    FCombatCheckpointData& Checkpoint = Save.CombatCheckpoint;
    bool bCheckpointChanged = false;
    for (FCombatCheckpointUnit& Unit : Checkpoint.Units)
    {
        if (Unit.Skills.RemoveAll([](const FSoftObjectPath& Path) { return IsRemovedSkill(Path); }) == 0) continue;
        bCheckpointChanged = true;
        // Refresh default metadata after removing its skill; shared ability assets remain available.
        // 스킬 제거 후 기본 메타데이터를 갱신하며 공유 Ability 에셋은 유지합니다.
        const USkillDefinitionDataAsset* First = Unit.Skills.IsEmpty() ? nullptr : Cast<USkillDefinitionDataAsset>(Unit.Skills[0].TryLoad());
        Unit.DefaultAttackAbility = First ? FSoftObjectPath(First->AbilityClass.Get()) : FSoftObjectPath();
    }
    for (FPartySnapshotMember& Member : Checkpoint.OpponentSnapshot.Members) bCheckpointChanged |= Member.SkillIds.RemoveAll([](FName Id) { return IsRemovedSkillId(Id); }) > 0;
    for (FCombatCheckpointRoundPlan& Plan : Checkpoint.RoundPlans)
    {
        if (!IsRemovedSkillId(Plan.Command.SkillId)) continue;
        // Cancel only the deleted skill reservation before any cost is paid, preserving independent movement.
        // 비용 차감 전에 삭제 스킬 예약만 취소하며 독립된 이동 예약은 보존합니다.
        Plan.Command = FCombatRoundCommand();
        Plan.Command.UnitId = Plan.UnitId;
        const FCombatCheckpointUnit* Unit = Checkpoint.Units.FindByPredicate([&Plan](const FCombatCheckpointUnit& Saved) { return Saved.RoundUnitId == Plan.UnitId; });
        if (Unit && !Unit->bDead && Unit->Team == ETeam::Player && Unit->PartyControlMode == EPartyControlMode::Human) Plan.bReady = false;
        bCheckpointChanged = true;
    }
    if (bCheckpointChanged && Checkpoint.SchemaVersion == 3)
    {
        OutError = NSLOCTEXT("RunContentMigration", "PlanRevision", "휩쓸기 제거 후 저장된 준비 계획을 갱신할 수 없습니다. 원본 저장을 유지합니다.");
        if (Checkpoint.PlanRevision < 1 || Checkpoint.PlanRevision >= MAX_int32 - 1) return false;
        ++Checkpoint.PlanRevision;
    }
    FRunSkillShopState& Shop = Save.SkillShopState;
    const bool bHadCatalog = !Shop.Catalog.IsEmpty();
    const int32 RemovedOffers = Shop.Offers.RemoveAll([](const FRunSkillShopOffer& Offer) { return IsRemovedSkill(Offer.Skill); });
    const int32 RemovedCandidates = Shop.Catalog.RemoveAll([](const FRunSkillShopOffer& Offer) { return IsRemovedSkill(Offer.Skill); });
    if (RemovedOffers > 0 || RemovedCandidates > 0)
    {
        OutError = NSLOCTEXT("RunContentMigration", "ShopRepair", "삭제된 휩쓸기 상품을 현재 상점 후보로 교체할 수 없습니다. 원본 저장을 유지합니다.");
        if (Shop.SchemaVersion != 1) return false;
    }
    if (bHadCatalog && (RemovedOffers > 0 || RemovedCandidates > 0))
    {
        if (Shop.Catalog.Num() < FRunSkillShopState::OfferCount) return false;
        if (Shop.Revision <= 0 || Shop.Revision == MAX_int32) return false;
        if (RemovedOffers > 0)
        {
            TArray<FGameplayTagWeightedCandidate> Candidates;
            for (const FRunSkillShopOffer& Offer : Shop.Catalog)
            {
                FGameplayTagWeightedCandidate& Candidate = Candidates.AddDefaulted_GetRef();
                Candidate.Tags = Offer.Tags;
                Candidate.BaseWeight = Shop.Offers.ContainsByPredicate([&Offer](const FRunSkillShopOffer& Existing) { return Existing.Skill == Offer.Skill; }) ? 0.0f : Offer.BaseWeight;
            }
            // The same original save selects the same replacement without charging or rerolling retained offers.
            // 같은 원본 저장은 비용 차감이나 남은 상품 리롤 없이 동일한 대체 상품을 선택합니다.
            FRandomStream Random(static_cast<int32>(HashCombine(GetTypeHash(Save.Identity.RunId), GetTypeHash(Shop.Revision))));
            TArray<int32> Selected;
            if (!GameplayTagCandidateSelection::Select(Candidates, Shop.Query, RemovedOffers, false, Random, Selected)) return false;
            for (int32 Index : Selected)
            {
                FRunSkillShopOffer Offer = Shop.Catalog[Index];
                Offer.OfferId = FName(*FString::Printf(TEXT("SkillMigration_%s_%d_%d"), *Save.Identity.RunId.ToString(EGuidFormats::Digits), Shop.Revision, Index));
                if (Shop.Offers.ContainsByPredicate([&Offer](const FRunSkillShopOffer& Existing) { return Existing.OfferId == Offer.OfferId; })) return false;
                Shop.Offers.Add(MoveTemp(Offer));
            }
        }
        ++Shop.Revision;
    }
    OutError = FText::GetEmpty();
    return true;
}
