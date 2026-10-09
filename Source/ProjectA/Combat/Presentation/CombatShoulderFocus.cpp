#include "Combat/Presentation/CombatShoulderFocus.h"

int32 FCombatShoulderFocus::Update(const FCombatRoundView& View, int32 ParticipantSlot, int32 PreferredUnitId)
{
    if (CombatId != View.CombatId || RoundNumber != View.RoundNumber || OwnerSlot != ParticipantSlot)
    {
        Reset();
        CombatId = View.CombatId;
        RoundNumber = View.RoundNumber;
        OwnerSlot = ParticipantSlot;
    }
    if (!CombatId.IsValid() || RoundNumber <= 0 || OwnerSlot <= 0) return INDEX_NONE;
    if (View.Phase != ECombatRoundPhase::Planning && View.Phase != ECombatRoundPhase::Resolving)
    {
        Reset();
        return INDEX_NONE;
    }
    const auto IsOwned = [this](const FCombatRoundUnitView& Unit) { return Unit.UnitId != INDEX_NONE && !Unit.bEnemy && Unit.OwnerSlot == OwnerSlot; };
    const FCombatRoundUnitView* Focus = View.Units.FindByPredicate([this](const FCombatRoundUnitView& Unit) { return Unit.UnitId == UnitId; });
    // A cancelled Ready can be submitted again in the same round, with a different selected unit.
    // 준비를 취소하면 같은 라운드에서 다른 캐릭터를 선택하여 다시 준비할 수 있습니다.
    if (View.Phase == ECombatRoundPhase::Planning && Focus && !Focus->bReady)
    {
        UnitId = INDEX_NONE;
        bFinished = false;
        Focus = nullptr;
    }
    if (bFinished) return INDEX_NONE;
    if (UnitId == INDEX_NONE)
    {
        Focus = View.Units.FindByPredicate([&](const FCombatRoundUnitView& Unit) { return IsOwned(Unit) && Unit.UnitId == PreferredUnitId && Unit.HP > 0.f; });
        if (!Focus) Focus = View.Units.FindByPredicate([&](const FCombatRoundUnitView& Unit) { return IsOwned(Unit) && Unit.HP > 0.f; });
        if (!Focus || (View.Phase == ECombatRoundPhase::Planning && !Focus->bReady)) return INDEX_NONE;
        UnitId = Focus->UnitId;
    }
    if (!Focus || !IsOwned(*Focus) || Focus->HP <= 0.f || CombatRoundRules::IsTerminal(Focus->ActionPhase))
    {
        Finish();
        return INDEX_NONE;
    }
    return UnitId;
}
