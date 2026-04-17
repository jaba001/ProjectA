#include "Animation/Notify/AN_SkillRelease.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagContainer.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

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

    FGameplayEventData EventData;
    EventData.Instigator = OwnerActor;
    EventData.EventTag = SkillReleaseEventTag;

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventData.EventTag, EventData);
}