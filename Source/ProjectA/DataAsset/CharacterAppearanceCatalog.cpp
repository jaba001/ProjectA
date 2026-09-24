#include "DataAsset/CharacterAppearanceCatalog.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceSlot, "Appearance.Slot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceSlotHead, "Appearance.Slot.Head");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceSlotTorso, "Appearance.Slot.Torso");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceSlotLegs, "Appearance.Slot.Legs");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceSlotFeet, "Appearance.Slot.Feet");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceSlotHands, "Appearance.Slot.Hands");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceSlotShoulders, "Appearance.Slot.Shoulders");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceSlotBracers, "Appearance.Slot.Bracers");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceSlotBack, "Appearance.Slot.Back");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceBody, "Appearance.Body");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceBodyHead, "Appearance.Body.Head");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceBodyTorso, "Appearance.Body.Torso");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceBodyArms, "Appearance.Body.Arms");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceBodyHands, "Appearance.Body.Hands");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceBodyLegs, "Appearance.Body.Legs");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AppearanceBodyFeet, "Appearance.Body.Feet");

const FCharacterAppearanceItem* UCharacterAppearanceCatalog::FindItem(FName ItemId) const
{
    if (ItemId.IsNone() || Items.Num() > 512) return nullptr;
    return Items.FindByPredicate([ItemId](const FCharacterAppearanceItem& Item) { return Item.ItemId == ItemId; });
}

bool UCharacterAppearanceCatalog::ValidateSelection(const FCharacterAppearanceSelection& InSelection, FText& OutError) const
{
    OutError = FText::GetEmpty();
    const auto Fail = [&OutError](const FText& Reason)
    {
        OutError = Reason;
        return false;
    };
    if (Slots.IsEmpty() || Slots.Num() > 16 || Items.Num() > 512 || BodyParts.IsEmpty() || BodyParts.Num() > 32 || InSelection.ItemIds.Num() > 16)
    {
        return Fail(NSLOCTEXT("CharacterAppearance", "InvalidCatalogSize", "의상 목록 또는 선택 개수가 올바르지 않습니다."));
    }

    TSet<FGameplayTag> SlotTags;
    for (const FCharacterAppearanceSlot& Slot : Slots)
    {
        if (!Slot.SlotTag.MatchesTag(TAG_AppearanceSlot) || Slot.SlotTag == TAG_AppearanceSlot || SlotTags.Contains(Slot.SlotTag))
        {
            return Fail(NSLOCTEXT("CharacterAppearance", "InvalidSlot", "의상 부위 태그가 없거나 중복되었습니다."));
        }
        SlotTags.Add(Slot.SlotTag);
    }

    FGameplayTagContainer BodyTags;
    for (const FCharacterAppearanceBodyPart& Part : BodyParts)
    {
        if (!Part.PartTag.MatchesTag(TAG_AppearanceBody) || Part.PartTag == TAG_AppearanceBody || Part.Mesh.IsNull())
        {
            return Fail(NSLOCTEXT("CharacterAppearance", "InvalidBody", "기본 신체 파츠가 올바르지 않습니다."));
        }
        BodyTags.AddTag(Part.PartTag);
    }

    TSet<FName> CatalogIds;
    for (const FCharacterAppearanceItem& Item : Items)
    {
        if (Item.ItemId.IsNone() || CatalogIds.Contains(Item.ItemId) || !SlotTags.Contains(Item.SlotTag) || Item.Meshes.IsEmpty() || Item.Meshes.Num() > 8 || Item.HiddenBodyParts.Num() > 32 || !BodyTags.HasAllExact(Item.HiddenBodyParts))
        {
            return Fail(NSLOCTEXT("CharacterAppearance", "InvalidItem", "의상 항목의 식별자·부위·메시 설정이 올바르지 않습니다."));
        }
        for (const TSoftObjectPtr<USkeletalMesh>& Mesh : Item.Meshes)
        {
            if (Mesh.IsNull()) return Fail(NSLOCTEXT("CharacterAppearance", "MissingItemMesh", "의상 메시가 지정되지 않았습니다."));
        }
        CatalogIds.Add(Item.ItemId);
    }

    TSet<FName> SelectedIds;
    TSet<FGameplayTag> SelectedSlots;
    int32 MeshCount = BodyParts.Num();
    for (FName ItemId : InSelection.ItemIds)
    {
        const FCharacterAppearanceItem* Item = FindItem(ItemId);
        if (!Item || SelectedIds.Contains(ItemId) || SelectedSlots.Contains(Item->SlotTag))
        {
            return Fail(NSLOCTEXT("CharacterAppearance", "InvalidSelection", "목록에 없는 의상 또는 같은 부위의 중복 의상이 선택되었습니다."));
        }
        SelectedIds.Add(ItemId);
        SelectedSlots.Add(Item->SlotTag);
        MeshCount += Item->Meshes.Num();
        if (MeshCount > 64) return Fail(NSLOCTEXT("CharacterAppearance", "TooManyMeshes", "선택한 의상 파츠가 너무 많습니다."));
    }
    return true;
}
