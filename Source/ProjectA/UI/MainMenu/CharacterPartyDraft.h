#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunTypes.h"

// Party capacity belongs to the draft, independently of the available profession catalogue.
// 파티 정원은 사용 가능한 직업 목록과 독립적으로 초안에서 관리합니다.
class PROJECTA_API FCharacterPartyDraft
{
public:
    static constexpr int32 Capacity = 4;

    FCharacterPartyDraft();
    void Reset(const TArray<FName>& AvailableClasses, bool bInitiallyCreated);
    bool SetClass(int32 SlotIndex, FName ClassId);
    bool SetName(int32 SlotIndex, const FText& Name);
    bool Create(int32 SlotIndex);
    bool Clear(int32 SlotIndex);
    bool SelectControlled(int32 SlotIndex);
    bool IsCreated(int32 SlotIndex) const;
    int32 GetControlledSlot() const { return ControlledSlot; }
    const FRunPartyMember* GetSlot(int32 SlotIndex) const;
    TArray<FRunPartyMember> ExportParty() const;

private:
    TArray<FRunPartyMember> Slots;
    TArray<FName> Classes;
    int32 ControlledSlot = INDEX_NONE;
};
