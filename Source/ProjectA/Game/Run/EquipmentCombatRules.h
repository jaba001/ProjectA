#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "EquipmentCombatRules.generated.h"

struct FCombatEquipmentRequirement;
struct FRunEquipmentVisual;

// Grant an intrinsic skill from item tags while its execution requirements remain on the skill.
// 아이템 태그로 기본 스킬을 부여하며 실제 사용 조건은 스킬에 유지합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FEquipmentIntrinsicSkill
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "Equipment|Skills")
    FGameplayTagQuery ItemQuery;

    UPROPERTY(EditDefaultsOnly, Category = "Equipment|Skills")
    FSoftObjectPath Skill;
};

// Authored references connect equipment combinations and basic attacks without name-based combat branches.
// 작성 가능한 참조로 이름별 전투 분기 없이 장비 조합과 기본 공격을 연결합니다.
UCLASS(Config = Game, DefaultConfig)
class PROJECTA_API UEquipmentCombatRules : public UObject
{
    GENERATED_BODY()

public:
    UEquipmentCombatRules();

    UPROPERTY(Config, EditDefaultsOnly, Category = "Equipment|Skills")
    TArray<FEquipmentIntrinsicSkill> IntrinsicSkills;

    UPROPERTY(Config, EditDefaultsOnly, Category = "Equipment|Skills")
    TArray<FSoftObjectPath> ConditionalSkills;

    UPROPERTY(Config, EditDefaultsOnly, Category = "Equipment|Skills")
    TArray<FSoftObjectPath> BasicWeaponAttacks;
};

namespace EquipmentCombatRules
{
    PROJECTA_API TArray<FSoftObjectPath> GetIntrinsicSkills(const FGameplayTagContainer& ItemTags);
    PROJECTA_API bool Matches(const TArray<FRunEquipmentVisual>& Equipment, const TArray<FCombatEquipmentRequirement>& Requirements);
    PROJECTA_API bool ResolveDamage(const TArray<FRunEquipmentVisual>& Equipment, FGameplayTag Slot, float& OutDamage);
}
