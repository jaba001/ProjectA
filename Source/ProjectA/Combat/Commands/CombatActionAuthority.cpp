#include "Combat/Commands/CombatActionAuthority.h"
#include "AbilitySystemComponent.h"
#include "Combat/CombatManager.h"
#include "Combat/Library/CombatTargetingLibrary.h"
#include "Controller/PartyPlayerController.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "Game/Run/RunIdentityLibrary.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/UnitBase.h"
#include "Unit/PlayerUnit.h"

ACombatManager* UCombatActionAuthority::GetManager() const
{
    return GetTypedOuter<ACombatManager>();
}

void UCombatActionAuthority::Reset()
{
    // Preserve the Run requirement so clearing a failed configuration cannot reopen offline compatibility.
    // 실패한 설정을 지워도 오프라인 호환이 다시 열리지 않도록 Run 설정 필요 여부를 유지합니다.
    for (const TPair<TWeakObjectPtr<APartyPlayerController>, FRunAccountId>& Entry : Participants)
    {
        if (Entry.Key.IsValid())
        {
            Entry.Key->SetCombatParticipantBinding(FRunAccountId(), FGuid());
        }
    }
    UnitsById.Reset();
    CharacterIds.Reset();
    Participants.Reset();
    ParticipantBindingIds.Reset();
    LastRequestSequences.Reset();
    LastAIRequestSequences.Reset();
    CombatInstanceId.Invalidate();
    RunIdentity = FRunIdentityData();
    PartyMembers.Reset();
    bRunConfigured = false;
}

void UCombatActionAuthority::RegisterUnits(const TArray<AUnitBase*>& Units)
{
    Reset();
    ACombatManager* Manager = GetManager();
    if (!IsValid(Manager) || !Manager->HasAuthority())
    {
        return;
    }
    for (AUnitBase* Unit : Units)
    {
        if (IsValid(Unit) && Unit->GetWorld() == Manager->GetWorld() && !GetUnitId(Unit).IsValid())
        {
            UnitsById.Add(FGuid::NewGuid(), Unit);
        }
    }
}

void UCombatActionAuthority::BeginCombat()
{
    ACombatManager* Manager = GetManager();
    if (!IsValid(Manager) || !Manager->HasAuthority())
    {
        return;
    }
    CombatInstanceId = FGuid::NewGuid();
    LastRequestSequences.Reset();
    LastAIRequestSequences.Reset();
    TMap<FGuid, TWeakObjectPtr<AUnitBase>> NewIds;
    for (const TPair<FGuid, TWeakObjectPtr<AUnitBase>>& Entry : UnitsById)
    {
        if (Entry.Value.IsValid())
        {
            NewIds.Add(FGuid::NewGuid(), Entry.Value);
        }
    }
    UnitsById = MoveTemp(NewIds);
}

bool UCombatActionAuthority::ConfigureRun(const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, const TMap<int32, TObjectPtr<AUnitBase>>& PartyActors, FText& OutError)
{
    ACombatManager* Manager = GetManager();
    if (!IsValid(Manager) || !Manager->HasAuthority() || Manager->GetTurnManager())
    {
        OutError = NSLOCTEXT("CombatRequest", "ConfigureContext", "Run 소유권은 서버에서 전투 시작 전에 설정해야 합니다.");
        return false;
    }
    bRequiresRunConfiguration = true;
    bRunConfigured = false;
    CharacterIds.Reset();
    if (!URunIdentityLibrary::ValidateIdentity(Identity, Members, OutError))
    {
        return false;
    }
    TMap<TWeakObjectPtr<AUnitBase>, FGuid> NewCharacterIds;
    for (const FRunPartyMember& Member : Members)
    {
        AUnitBase* Unit = PartyActors.FindRef(Member.SlotIndex);
        if (!Member.bCreated)
        {
            if (PartyActors.Contains(Member.SlotIndex))
            {
                OutError = NSLOCTEXT("CombatRequest", "EmptyPartyActor", "빈 파티 슬롯에는 전투 유닛을 연결할 수 없습니다.");
                return false;
            }
            continue;
        }
        if (!Unit && Member.CurrentHP == 0.0f && !PartyActors.Contains(Member.SlotIndex))
        {
            continue;
        }
        const TWeakObjectPtr<AUnitBase> UnitKey(Unit);
        if (!IsValid(Unit) || Unit->GetWorld() != Manager->GetWorld() || Unit->GetTeam() != ETeam::Player || !GetUnitId(Unit).IsValid() || !Manager->GetRegisteredUnits().Contains(Unit) || NewCharacterIds.Contains(UnitKey))
        {
            OutError = NSLOCTEXT("CombatRequest", "PartyActor", "파티 캐릭터는 중복 없이 등록된 아군 전투 유닛에 연결해야 합니다.");
            return false;
        }
        NewCharacterIds.Add(UnitKey, Member.CharacterId);
    }
    if (NewCharacterIds.Num() != PartyActors.Num())
    {
        OutError = NSLOCTEXT("CombatRequest", "UnknownPartyActor", "Run에 없는 파티 슬롯의 전투 유닛이 있습니다.");
        return false;
    }
    for (const TPair<FGuid, TWeakObjectPtr<AUnitBase>>& Entry : UnitsById)
    {
        if (AUnitBase* Unit = Entry.Value.Get(); IsValid(Unit) && Unit->GetTeam() == ETeam::Player && !NewCharacterIds.Contains(TWeakObjectPtr<AUnitBase>(Unit)))
        {
            OutError = NSLOCTEXT("CombatRequest", "UnmappedPlayer", "등록된 아군 전투 유닛의 원래 캐릭터를 확인할 수 없습니다.");
            return false;
        }
    }
    RunIdentity = Identity;
    PartyMembers = Members;
    CharacterIds = MoveTemp(NewCharacterIds);
    bRunConfigured = true;
    for (const TPair<int32, TObjectPtr<AUnitBase>>& Entry : PartyActors)
    {
        if (APlayerUnit* Player = Cast<APlayerUnit>(Entry.Value))
        {
            Player->InitializeAutoCombat(Manager);
        }
    }
    OutError = FText::GetEmpty();
    return true;
}

bool UCombatActionAuthority::BindParticipant(APartyPlayerController* Controller, const FRunAccountId& AccountId)
{
    ACombatManager* Manager = GetManager();
    if (!IsValid(Manager) || !Manager->HasAuthority() || !IsValid(Controller) || Controller->GetWorld() != Manager->GetWorld() || !bRunConfigured || !URunIdentityLibrary::IsOriginalParticipant(RunIdentity, AccountId))
    {
        return false;
    }
    const TWeakObjectPtr<APartyPlayerController> ControllerKey(Controller);
    if (const FRunAccountId* Existing = Participants.Find(ControllerKey))
    {
        return *Existing == AccountId && GetParticipantBindingId(Controller).IsValid();
    }
    for (auto It = Participants.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid())
        {
            ParticipantBindingIds.Remove(It.Key());
            LastRequestSequences.Remove(It.Key());
            It.RemoveCurrent();
        }
        else if (It.Value() == AccountId)
        {
            return false;
        }
    }
    Participants.Add(ControllerKey, AccountId);
    ParticipantBindingIds.Add(ControllerKey, FGuid::NewGuid());
    Controller->SetCombatParticipantBinding(AccountId, ParticipantBindingIds.FindRef(ControllerKey));
    return true;
}

FGuid UCombatActionAuthority::GetParticipantBindingId(const APartyPlayerController* Controller) const
{
    if (!IsValid(Controller))
    {
        return FGuid();
    }
    return ParticipantBindingIds.FindRef(TWeakObjectPtr<APartyPlayerController>(const_cast<APartyPlayerController*>(Controller)));
}

bool UCombatActionAuthority::AllowsStandaloneLegacy(const APartyPlayerController* Controller) const
{
    const ACombatManager* Manager = GetManager();
    return IsValid(Manager) && IsValid(Controller) && Manager->GetNetMode() == NM_Standalone && Controller->IsLocalController() && RunIdentity.Origin == ERunIdentityOrigin::LegacyOffline && (!bRequiresRunConfiguration || bRunConfigured);
}

bool UCombatActionAuthority::CanControllerControl(const APartyPlayerController* Controller, const AUnitBase* Unit) const
{
    const ACombatManager* Manager = GetManager();
    if (!IsValid(Manager) || !Manager->HasAuthority() || !IsValid(Controller) || !IsValid(Unit) || Controller->GetWorld() != Manager->GetWorld() || Unit->GetWorld() != Manager->GetWorld() || Controller->GetCombatManager() != Manager || Unit->GetTeam() != ETeam::Player || !GetUnitId(Unit).IsValid() || !Manager->GetRegisteredUnits().Contains(Unit))
    {
        return false;
    }
    if (const APlayerUnit* Player = Cast<APlayerUnit>(Unit); Player && Player->IsServerAIControlled())
    {
        return false;
    }
    if (AllowsStandaloneLegacy(Controller))
    {
        return true;
    }
    if (!bRunConfigured)
    {
        return false;
    }
    const FRunAccountId* Participant = Participants.Find(TWeakObjectPtr<APartyPlayerController>(const_cast<APartyPlayerController*>(Controller)));
    const FGuid* CharacterId = CharacterIds.Find(TWeakObjectPtr<AUnitBase>(const_cast<AUnitBase*>(Unit)));
    return Participant && CharacterId && GetParticipantBindingId(Controller).IsValid() && URunIdentityLibrary::IsCharacterOwner(RunIdentity, PartyMembers, *CharacterId, *Participant);
}

FGuid UCombatActionAuthority::GetUnitId(const AUnitBase* Unit) const
{
    if (!IsValid(Unit))
    {
        return FGuid();
    }
    for (const TPair<FGuid, TWeakObjectPtr<AUnitBase>>& Entry : UnitsById)
    {
        if (Entry.Value.Get() == Unit)
        {
            return Entry.Key;
        }
    }
    return FGuid();
}

FGuid UCombatActionAuthority::GetCharacterId(const AUnitBase* Unit) const
{
    if (!bRunConfigured || !IsValid(Unit))
    {
        return FGuid();
    }
    return CharacterIds.FindRef(TWeakObjectPtr<AUnitBase>(const_cast<AUnitBase*>(Unit)));
}

FRunAccountId UCombatActionAuthority::GetOwnerAccountId(const AUnitBase* Unit) const
{
    const FGuid CharacterId = GetCharacterId(Unit);
    if (CharacterId.IsValid())
    {
        for (const FRunPartyMember& Member : PartyMembers)
        {
            if (Member.bCreated && Member.CharacterId == CharacterId)
            {
                return Member.OwnerAccountId;
            }
        }
    }
    return FRunAccountId();
}

AUnitBase* UCombatActionAuthority::ResolveUnit(FGuid UnitId) const
{
    const TWeakObjectPtr<AUnitBase>* Found = UnitsById.Find(UnitId);
    AUnitBase* Unit = Found ? Found->Get() : nullptr;
    const ACombatManager* Manager = GetManager();
    return IsValid(Unit) && IsValid(Manager) && Unit->GetWorld() == Manager->GetWorld() && Manager->GetRegisteredUnits().Contains(Unit) ? Unit : nullptr;
}

FCombatActionResponse UCombatActionAuthority::Execute(APartyPlayerController* Controller, const FCombatActionRequest& Request)
{
    FCombatActionResponse Response;
    Response.CombatInstanceId = Request.CombatInstanceId;
    Response.RequestSequence = Request.RequestSequence;
    const auto Reject = [&Response](ECombatRequestResult Result, const TCHAR* Message)
    {
        Response.Result = Result;
        Response.Message = FText::FromString(Message);
        return Response;
    };
    ACombatManager* Manager = GetManager();
    if (!IsValid(Manager) || !Manager->HasAuthority() || !IsValid(Controller) || Controller->GetWorld() != Manager->GetWorld() || Controller->GetCombatManager() != Manager || !Manager->IsCombatActive() || !CombatInstanceId.IsValid() || Request.CombatInstanceId != CombatInstanceId || (bRequiresRunConfiguration && !bRunConfigured) || Request.RunId != RunIdentity.RunId || Request.HostEpoch != RunIdentity.HostEpoch)
    {
        return Reject(ECombatRequestResult::InvalidContext, TEXT("현재 서버 전투와 일치하지 않거나 전투 입력을 받을 수 없습니다."));
    }
    if (Request.Version != 1 || Request.RequestSequence <= 0)
    {
        return Reject(ECombatRequestResult::InvalidRequest, TEXT("지원하지 않는 명령 버전 또는 요청 순번입니다."));
    }
    const TWeakObjectPtr<APartyPlayerController> ControllerKey(Controller);
    if (AllowsStandaloneLegacy(Controller))
    {
        if (Request.ParticipantBindingId.IsValid())
        {
            return Reject(ECombatRequestResult::InvalidContext, TEXT("오프라인 호환 명령에는 참가자 바인딩을 지정할 수 없습니다."));
        }
    }
    else
    {
        if (!bRunConfigured || !Participants.Contains(ControllerKey))
        {
            return Reject(ECombatRequestResult::UnboundParticipant, TEXT("이 연결에 원래 Run 참가자가 연결되지 않았습니다."));
        }
        const FGuid BindingId = GetParticipantBindingId(Controller);
        if (!BindingId.IsValid() || Request.ParticipantBindingId != BindingId)
        {
            return Reject(ECombatRequestResult::InvalidContext, TEXT("요청한 참가자 연결은 현재 바인딩과 일치하지 않습니다."));
        }
    }
    int64& LastSequence = LastRequestSequences.FindOrAdd(ControllerKey);
    if (Request.RequestSequence <= LastSequence)
    {
        return Reject(ECombatRequestResult::DuplicateRequest, TEXT("이미 처리했거나 순서가 지난 전투 요청입니다."));
    }
    // Reserve before mutable validation and dispatch so a rejected request cannot succeed on replay.
    // 거절된 요청이 재전송으로 성공하지 않도록 변경 가능한 검증과 실행 전에 순번을 기록합니다.
    LastSequence = Request.RequestSequence;
    if (!Controller->IsCombatInputEnabled())
    {
        return Reject(ECombatRequestResult::InvalidContext, TEXT("현재 서버 전투와 일치하지 않거나 전투 입력을 받을 수 없습니다."));
    }
    if (!Manager->GetTurnManager() || Request.TurnSerial != Manager->GetTurnManager()->GetTurnCounter())
    {
        return Reject(ECombatRequestResult::InvalidContext, TEXT("요청한 턴은 현재 턴이 아닙니다."));
    }
    if (!Request.UnitId.IsValid())
    {
        return Reject(ECombatRequestResult::InvalidRequest, TEXT("행동할 전투 유닛 식별자가 필요합니다."));
    }
    AUnitBase* Unit = ResolveUnit(Request.UnitId);
    if (!Unit)
    {
        return Reject(ECombatRequestResult::UnavailableUnit, TEXT("현재 전투에 등록되지 않은 유닛입니다."));
    }
    if (!CanControllerControl(Controller, Unit))
    {
        return Reject(ECombatRequestResult::NotOwner, TEXT("자신이 소유한 캐릭터만 직접 조작할 수 있습니다."));
    }
    return ExecuteUnitAction(Unit, Request);
}

bool UCombatActionAuthority::HasOriginalOwner(const APlayerUnit* Unit) const
{
    if (!bRunConfigured || !GetCharacterId(Unit).IsValid())
    {
        return false;
    }
    const FRunAccountId Owner = GetOwnerAccountId(Unit);
    const FRunParticipantData* Participant = RunIdentity.OriginalParticipants.FindByPredicate([&Owner](const FRunParticipantData& Entry) { return Entry.AccountId == Owner; });
    return Participant != nullptr;
}

bool UCombatActionAuthority::SetPartyControlMode(APlayerUnit* Unit, EPartyControlMode Mode, FText& OutError)
{
    OutError = FText::FromString(TEXT("원래 소유자가 확인된 유휴 아군의 조작 방식은 서버에서 전투 시작 전에만 설정할 수 있습니다."));
    ACombatManager* Manager = GetManager();
    if (!IsValid(Manager) || !Manager->HasAuthority() || Manager->GetTurnManager() || !bRunConfigured || !IsValid(Unit) || Unit->GetWorld() != Manager->GetWorld() || Unit->GetTeam() != ETeam::Player || !HasOriginalOwner(Unit) || !GetUnitId(Unit).IsValid() || !Manager->GetRegisteredUnits().Contains(Unit) || (Mode != EPartyControlMode::Human && Mode != EPartyControlMode::ServerAI))
    {
        return false;
    }
    if (!Unit->ApplyPartyControlMode(Mode))
    {
        return false;
    }
    Manager->PublishCombatView();
    OutError = FText::GetEmpty();
    return true;
}

FCombatActionResponse UCombatActionAuthority::ExecuteServerAI(APlayerUnit* Unit, const FCombatActionRequest& Request, FGuid ControlSessionId)
{
    FCombatActionResponse Response;
    Response.CombatInstanceId = Request.CombatInstanceId;
    Response.RequestSequence = Request.RequestSequence;
    Response.Result = ECombatRequestResult::InvalidContext;
    Response.Message = FText::FromString(TEXT("현재 서버 AI의 전투·조작 세션과 일치하지 않습니다."));
    ACombatManager* Manager = GetManager();
    if (!IsValid(Manager) || !Manager->HasAuthority() || !Manager->IsCombatActive() || !bRunConfigured || !IsValid(Unit) || Unit->GetTeam() != ETeam::Player || ResolveUnit(Request.UnitId) != Unit || !Unit->IsServerAIControlled() || !HasOriginalOwner(Unit) || !ControlSessionId.IsValid() || Unit->GetAIControlSessionId() != ControlSessionId || !CombatInstanceId.IsValid() || Request.CombatInstanceId != CombatInstanceId || Request.RunId != RunIdentity.RunId || Request.HostEpoch != RunIdentity.HostEpoch || Request.ParticipantBindingId.IsValid())
    {
        return Response;
    }
    if (Request.Version != 1 || Request.RequestSequence <= 0)
    {
        Response.Result = ECombatRequestResult::InvalidRequest;
        return Response;
    }
    int64& LastSequence = LastAIRequestSequences.FindOrAdd(ControlSessionId);
    if (Request.RequestSequence <= LastSequence)
    {
        Response.Result = ECombatRequestResult::DuplicateRequest;
        return Response;
    }
    // AI has its own execution session; rejected mutable requests cannot become valid through replay.
    // AI는 별도 실행 세션을 사용하며 변경 가능한 조건으로 거절된 요청은 재전송으로 승인되지 않습니다.
    LastSequence = Request.RequestSequence;
    return ExecuteUnitAction(Unit, Request);
}

FCombatActionResponse UCombatActionAuthority::ExecuteUnitAction(AUnitBase* Unit, const FCombatActionRequest& Request)
{
    FCombatActionResponse Response;
    Response.CombatInstanceId = Request.CombatInstanceId;
    Response.RequestSequence = Request.RequestSequence;
    const auto Reject = [&Response](ECombatRequestResult Result, const TCHAR* Message)
    {
        Response.Result = Result;
        Response.Message = FText::FromString(Message);
        return Response;
    };
    ACombatManager* Manager = GetManager();
    if (!Manager || !Manager->HasAuthority() || !Manager->IsCombatActive() || !Manager->GetTurnManager() || Request.TurnSerial != Manager->GetTurnManager()->GetTurnCounter())
    {
        return Reject(ECombatRequestResult::InvalidContext, TEXT("요청한 턴은 현재 서버 턴이 아닙니다."));
    }
    if (Unit != Manager->GetCurrentUnit() || !Unit->IsActiveTurn() || !Unit->IsUnitAlive() || Unit->IsBusy())
    {
        return Reject(ECombatRequestResult::UnavailableUnit, TEXT("현재 턴의 생존하고 행동 중이 아닌 유닛만 명령할 수 있습니다."));
    }
    if (Request.Kind != ECombatActionKind::Skill && Request.SkillId != FPrimaryAssetId())
    {
        return Reject(ECombatRequestResult::InvalidRequest, TEXT("이 행동에는 스킬 식별자를 지정할 수 없습니다."));
    }
    if (Request.Kind == ECombatActionKind::EndTurn)
    {
        if (Request.TargetUnitId.IsValid() || Request.TargetCoord != FIntPoint::ZeroValue)
        {
            return Reject(ECombatRequestResult::InvalidRequest, TEXT("턴 종료에는 대상을 지정할 수 없습니다."));
        }
        if (!Manager->RequestEndTurnForUnit(Unit))
        {
            return Reject(ECombatRequestResult::UnavailableUnit, TEXT("현재 유닛의 턴을 종료할 수 없습니다."));
        }
    }
    else if (Request.Kind == ECombatActionKind::Move || Request.Kind == ECombatActionKind::Skill || Request.Kind == ECombatActionKind::HealingItem)
    {
        ACombatGridTile* Tile = Manager->GetTileByCoord(Request.TargetCoord);
        if (!IsValid(Tile) || Tile->GetWorld() != Manager->GetWorld())
        {
            return Reject(ECombatRequestResult::InvalidTarget, TEXT("현재 전투 Grid에 없는 대상 좌표입니다."));
        }
        if (Request.Kind == ECombatActionKind::Move)
        {
            if (Request.TargetUnitId.IsValid())
            {
                return Reject(ECombatRequestResult::InvalidRequest, TEXT("이동은 유닛 대신 빈 타일을 대상으로 지정해야 합니다."));
            }
            if (!Manager->CalculateReachableMoveTiles(Unit).Contains(Tile))
            {
                return Reject(ECombatRequestResult::InvalidTarget, TEXT("현재 위치에서 이동할 수 없는 타일입니다."));
            }
            if (!Unit->HasEnoughSubActionPoint(1))
            {
                return Reject(ECombatRequestResult::InsufficientResources, TEXT("이동에 필요한 SubAP가 부족합니다."));
            }
            Unit->StartMoveAction(Tile);
        }
        else if (Request.Kind == ECombatActionKind::HealingItem)
        {
            AUnitBase* Target = ResolveUnit(Request.TargetUnitId);
            if (!Target || Target->GetCurrentTile() != Tile || Tile->GetOccupyingUnit() != Target || Target->GetTeam() != Unit->GetTeam() || !Target->IsUnitAlive())
            {
                return Reject(ECombatRequestResult::InvalidTarget, TEXT("회복 대상과 대상 타일이 현재 아군 유닛과 일치하지 않습니다."));
            }
            if (!Unit->CanUseHealingItem(Target))
            {
                return Reject(ECombatRequestResult::InsufficientResources, TEXT("회복약을 사용할 자원이나 회복 가능한 HP가 없습니다."));
            }
            Unit->StartItemAction(Target);
        }
        else
        {
            USkillDefinitionDataAsset* Skill = nullptr;
            for (USkillDefinitionDataAsset* Candidate : Unit->GetEquippedSkillDataAssets())
            {
                if (IsValid(Candidate) && Candidate->GetPrimaryAssetId() == Request.SkillId)
                {
                    if (Skill)
                    {
                        return Reject(ECombatRequestResult::InvalidSkill, TEXT("장착 스킬 식별자가 중복되어 명령을 해석할 수 없습니다."));
                    }
                    Skill = Candidate;
                }
            }
            UAbilitySystemComponent* ASC = Unit->GetAbilitySystemComponent();
            FGameplayAbilitySpec* Spec = Skill && Skill->AbilityClass && ASC ? ASC->FindAbilitySpecFromClass(Skill->AbilityClass) : nullptr;
            if (!Request.SkillId.IsValid() || !UCombatTargetingLibrary::IsSupportedSkillArea(Skill) || !Skill->AbilityClass || Skill->ActionPointCost <= 0 || !Spec || Spec->IsActive())
            {
                return Reject(ECombatRequestResult::InvalidSkill, TEXT("실제로 장착하고 부여받은 사용 가능한 스킬이 아닙니다."));
            }
            const bool bNeedsTargetUnit = Skill->bMoveToTarget || Skill->TargetRule == ESkillTargetRule::EnemyUnit || Skill->TargetRule == ESkillTargetRule::AllyUnit || Skill->TargetRule == ESkillTargetRule::AnyUnit;
            if (bNeedsTargetUnit)
            {
                AUnitBase* Target = ResolveUnit(Request.TargetUnitId);
                if (!Target || Target->GetCurrentTile() != Tile || Tile->GetOccupyingUnit() != Target)
                {
                    return Reject(ECombatRequestResult::InvalidTarget, TEXT("대상 유닛이 선택한 타일에 더 이상 존재하지 않습니다."));
                }
            }
            else if (Request.TargetUnitId.IsValid())
            {
                return Reject(ECombatRequestResult::InvalidRequest, TEXT("타일 대상 스킬에는 유닛 식별자를 지정할 수 없습니다."));
            }
            if (!UCombatTargetingLibrary::IsValidSkillTarget(Unit, Skill, Tile))
            {
                return Reject(ECombatRequestResult::InvalidTarget, TEXT("스킬의 현재 대상 규칙을 만족하지 않습니다."));
            }
            if (!Unit->HasEnoughActionPoint(Skill->ActionPointCost))
            {
                return Reject(ECombatRequestResult::InsufficientResources, TEXT("스킬에 필요한 AP가 부족합니다."));
            }
            Unit->StartSkill(Skill, Tile);
        }
    }
    else
    {
        return Reject(ECombatRequestResult::InvalidRequest, TEXT("지원하지 않는 전투 행동 종류입니다."));
    }
    Response.Result = ECombatRequestResult::Accepted;
    Response.Message = FText::GetEmpty();
    return Response;
}
