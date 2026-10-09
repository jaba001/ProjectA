#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CombatMeleeVfxCatalog.generated.h"

class UAnimMontage;
class UNiagaraSystem;
struct FCombatRoundSkill;
struct FCombatSkillVfx;

USTRUCT(BlueprintType)
struct PROJECTA_API FCombatMeleeVfxRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName RuleId;

    // Unverified source effects remain unchanged until a rule is explicitly enabled.
    // 근거가 확인되지 않은 원본 효과는 규칙을 명시적으로 켜기 전까지 변경하지 않습니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTagQuery SkillQuery;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UNiagaraSystem> Niagara;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TSoftObjectPtr<UAnimMontage>> CastMontages;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<FName, float> FloatOverrides;
};

// Native cosmetic rules modify only the launched visual copy, never a skill definition or saved profile.
// 네이티브 표현 규칙은 발동한 시각 사본만 보정하며 스킬 정의나 저장 프로필을 변경하지 않습니다.
UCLASS(BlueprintType)
class PROJECTA_API UCombatMeleeVfxCatalog : public UDataAsset
{
    GENERATED_BODY()

public:
    UCombatMeleeVfxCatalog();

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FCombatMeleeVfxRule> Rules;

    FCombatSkillVfx Resolve(const FCombatRoundSkill& Skill, const UAnimMontage* ResolvedCastMontage) const;
};
