#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunTypes.h"

namespace RunEquipmentRules
{
    PROJECTA_API bool Validate(const FRunPartyMember& Member, FText& OutError);
    PROJECTA_API int32 FindItemIndexAtSlot(const FRunPartyMember& Member, FGameplayTag Slot);
    PROJECTA_API bool IsItemEquipped(const FRunPartyMember& Member, int32 ItemIndex);
    PROJECTA_API bool CanDrop(const FRunPartyMember& Member, int32 ItemIndex, FGameplayTag TargetSlot, FText& OutError);
    PROJECTA_API bool Apply(FRunPartyMember& Member, const FRunEquipmentCommand& Command, FText& OutError);
    PROJECTA_API bool BuildVisuals(const FRunPartyMember& Member, TArray<FRunEquipmentVisual>& OutVisuals, FText& OutError);
    PROJECTA_API void InitializeStartingEquipment(FRunPartyMember& Member, const TArray<FRunItemDefinition>& Items);
}
