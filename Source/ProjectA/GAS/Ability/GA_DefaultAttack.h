#pragma once

#include "CoreMinimal.h"
#include "GAS/Ability/GA_AttackBase.h"
#include "GA_DefaultAttack.generated.h"

// Keep authored Blueprint identity without the removed direct or tile-based damage path.
// 제거된 직접 또는 타일 기반 피해 경로 없이 제작 블루프린트 식별자를 유지합니다.
UCLASS()
class PROJECTA_API UGA_DefaultAttack : public UGA_AttackBase
{
    GENERATED_BODY()

public:
    UGA_DefaultAttack();
};
