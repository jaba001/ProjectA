#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CombatStatusTypes.generated.h"

// A skill applies harmful statuses through authored tags; the tag selects the rule, the value its strength.
// 스킬은 제작된 태그로 해로운 상태를 부여하며 태그가 규칙을, 수치가 세기를 결정합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCombatStatusApplication
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status", meta = (Categories = "Status"))
    FGameplayTag Status;

    // Burn, poison, bleeding, chill and shock use the value; freeze, stun and confusion ignore it.
    // 화상·중독·출혈·냉각·감전은 수치를 사용하며 빙결·스턴·혼란은 무시합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status", meta = (ClampMin = "1", ClampMax = "9999"))
    int32 Value = 1;
};

// Serializable per-unit status state shared by replication and the ready-boundary checkpoint.
// 복제와 준비 경계 체크포인트가 함께 사용하는 직렬화 가능한 유닛별 상태입니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCombatStatusEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Status")
    FGameplayTag Status;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Status")
    int32 Value = 0;

    // Duration statuses count the upcoming planning boundaries; zero means active in the current round only.
    // 지속형 상태는 남은 계획 경계 수를 세며 0은 현재 라운드에만 유효함을 뜻합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Status")
    int32 RemainingRounds = 0;
};
