#include "Animation/Notify/AN_CloseRangeAttackHit.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagContainer.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAN_CloseRangeAttackHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    // Exit if there is no valid mesh component
    if (!MeshComp)
    {
        return;
    }

    // Retrieve the owning actor that is currently playing the montage
    AActor* OwnerActor = MeshComp->GetOwner();

    if (!OwnerActor)
    {
        return;
    }

    // Forward the hit event to the GAS Ability
    FGameplayEventData EventData;
    EventData.Instigator = OwnerActor;
    EventData.EventTag = FGameplayTag::RequestGameplayTag(FName("Event.Attack.Hit"));

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
        OwnerActor,
        EventData.EventTag,
        EventData
    );

    //UE_LOG(LogTemp, Log, TEXT("[AnimNotify] CloseRangeAttackHit | Owner=%s | EventTag=%s"), *GetNameSafe(OwnerActor), *EventData.EventTag.ToString());
}