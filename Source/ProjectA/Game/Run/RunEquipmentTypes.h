#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RunEquipmentTypes.generated.h"

// Inventory indices identify individual copies in the append-only Run item array.
// Run의 추가 전용 아이템 배열에서 인덱스로 각각의 보유 사본을 식별합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunEquipmentSlot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Equipment")
    FGameplayTag SlotTag;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Equipment")
    int32 ItemIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunEquipmentState
{
    GENERATED_BODY()

    // Older saves retain skill-driven visuals until their first explicit equipment change.
    // 이전 저장은 최초 명시적 장비 변경 전까지 기존 스킬 기반 표시를 유지합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Equipment")
    bool bHasLoadout = false;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Equipment")
    int32 Revision = 0;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Equipment")
    TArray<FRunEquipmentSlot> Slots;
};

// An empty target slot moves an equipped item back into its owner's inventory.
// 대상 슬롯이 비어 있으면 장착 아이템을 소유자의 인벤토리로 돌려놓습니다.
USTRUCT()
struct PROJECTA_API FRunEquipmentCommand
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid CharacterId;

    UPROPERTY()
    int32 ItemIndex = INDEX_NONE;

    UPROPERTY()
    FGameplayTag TargetSlot;

    UPROPERTY()
    int32 ExpectedRevision = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunEquipmentVisual
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    FSoftObjectPath Asset;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    FName SocketName;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    FTransform RelativeTransform;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunStartingEquipment
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    FSoftObjectPath Asset;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    FGameplayTag SlotTag;
};
