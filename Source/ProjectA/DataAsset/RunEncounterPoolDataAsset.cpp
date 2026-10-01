#include "DataAsset/RunEncounterPoolDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "DataAsset/SkillPoolDataAsset.h"
#include "Types/GameplayTagCandidateSelection.h"

bool URunEncounterPoolDataAsset::ValidateGoldRewardRange(FText& OutError) const
{
    OutError = NSLOCTEXT("RunGoldReward", "InvalidRange", "골드 보상 범위는 1 이상이며 최소값이 최대값보다 크지 않아야 합니다.");
    if (GoldRewardMin <= 0 || GoldRewardMax < GoldRewardMin) return false;
    OutError = FText::GetEmpty();
    return true;
}

bool URunEncounterPoolDataAsset::BuildGoldRewards(FName NodeId, FRunGoldRewardState& OutState, FText& OutError) const
{
    if (!ValidateGoldRewardRange(OutError)) return false;
    OutError = NSLOCTEXT("RunGoldReward", "MissingNode", "골드 보상을 생성할 전투 노드가 필요합니다.");
    if (NodeId.IsNone()) return false;
    FRunGoldRewardState State;
    State.SchemaVersion = 1;
    State.NodeId = NodeId;
    // Prototype offers roll independently; duplicate amounts are valid choices.
    // 시험용 선택지는 독립적으로 추첨하며 같은 금액도 유효한 선택지입니다.
    for (int32 Index = 0; Index < 3; ++Index) State.GoldChoices.Add(FMath::RandRange(GoldRewardMin, GoldRewardMax));
    OutState = MoveTemp(State);
    OutError = FText::GetEmpty();
    return true;
}

URunEncounterPoolDataAsset::URunEncounterPoolDataAsset()
{
    SkillShopPool = TSoftObjectPtr<USkillPoolDataAsset>(FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/SkillPools/DA_SkillPool_Catalog.DA_SkillPool_Catalog")));
    for (int32 Index = 1; Index <= 3; ++Index)
    {
        FRunEncounterOffer& Offer = FixedOffers.AddDefaulted_GetRef();
        Offer.EncounterId = FName(*FString::Printf(TEXT("Shop_%02d"), Index));
        Offer.DisplayName = FText::FromString(FString::Printf(TEXT("상점%d"), Index));
        Offer.EncounterTag = Index == 2 ? FRunEncounterOffer::GetItemShopTag() : FRunEncounterOffer::GetSkillShopTag();
        Offer.DisplayName = Offer.GetDisplayName();
    }
    for (const TCHAR* AssetName : {TEXT("BPDA_swoard_attack"), TEXT("BPDA_RangedAttack"), TEXT("BPDA_AreaAttack"), TEXT("BPDA_SweepingStrike")})
    {
        FRunSkillShopOffer& Offer = FixedSkillOffers.AddDefaulted_GetRef();
        Offer.OfferId = FName(AssetName);
        Offer.Skill = FSoftObjectPath(FString::Printf(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/%s.%s"), AssetName, AssetName));
    }
}

bool URunEncounterPoolDataAsset::BuildFixedOffers(TArray<FRunEncounterOffer>& OutOffers, FText& OutError) const
{
    OutError = NSLOCTEXT("RunEncounter", "InvalidPool", "시험용 인카운터는 서로 다른 ID와 이름을 가진 상점 3개가 필요합니다.");
    if (FixedOffers.Num() != 3) return false;
    TSet<FName> Ids;
    for (const FRunEncounterOffer& Offer : FixedOffers)
    {
        if (Offer.EncounterId.IsNone() || Ids.Contains(Offer.EncounterId) || Offer.DisplayName.ToString().TrimStartAndEnd().IsEmpty() || !Offer.IsSupportedShop()) return false;
        Ids.Add(Offer.EncounterId);
    }
    OutOffers = FixedOffers;
    for (FRunEncounterOffer& Offer : OutOffers)
    {
        Offer.DisplayName = Offer.GetDisplayName();
        Offer.EncounterTag = Offer.GetResolvedTag();
    }
    OutError = FText::GetEmpty();
    return true;
}

bool URunEncounterPoolDataAsset::ValidateSkillShop(const FRunSkillShopState& State, FText& OutError)
{
    OutError = NSLOCTEXT("RunSkillShop", "InvalidCatalog", "스킬 상점의 상품·가격·스킬 데이터가 유효하지 않습니다.");
    if (State.SchemaVersion == 0)
    {
        if (!State.Offers.IsEmpty() || !State.Catalog.IsEmpty() || State.Revision != 0 || State.RerollPrice != 1 || !State.Query.IsEmpty()) return false;
        OutError = FText::GetEmpty();
        return true;
    }
    if (State.SchemaVersion != 1 || State.Recovery.Price <= 0 || State.Revision < 0 || State.RerollPrice <= 0) return false;
    const bool bLegacy = State.Catalog.IsEmpty();
    if (bLegacy && (State.Revision != 0 || State.RerollPrice != 1 || !State.Query.IsEmpty() || State.Offers.IsEmpty() || State.Offers.Num() > 32)) return false;
    if (!bLegacy && (State.Catalog.Num() < FRunSkillShopState::OfferCount || State.Catalog.Num() > 512 || State.Revision <= 0 || State.RerollPrice > State.Revision || State.Offers.Num() != FRunSkillShopState::OfferCount)) return false;
    const auto ValidateProducts = [bLegacy](const TArray<FRunSkillShopOffer>& Products)
    {
        TSet<FName> OfferIds{FRunSkillShopState::GetRecoveryOfferId(), FRunSkillShopState::GetRerollOfferId()};
        TSet<FName> SkillIds;
        for (const FRunSkillShopOffer& Offer : Products)
        {
            const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Offer.Skill.TryLoad());
            FCombatRoundSkill Definition;
            FText SkillError;
            if (Offer.OfferId.IsNone() || OfferIds.Contains(Offer.OfferId) || Offer.Price <= 0 || Offer.DisplayName.IsEmpty() || !IsValid(Skill) || !Skill->ResolveRoundSkill(Definition, SkillError) || SkillIds.Contains(Definition.SkillId)) return false;
            if (!bLegacy && (!FMath::IsFinite(Offer.BaseWeight) || Offer.BaseWeight < 0.0f || Offer.Tags != Definition.EffectTags)) return false;
            OfferIds.Add(Offer.OfferId);
            SkillIds.Add(Definition.SkillId);
        }
        return true;
    };
    if (!ValidateProducts(State.Offers) || (!bLegacy && !ValidateProducts(State.Catalog))) return false;
    for (const FRunSkillShopOffer& Offer : State.Offers)
    {
        if (bLegacy) break;
        const FRunSkillShopOffer* Candidate = State.Catalog.FindByPredicate([&Offer](const FRunSkillShopOffer& Product) { return Product.Skill == Offer.Skill; });
        if (!Candidate || Candidate->Price != Offer.Price || Candidate->DisplayName.ToString() != Offer.DisplayName.ToString() || Candidate->Description.ToString() != Offer.Description.ToString() || Candidate->Tags != Offer.Tags || Candidate->BaseWeight != Offer.BaseWeight || Offer.BaseWeight <= 0.0f || (!State.Query.IsEmpty() && !State.Query.Matches(Offer.Tags))) return false;
    }
    OutError = FText::GetEmpty();
    return true;
}

bool URunEncounterPoolDataAsset::RollSkillShop(FRunSkillShopState& State, bool bResetRerollPrice, FText& OutError)
{
    OutError = NSLOCTEXT("RunSkillShop", "CannotReroll", "현재 스킬 상점의 상품을 갱신할 수 없습니다.");
    if (State.SchemaVersion != 1 || State.Catalog.IsEmpty() || State.Revision < 0 || State.Revision == MAX_int32 || State.RerollPrice <= 0 || (!bResetRerollPrice && (State.Revision == 0 || State.RerollPrice == MAX_int32))) return false;
    if (State.Revision > 0 && !ValidateSkillShop(State, OutError)) return false;
    TArray<FGameplayTagWeightedCandidate> Candidates;
    Candidates.Reserve(State.Catalog.Num());
    for (const FRunSkillShopOffer& Offer : State.Catalog)
    {
        FGameplayTagWeightedCandidate& Candidate = Candidates.AddDefaulted_GetRef();
        Candidate.Tags = Offer.Tags;
        Candidate.BaseWeight = Offer.BaseWeight;
    }
    FRandomStream Random(FMath::Rand());
    TArray<int32> SelectedIndices;
    if (!GameplayTagCandidateSelection::Select(Candidates, State.Query, FRunSkillShopState::OfferCount, false, Random, SelectedIndices))
    {
        OutError = NSLOCTEXT("RunSkillShop", "InsufficientCandidates", "태그 조건을 만족하는 스킬 상점 후보가 5개 이상 필요합니다.");
        return false;
    }
    FRunSkillShopState Updated = State;
    Updated.Offers.Reset();
    for (const int32 Index : SelectedIndices)
    {
        FRunSkillShopOffer Offer = State.Catalog[Index];
        Offer.OfferId = FName(*FString::Printf(TEXT("Skill_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        Updated.Offers.Add(MoveTemp(Offer));
    }
    ++Updated.Revision;
    Updated.RerollPrice = bResetRerollPrice ? 1 : State.RerollPrice + 1;
    if (!ValidateSkillShop(Updated, OutError)) return false;
    State = MoveTemp(Updated);
    return true;
}

bool URunEncounterPoolDataAsset::BuildSkillShop(FRunSkillShopState& OutState, FText& OutError) const
{
    OutError = NSLOCTEXT("RunSkillShop", "InvalidStartingGold", "시작 골드는 0 이상이어야 합니다.");
    if (StartingGold < 0) return false;
    FRunSkillShopState State;
    State.SchemaVersion = 1;
    State.Catalog = FixedSkillOffers;
    State.Query = SkillShopQuery;
    State.Recovery = Recovery;
    if (!SkillShopPool.IsNull())
    {
        const USkillPoolDataAsset* Pool = SkillShopPool.LoadSynchronous();
        OutError = NSLOCTEXT("RunSkillShop", "MissingPool", "스킬 상점의 후보 풀을 불러올 수 없습니다.");
        if (!IsValid(Pool)) return false;
        for (const FSkillPoolEntry& Entry : Pool->Entries)
        {
            if (!IsValid(Entry.Skill) || Entry.Weight < 0) return false;
            FRunSkillShopOffer& Offer = State.Catalog.AddDefaulted_GetRef();
            Offer.OfferId = Entry.Skill->GetFName();
            Offer.Skill = FSoftObjectPath(Entry.Skill);
            Offer.BaseWeight = Entry.Weight;
        }
    }
    for (FRunSkillShopOffer& Offer : State.Catalog)
    {
        const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Offer.Skill.TryLoad());
        FCombatRoundSkill Definition;
        if (!IsValid(Skill))
        {
            OutError = FText::Format(NSLOCTEXT("RunSkillShop", "MissingProductSkill", "상품 스킬 에셋을 불러올 수 없습니다: {0}"), FText::FromString(Offer.Skill.ToString()));
            return false;
        }
        if (!Skill->ResolveRoundSkill(Definition, OutError)) return false;
        Offer.Tags = Definition.EffectTags;
        Offer.DisplayName = Definition.Name;
        Offer.Description = FText::Format(NSLOCTEXT("RunSkillShop", "SkillDetails", "{0}\nAP {1} · 위력 {2}"), Skill->SkillDescription, FText::AsNumber(Definition.ActionPointCost), FText::AsNumber(Definition.Power));
    }
    if (!RollSkillShop(State, true, OutError)) return false;
    OutState = MoveTemp(State);
    return true;
}
