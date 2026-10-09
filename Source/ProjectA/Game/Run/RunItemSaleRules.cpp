#include "Game/Run/RunItemSaleRules.h"

#include "Game/Run/RunEncounterTypes.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunItemShopCatalog.h"

int32 RunItemSaleRules::GetPrice(const FRunItemDefinition& Item)
{
    return Item.Price > 0 ? FMath::Max(1, Item.Price / 2) : 0;
}

bool RunItemSaleRules::Validate(const FRunPartyMember& Member, const FRunItemShopState& Shop, const FRunEncounterProgress& Encounter, const FRunItemSaleCommand& Command, FText& OutError)
{
    OutError = NSLOCTEXT("RunItemSale", "ItemShopOnly", "아이템 판매는 아이템 상점에서만 가능합니다.");
    if ((Encounter.SchemaVersion != 1 && Encounter.SchemaVersion != 2) || Encounter.bCompleted || !Encounter.IsItemShop()) return false;
    OutError = NSLOCTEXT("RunItemSale", "ChangedVisit", "상점이 변경되었습니다. 현재 상점에서 다시 선택하세요.");
    if (Command.EncounterId.IsNone() || Command.EncounterId != Encounter.SelectedEncounterId || Shop.SchemaVersion != 1 || Shop.Revision <= 0 || Shop.Revision == MAX_int32 || Command.ExpectedShopRevision != Shop.Revision || (Shop.SelectionVersion != 0 && Shop.SelectionVersion != 1) || (Shop.SelectionVersion == 1 && Shop.ActiveEncounterId != Command.EncounterId)) return false;
    OutError = NSLOCTEXT("RunItemSale", "LivingOwnerOnly", "본인이 직접 조작하는 생존 캐릭터의 보유품만 판매할 수 있습니다.");
    if (!Member.bCreated || !Member.bHasSkillLoadout || !FMath::IsFinite(Member.CurrentHP) || Member.CurrentHP <= 0.0f) return false;
    if (!RunEquipmentRules::Validate(Member, OutError)) return false;
    OutError = NSLOCTEXT("RunItemSale", "ChangedInventory", "보유품이나 장비가 변경되었습니다. 최신 목록에서 다시 선택하세요.");
    if (!Command.CharacterId.IsValid() || Command.CharacterId != Member.CharacterId || Command.ExpectedEquipmentRevision != Member.Equipment.Revision || Member.Equipment.Revision == MAX_int32 || !Member.Items.IsValidIndex(Command.ItemIndex)) return false;
    const FRunItemDefinition& Item = Member.Items[Command.ItemIndex];
    if (Command.Asset != Item.Asset || Command.ItemInstanceId != Item.ItemInstanceId || !RunItemShopCatalog::ValidateItem(Item)) return false;
    OutError = NSLOCTEXT("RunItemSale", "UnequipFirst", "장착 중인 아이템은 먼저 해제한 뒤 판매하세요.");
    if (RunEquipmentRules::IsItemEquipped(Member, Command.ItemIndex)) return false;
    OutError = NSLOCTEXT("RunItemSale", "InvalidPrice", "이 아이템의 판매 금액을 계산할 수 없습니다.");
    const int32 Price = GetPrice(Item);
    if (Price <= 0) return false;
    OutError = NSLOCTEXT("RunItemSale", "GoldOverflow", "보유 골드 한도를 초과하여 판매할 수 없습니다.");
    if (Member.Gold < 0 || Member.Gold > MAX_int32 - Price) return false;
    OutError = FText::GetEmpty();
    return true;
}

bool RunItemSaleRules::Apply(FRunPartyMember& Member, FRunItemShopState& Shop, const FRunEncounterProgress& Encounter, const FRunItemSaleCommand& Command, FText& OutError)
{
    if (!Validate(Member, Shop, Encounter, Command, OutError)) return false;
    FRunPartyMember Candidate = Member;
    Candidate.Gold += GetPrice(Candidate.Items[Command.ItemIndex]);
    Candidate.Items.RemoveAt(Command.ItemIndex);
    // Keep every surviving copy and its skill rights unchanged; only indices after the removed copy shift.
    // 남은 모든 사본과 스킬 권한을 보존하며 삭제 사본 뒤의 인덱스만 이동합니다.
    for (FRunEquipmentSlot& Slot : Candidate.Equipment.Slots)
    {
        if (Slot.ItemIndex > Command.ItemIndex) --Slot.ItemIndex;
    }
    ++Candidate.Equipment.Revision;
    if (!RunEquipmentRules::Validate(Candidate, OutError)) return false;
    Member = MoveTemp(Candidate);
    ++Shop.Revision;
    OutError = FText::GetEmpty();
    return true;
}
