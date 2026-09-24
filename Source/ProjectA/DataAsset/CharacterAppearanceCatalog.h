#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Unit/CharacterAppearanceTypes.h"
#include "CharacterAppearanceCatalog.generated.h"

class USkeletalMesh;
class UMaterialInterface;
class UAnimInstance;
class UAnimationAsset;

// Body identifiers are extensible catalog entries rather than a fixed gender enumeration.
// 몸체 식별자는 고정된 성별 열거형 대신 확장 가능한 카탈로그 항목으로 관리합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCharacterAppearanceBodyVariant
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FName BodyId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    TSoftObjectPtr<USkeletalMesh> Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    TSoftClassPtr<UAnimInstance> AnimationClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    TSoftObjectPtr<UAnimationAsset> PreviewAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FTransform MeshTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FTransform PreviewMeshTransform = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FCharacterAppearanceSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (Categories = "Appearance.Slot"))
    FGameplayTag SlotTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FText DisplayName;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FCharacterAppearanceItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FName ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (Categories = "Appearance.Slot"))
    FGameplayTag SlotTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    TArray<TSoftObjectPtr<USkeletalMesh>> Meshes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (Categories = "Appearance.Body"))
    FGameplayTagContainer HiddenBodyParts;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FCharacterAppearanceBodyPart
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (Categories = "Appearance.Body"))
    FGameplayTag PartTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    TSoftObjectPtr<USkeletalMesh> Mesh;

    // Empty overrides retain the source part materials; populated entries follow material slot order.
    // 빈 재질 목록은 원본 파츠 재질을 유지하며 지정한 재질은 슬롯 순서대로 적용합니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    TArray<TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;
};

UCLASS(BlueprintType)
class PROJECTA_API UCharacterAppearanceCatalog : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    TArray<FCharacterAppearanceBodyVariant> BodyVariants;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FName DefaultBodyId;

    // Retain saved outfit identifiers while temporarily hiding outfit controls and rendering.
    // 의상 선택 UI와 렌더링을 임시로 숨겨도 저장된 의상 식별자는 유지합니다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    bool bEnableOutfits = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    TArray<FCharacterAppearanceSlot> Slots;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    TArray<FCharacterAppearanceItem> Items;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    TArray<FCharacterAppearanceBodyPart> BodyParts;

    UFUNCTION(BlueprintPure, Category = "Appearance")
    bool ValidateSelection(const FCharacterAppearanceSelection& InSelection, FText& OutError) const;

    const FCharacterAppearanceItem* FindItem(FName ItemId) const;
    const FCharacterAppearanceBodyVariant* FindBodyVariant(FName BodyId) const;
};
