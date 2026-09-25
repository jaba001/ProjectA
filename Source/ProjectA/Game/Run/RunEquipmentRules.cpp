#include "Game/Run/RunEquipmentRules.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Profession/ProfessionBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogRunEquipment, Log, All);

bool RunEquipmentRules::Validate(const FRunPartyMember& Member, FText& OutError)
{
    OutError = NSLOCTEXT("RunEquipment", "InvalidState", "저장된 장비 슬롯 또는 보유 아이템 연결이 올바르지 않습니다.");
    const FRunEquipmentState& State = Member.Equipment;
    if (State.Revision < 0 || State.Slots.Num() > 9) return false;
    if (!State.bHasLoadout)
    {
        if (State.Revision != 0 || !State.Slots.IsEmpty()) return false;
        OutError = FText::GetEmpty();
        return true;
    }
    if (!Member.bCreated || !Member.bHasSkillLoadout) return false;
    const URunEquipmentCatalog& Catalog = URunEquipmentCatalog::Get();
    FGameplayTagContainer Occupied;
    TSet<int32> Copies;
    for (const FRunEquipmentSlot& Slot : State.Slots)
    {
        if (!Member.Items.IsValidIndex(Slot.ItemIndex) || Copies.Contains(Slot.ItemIndex)) return false;
        const FRunEquipmentProfile* Profile = Catalog.ResolveProfile(Member.Items[Slot.ItemIndex]);
        if (!Profile || !Profile->AllowedSlots.HasTagExact(Slot.SlotTag) || Catalog.ResolveSlot(*Profile, Slot.SlotTag) != Slot.SlotTag) return false;
        const FGameplayTagContainer Required = Catalog.GetOccupiedSlots(*Profile, Slot.SlotTag);
        if (Required.IsEmpty() || Occupied.HasAny(Required)) return false;
        FName Socket;
        FTransform Offset;
        if (!Catalog.GetAttachment(*Profile, Slot.SlotTag, Socket, Offset)) return false;
        Occupied.AppendTags(Required);
        Copies.Add(Slot.ItemIndex);
    }
    OutError = FText::GetEmpty();
    return true;
}

int32 RunEquipmentRules::FindItemIndexAtSlot(const FRunPartyMember& Member, FGameplayTag Slot)
{
    const URunEquipmentCatalog& Catalog = URunEquipmentCatalog::Get();
    for (const FRunEquipmentSlot& Entry : Member.Equipment.Slots)
    {
        if (!Member.Items.IsValidIndex(Entry.ItemIndex)) continue;
        const FRunEquipmentProfile* Profile = Catalog.ResolveProfile(Member.Items[Entry.ItemIndex]);
        if (Profile && Catalog.GetOccupiedSlots(*Profile, Entry.SlotTag).HasTagExact(Slot)) return Entry.ItemIndex;
    }
    return INDEX_NONE;
}

bool RunEquipmentRules::IsItemEquipped(const FRunPartyMember& Member, int32 ItemIndex)
{
    return Member.Equipment.Slots.ContainsByPredicate([ItemIndex](const FRunEquipmentSlot& Slot) { return Slot.ItemIndex == ItemIndex; });
}

bool RunEquipmentRules::Apply(FRunPartyMember& Member, const FRunEquipmentCommand& Command, FText& OutError)
{
    if (!Validate(Member, OutError)) return false;
    OutError = NSLOCTEXT("RunEquipment", "Stale", "장비 상태가 변경되었습니다. 현재 목록에서 다시 드래그하세요.");
    if (Command.CharacterId != Member.CharacterId || Command.ExpectedRevision != Member.Equipment.Revision || Member.Equipment.Revision == MAX_int32) return false;
    OutError = NSLOCTEXT("RunEquipment", "InvalidItem", "보유한 아이템만 장착하거나 해제할 수 있습니다.");
    if (!Member.Items.IsValidIndex(Command.ItemIndex)) return false;
    const URunEquipmentCatalog& Catalog = URunEquipmentCatalog::Get();
    const FRunEquipmentSlot* Previous = Member.Equipment.Slots.FindByPredicate([&Command](const FRunEquipmentSlot& Slot) { return Slot.ItemIndex == Command.ItemIndex; });
    const FGameplayTag PreviousSlot = Previous ? Previous->SlotTag : FGameplayTag();
    FRunEquipmentState Candidate = Member.Equipment;
    Candidate.Slots.RemoveAll([&Command](const FRunEquipmentSlot& Slot) { return Slot.ItemIndex == Command.ItemIndex; });
    if (!Command.TargetSlot.IsValid())
    {
        if (!Previous) return false;
    }
    else
    {
        OutError = NSLOCTEXT("RunEquipment", "UnsupportedSlot", "이 아이템은 해당 슬롯에 장착할 수 없습니다. 장착 분류가 없는 아이템은 보관만 가능합니다.");
        const FRunEquipmentProfile* Profile = Catalog.ResolveProfile(Member.Items[Command.ItemIndex]);
        if (!Profile || !Profile->AllowedSlots.HasTagExact(Command.TargetSlot)) return false;
        const FGameplayTag Target = Catalog.ResolveSlot(*Profile, Command.TargetSlot);
        if (PreviousSlot == Target) return false;
        const FGameplayTagContainer Required = Catalog.GetOccupiedSlots(*Profile, Target);
        TArray<FRunEquipmentSlot> Displaced;
        for (const FRunEquipmentSlot& Slot : Candidate.Slots)
        {
            const FRunEquipmentProfile* Existing = Catalog.ResolveProfile(Member.Items[Slot.ItemIndex]);
            if (Catalog.GetOccupiedSlots(*Existing, Slot.SlotTag).HasAny(Required)) Displaced.Add(Slot);
        }
        Candidate.Slots.RemoveAll([&Displaced](const FRunEquipmentSlot& Slot) { return Displaced.ContainsByPredicate([&Slot](const FRunEquipmentSlot& Other) { return Other.ItemIndex == Slot.ItemIndex; }); });
        FRunEquipmentSlot& Equipped = Candidate.Slots.AddDefaulted_GetRef();
        Equipped.ItemIndex = Command.ItemIndex;
        Equipped.SlotTag = Target;
        // Swap compatible single-slot equipment; all other displaced copies return to the bag.
        // 호환되는 단일 슬롯 장비끼리는 교환하고 나머지 밀려난 사본은 가방으로 돌려놓습니다.
        if (PreviousSlot.IsValid() && Required.Num() == 1 && Displaced.Num() == 1 && Catalog.GetOccupiedSlots(*Profile, PreviousSlot).Num() == 1)
        {
            FRunEquipmentSlot Replacement = Displaced[0];
            const FRunEquipmentProfile* Other = Catalog.ResolveProfile(Member.Items[Replacement.ItemIndex]);
            if (Other->AllowedSlots.HasTagExact(PreviousSlot) && Catalog.GetOccupiedSlots(*Other, PreviousSlot).Num() == 1)
            {
                Replacement.SlotTag = Catalog.ResolveSlot(*Other, PreviousSlot);
                Candidate.Slots.Add(Replacement);
            }
        }
    }
    Candidate.bHasLoadout = true;
    ++Candidate.Revision;
    FRunPartyMember Validated = Member;
    Validated.Equipment = Candidate;
    if (!Validate(Validated, OutError)) return false;
    Member.Equipment = MoveTemp(Candidate);
    OutError = FText::GetEmpty();
    return true;
}

bool RunEquipmentRules::CanDrop(const FRunPartyMember& Member, int32 ItemIndex, FGameplayTag TargetSlot, FText& OutError)
{
    FRunPartyMember Candidate = Member;
    FRunEquipmentCommand Command;
    Command.CharacterId = Member.CharacterId;
    Command.ItemIndex = ItemIndex;
    Command.TargetSlot = TargetSlot;
    Command.ExpectedRevision = Member.Equipment.Revision;
    return Apply(Candidate, Command, OutError);
}

bool RunEquipmentRules::BuildVisuals(const FRunPartyMember& Member, TArray<FRunEquipmentVisual>& OutVisuals, FText& OutError)
{
    OutVisuals.Reset();
    if (!Validate(Member, OutError)) return false;
    const URunEquipmentCatalog& Catalog = URunEquipmentCatalog::Get();
    for (const FRunEquipmentSlot& Slot : Member.Equipment.Slots)
    {
        const FRunItemDefinition& Item = Member.Items[Slot.ItemIndex];
        const FRunEquipmentProfile* Profile = Catalog.ResolveProfile(Item);
        UObject* Mesh = Item.Asset.TryLoad();
        OutError = NSLOCTEXT("RunEquipment", "MissingMesh", "장비의 원본 메시를 불러올 수 없습니다. 기존 장비를 유지합니다.");
        if (!Mesh || (!Mesh->IsA<UStaticMesh>() && !Mesh->IsA<USkeletalMesh>())) return false;
        FRunEquipmentVisual Visual;
        Visual.Asset = Item.Asset;
        if (!Catalog.GetAttachment(*Profile, Slot.SlotTag, Visual.SocketName, Visual.RelativeTransform)) return false;
        OutVisuals.Add(Visual);
    }
    OutError = FText::GetEmpty();
    return true;
}

void RunEquipmentRules::InitializeStartingEquipment(FRunPartyMember& Member, const TArray<FRunItemDefinition>& Items)
{
    Member.Equipment = FRunEquipmentState();
    Member.Equipment.bHasLoadout = Member.bCreated;
    const UProfessionBase* Profession = Member.bCreated ? UProfessionBase::FindProfession(Member.ClassId) : nullptr;
    if (!Profession) return;
    for (const FRunStartingEquipment& Starting : Profession->StartingEquipment)
    {
        const FRunItemDefinition* Item = Items.FindByPredicate([&Starting](const FRunItemDefinition& Candidate) { return Candidate.Asset == Starting.Asset; });
        UObject* Mesh = Item ? Item->Asset.TryLoad() : nullptr;
        if (!Mesh || (!Mesh->IsA<UStaticMesh>() && !Mesh->IsA<USkeletalMesh>()))
        {
            UE_LOG(LogRunEquipment, Warning, TEXT("Starting equipment unavailable; slot remains empty: %s / 시작 장비 원본 없음: %s"), *Member.ClassId.ToString(), *Starting.Asset.ToString());
            continue;
        }
        FRunPartyMember Candidate = Member;
        FRunEquipmentCommand Command;
        Command.CharacterId = Member.CharacterId;
        Command.ItemIndex = Candidate.Items.Add(*Item);
        Command.TargetSlot = Starting.SlotTag;
        Command.ExpectedRevision = Candidate.Equipment.Revision;
        FText Error;
        if (!Apply(Candidate, Command, Error))
        {
            UE_LOG(LogRunEquipment, Warning, TEXT("Starting equipment unsupported; slot remains empty: %s / 시작 장비 설정 확인: %s"), *Starting.Asset.ToString(), *Error.ToString());
            continue;
        }
        Member = MoveTemp(Candidate);
    }
}
