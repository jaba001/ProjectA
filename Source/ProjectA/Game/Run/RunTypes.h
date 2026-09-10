#pragma once

#include "CoreMinimal.h"
#include "Game/Run/RunIdentityTypes.h"
#include "RunTypes.generated.h"

UENUM(BlueprintType)
enum class ERunNodeType : uint8
{
    Combat
};

UENUM(BlueprintType)
enum class ERunPhase : uint8
{
    None,
    Map,
    Preparing,
    Combat,
    Result,
    Complete,
    Defeat
};

// Runtime party data survives level travel without keeping combat actors alive.
// 전투 액터를 유지하지 않고 레벨 이동 동안 보존하는 런타임 파티 데이터입니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunPartyMember
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
    int32 SlotIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
    FText CharacterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
    FName ClassId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
    bool bCreated = false;

    // Ownership survives travel and does not change when a host or controller changes.
    // 소유권은 레벨 이동 후에도 유지하며 Host나 조작 주체가 바뀌어도 변경하지 않습니다.
    UPROPERTY(BlueprintReadOnly, Category = "Run|Identity")
    FGuid CharacterId;

    UPROPERTY(BlueprintReadOnly, Category = "Run|Identity")
    FRunAccountId OwnerAccountId;

    // Negative HP means that the first spawn uses the unit class default.
    // 음수 HP는 첫 스폰에서 유닛 클래스의 기본값을 사용함을 뜻합니다.
    UPROPERTY(BlueprintReadOnly, Category = "Run")
    float CurrentHP = -1.0f;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunNodeDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
    FName NodeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
    FName EncounterId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
    ERunNodeType NodeType = ERunNodeType::Combat;
};
