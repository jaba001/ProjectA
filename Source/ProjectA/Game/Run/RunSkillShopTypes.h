#pragma once

#include "CoreMinimal.h"
#include "RunSkillShopTypes.generated.h"

// Freeze authored products as serializable values when a Run starts.
// Run 시작 시 작성된 상품을 직렬화 가능한 값으로 고정합니다.
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
};
