#include "Game/Run/RunItemShopCatalog.h"
#include "Misc/FileHelper.h"
#include "String/LexFromString.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NativeGameplayTags.h"
#include "Serialization/Csv/CsvParser.h"
#include "Types/GameplayTagCandidateSelection.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeapon, "Item.Weapon");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponSword, "Item.Weapon.Sword");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponDagger, "Item.Weapon.Dagger");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponAxe, "Item.Weapon.Axe");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponHammer, "Item.Weapon.Hammer");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponMaceClub, "Item.Weapon.MaceClub");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponSpear, "Item.Weapon.Spear");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponScythe, "Item.Weapon.Scythe");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponBow, "Item.Weapon.Bow");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponCrossbow, "Item.Weapon.Crossbow");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponStaffWand, "Item.Weapon.StaffWand");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponSpellbook, "Item.Weapon.Spellbook");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponShield, "Item.Weapon.Shield");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponGauntlet, "Item.Weapon.Gauntlet");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponFirearm, "Item.Weapon.Firearm");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponThrown, "Item.Weapon.Thrown");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponExplosive, "Item.Weapon.Explosive");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponArrowBolt, "Item.Weapon.ArrowBolt");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponBullet, "Item.Weapon.Bullet");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemWeaponOther, "Item.Weapon.Other");

namespace
{
    constexpr int32 OfferCount = 5;
    constexpr int32 MaximumCatalogSize = 4096;
    constexpr int32 MaximumDisplayNameLength = 256;

    // Validate saved display text independently of the current CSV so frozen runs keep their original names.
    // 저장된 표시 문구는 현재 CSV와 독립적으로 검증하여 기존 Run의 확정된 이름을 보존합니다.
    bool IsValidDisplayName(const FString& Name)
    {
        if (Name.Len() > MaximumDisplayNameLength || Name.TrimStartAndEnd().IsEmpty()) return false;
        for (const TCHAR Character : Name)
        {
            if (Character < TEXT(' ') || Character == 0x7f) return false;
        }
        return true;
    }

    // Translate the authored CSV categories once; runtime selection uses GameplayTagQuery.
    // 작성된 CSV 분류를 변환하고 런타임 선택은 GameplayTagQuery로 판정합니다.
    FGameplayTag FindCategoryTag(const FString& Category)
    {
        static const TMap<FString, FGameplayTag> CategoryTags =
        {
            { TEXT("검"), TAG_ItemWeaponSword },
            { TEXT("단검"), TAG_ItemWeaponDagger },
            { TEXT("도끼"), TAG_ItemWeaponAxe },
            { TEXT("망치"), TAG_ItemWeaponHammer },
            { TEXT("철퇴·곤봉"), TAG_ItemWeaponMaceClub },
            { TEXT("창"), TAG_ItemWeaponSpear },
            { TEXT("낫"), TAG_ItemWeaponScythe },
            { TEXT("활"), TAG_ItemWeaponBow },
            { TEXT("석궁"), TAG_ItemWeaponCrossbow },
            { TEXT("지팡이·완드"), TAG_ItemWeaponStaffWand },
            { TEXT("마법서"), TAG_ItemWeaponSpellbook },
            { TEXT("방패"), TAG_ItemWeaponShield },
            { TEXT("건틀릿"), TAG_ItemWeaponGauntlet },
            { TEXT("총기"), TAG_ItemWeaponFirearm },
            { TEXT("투척 무기"), TAG_ItemWeaponThrown },
            { TEXT("폭발물"), TAG_ItemWeaponExplosive },
            { TEXT("화살·볼트"), TAG_ItemWeaponArrowBolt },
            { TEXT("탄환"), TAG_ItemWeaponBullet },
            { TEXT("기타"), TAG_ItemWeaponOther }
        };
        const FGameplayTag* Tag = CategoryTags.Find(Category);
        return Tag ? *Tag : FGameplayTag();
    }

    bool ValidateCatalog(const TArray<FRunItemDefinition>& Catalog)
    {
        if (Catalog.Num() < OfferCount || Catalog.Num() > MaximumCatalogSize) return false;
        TSet<FSoftObjectPath> Assets;
        for (const FRunItemDefinition& Item : Catalog)
        {
            if (!RunItemShopCatalog::ValidateItem(Item) || Assets.Contains(Item.Asset)) return false;
            Assets.Add(Item.Asset);
        }
        return true;
    }
}

FGameplayTag RunItemShopCatalog::GetWeaponTag()
{
    return TAG_ItemWeapon;
}

bool RunItemShopCatalog::ValidateItem(const FRunItemDefinition& Item)
{
    const FString AssetPath = Item.Asset.ToString();
    return Item.Asset.IsValid() && Item.Asset.GetSubPathUtf8String().IsEmpty() && AssetPath.StartsWith(TEXT("/Game/")) && FPackageName::IsValidObjectPath(AssetPath) && IsValidDisplayName(Item.DisplayName.ToString()) && Item.Tags.HasTag(TAG_ItemWeapon) && Item.Price > 0;
}

bool RunItemShopCatalog::Load(TArray<FRunItemDefinition>& OutCatalog, FText& OutError)
{
    OutError = FText::GetEmpty();
    FString CsvText;
    if (!FFileHelper::LoadFileToString(CsvText, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Docs/WEAPON_ASSETS.csv"))))
    {
        OutError = NSLOCTEXT("RunItemShop", "MissingCatalog", "무기 에셋 CSV를 읽을 수 없습니다.");
        return false;
    }
    return LoadFromString(MoveTemp(CsvText), OutCatalog, OutError);
}

bool RunItemShopCatalog::LoadFromString(FString CsvText, TArray<FRunItemDefinition>& OutCatalog, FText& OutError)
{
    OutError = FText::GetEmpty();
    const FCsvParser Parser(MoveTemp(CsvText));
    const FCsvParser::FRows& Rows = Parser.GetRows();
    const int32 ColumnCount = Rows.IsEmpty() ? 0 : Rows[0].Num();
    const bool bHasGameName = ColumnCount == 5;
    if (Rows.Num() < OfferCount + 1 || Rows.Num() > MaximumCatalogSize + 1 || (ColumnCount != 4 && ColumnCount != 5) || FCString::Strcmp(Rows[0][0], TEXT("무기 종류")) != 0 || FCString::Strcmp(Rows[0][1], TEXT("위치")) != 0 || FCString::Strcmp(Rows[0][2], TEXT("에셋 이름")) != 0 || FCString::Strcmp(Rows[0][3], TEXT("가격(G)")) != 0 || (bHasGameName && FCString::Strcmp(Rows[0][4], TEXT("게임 내 이름")) != 0))
    {
        OutError = NSLOCTEXT("RunItemShop", "InvalidCatalogHeader", "무기 에셋 CSV의 열 또는 상품 개수가 올바르지 않습니다.");
        return false;
    }

    TArray<FRunItemDefinition> Catalog;
    Catalog.Reserve(Rows.Num() - 1);
    for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
    {
        const TArray<const TCHAR*>& Row = Rows[RowIndex];
        if (Row.Num() != ColumnCount)
        {
            OutError = FText::Format(NSLOCTEXT("RunItemShop", "InvalidCatalogRow", "무기 에셋 CSV의 {0}행이 올바르지 않습니다."), FText::AsNumber(RowIndex + 1));
            return false;
        }
        FRunItemDefinition Item;
        const FGameplayTag CategoryTag = FindCategoryTag(Row[0]);
        Item.Asset = FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), Row[1], Row[2], Row[2]));
        Item.DisplayName = FText::FromString(Row[bHasGameName ? 4 : 2]);
        Item.Tags.AddTag(TAG_ItemWeapon);
        if (CategoryTag.IsValid()) Item.Tags.AddTag(CategoryTag);
        if (!CategoryTag.IsValid() || !LexTryParseString(Item.Price, Row[3]) || !ValidateItem(Item))
        {
            OutError = FText::Format(NSLOCTEXT("RunItemShop", "InvalidCatalogItem", "무기 에셋 CSV의 {0}행 분류·경로·이름·가격이 올바르지 않습니다."), FText::AsNumber(RowIndex + 1));
            return false;
        }
        Catalog.Add(MoveTemp(Item));
    }
    if (!ValidateCatalog(Catalog))
    {
        OutError = NSLOCTEXT("RunItemShop", "DuplicateCatalogItem", "무기 에셋 CSV에 중복되거나 잘못된 상품이 있습니다.");
        return false;
    }
    OutCatalog = MoveTemp(Catalog);
    return true;
}

bool RunItemShopCatalog::Validate(const FRunItemShopState& State, FText& OutError)
{
    OutError = FText::GetEmpty();
    if (State.SchemaVersion == 0 && State.Catalog.IsEmpty() && State.Offers.IsEmpty() && State.Revision == 0 && State.RerollPrice == 1) return true;
    const auto Fail = [&OutError]()
    {
        OutError = NSLOCTEXT("RunItemShop", "InvalidShopState", "아이템 상점의 저장된 상품·가격·갱신 상태가 올바르지 않습니다.");
        return false;
    };
    if (State.SchemaVersion != 1 || !ValidateCatalog(State.Catalog) || State.Revision < 0 || State.RerollPrice <= 0) return Fail();
    if (State.Revision == 0) return State.Offers.IsEmpty() ? true : Fail();
    if (State.Offers.Num() != OfferCount) return Fail();

    TSet<FName> OfferIds;
    for (const FRunItemShopOffer& Offer : State.Offers)
    {
        if (Offer.OfferId.IsNone() || Offer.OfferId == FRunItemShopState::GetRerollOfferId() || OfferIds.Contains(Offer.OfferId) || !ValidateItem(Offer.Item)) return Fail();
        const FRunItemDefinition* CatalogItem = State.Catalog.FindByPredicate([&Offer](const FRunItemDefinition& Item) { return Item.Asset == Offer.Item.Asset; });
        if (!CatalogItem || CatalogItem->Price != Offer.Item.Price || CatalogItem->DisplayName.ToString() != Offer.Item.DisplayName.ToString() || CatalogItem->Tags != Offer.Item.Tags) return Fail();
        OfferIds.Add(Offer.OfferId);
    }
    return true;
}

bool RunItemShopCatalog::Roll(FRunItemShopState& State, bool bAllowDuplicates, const FGameplayTagQuery& Query, FText& OutError)
{
    if (!Validate(State, OutError)) return false;
    if (State.SchemaVersion != 1 || State.Revision == MAX_int32)
    {
        OutError = NSLOCTEXT("RunItemShop", "CannotRollShop", "아이템 상점의 상품을 갱신할 수 없습니다.");
        return false;
    }

    TArray<FGameplayTagWeightedCandidate> Candidates;
    Candidates.Reserve(State.Catalog.Num());
    for (const FRunItemDefinition& Item : State.Catalog)
    {
        FGameplayTagWeightedCandidate& Candidate = Candidates.AddDefaulted_GetRef();
        Candidate.Tags = Item.Tags;
        Candidate.BaseWeight = 1.0f;
    }
    FRandomStream Random(FMath::Rand());
    TArray<int32> SelectedIndices;
    if (!GameplayTagCandidateSelection::Select(Candidates, Query, OfferCount, bAllowDuplicates, Random, SelectedIndices))
    {
        OutError = NSLOCTEXT("RunItemShop", "InsufficientCandidates", "태그 조건을 만족하는 아이템 상점 후보가 부족합니다.");
        return false;
    }

    TArray<FRunItemShopOffer> Offers;
    Offers.Reserve(OfferCount);
    for (const int32 Index : SelectedIndices)
    {
        FRunItemShopOffer& Offer = Offers.AddDefaulted_GetRef();
        Offer.OfferId = FName(*FString::Printf(TEXT("Weapon_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
        Offer.Item = State.Catalog[Index];
    }
    State.Offers = MoveTemp(Offers);
    ++State.Revision;
    return true;
}
