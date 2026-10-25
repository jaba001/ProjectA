#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "EquipmentCombatRules.generated.h"

struct FCombatEquipmentRequirement;
struct FRunEquipmentVisual;

// Authored references connect equipment combinations and basic attacks without name-based combat branches.
// 작성 가능한 참조로 이름별 전투 분기 없이 장비 조합과 기본 공격을 연결합니다.
UCLASS(Config = Game, DefaultConfig)
class PROJECTA_API UEquipmentCombatRules : public UObject
{
    GENERATED_BODY()

public:
    UEquipmentCombatRules();

    UPROPERTY(Config, EditDefaultsOnly, Category = "Equipment|Skills")
    TArray<FSoftObjectPath> ConditionalSkills;

    UPROPERTY(Config, EditDefaultsOnly, Category = "Equipment|Skills")
    TArray<FSoftObjectPath> BasicWeaponAttacks;
};

namespace EquipmentCombatRules
{
    PROJECTA_API bool Matches(const TArray<FRunEquipmentVisual>& Equipment, const TArray<FCombatEquipmentRequirement>& Requirements);
    PROJECTA_API bool ResolveDamage(const TArray<FRunEquipmentVisual>& Equipment, FGameplayTag Slot, float& OutDamage);
}
