#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunItemShopTypes.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EquipmentMainHand, "Equipment.Slot.Weapon.MainHand");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EquipmentOffHand, "Equipment.Slot.Weapon.OffHand");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EquipmentHead, "Equipment.Slot.Head");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EquipmentHands, "Equipment.Slot.Hands");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EquipmentFeet, "Equipment.Slot.Feet");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EquipmentBody, "Equipment.Slot.Body");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EquipmentNeck, "Equipment.Slot.Neck");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EquipmentRingLeft, "Equipment.Slot.Ring.Left");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EquipmentRingRight, "Equipment.Slot.Ring.Right");

namespace
{
    FRunEquipmentProfile MakeWeaponProfile(FName ProfileId, FName CategoryTag)
    {
        FRunEquipmentProfile Profile;
        Profile.ProfileId = ProfileId;
        Profile.RequiredTags = FGameplayTagQuery::MakeQuery_MatchTag(FGameplayTag::RequestGameplayTag(CategoryTag));
        Profile.AllowedSlots.AddTag(TAG_EquipmentMainHand);
        Profile.AllowedSlots.AddTag(TAG_EquipmentOffHand);
        FRunEquipmentAttachment& MainAttachment = Profile.Attachments.AddDefaulted_GetRef();
        MainAttachment.SlotTag = TAG_EquipmentMainHand;
        MainAttachment.SocketName = TEXT("hand_r");
        FRunEquipmentAttachment& OffAttachment = Profile.Attachments.AddDefaulted_GetRef();
        OffAttachment.SlotTag = TAG_EquipmentOffHand;
        OffAttachment.SocketName = TEXT("hand_l");
        return Profile;
    }
}

URunEquipmentCatalog::URunEquipmentCatalog()
{
    Profiles.Add(MakeWeaponProfile(TEXT("Dagger"), TEXT("Item.Weapon.Dagger")));

    FRunEquipmentProfile Shield = MakeWeaponProfile(TEXT("Shield"), TEXT("Item.Weapon.Shield"));
    Shield.AllowedSlots.RemoveTag(TAG_EquipmentMainHand);
    Profiles.Add(MoveTemp(Shield));

    FRunEquipmentProfile Bow = MakeWeaponProfile(TEXT("Bow"), TEXT("Item.Weapon.Bow"));
    Bow.OccupiedSlots = Bow.AllowedSlots;
    Bow.PreferredSlot = TAG_EquipmentOffHand;
    Profiles.Add(MoveTemp(Bow));

    FRunEquipmentProfile OneHandedSword = MakeWeaponProfile(TEXT("OneHandedSword"), TEXT("Item.Weapon.Sword"));
    OneHandedSword.ItemAssets.Add(FSoftObjectPath(TEXT("/Game/PurePoly/FreeLowPolyFantasyRPGWeapons/Meshes/SM_PP_Theme_11_Sword_One-Handed_003.SM_PP_Theme_11_Sword_One-Handed_003")));
    Profiles.Add(MoveTemp(OneHandedSword));

    FRunEquipmentProfile TwoHandedSword = MakeWeaponProfile(TEXT("TwoHandedSword"), TEXT("Item.Weapon.Sword"));
    TwoHandedSword.ItemAssets.Add(FSoftObjectPath(TEXT("/Game/PurePoly/FreeLowPolyFantasyRPGWeapons/Meshes/SM_PP_Theme_06_Sword_Two-Handed_001.SM_PP_Theme_06_Sword_Two-Handed_001")));
    TwoHandedSword.OccupiedSlots = TwoHandedSword.AllowedSlots;
    TwoHandedSword.PreferredSlot = TAG_EquipmentMainHand;
    Profiles.Add(MoveTemp(TwoHandedSword));

    FRunEquipmentProfile StartingStaff = MakeWeaponProfile(TEXT("StartingStaff"), TEXT("Item.Weapon.StaffWand"));
    StartingStaff.ItemAssets.Add(FSoftObjectPath(TEXT("/Game/MageStaff_FreeWeapons/SM_Staff_01.SM_Staff_01")));
    StartingStaff.OccupiedSlots = StartingStaff.AllowedSlots;
    StartingStaff.PreferredSlot = TAG_EquipmentMainHand;
    Profiles.Add(MoveTemp(StartingStaff));
}

const URunEquipmentCatalog& URunEquipmentCatalog::Get()
{
    return *GetDefault<URunEquipmentCatalog>();
}

FGameplayTag URunEquipmentCatalog::GetWeaponSlot(int32 Index)
{
    if (Index == 0) return TAG_EquipmentMainHand;
    if (Index == 1) return TAG_EquipmentOffHand;
    return FGameplayTag();
}

TArray<FGameplayTag> URunEquipmentCatalog::GetSlotTags()
{
    return { TAG_EquipmentMainHand, TAG_EquipmentOffHand, TAG_EquipmentHead, TAG_EquipmentHands, TAG_EquipmentFeet, TAG_EquipmentBody, TAG_EquipmentNeck, TAG_EquipmentRingLeft, TAG_EquipmentRingRight };
}

const FRunEquipmentProfile* URunEquipmentCatalog::ResolveProfile(const FRunItemDefinition& Item) const
{
    const FRunEquipmentProfile* GeneralProfile = nullptr;
    for (const FRunEquipmentProfile& Profile : Profiles)
    {
        if (!Profile.bEnabled || Profile.RequiredTags.IsEmpty() || !Profile.RequiredTags.Matches(Item.Tags)) continue;
        if (Profile.ItemAssets.Contains(Item.Asset)) return &Profile;
        if (!GeneralProfile && Profile.ItemAssets.IsEmpty()) GeneralProfile = &Profile;
    }
    return GeneralProfile;
}

FGameplayTag URunEquipmentCatalog::ResolveSlot(const FRunEquipmentProfile& Profile, FGameplayTag RequestedSlot)
{
    if (!Profile.bEnabled || !Profile.AllowedSlots.HasTagExact(RequestedSlot)) return FGameplayTag();
    const FGameplayTag ResolvedSlot = Profile.PreferredSlot.IsValid() ? Profile.PreferredSlot : RequestedSlot;
    return Profile.AllowedSlots.HasTagExact(ResolvedSlot) ? ResolvedSlot : FGameplayTag();
}

FGameplayTagContainer URunEquipmentCatalog::GetOccupiedSlots(const FRunEquipmentProfile& Profile, FGameplayTag AnchorSlot)
{
    FGameplayTagContainer OccupiedSlots;
    if (ResolveSlot(Profile, AnchorSlot) != AnchorSlot) return OccupiedSlots;
    OccupiedSlots = Profile.OccupiedSlots;
    OccupiedSlots.AddTag(AnchorSlot);
    return OccupiedSlots;
}

bool URunEquipmentCatalog::GetAttachment(const FRunEquipmentProfile& Profile, FGameplayTag SlotTag, FName& OutSocketName, FTransform& OutRelativeTransform)
{
    OutSocketName = NAME_None;
    OutRelativeTransform = FTransform::Identity;
    if (ResolveSlot(Profile, SlotTag) != SlotTag) return false;
    const FRunEquipmentAttachment* Attachment = Profile.Attachments.FindByPredicate([SlotTag](const FRunEquipmentAttachment& Candidate) { return Candidate.SlotTag == SlotTag; });
    if (!Attachment || Attachment->SocketName.IsNone() || !Attachment->RelativeTransform.IsValid() || Attachment->RelativeTransform.GetScale3D().GetAbsMin() <= SMALL_NUMBER) return false;
    OutSocketName = Attachment->SocketName;
    OutRelativeTransform = Attachment->RelativeTransform;
    return true;
}
