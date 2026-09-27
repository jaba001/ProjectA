#include "DataAsset/RunEncounterPoolDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"

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
        if (Offer.IsTestSkillShop() && FixedTestSkillOffers.IsEmpty())
        {
            OutError = NSLOCTEXT("RunSkillShop", "MissingTestCatalog", "테스트 스킬 상점에는 무료 스킬 상품 목록이 필요합니다.");
            return false;
        }
        Ids.Add(Offer.EncounterId);
    }
    OutOffers = FixedOffers;
    for (FRunEncounterOffer& Offer : OutOffers)
    {
        Offer.EncounterTag = Offer.GetResolvedTag();
        Offer.DisplayName = Offer.GetDisplayName();
    }
    OutError = FText::GetEmpty();
    return true;
}

bool URunEncounterPoolDataAsset::ValidateSkillShop(const FRunSkillShopState& State, FText& OutError)
{
    OutError = NSLOCTEXT("RunSkillShop", "InvalidCatalog", "스킬 상점의 상품·가격·스킬 데이터가 유효하지 않습니다.");
    if (State.SchemaVersion == 0)
    {
        if (!State.Offers.IsEmpty() || !State.TestOffers.IsEmpty()) return false;
        OutError = FText::GetEmpty();
        return true;
    }
    if (State.SchemaVersion != 1 || State.Offers.IsEmpty() || State.Offers.Num() > 32 || State.TestOffers.Num() > 1024 || State.Recovery.Price <= 0) return false;
    for (const bool bTestShop : {false, true})
    {
        TSet<FName> OfferIds;
        OfferIds.Add(FRunSkillShopState::GetRecoveryOfferId());
        TSet<FName> SkillIds;
        for (const FRunSkillShopOffer& Offer : State.GetOffers(bTestShop))
        {
            const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Offer.Skill.TryLoad());
            FCombatRoundSkill Definition;
            FText SkillError;
            if (Offer.OfferId.IsNone() || OfferIds.Contains(Offer.OfferId) || (bTestShop ? Offer.Price != 0 : Offer.Price <= 0) || Offer.DisplayName.IsEmpty() || !IsValid(Skill) || !Skill->ResolveRoundSkill(Definition, SkillError) || SkillIds.Contains(Definition.SkillId)) return false;
            OfferIds.Add(Offer.OfferId);
            SkillIds.Add(Definition.SkillId);
        }
    }
    OutError = FText::GetEmpty();
    return true;
}

bool URunEncounterPoolDataAsset::BuildSkillShop(FRunSkillShopState& OutState, FText& OutError) const
{
    OutError = NSLOCTEXT("RunSkillShop", "InvalidStartingGold", "시작 골드는 0 이상이어야 합니다.");
    if (StartingGold < 0) return false;
    FRunSkillShopState State;
    State.SchemaVersion = 1;
    State.Offers = FixedSkillOffers;
    State.TestOffers = FixedTestSkillOffers;
    State.Recovery = Recovery;
    if (FixedOffers.ContainsByPredicate([](const FRunEncounterOffer& Offer) { return Offer.IsTestSkillShop(); }) && State.TestOffers.IsEmpty())
    {
        OutError = NSLOCTEXT("RunSkillShop", "MissingTestCatalog", "테스트 스킬 상점에는 무료 스킬 상품 목록이 필요합니다.");
        return false;
    }
    for (TArray<FRunSkillShopOffer>* Offers : {&State.Offers, &State.TestOffers})
    {
        for (FRunSkillShopOffer& Offer : *Offers)
        {
            const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Offer.Skill.TryLoad());
            FCombatRoundSkill Definition;
            if (!IsValid(Skill))
            {
                OutError = FText::Format(NSLOCTEXT("RunSkillShop", "MissingProductSkill", "상품 스킬 에셋을 불러올 수 없습니다: {0}"), FText::FromString(Offer.Skill.ToString()));
                return false;
            }
            if (!Skill->ResolveRoundSkill(Definition, OutError)) return false;
            Offer.DisplayName = Definition.Name;
            Offer.Description = FText::Format(NSLOCTEXT("RunSkillShop", "SkillDetails", "{0}\nAP {1} · 위력 {2}"), Skill->SkillDescription, FText::AsNumber(Definition.ActionPointCost), FText::AsNumber(Definition.Power));
        }
    }
    if (!ValidateSkillShop(State, OutError)) return false;
    OutState = MoveTemp(State);
    return true;
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult URunEncounterPoolDataAsset::IsDataValid(FDataValidationContext& Context) const
{
    bool bValid = Super::IsDataValid(Context) != EDataValidationResult::Invalid;
    FText Error;
    TArray<FRunEncounterOffer> Offers;
    FRunSkillShopState Shop;
    if (!BuildFixedOffers(Offers, Error))
    {
        Context.AddError(Error);
        bValid = false;
    }
    if (!BuildSkillShop(Shop, Error))
    {
        Context.AddError(Error);
        bValid = false;
    }
    if (!ValidateGoldRewardRange(Error))
    {
        Context.AddError(Error);
        bValid = false;
    }
    return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
