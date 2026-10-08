#include "Game/Run/RunEncounterTypes.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EncounterSkillShop, "Encounter.Shop.Skill");
// Keep retired save tags readable while resolving them to the ordinary skill shop.
// 제거된 상점 태그의 저장 호환을 유지하면서 일반 스킬상점으로 해석합니다.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RetiredTestSkillShop, "Encounter.Shop.Skill.Test");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EncounterItemShop, "Encounter.Shop.Item");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EncounterBasicItemShop, "Encounter.Shop.Item.Basic");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EncounterRarityItemShop, "Encounter.Shop.Item.Rarity");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EncounterTagItemShop, "Encounter.Shop.Item.Tag");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EncounterRecovery, "Encounter.Service.Recovery");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EncounterRevival, "Encounter.Service.Revival");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EncounterConsumables, "Encounter.Shop.Consumable");

FGameplayTag FRunEncounterOffer::GetRecoveryTag() { return TAG_EncounterRecovery; }
FGameplayTag FRunEncounterOffer::GetBasicItemShopTag()
{
    return TAG_EncounterBasicItemShop;
}

FGameplayTag FRunEncounterOffer::GetRarityItemShopTag()
{
    return TAG_EncounterRarityItemShop;
}

FGameplayTag FRunEncounterOffer::GetTagItemShopTag()
{
    return TAG_EncounterTagItemShop;
}
FGameplayTag FRunEncounterOffer::GetRevivalTag() { return TAG_EncounterRevival; }
FGameplayTag FRunEncounterOffer::GetConsumableShopTag() { return TAG_EncounterConsumables; }

bool FRunEncounterOffer::IsService() const
{
    return Type == ERunEncounterType::Shop && (GetResolvedTag().MatchesTag(GetRecoveryTag()) || GetResolvedTag().MatchesTag(GetRevivalTag()) || GetResolvedTag().MatchesTag(GetConsumableShopTag()));
}

bool FRunEncounterOffer::IsSupportedEncounter() const
{
    return IsSupportedShop() || IsService();
}

FGameplayTag FRunEncounterOffer::GetSkillShopTag()
{
    return TAG_EncounterSkillShop;
}

FGameplayTag FRunEncounterOffer::GetItemShopTag()
{
    return TAG_EncounterItemShop;
}

FGameplayTag FRunEncounterOffer::GetResolvedTag() const
{
    if (EncounterTag.MatchesTag(TAG_RetiredTestSkillShop)) return GetSkillShopTag();
    if (EncounterTag.IsValid()) return EncounterTag;
    // Resolve only legacy untagged definitions by their historical identifier.
    // 태그가 없는 기존 정의만 과거 식별자로 해석합니다.
    if (Type == ERunEncounterType::Shop) return EncounterId == TEXT("Shop_02") ? GetItemShopTag() : GetSkillShopTag();
    return FGameplayTag();
}

FText FRunEncounterOffer::GetDisplayName() const
{
    if (EncounterTag.MatchesTag(TAG_RetiredTestSkillShop)) return NSLOCTEXT("RunEncounter", "RetiredTestShop", "스킬상점");
    // Rename the historical default labels while preserving authored names and saved identifiers.
    // 과거 기본 표시명만 바꾸고 사용자가 작성한 이름과 저장 식별자는 보존합니다.
    if (EncounterId == TEXT("Shop_01") && DisplayName.ToString() == TEXT("상점1")) return NSLOCTEXT("RunEncounter", "SkillShop", "스킬상점");
    if (EncounterId == TEXT("Shop_02") && DisplayName.ToString() == TEXT("상점2")) return NSLOCTEXT("RunEncounter", "ItemShop", "아이템상점");
    return DisplayName;
}

bool FRunEncounterOffer::IsItemShop() const
{
    return Type == ERunEncounterType::Shop && GetResolvedTag().MatchesTag(GetItemShopTag());
}

bool FRunEncounterOffer::IsSupportedShop() const
{
    return Type == ERunEncounterType::Shop && (IsItemShop() || GetResolvedTag().MatchesTag(GetSkillShopTag()));
}

const FRunEncounterOffer* FRunEncounterProgress::FindSelectedOffer() const
{
    return SelectedEncounterId.IsNone() ? nullptr : Offers.FindByPredicate([this](const FRunEncounterOffer& Offer) { return Offer.EncounterId == SelectedEncounterId; });
}

bool FRunEncounterProgress::IsItemShop() const
{
    const FRunEncounterOffer* Offer = FindSelectedOffer();
    return Offer && Offer->IsItemShop();
}
