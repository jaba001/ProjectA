#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunWeaponSkillRules.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NativeGameplayTags.h"
#include "Serialization/Csv/CsvParser.h"
#include "Types/GameplayTagCandidateSelection.h"
#include "UObject/TextProperty.h"

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

    // Consume the full price field and reject overflow before arithmetic can wrap a positive cost.
    // 가격 필드 전체를 해석하고 양수 비용이 뒤집히기 전에 정수 범위 초과를 거절합니다.
    bool ParsePrice(const TCHAR* Text, int32& OutPrice)
    {
        const FString Value = FString(Text).TrimStartAndEnd();
        int32 Index = Value.StartsWith(TEXT("+")) ? 1 : 0;
        if (Index >= Value.Len()) return false;
        int32 Price = 0;
        for (; Index < Value.Len(); ++Index)
        {
            const TCHAR Character = Value[Index];
            if (Character < TEXT('0') || Character > TEXT('9')) return false;
            const int32 Digit = Character - TEXT('0');
            if (Price > (MAX_int32 - Digit) / 10) return false;
            Price = Price * 10 + Digit;
        }
        if (Price <= 0) return false;
        OutPrice = Price;
        return true;
    }

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

    FGameplayTag FindRarityTag(const FString& Rarity)
    {
        static const TMap<FString, FName> RarityTags =
        {
            {TEXT("흰색"), TEXT("Item.Rarity.White")},
            {TEXT("초록색"), TEXT("Item.Rarity.Green")},
            {TEXT("파란색"), TEXT("Item.Rarity.Blue")},
            {TEXT("보라색"), TEXT("Item.Rarity.Purple")},
            {TEXT("주황색"), TEXT("Item.Rarity.Orange")}
        };
        const FName* Tag = RarityTags.Find(Rarity);
        return Tag ? FGameplayTag::RequestGameplayTag(*Tag, false) : FGameplayTag();
    }

    bool ValidateCatalog(const TArray<FRunItemDefinition>& Catalog)
    {
        if (Catalog.Num() < OfferCount || Catalog.Num() > MaximumCatalogSize) return false;
        TSet<FSoftObjectPath> Assets;
        for (const FRunItemDefinition& Item : Catalog)
        {
            if (!RunItemShopCatalog::ValidateItem(Item) || Item.GenerationVersion != 0 || Assets.Contains(Item.Asset)) return false;
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
    if (!Item.Asset.IsValid() || !Item.Asset.GetSubPathUtf8String().IsEmpty() || !AssetPath.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidObjectPath(AssetPath) || !IsValidDisplayName(Item.DisplayName.ToString()) || !Item.Tags.HasTag(TAG_ItemWeapon) || Item.Price <= 0) return false;
    if (Item.GenerationVersion == 0) return !Item.ItemInstanceId.IsValid() && !Item.RarityTag.IsValid() && Item.GrantedSkills.IsEmpty();
    if (Item.GenerationVersion != 1 || !Item.ItemInstanceId.IsValid() || !Item.RarityTag.IsValid()) return false;
    if (Item.CatalogRarityTag.IsValid() && Item.CatalogRarityTag != Item.RarityTag) return false;
    TSet<FSoftObjectPath> Skills;
    for (const FSoftObjectPath& Skill : Item.GrantedSkills)
    {
        if (!Skill.IsValid() || !Skill.GetSubPathUtf8String().IsEmpty() || !Skill.ToString().StartsWith(TEXT("/Game/")) || !FPackageName::IsValidObjectPath(Skill.ToString()) || Skills.Contains(Skill)) return false;
        Skills.Add(Skill);
    }
    return true;
}

bool RunItemShopCatalog::IsSameDefinition(const FRunItemDefinition& Left, const FRunItemDefinition& Right)
{
    // Editor SaveGame serialization assigns different keys to copied CSV text; compare its value and retain every other reflected field.
    // 에디터 SaveGame 직렬화는 복사한 CSV 문구에 서로 다른 키를 부여하므로 문구 값과 나머지 모든 리플렉션 필드를 비교합니다.
    if (!FTextProperty::Identical_Implementation(Left.DisplayName, Right.DisplayName, 0, FTextProperty::EIdenticalLexicalCompareMethod::DisplayString)) return false;
    FRunItemDefinition Comparable = Right;
    Comparable.DisplayName = Left.DisplayName;
    return FRunItemDefinition::StaticStruct()->CompareScriptStruct(&Left, &Comparable, 0);
}

bool RunItemShopCatalog::IsSameBaseDefinition(const FRunItemDefinition& Left, const FRunItemDefinition& Right)
{
    // Catalog identity retains authored grade metadata and excludes only per-copy results.
    // 카탈로그 동일성은 작성 등급 메타데이터를 보존하며 사본별 생성 결과만 제외합니다.
    FRunItemDefinition LeftBase = Left;
    FRunItemDefinition RightBase = Right;
    for (FRunItemDefinition* Item : { &LeftBase, &RightBase })
    {
        Item->GenerationVersion = 0;
        Item->ItemInstanceId.Invalidate();
        Item->RarityTag = FGameplayTag();
        Item->GrantedSkills.Reset();
    }
    return IsSameDefinition(LeftBase, RightBase);
}

bool RunItemShopCatalog::Load(TArray<FRunItemDefinition>& OutCatalog, FText& OutError)
{
    OutError = FText::GetEmpty();
    FString CsvText;
    if (!FFileHelper::LoadFileToString(CsvText, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/WEAPON_ASSETS.csv"))))
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
    const bool bHasGameName = ColumnCount >= 5;
    const bool bHasRarity = ColumnCount >= 6;
    const bool bHasRationale = ColumnCount == 7;
    if (Rows.Num() < OfferCount + 1 || Rows.Num() > MaximumCatalogSize + 1 || ColumnCount < 4 || ColumnCount > 7 || FCString::Strcmp(Rows[0][0], TEXT("무기 종류")) != 0 || FCString::Strcmp(Rows[0][1], TEXT("위치")) != 0 || FCString::Strcmp(Rows[0][2], TEXT("에셋 이름")) != 0 || FCString::Strcmp(Rows[0][3], TEXT("가격(G)")) != 0 || (bHasGameName && FCString::Strcmp(Rows[0][4], TEXT("게임 내 이름")) != 0) || (bHasRarity && FCString::Strcmp(Rows[0][5], TEXT("등급")) != 0) || (bHasRationale && FCString::Strcmp(Rows[0][6], TEXT("분류 근거")) != 0))
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
        if (bHasRarity) Item.CatalogRarityTag = FindRarityTag(Row[5]);
        Item.Tags.AddTag(TAG_ItemWeapon);
        if (CategoryTag.IsValid()) Item.Tags.AddTag(CategoryTag);
        if ((bHasRarity && !Item.CatalogRarityTag.IsValid()) || (bHasRationale && !IsValidDisplayName(Row[6])))
        {
            OutError = FText::Format(NSLOCTEXT("RunItemShop", "InvalidCatalogRarity", "무기 에셋 CSV의 {0}행 등급 또는 분류 근거가 올바르지 않습니다. 등급은 흰색·초록색·파란색·보라색·주황색을 사용합니다."), FText::AsNumber(RowIndex + 1));
            return false;
        }
        if (!CategoryTag.IsValid() || !ParsePrice(Row[3], Item.Price) || !ValidateItem(Item))
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

bool RunItemShopCatalog::Validate(const FRunItemShopState& State, FText& OutError, const FRunWeaponSkillRulesState* WeaponSkillRules)
{
    OutError = FText::GetEmpty();
    if (WeaponSkillRules && !RunWeaponSkillRules::Validate(*WeaponSkillRules, OutError)) return false;
    if (State.SchemaVersion == 0 && State.Catalog.IsEmpty() && State.Offers.IsEmpty() && State.Revision == 0 && State.RerollPrice == 1) return true;
    const auto Fail = [&OutError]()
    {
        OutError = NSLOCTEXT("RunItemShop", "InvalidShopState", "아이템 상점의 저장된 상품·가격·갱신 상태가 올바르지 않습니다.");
        return false;
    };
    if (State.SchemaVersion != 1 || !ValidateCatalog(State.Catalog) || State.Revision < 0 || State.RerollPrice <= 0) return Fail();
    if (State.Revision == 0) return State.Offers.IsEmpty() ? true : Fail();
    if (State.Offers.Num() != OfferCount) return Fail();

    const bool bGenerated = WeaponSkillRules && WeaponSkillRules->SchemaVersion == 1;
    TSet<FName> OfferIds;
    TSet<FGuid> ItemInstanceIds;
    for (const FRunItemShopOffer& Offer : State.Offers)
    {
        if (Offer.OfferId.IsNone() || Offer.OfferId == FRunItemShopState::GetRerollOfferId() || OfferIds.Contains(Offer.OfferId) || !ValidateItem(Offer.Item)) return Fail();
        const FRunItemDefinition* CatalogItem = State.Catalog.FindByPredicate([&Offer](const FRunItemDefinition& Item) { return Item.Asset == Offer.Item.Asset; });
        if (!CatalogItem || !IsSameBaseDefinition(*CatalogItem, Offer.Item)) return Fail();
        if (bGenerated)
        {
            if (!RunWeaponSkillRules::ValidateGeneratedCopy(Offer.Item, *WeaponSkillRules, OutError) || ItemInstanceIds.Contains(Offer.Item.ItemInstanceId)) return Fail();
            ItemInstanceIds.Add(Offer.Item.ItemInstanceId);
        }
        else if (Offer.Item.GenerationVersion != 0) return Fail();
        OfferIds.Add(Offer.OfferId);
    }
    return true;
}

bool RunItemShopCatalog::Roll(FRunItemShopState& State, bool bAllowDuplicates, const FGameplayTagQuery& Query, FText& OutError, const FRunWeaponSkillRulesState* WeaponSkillRules)
{
    if (!Validate(State, OutError, WeaponSkillRules)) return false;
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
        Candidate.BaseWeight = WeaponSkillRules && WeaponSkillRules->SchemaVersion == 1 && !RunWeaponSkillRules::CanGenerate(Item, *WeaponSkillRules) ? 0.0f : 1.0f;
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
        if (WeaponSkillRules && WeaponSkillRules->SchemaVersion == 1 && !RunWeaponSkillRules::Generate(State.Catalog[Index], *WeaponSkillRules, Random, Offer.Item, OutError)) return false;
    }
    State.Offers = MoveTemp(Offers);
    ++State.Revision;
    return true;
}
