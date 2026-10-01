#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RunSkillShopTypes.generated.h"

// Freeze authored candidates and their tag selection data when a Run starts.
// Run 시작 시 작성된 후보와 태그 선택 데이터를 값으로 고정합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunSkillShopOffer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
    FName OfferId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (AllowedClasses = "/Script/ProjectA.SkillDefinitionDataAsset"))
    FSoftObjectPath Skill;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "1"))
    int32 Price = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    FGameplayTagContainer Tags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0"))
    float BaseWeight = 1.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunShopRecoveryOffer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "1"))
    int32 Price = 1;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunSkillShopState
{
    GENERATED_BODY()

    // Older saves keep their existing loadouts and do not receive retroactive gold or products.
    // 이전 저장은 기존 스킬 구성을 유지하며 골드나 상품을 소급 지급하지 않습니다.
    UPROPERTY()
    int32 SchemaVersion = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    TArray<FRunSkillShopOffer> Offers;

    // Missing catalog fields preserve the fixed offers of older saves.
    // 카탈로그 필드가 없는 이전 저장은 고정 상품을 유지합니다.
    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    TArray<FRunSkillShopOffer> Catalog;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    FGameplayTagQuery Query;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    int32 Revision = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    int32 RerollPrice = 1;

    // Existing skill-shop saves inherit the recovery service without replacing their frozen skill offers.
    // 기존 스킬 상점 저장은 고정된 스킬 상품을 교체하지 않고 회복 서비스를 추가합니다.
    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    FRunShopRecoveryOffer Recovery;

    static FName GetRecoveryOfferId() { return TEXT("HPRecovery"); }
    static FName GetRerollOfferId() { return TEXT("SkillShopReroll"); }
    static constexpr int32 OfferCount = 5;
};
