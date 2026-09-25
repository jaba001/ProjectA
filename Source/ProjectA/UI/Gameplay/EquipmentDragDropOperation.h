#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "GameplayTagContainer.h"
#include "EquipmentDragDropOperation.generated.h"

class AGameplayPlayerController;
struct FRunPartyMember;

UCLASS()
class PROJECTA_API UEquipmentDragDropOperation : public UDragDropOperation
{
    GENERATED_BODY()

public:
    UPROPERTY(Transient)
    FGuid CharacterId;

    UPROPERTY(Transient)
    int32 ItemIndex = INDEX_NONE;

    UPROPERTY(Transient)
    int32 ExpectedRevision = INDEX_NONE;
};

// A drag carries only a stable inventory index and revision; the server validates and applies the change.
// 드래그에는 안정적인 인벤토리 인덱스와 버전만 담고 서버에서 변경을 검증하고 적용합니다.
struct FEquipmentDropRequest
{
    static bool CanDrop(const AGameplayPlayerController* Controller, const FRunPartyMember& Member, bool bCanChange, const UEquipmentDragDropOperation* Operation, FGameplayTag TargetSlot, FText& Error);
    static bool Submit(AGameplayPlayerController* Controller, const FRunPartyMember& Member, bool bCanChange, const UEquipmentDragDropOperation* Operation, FGameplayTag TargetSlot, FText& Error);
};
