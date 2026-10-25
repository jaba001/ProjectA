#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RunEquipmentTypes.generated.h"

// Inventory indices identify owned copies; selling a copy shifts later indices in the same transaction.
// 인덱스로 보유 사본을 식별하며 판매 시 같은 트랜잭션에서 뒤의 인덱스를 이동합니다.
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

    // Equipment changes and item removal invalidate pending drags and sale requests together.
    // 장비 변경과 아이템 삭제는 대기 중인 드래그와 판매 요청을 함께 무효화합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Equipment")
    int32 Revision = 0;

    // Older loadouts keep their saved skill list until an explicit equipment transaction upgrades it.
    // 이전 장비 상태는 명시적 장비 변경으로 갱신하기 전까지 저장된 스킬 목록을 유지합니다.
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Equipment")
    int32 ConditionalSkillVersion = 0;

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
    FGameplayTag SlotTag;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    FGameplayTagContainer ItemTags;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    float WeaponDamage = 10.f;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    FSoftObjectPath Asset;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    FName SocketName;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    FTransform RelativeTransform;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    bool bFollowBodyPose = false;
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
