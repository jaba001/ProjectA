#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AN_SkillRelease.generated.h"

UCLASS()
class PROJECTA_API UAN_SkillRelease : public UAnimNotify
{
    GENERATED_BODY()

public:
    UAN_SkillRelease();

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

protected:
    // Event tag to trigger skill release timing in GAS abilities
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
    FGameplayTag SkillReleaseEventTag;
};