#include "UI/MainMenu/CharacterPartyDraft.h"

FCharacterPartyDraft::FCharacterPartyDraft()
{
    Reset({}, false);
}

void FCharacterPartyDraft::Reset(const TArray<FName>& AvailableClasses, bool bInitiallyCreated)
{
    Classes = AvailableClasses;
    Slots.Reset(Capacity);
    ControlledSlot = INDEX_NONE;
    for (int32 Index = 0; Index < Capacity; ++Index)
    {
        FRunPartyMember& Slot = Slots.AddDefaulted_GetRef();
        Slot.SlotIndex = Index;
        Slot.ClassId = Classes.IsEmpty() ? NAME_None : Classes[Index % Classes.Num()];
        Slot.bCreated = bInitiallyCreated && !Slot.ClassId.IsNone();
    }
}

bool FCharacterPartyDraft::SetClass(int32 SlotIndex, FName ClassId)
{
    if (!Slots.IsValidIndex(SlotIndex) || !Classes.Contains(ClassId)) return false;
    if (Slots[SlotIndex].ClassId != ClassId) Slots[SlotIndex].Appearance.ItemIds.Reset();
    Slots[SlotIndex].ClassId = ClassId;
    return true;
}

bool FCharacterPartyDraft::SetName(int32 SlotIndex, const FText& Name)
{
    if (!Slots.IsValidIndex(SlotIndex)) return false;
    Slots[SlotIndex].CharacterName = Name;
    return true;
}

bool FCharacterPartyDraft::Create(int32 SlotIndex)
{
    if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].ClassId.IsNone()) return false;
    Slots[SlotIndex].bCreated = true;
    return true;
}

bool FCharacterPartyDraft::SetAppearance(int32 SlotIndex, const FCharacterAppearanceSelection& Appearance)
{
    if (!IsCreated(SlotIndex)) return false;
    Slots[SlotIndex].Appearance = Appearance;
    return true;
}

bool FCharacterPartyDraft::Clear(int32 SlotIndex)
{
    if (!Slots.IsValidIndex(SlotIndex)) return false;
    Slots[SlotIndex].bCreated = false;
    Slots[SlotIndex].CharacterName = FText::GetEmpty();
    Slots[SlotIndex].Appearance = FCharacterAppearanceSelection();
    if (ControlledSlot == SlotIndex) ControlledSlot = INDEX_NONE;
    return true;
}

bool FCharacterPartyDraft::SelectControlled(int32 SlotIndex)
{
    if (!IsCreated(SlotIndex)) return false;
    ControlledSlot = SlotIndex;
    return true;
}

bool FCharacterPartyDraft::IsCreated(int32 SlotIndex) const
{
    return Slots.IsValidIndex(SlotIndex) && Slots[SlotIndex].bCreated;
}

const FRunPartyMember* FCharacterPartyDraft::GetSlot(int32 SlotIndex) const
{
    return Slots.IsValidIndex(SlotIndex) ? &Slots[SlotIndex] : nullptr;
}

TArray<FRunPartyMember> FCharacterPartyDraft::ExportParty() const
{
    TArray<FRunPartyMember> Result = Slots;
    for (FRunPartyMember& Slot : Result) Slot.bPlayerControlled = Slot.bCreated && Slot.SlotIndex == ControlledSlot;
    return Result;
}
