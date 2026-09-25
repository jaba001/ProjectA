#include "UI/Gameplay/EquipmentDragDropOperation.h"

#include "Controller/GameplayPlayerController.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunTypes.h"

bool FEquipmentDropRequest::CanDrop(const AGameplayPlayerController* Controller, const FRunPartyMember& Member, bool bCanChange, const UEquipmentDragDropOperation* Operation, FGameplayTag TargetSlot, FText& Error)
{
    if (!Operation) return false;
    if (!Controller || !bCanChange || Controller->IsEquipmentChangePending())
    {
        Error = NSLOCTEXT("Equipment", "ChangeUnavailable", "지금은 장비를 변경할 수 없습니다. 상점에서 변경할 수 있습니다.");
        return false;
    }
    if (!Member.CharacterId.IsValid() || Operation->CharacterId != Member.CharacterId)
    {
        Error = NSLOCTEXT("Equipment", "WrongCharacter", "같은 캐릭터의 장비와 인벤토리 사이에서 이동하세요.");
        return false;
    }
    if (Operation->ExpectedRevision != Member.Equipment.Revision)
    {
        Error = NSLOCTEXT("Equipment", "StaleDrag", "장비 정보가 변경되었습니다. 아이템을 다시 끌어 주세요.");
        return false;
    }
    return RunEquipmentRules::CanDrop(Member, Operation->ItemIndex, TargetSlot, Error);
}

bool FEquipmentDropRequest::Submit(AGameplayPlayerController* Controller, const FRunPartyMember& Member, bool bCanChange, const UEquipmentDragDropOperation* Operation, FGameplayTag TargetSlot, FText& Error)
{
    if (!CanDrop(Controller, Member, bCanChange, Operation, TargetSlot, Error)) return false;
    FRunEquipmentCommand Command;
    Command.CharacterId = Operation->CharacterId;
    Command.ItemIndex = Operation->ItemIndex;
    Command.TargetSlot = TargetSlot;
    Command.ExpectedRevision = Operation->ExpectedRevision;
    Controller->RequestChangeEquipment(Command);
    return true;
}
