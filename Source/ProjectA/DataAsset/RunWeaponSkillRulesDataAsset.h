#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/Run/RunWeaponSkillTypes.h"
#include "RunWeaponSkillRulesDataAsset.generated.h"

// Author weapon eligibility and development rarity pools without changing existing GAS skill assets.
// 기존 GAS 스킬 에셋을 바꾸지 않고 무기 적합성과 개발용 등급 풀을 작성합니다.
UCLASS(BlueprintType)
class PROJECTA_API URunWeaponSkillRulesDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    URunWeaponSkillRulesDataAsset();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Skills")
    FRunWeaponSkillRulesState Rules;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Skills")
    bool bUseCsvBalance = true;

    // Resolve execution tags and freeze the complete authored rule set in each new Run.
    // 실행 태그를 해석하고 작성된 전체 규칙을 새 Run마다 고정합니다.
    UFUNCTION(BlueprintCallable, Category = "Run|WeaponSkills")
    bool BuildState(FRunWeaponSkillRulesState& OutState, FText& OutError) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
