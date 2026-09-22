#include "Controller/PartyPlayerController.h"

// Preserve reflected names and inert behavior for existing assets; new combat uses round planning.
// 기존 에셋의 리플렉션 이름과 비활성 동작을 유지하며 새 전투는 라운드 계획을 사용합니다.

namespace
{
    FCombatActionResponse RejectLegacyAction(const FCombatActionRequest& Request)
    {
        FCombatActionResponse Response;
        Response.CombatInstanceId = Request.CombatInstanceId;
        Response.RequestSequence = Request.RequestSequence;
        Response.Result = ECombatRequestResult::InvalidContext;
        Response.Message = FText::FromString(TEXT("개별 턴 즉시 실행은 종료되었습니다. 라운드 계획을 적용한 뒤 준비 완료를 사용하세요."));
        return Response;
    }
}

AUnitBase* APartyPlayerController::GetActiveUnit() const
{
    return nullptr;
}

void APartyPlayerController::RequestEndTurn()
{
    SubmitCombatActionRequest(FCombatActionRequest());
}

void APartyPlayerController::RequestHealingItem()
{
    SubmitCombatActionRequest(FCombatActionRequest());
}

bool APartyPlayerController::BuildCombatActionRequest(ECombatActionKind Kind, USkillDefinitionDataAsset* Skill, ACombatGridTile* TargetTile, FCombatActionRequest& OutRequest)
{
    OutRequest = FCombatActionRequest();
    OutRequest.Kind = Kind;
    return false;
}

FCombatActionResponse APartyPlayerController::SubmitCombatActionRequest(const FCombatActionRequest& Request)
{
    LastCombatActionResponse = RejectLegacyAction(Request);
    OnCombatActionResponse.Broadcast(LastCombatActionResponse);
    return LastCombatActionResponse;
}

void APartyPlayerController::ServerRequestCombatAction_Implementation(const FCombatActionRequest& Request)
{
    ClientReceiveCombatActionResponse(RejectLegacyAction(Request));
}

void APartyPlayerController::ClientReceiveCombatActionResponse_Implementation(const FCombatActionResponse& Response)
{
    LastCombatActionResponse = Response;
    OnCombatActionResponse.Broadcast(Response);
}

bool APartyPlayerController::CanUseActiveUnitAction() const
{
    return false;
}

bool APartyPlayerController::CanUseActiveUnitActionPoint(int32 Cost) const
{
    return false;
}

bool APartyPlayerController::CanUseActiveUnitSubActionPoint(int32 Cost) const
{
    return false;
}

void APartyPlayerController::SetTileInputMode(ETileInputMode NewMode)
{
    CancelTileInputMode();
}

void APartyPlayerController::EnterMoveMode()
{
    CancelTileInputMode();
}

void APartyPlayerController::EnterSkillMode(USkillDefinitionDataAsset* SkillData)
{
    CancelTileInputMode();
}

bool APartyPlayerController::IsValidTileForPendingSkill(ACombatGridTile* Tile) const
{
    return false;
}
