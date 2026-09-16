#include "Animation/Notify/AN_SkillRelease.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagContainer.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Unit/UnitBase.h"

UAN_SkillRelease::UAN_SkillRelease()
{
    SkillReleaseEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Attack.Release"));
}

void UAN_SkillRelease::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    (void)Animation;

    if (!MeshComp)
    {
        return;
    }

    AActor* OwnerActor = MeshComp->GetOwner();

    if (!OwnerActor)
    {
        return;
    }

    // Round units use server windup and collision; an authored legacy notify cannot release another attack.
    // 라운드 유닛은 서버 선딜과 충돌을 사용하며 제작된 기존 알림이 공격을 추가 발동할 수 없습니다.
    if (OwnerActor->IsA<AUnitBase>()) return;

    FGameplayEventData EventData;
    EventData.Instigator = OwnerActor;
    EventData.EventTag = SkillReleaseEventTag;

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventData.EventTag, EventData);
}
