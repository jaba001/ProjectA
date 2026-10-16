#include "Game/Run/RunArmorCatalog.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NativeGameplayTags.h"
#include "Serialization/Csv/CsvParser.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemArmorBracers, "Item.Armor.Hands.Bracers");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ItemAccessoryCloak, "Item.Accessory.Cloak");

namespace
{
    // Translate authored CSV categories once; body and hand tags are registered by the equipment catalog profiles.
    // 작성된 CSV 분류를 한 번 변환하며 몸통·손 태그는 장비 카탈로그 프로필이 등록합니다.
    FGameplayTag FindArmorCategoryTag(const FString& Category)
    {
        static const TMap<FString, FGameplayTag> CategoryTags =
        {
            { TEXT("갑옷"), FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Body")) },
            { TEXT("장갑"), FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Hands")) },
            { TEXT("팔목 보호대"), TAG_ItemArmorBracers },
            { TEXT("망토"), TAG_ItemAccessoryCloak }
        };
        const FGameplayTag* Tag = CategoryTags.Find(Category);
        return Tag ? *Tag : FGameplayTag();
    }
}

bool RunArmorCatalog::Load(TArray<FRunItemDefinition>& OutCatalog, FText& OutError)
{
    FString CsvText;
    if (!FFileHelper::LoadFileToString(CsvText, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/ARMOR_ASSETS.csv"))))
    {
        OutError = NSLOCTEXT("RunArmorCatalog", "MissingCatalog", "방어구 에셋 CSV를 읽을 수 없습니다.");
        return false;
    }
    return LoadFromString(MoveTemp(CsvText), OutCatalog, OutError);
}

bool RunArmorCatalog::LoadFromString(FString CsvText, TArray<FRunItemDefinition>& OutCatalog, FText& OutError)
{
    OutError = FText::GetEmpty();
    const FCsvParser Parser(MoveTemp(CsvText));
    const FCsvParser::FRows& Rows = Parser.GetRows();
    const TCHAR* Headers[] = { TEXT("장비 종류"), TEXT("위치"), TEXT("에셋 이름"), TEXT("가격(G)"), TEXT("게임 내 이름"), TEXT("등급"), TEXT("분류 근거") };
    bool bValidHeader = Rows.Num() > 1 && Rows[0].Num() == UE_ARRAY_COUNT(Headers);
    for (int32 Index = 0; bValidHeader && Index < UE_ARRAY_COUNT(Headers); ++Index) bValidHeader = FCString::Strcmp(Rows[0][Index], Headers[Index]) == 0;
    if (!bValidHeader)
    {
        OutError = NSLOCTEXT("RunArmorCatalog", "InvalidHeader", "방어구 에셋 CSV의 열 또는 행 개수가 올바르지 않습니다.");
        return false;
    }

    TArray<FRunItemDefinition> Catalog;
    TSet<FSoftObjectPath> Assets;
    for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
    {
        const TArray<const TCHAR*>& Row = Rows[RowIndex];
        const FText RowError = FText::Format(NSLOCTEXT("RunArmorCatalog", "InvalidRow", "방어구 에셋 CSV의 {0}행 분류·경로·이름·가격·등급이 올바르지 않습니다."), FText::AsNumber(RowIndex + 1));
        if (Row.Num() != UE_ARRAY_COUNT(Headers))
        {
            OutError = RowError;
            return false;
        }
        FRunItemDefinition Item;
        const FGameplayTag CategoryTag = FindArmorCategoryTag(Row[0]);
        const FString AssetPath = FString::Printf(TEXT("%s/%s.%s"), Row[1], Row[2], Row[2]);
        Item.Asset = FSoftObjectPath(AssetPath);
        Item.DisplayName = FText::FromString(FString(Row[4]).TrimStartAndEnd());
        Item.CatalogRarityTag = RunItemShopCatalog::ResolveRarityTag(Row[5]);
        if (CategoryTag.IsValid()) Item.Tags.AddTag(CategoryTag);
        const bool bValidPrice = LexTryParseString(Item.Price, Row[3]) && Item.Price > 0;
        if (!CategoryTag.IsValid() || !AssetPath.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidObjectPath(AssetPath) || Assets.Contains(Item.Asset) || Item.DisplayName.IsEmpty() || !bValidPrice || !Item.CatalogRarityTag.IsValid() || !URunEquipmentCatalog::Get().ResolveProfile(Item))
        {
            OutError = RowError;
            return false;
        }
        Assets.Add(Item.Asset);
        Catalog.Add(MoveTemp(Item));
    }
    OutCatalog = MoveTemp(Catalog);
    return true;
}
