#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RunEncounterTypes.generated.h"

UENUM(BlueprintType)
enum class ERunEncounterType : uint8
{
    Shop
};

// Freeze presentation and identity as values instead of retaining an encounter actor or widget.
// 인카운터 액터나 위젯 대신 표시 정보와 식별자를 값으로 보관합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunEncounterOffer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
    FName EncounterId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
    ERunEncounterType Type = ERunEncounterType::Shop;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter", meta = (Categories = "Encounter"))
    FGameplayTag EncounterTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Encounter|Items")
    FGameplayTagQuery ItemQuery;

    // Zero retains five-item stock; specialized new shops may show fewer available distinct assets.
    // 0은 기존 5개 진열을 유지하며 새 전문 상점은 가용한 서로 다른 에셋만 진열합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Encounter|Items")
    int32 ItemStockPolicyVersion = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Encounter|Selection")
    FGameplayTag SelectionGroupTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Encounter|Selection")
    float GroupWeight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Encounter|Selection")
    float VariantWeight = 0.0f;

    static FGameplayTag GetSkillShopTag();
    static FGameplayTag GetItemShopTag();
    static FGameplayTag GetBasicItemShopTag();
    static FGameplayTag GetRarityItemShopTag();
    static FGameplayTag GetTagItemShopTag();
    static FGameplayTag GetRecoveryTag();
    static FGameplayTag GetRevivalTag();
    static FGameplayTag GetConsumableShopTag();
    FGameplayTag GetResolvedTag() const;
    FText GetDisplayName() const;
    bool IsItemShop() const;
    bool IsSupportedShop() const;
    bool IsService() const;
    bool IsSupportedEncounter() const;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunEncounterProgress
{
    GENERATED_BODY()

    // Zero preserves the two-combat route of saves created before encounter choices existed.
    // 0은 인카운터 선택 도입 전에 생성한 저장의 두 전투 경로를 유지합니다.
    UPROPERTY()
    int32 SchemaVersion = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter")
    TArray<FRunEncounterOffer> Offers;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter")
    FName SelectedEncounterId;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter")
    bool bCompleted = false;

    // Bind the current shop visit to its completed combat boundary without changing older saves.
    // 이전 저장을 변경하지 않고 현재 상점 방문을 완료 전투 경계에 연결합니다.
    UPROPERTY(BlueprintReadOnly, Category = "Encounter")
    int32 AfterCompletedNodeCount = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter")
    int32 VisitIndex = 0;

    const FRunEncounterOffer* FindSelectedOffer() const;
    bool IsItemShop() const;
};
