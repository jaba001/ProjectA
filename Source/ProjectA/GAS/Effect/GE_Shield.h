#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Shield.generated.h"

// An instant shield grant stores absorption until the round boundary clears it.
// 즉시 보호막 부여로 저장한 피해 흡수량은 라운드 경계에서 제거합니다.
UCLASS()
class PROJECTA_API UGE_Shield : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UGE_Shield();
};
