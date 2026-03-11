#include "Animation/Notify/AN_CloseRangeAttackHit.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagContainer.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAN_CloseRangeAttackHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    // 유효한 메시 컴포넌트가 없으면 종료
    if (!MeshComp)
    {
        return;
    }

    // 몽타주를 재생 중인 소유 액터를 가져온다.
    AActor* OwnerActor = MeshComp->GetOwner();

    if (!OwnerActor)
    {
        return;
    }

    // 히트 이벤트를 GAS Ability로 전달한다.
    FGameplayEventData EventData;
    EventData.Instigator = OwnerActor;
    EventData.EventTag = FGameplayTag::RequestGameplayTag(FName("Event.Attack.Hit"));

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
        OwnerActor,
        EventData.EventTag,
        EventData
    );

    UE_LOG(LogTemp, Warning, TEXT("[AnimNotify] CloseRangeAttackHit | Owner=%s | EventTag=%s"), *GetNameSafe(OwnerActor), *EventData.EventTag.ToString());
}