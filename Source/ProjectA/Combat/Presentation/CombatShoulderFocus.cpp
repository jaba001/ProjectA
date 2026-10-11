#include "Combat/Presentation/CombatShoulderFocus.h"

bool FCombatShoulderFocus::FinishAtReturnPoint(const FCombatRoundUnitView& Unit, const FCombatRoundSkill* Skill)
{
    if (bFinished || UnitId == INDEX_NONE || Unit.UnitId != UnitId || !Skill) return false;
    const bool bMeleeReturn = Skill->Kind == ECombatRoundSkillKind::Melee && Unit.ActionPhase == ECombatRoundActionPhase::Returning;
    const bool bSkillImpact = (Skill->Kind == ECombatRoundSkillKind::Projectile || Skill->Kind == ECombatRoundSkillKind::GroundAttack) && Unit.bSkillImpactSettled;
    if (!bMeleeReturn && !bSkillImpact) return false;
    Finish();
    return true;
}

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
    // A completed cast can still have a projectile in flight; cancellation and death never retain the shot.
    // 시전 완료 뒤에도 투사체는 비행할 수 있지만 취소·사망 시에는 시점을 유지하지 않습니다.
    const bool bActionEnded = Focus && (Focus->ActionPhase == ECombatRoundActionPhase::Cancelled || (Focus->ActionPhase == ECombatRoundActionPhase::Complete && !Focus->bAwaitingSkillImpact));
    if (!Focus || !IsOwned(*Focus) || Focus->HP <= 0.f || bActionEnded)
    {
        Finish();
        return INDEX_NONE;
    }
    return UnitId;
}
