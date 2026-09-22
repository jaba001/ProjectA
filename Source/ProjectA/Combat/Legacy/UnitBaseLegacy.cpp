#include "Unit/UnitBase.h"

// Preserve reflected names and inert behavior for existing assets; new combat uses round planning.
// 기존 에셋의 리플렉션 이름과 비활성 동작을 유지하며 새 전투는 라운드 계획을 사용합니다.

void AUnitBase::OnTurnStart()
{
    // Only the coordinator can start round actions.
    // 라운드 행동은 조정자만 시작할 수 있습니다.
    OnTurnEnd();
}

void AUnitBase::OnTurnEnd()
{
    if (!HasAuthority())
    {
        return;
    }

    bIsActiveTurn = false;
    bTurnMustEndAfterCurrentAction = false;
    ForceNetUpdate();
}

void AUnitBase::MoveToTile(ACombatGridTile* TargetTile)
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::MoveToTarget(AUnitBase* TargetUnit)
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::ReturnToOriginalTile()
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::SnapToTile(ACombatGridTile* Tile, const FRotator& TargetRotation)
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::OnSnapToTileFinished()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::OnReturnToOriginalTileFinished()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::HandleMoveCompleted()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::HandleMoveFailed(EUnitActionResult Result)
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::StartSkill(USkillDefinitionDataAsset* SkillData, ACombatGridTile* TargetTile)
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::ExecuteSkillAtTarget()
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

TArray<AUnitBase*> AUnitBase::ResolveSkillTargetUnits()
{
    return {};
}

void AUnitBase::OnSkillFinished()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::ClearSkillContext()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::StartMoveAction(ACombatGridTile* TargetTile)
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::OnMoveActionFinished()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::ClearMoveContext()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

bool AUnitBase::CanUseHealingItem(AUnitBase* TargetUnit) const
{
    return false;
}

void AUnitBase::StartItemAction(AUnitBase* TargetUnit)
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::ExecuteItemAtTarget()
{
    UE_LOG(LogTemp, Warning, TEXT("[UnitBase] Sequential action entry is retired; submit a round plan."));
}

void AUnitBase::OnItemFinished()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}

void AUnitBase::ClearItemContext()
{
    // Compatibility callback has no sequential action to advance.
    // 호환 콜백은 순차 행동을 진행하지 않습니다.
}
