#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Game/Run/RunSkillBalanceTypes.h"
#include "RunItemShopTypes.generated.h"

// Keep catalog entries and purchases serializable without loading their source assets.
// 원본 에셋을 로드하지 않고 카탈로그와 구매 내역을 직렬화 가능한 값으로 보관합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunItemDefinition
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    FSoftObjectPath Asset;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    FGameplayTagContainer Tags;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    int32 Price = 1;

    // Freeze authored rarity in the Run catalog; an absent tag preserves legacy random rarity selection.
    // 작성된 등급을 Run 카탈로그에 고정하며 태그가 없으면 기존 무작위 등급 선정을 유지합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Item|Generation")
    FGameplayTag CatalogRarityTag;

    // Generated copies keep their original result; version zero preserves legacy catalog items.
    // 생성 사본은 최초 결과를 유지하며 버전 0은 기존 카탈로그 아이템을 보존합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Item|Generation")
    int32 GenerationVersion = 0;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Item|Generation")
    FGuid ItemInstanceId;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Item|Generation")
    FGameplayTag RarityTag;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Item|Generation")
    TArray<FSoftObjectPath> GrantedSkills;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Item|Generation")
    int32 SkillBalanceVersion = 0;

    // Parallel to GrantedSkills for compact authoritative shop and inventory presentation.
    // 상점·인벤토리의 간결한 권위 표시를 위해 GrantedSkills와 같은 순서로 보관합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Item|Generation")
    TArray<FRunSkillBalance> GrantedSkillBalances;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunItemShopOffer
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    FName OfferId;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    FRunItemDefinition Item;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    bool bSold = false;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunItemRarityProbability
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Shop|Rarity")
    FGameplayTag RarityTag;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Shop|Rarity")
    int32 ProbabilityBasisPoints = 0;
};

// Freeze shop selection independently of fixed item grades and generated weapon skill rules.
// 고정 아이템 등급·부여 스킬 규칙과 별개로 상점 추첨 정책을 저장합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunItemRarityProbabilityState
{
    GENERATED_BODY()

    // Missing policy metadata preserves the original uniform item selection in older Runs.
    // 정책 메타데이터가 없으면 기존 Run의 아이템 균등 추첨을 유지합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Shop|Rarity")
    int32 SchemaVersion = 0;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Shop|Rarity")
    TArray<FRunItemRarityProbability> Entries;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunItemShopState
{
    GENERATED_BODY()

    UPROPERTY()
    int32 SchemaVersion = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    TArray<FRunItemDefinition> Catalog;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    TArray<FRunItemShopOffer> Offers;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    int32 Revision = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    int32 RerollPrice = 1;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Shop|Rarity")
    FRunItemRarityProbabilityState RarityProbabilities;

    // Version zero preserves the original five-slot shop; new Runs freeze the visited shop profile.
    // 버전 0은 기존 5칸 상점을 보존하며 새 Run은 방문한 상점의 상품 조건을 고정합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Shop|Selection")
    int32 SelectionVersion = 0;

    // Version zero preserves saved stock; newly generated stock requires an equipment profile.
    // 버전 0은 저장된 진열을 보존하며 새로 생성한 진열은 장착 프로필을 요구합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Shop|Selection")
    int32 EquipmentSelectionVersion = 0;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Shop|Selection")
    FName ActiveEncounterId;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Shop|Selection")
    FGameplayTagQuery ActiveItemQuery;

    // Basic shops require five products; specialized shops use up to five eligible products.
    // 기본 상점은 상품 5개를 요구하며 전문 상점은 적격 상품을 최대 5개 사용합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Shop|Selection")
    int32 ActiveStockPolicyVersion = 0;

    static FName GetEncounterId() { return TEXT("Shop_02"); }
    static FName GetRerollOfferId() { return TEXT("ItemShopReroll"); }
};
