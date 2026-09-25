#include "Game/Run/RunEncounterTypes.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EncounterSkillShop, "Encounter.Shop.Skill");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EncounterItemShop, "Encounter.Shop.Item");

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
    if (EncounterTag.IsValid()) return EncounterTag;
    // Resolve only legacy untagged definitions by their historical identifier.
    // 태그가 없는 기존 정의만 과거 식별자로 해석합니다.
    if (Type == ERunEncounterType::Shop) return EncounterId == TEXT("Shop_02") ? GetItemShopTag() : GetSkillShopTag();
    return FGameplayTag();
}

FText FRunEncounterOffer::GetDisplayName() const
{
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
