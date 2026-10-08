#include "Game/Run/RunCombatRewards.h"

#include "Game/Run/RunItemRarityProbabilities.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunWeaponSkillRules.h"

namespace
{
    constexpr int32 ItemChoiceCount = 3;

    bool ResolveGoldRange(const FRunItemShopState& Shop, const FRunWeaponSkillRulesState& Rules, const TArray<int32>& GoldChoices, int32& OutMinimum, int32& OutMaximum, FText& OutError)
    {
        OutError = NSLOCTEXT("RunCombatRewards", "InvalidSource", "전투 보상의 저장된 아이템·스킬 규칙 또는 골드 범위가 올바르지 않습니다.");
        if (Shop.SchemaVersion != 1 || Rules.SchemaVersion != 1 || GoldChoices.Num() != ItemChoiceCount) return false;
        int32 Minimum = MAX_int32;
        int32 Maximum = 0;
        for (const int32 Amount : GoldChoices)
        {
            if (Amount <= 0 || Amount > 1000) return false;
            Minimum = FMath::Min(Minimum, Amount);
            Maximum = FMath::Max(Maximum, Amount);
        }
        OutMinimum = Minimum;
        OutMaximum = Maximum;
        return true;
    }
}

bool RunCombatRewards::Build(FName NodeId, const FRunItemShopState& Shop, const FRunWeaponSkillRulesState& Rules, const TArray<int32>& GoldChoices, FRandomStream& Random, FRunGoldRewardState& OutState, FText& OutError)
{
    int32 Minimum = 0;
    int32 Maximum = 0;
    if (!ResolveGoldRange(Shop, Rules, GoldChoices, Minimum, Maximum, OutError)) return false;
    OutError = NSLOCTEXT("RunCombatRewards", "MissingNode", "전투 보상을 생성할 노드가 유효하지 않습니다.");
    if (NodeId.IsNone()) return false;
    // Use equippable items from the frozen pool independently of the last visited shop's profile.
    // 마지막 방문 상점 조건과 무관하게 저장된 풀의 장착 가능한 아이템으로 전투 보상을 생성합니다.
    FRandomStream RewardRandom = Random;
    const FGameplayTagQuery Query = FGameplayTagQuery::MakeQuery_MatchTag(RunItemShopCatalog::GetWeaponTag());
    TArray<int32> Indices;
    if (!RunItemRarityProbabilities::Select(Shop.Catalog, Shop.RarityProbabilities, Query, ItemChoiceCount, false, RewardRandom, Indices, OutError, &Rules, true)) return false;
    FRunGoldRewardState State;
    State.SchemaVersion = 2;
    State.NodeId = NodeId;
    for (const int32 Index : Indices)
    {
        FRunItemDefinition Item;
        if (!RunWeaponSkillRules::Generate(Shop.Catalog[Index], Rules, RewardRandom, Item, OutError)) return false;
        State.ItemChoices.Add(MoveTemp(Item));
    }
    State.BonusGold = RewardRandom.RandRange(Minimum, Maximum);
    if (!Validate(State, Shop, Rules, GoldChoices, OutError)) return false;
    OutState = MoveTemp(State);
    Random = RewardRandom;
    return true;
}

bool RunCombatRewards::Validate(const FRunGoldRewardState& State, const FRunItemShopState& Shop, const FRunWeaponSkillRulesState& Rules, const TArray<int32>& GoldChoices, FText& OutError)
{
    int32 Minimum = 0;
    int32 Maximum = 0;
    if (!ResolveGoldRange(Shop, Rules, GoldChoices, Minimum, Maximum, OutError)) return false;
    const auto Fail = [&OutError]()
    {
        OutError = NSLOCTEXT("RunCombatRewards", "InvalidReward", "저장된 전투 보상의 아이템·등급·스킬 또는 골드가 고정된 규칙과 일치하지 않습니다.");
        return false;
    };
    if (State.SchemaVersion != 2 || State.NodeId.IsNone() || !State.GoldChoices.IsEmpty() || State.ItemChoices.Num() != ItemChoiceCount || State.BonusGold < Minimum || State.BonusGold > Maximum) return Fail();
    const FGameplayTagQuery Query = FGameplayTagQuery::MakeQuery_MatchTag(RunItemShopCatalog::GetWeaponTag());
    TArray<int32> EligibleIndices;
    if (!RunItemRarityProbabilities::GetEligibleIndices(Shop.Catalog, Shop.RarityProbabilities, Query, EligibleIndices, OutError, &Rules)) return false;
    TSet<FSoftObjectPath> Assets;
    TSet<FGuid> ItemIds;
    for (const FRunItemDefinition& Item : State.ItemChoices)
    {
        const int32 CatalogIndex = Shop.Catalog.IndexOfByPredicate([&Item](const FRunItemDefinition& Base) { return Base.Asset == Item.Asset; });
        if (CatalogIndex == INDEX_NONE || !EligibleIndices.Contains(CatalogIndex) || !RunItemShopCatalog::IsSameBaseDefinition(Shop.Catalog[CatalogIndex], Item) || !RunItemShopCatalog::ValidateItem(Item) || Item.GenerationVersion != 1 || Assets.Contains(Item.Asset) || ItemIds.Contains(Item.ItemInstanceId)) return Fail();
        if (!RunWeaponSkillRules::ValidateGeneratedCopy(Item, Rules, OutError)) return false;
        Assets.Add(Item.Asset);
        ItemIds.Add(Item.ItemInstanceId);
    }
    OutError = FText::GetEmpty();
    return true;
}
