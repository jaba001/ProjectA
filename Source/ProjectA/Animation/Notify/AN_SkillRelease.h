#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AN_SkillRelease.generated.h"

// Animation notify that emits the skill release gameplay event.
// 스킬 발동 타이밍의 게임플레이 이벤트를 발생시키는 애니메이션 노티파이입니다.
UCLASS()
class PROJECTA_API UAN_SkillRelease : public UAnimNotify
{
    GENERATED_BODY()

public:
    // Sets the default gameplay event tag for skill release.
    // 스킬 발동용 기본 게임플레이 이벤트 태그를 설정합니다.
    UAN_SkillRelease();

    // Sends the skill release event when the notify is reached.
    // 노티파이 시점에 스킬 발동 이벤트를 전송합니다.
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

protected:
    // Event tag to trigger skill release timing in GAS abilities.
    // GAS 어빌리티의 스킬 발동 타이밍을 트리거하는 이벤트 태그입니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    FGameplayTag SkillReleaseEventTag;
};
