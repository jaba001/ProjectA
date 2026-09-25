#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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

    static FName GetEncounterId() { return TEXT("Shop_02"); }
    static FName GetRerollOfferId() { return TEXT("ItemShopReroll"); }
};
