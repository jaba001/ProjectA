#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MonsterAnimInstance.generated.h"

// Read round-owned movement for monster presentation without a second movement simulation.
// 별도 이동 시뮬레이션 없이 라운드가 관리하는 이동으로 몬스터 표현을 갱신합니다.
UCLASS(Blueprintable)
class PROJECTA_API UMonsterAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UMonsterAnimInstance();
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    // The locomotion BlendSpace samples horizontal world speed in centimeters per second.
    // 이동 BlendSpace는 초당 센티미터 단위의 수평 월드 속도를 사용합니다.
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Monster|Animation")
    float GroundSpeed = 0.f;
};
