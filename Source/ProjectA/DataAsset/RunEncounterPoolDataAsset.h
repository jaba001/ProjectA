#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/Run/RunEncounterTypes.h"
#include "Game/Run/RunSkillShopTypes.h"
#include "RunEncounterPoolDataAsset.generated.h"

// Author fixed prototype choices here; weighted generation can later produce the same runtime offers.
// 고정된 시험용 선택지를 정의하며 향후 가중치 추첨도 같은 런타임 선택 목록을 생성합니다.
UCLASS(BlueprintType)
class PROJECTA_API URunEncounterPoolDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    URunEncounterPoolDataAsset();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Encounter")
    TArray<FRunEncounterOffer> FixedOffers;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0"))
    int32 StartingGold = 10;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
    TArray<FRunSkillShopOffer> FixedSkillOffers;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shop")
    FRunShopRecoveryOffer Recovery;

    bool BuildFixedOffers(TArray<FRunEncounterOffer>& OutOffers, FText& OutError) const;
    bool BuildSkillShop(FRunSkillShopState& OutState, FText& OutError) const;
    static bool ValidateSkillShop(const FRunSkillShopState& State, FText& OutError);
};
