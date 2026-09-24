#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Unit/CharacterAppearanceTypes.h"
#include "CharacterAppearanceCatalog.generated.h"

class USkeletalMesh;

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
};

UCLASS(BlueprintType)
class PROJECTA_API UCharacterAppearanceCatalog : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    TArray<FCharacterAppearanceSlot> Slots;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    TArray<FCharacterAppearanceItem> Items;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    TArray<FCharacterAppearanceBodyPart> BodyParts;

    UFUNCTION(BlueprintPure, Category = "Appearance")
    bool ValidateSelection(const FCharacterAppearanceSelection& InSelection, FText& OutError) const;

    const FCharacterAppearanceItem* FindItem(FName ItemId) const;
};
