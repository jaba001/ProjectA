#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Damage.generated.h"

// GameplayEffect class used for damage application.
// 피해 적용에 사용하는 게임플레이 이펙트 클래스입니다.
UCLASS()
class PROJECTA_API UGE_Damage : public UGameplayEffect
{
    GENERATED_BODY()

public:
    // Configures the default damage effect metadata.
    // 기본 피해 이펙트 메타데이터를 설정합니다.
    UGE_Damage();
};
