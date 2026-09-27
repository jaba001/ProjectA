#include "Combat/Round/CombatRoundCoordinator.h"
#include "Combat/CombatManager.h"
#include "Combat/Commands/CombatActionAuthority.h"
#include "Controller/PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/GameModes/CombatDebugGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Unit/UnitBase.h"
#include "Unit/UnitDataRules.h"

bool ACombatRoundCoordinator::CanEditDebugUnit(APlayerController* Controller, int32 UnitId, FText& OutError) const
{
    OutError = FText::GetEmpty();
    if (!ACombatDebugGameMode::IsDebugWorld(GetWorld()) || !HasExecutionAuthority() || !IsValid(Controller) || !Controller->IsLocalController() || Controller->GetWorld() != GetWorld())
    {
        OutError = FText::FromString(TEXT("독립 디버그 전투에서만 장착을 변경할 수 있습니다."));
        return false;
    }
    if (View.Phase != ECombatRoundPhase::Planning || bSAPMovementInProgress || PlanningMoveIndex != INDEX_NONE || !Projectiles.IsEmpty() || !ActiveEffects.IsEmpty())
    {
        OutError = FText::FromString(TEXT("계획 단계에서 이동과 효과가 모두 끝난 뒤 변경하세요."));
        return false;
    }
    const int32 Index = FindUnitIndex(UnitId);
    const int32 OwnerSlot = GetParticipantSlot(Controller);
    if (!View.Units.IsValidIndex(Index) || OwnerSlot <= 0)
    {
        OutError = FText::FromString(TEXT("변경할 소유 유닛을 선택하세요."));
        return false;
    }
    const FCombatRoundUnitView& Entry = View.Units[Index];
    if (!IsValid(Entry.Unit) || !Entry.Unit->IsUnitAlive() || Entry.bEnemy || Entry.OwnerSlot != OwnerSlot || !CombatManager->GetActionAuthority()->CanControllerControl(Cast<APartyPlayerController>(Controller), Entry.Unit))
    {
        OutError = FText::FromString(TEXT("생존한 자신의 아군만 변경할 수 있습니다."));
        return false;
    }
    if (!Entry.Unit->GetVelocity().IsNearlyZero() || (Entry.Unit->GetCharacterMovement() && !Entry.Unit->GetCharacterMovement()->Velocity.IsNearlyZero()))
    {
        OutError = FText::FromString(TEXT("유닛이 정지한 뒤 변경하세요."));
        return false;
    }
    return true;
}

bool ACombatRoundCoordinator::SetDebugUnitSkills(APlayerController* Controller, int32 UnitId, const TArray<TObjectPtr<USkillDefinitionDataAsset>>& Definitions, FText& OutError)
{
    if (!CanEditDebugUnit(Controller, UnitId, OutError) || !UnitDataRules::ValidateSkills(Definitions, false, OutError)) return false;
    const int32 ChangedIndex = FindUnitIndex(UnitId);
    if (View.Units[ChangedIndex].Unit->GetEquippedSkillDataAssets() == Definitions) return true;
    TArray<FCombatRoundSkill> NewSkills;
    TArray<TArray<FName>> NewUnitSkillIds;
    TMap<FName, USkillDefinitionDataAsset*> DefinitionsById;
    NewUnitSkillIds.SetNum(View.Units.Num());
    // Validate every replacement profile before mutating either the unit or replicated planning state.
    // 유닛 또는 복제되는 계획 상태를 변경하기 전에 모든 교체 프로필을 검증합니다.
    for (int32 Index = 0; Index < View.Units.Num(); ++Index)
    {
        const AUnitBase* Unit = View.Units[Index].Unit;
        if (!IsValid(Unit))
        {
            OutError = FText::FromString(TEXT("전투 유닛이 사라져 스킬 목록을 갱신할 수 없습니다."));
            return false;
        }
        const TArray<TObjectPtr<USkillDefinitionDataAsset>>& UnitSkills = Index == ChangedIndex ? Definitions : Unit->GetEquippedSkillDataAssets();
        if (!UnitDataRules::ValidateSkills(UnitSkills, false, OutError)) return false;
        for (USkillDefinitionDataAsset* Definition : UnitSkills)
        {
            FCombatRoundSkill Skill;
            if (!Definition->ResolveRoundSkill(Skill, OutError)) return false;
            if (USkillDefinitionDataAsset* const* Existing = DefinitionsById.Find(Skill.SkillId))
            {
                if (*Existing != Definition)
                {
                    OutError = FText::FromString(FString::Printf(TEXT("서로 다른 스킬의 식별자가 중복됩니다: %s"), *Skill.SkillId.ToString()));
                    return false;
                }
            }
            else
            {
                DefinitionsById.Add(Skill.SkillId, Definition);
                NewSkills.Add(Skill);
            }
            NewUnitSkillIds[Index].Add(Skill.SkillId);
        }
    }
    View.Units[ChangedIndex].Unit->SetDebugEquippedSkills(Definitions);
    Skills = MoveTemp(NewSkills);
    for (int32 Index = 0; Index < View.Units.Num(); ++Index) View.Units[Index].SkillIds = MoveTemp(NewUnitSkillIds[Index]);
    ResetDebugUnitPlan(ChangedIndex, FText::FromString(TEXT("스킬 장착이 변경되어 해당 유닛의 계획과 자신의 준비가 해제되었습니다.")));
    return true;
}

void ACombatRoundCoordinator::NotifyDebugEquipmentChanged(APlayerController* Controller, int32 UnitId)
{
    FText Error;
    if (!CanEditDebugUnit(Controller, UnitId, Error)) return;
    ResetDebugUnitPlan(FindUnitIndex(UnitId), FText::FromString(TEXT("장비가 변경되어 해당 유닛의 계획과 자신의 준비가 해제되었습니다.")));
}

void ACombatRoundCoordinator::ResetDebugUnitPlan(int32 UnitIndex, const FText& Message)
{
    FCombatRoundUnitView& Entry = View.Units[UnitIndex];
    Entry.Command = FCombatRoundCommand();
    Entry.Command.UnitId = Entry.UnitId;
    Entry.Command.TargetCoord = Entry.HomeCoord;
    Entry.Command.DestinationCoord = Entry.HomeCoord;
    Entry.bHasMovePlan = false;
    Entry.MoveDestinationCoord = Entry.HomeCoord;
    Entry.ActionPhase = ECombatRoundActionPhase::Planned;
    Entry.Status = FText::FromString(TEXT("스킬 미선택 · 턴 넘기기"));
    Actions[UnitIndex] = FActionRuntime();
    Actions[UnitIndex].OriginalLocation = Entry.Unit->GetActorLocation();
    ClearOwnerReady(Entry.OwnerSlot);
    ++View.PlanRevision;
    View.Message = Message;
    bLockRetryBlocked = false;
    PublishState();
}
