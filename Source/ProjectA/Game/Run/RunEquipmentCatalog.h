#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "RunEquipmentCatalog.generated.h"

struct FRunItemDefinition;

// Resolve body-specific source meshes without changing the owned item's stable identity.
// 보유 아이템의 고정 식별자를 바꾸지 않고 몸체별 원본 메시를 선택합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunEquipmentBodyMesh
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    FName BodyId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    FSoftObjectPath ItemAsset;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    FSoftObjectPath MeshAsset;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    TArray<FSoftObjectPath> BodyMaterials;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunEquipmentAttachment
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    FGameplayTag SlotTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    FName SocketName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    FTransform RelativeTransform = FTransform::Identity;

    // Worn skeletal garments follow the animated body pose instead of a socket.
    // 착용형 스켈레탈 의상은 소켓 대신 애니메이션되는 몸체 포즈를 따릅니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    bool bFollowBodyPose = false;
};

// Profiles classify catalog tags and keep asset-specific attachment corrections in authored data.
// 프로필은 카탈로그 태그를 분류하고 에셋별 장착 보정을 작성 가능한 데이터로 보관합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FRunEquipmentProfile
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    FName ProfileId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    bool bEnabled = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    FGameplayTagQuery RequiredTags;

    // An empty asset list accepts every matching tagged item; specific entries override general profiles.
    // 에셋 목록이 비어 있으면 태그가 맞는 모든 아이템을 허용하며 특정 에셋 설정을 일반 프로필보다 우선합니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    TArray<FSoftObjectPath> ItemAssets;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    FGameplayTagContainer AllowedSlots;

    // An empty occupancy list occupies only the resolved anchor slot.
    // 점유 목록이 비어 있으면 결정된 기준 슬롯 하나만 점유합니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    FGameplayTagContainer OccupiedSlots;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    FGameplayTag PreferredSlot;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    TArray<FRunEquipmentAttachment> Attachments;
};

UCLASS(BlueprintType, Blueprintable, Config = Game, DefaultConfig)
class PROJECTA_API URunEquipmentCatalog : public UObject
{
    GENERATED_BODY()

public:
    URunEquipmentCatalog();

    UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    TArray<FRunEquipmentProfile> Profiles;

    UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    TArray<FRunEquipmentBodyMesh> BodyMeshes;

    static const URunEquipmentCatalog& Get();
    static FGameplayTag GetWeaponSlot(int32 Index);
    static TArray<FGameplayTag> GetSlotTags();
    const FRunEquipmentProfile* ResolveProfile(const FRunItemDefinition& Item) const;
    FSoftObjectPath ResolveBodyMesh(const FSoftObjectPath& ItemAsset, FName BodyId) const;
    TArray<FSoftObjectPath> ResolveBodyMaterials(const FSoftObjectPath& ItemAsset, FName BodyId) const;
    static FGameplayTag ResolveSlot(const FRunEquipmentProfile& Profile, FGameplayTag RequestedSlot);
    static FGameplayTagContainer GetOccupiedSlots(const FRunEquipmentProfile& Profile, FGameplayTag AnchorSlot);
    static bool GetAttachment(const FRunEquipmentProfile& Profile, FGameplayTag SlotTag, FName& OutSocketName, FTransform& OutRelativeTransform);
    static bool GetAttachment(const FRunEquipmentProfile& Profile, FGameplayTag SlotTag, FName& OutSocketName, FTransform& OutRelativeTransform, bool& bOutFollowBodyPose);
};
