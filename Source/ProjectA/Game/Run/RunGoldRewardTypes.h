#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunItemShopTypes.h"
#include "RunGoldRewardTypes.generated.h"

USTRUCT(BlueprintType)
struct PROJECTA_API FRunGoldRewardClaim
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Reward")
    FGuid CharacterId;

    UPROPERTY(BlueprintReadOnly, Category = "Reward")
    int32 ChoiceIndex = INDEX_NONE;

    // Keep the claim after selling its awarded copy; absent data preserves older unsold receipts.
    // 지급 사본을 판매한 뒤에도 수령 기록을 유지하며 누락된 값은 이전 미판매 기록을 보존합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Reward")
    bool bItemSold = false;
};

// Store generated offers and individual claims together so loading never rerolls or pays twice.
// 저장을 불러와도 재추첨하거나 중복 지급하지 않도록 생성된 선택지와 개인 수령 내역을 함께 보관합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunGoldRewardState
{
    GENERATED_BODY()

    // Existing saves remain compatible without granting rewards retroactively.
    // 기존 저장은 보상을 소급 지급하지 않고 호환성을 유지합니다.
    UPROPERTY()
    int32 SchemaVersion = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Reward")
    FName NodeId;

    UPROPERTY(BlueprintReadOnly, Category = "Reward")
    TArray<int32> GoldChoices;

    UPROPERTY(BlueprintReadOnly, Category = "Reward")
    TArray<FRunGoldRewardClaim> Claims;

    // Version two freezes three item copies and one gold award independently of the selected card.
    // 버전 2는 선택한 카드와 무관하게 아이템 사본 3개와 골드 보상 하나를 고정합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Reward")
    TArray<FRunItemDefinition> ItemChoices;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Reward")
    int32 BonusGold = 0;
};
