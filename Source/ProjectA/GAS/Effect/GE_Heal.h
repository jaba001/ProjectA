#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Heal.generated.h"

// Positive Data.Heal restores a living unit's HP immediately.
// 양수 Data.Heal로 살아 있는 유닛의 HP를 즉시 회복합니다.
UCLASS()
class PROJECTA_API UGE_Heal : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UGE_Heal();
};
