#include "Combat/Commands/CombatActionAuthority.h"
#include "Combat/CombatManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Controller/PartyPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/Run/RunIdentityLibrary.h"
#include "Game/Run/RunParticipationLibrary.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Unit/UnitBase.h"
#include "Unit/PlayerUnit.h"

ACombatManager* UCombatActionAuthority::GetManager() const
{
    return GetTypedOuter<ACombatManager>();
}

bool UCombatActionAuthority::HasManagedExecutionAuthority(bool bAllowResumePending) const
{
    const ACombatManager* Manager = GetManager();
    const URunStateSubsystem* Run = IsValid(Manager) && Manager->GetGameInstance() ? Manager->GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    if (!bManagedExecution)
    {
        return !Run || !Run->IsManagedRun();
    }
    if (!bRunConfigured || !Run || !Run->IsManagedRun() || !Run->HasManagedLease() || (!bAllowResumePending && Run->IsManagedResumePending()) || Run->GetRunIdentity().Origin != ERunIdentityOrigin::LocalDevelopment || Run->GetLocalCaller() != RunIdentity.HostAccountId || !ManagedSessionId.IsValid() || ManagedSessionId != Run->GetManagedStamp().SessionId || !FRunIdentityData::StaticStruct()->CompareScriptStruct(&RunIdentity, &Run->GetRunIdentity(), 0))
    {
        return false;
    }
    FText Error;
    return URunParticipationLibrary::Validate(Run->GetParticipation(), RunIdentity, PartyMembers, Error);
}

bool UCombatActionAuthority::IsManagedHumanParticipant(const FRunAccountId& AccountId) const
{
    if (!bManagedExecution)
    {
        return true;
    }
    const ACombatManager* Manager = GetManager();
    const URunStateSubsystem* Run = IsValid(Manager) && Manager->GetGameInstance() ? Manager->GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    return Run && Run->IsManagedRun() && Run->GetParticipation().HumanParticipants.Contains(AccountId);
}

void UCombatActionAuthority::Reset()
{
    // Preserve Run and managed requirements so clearing a configuration or lease never opens offline compatibility.
    // 설정이나 lease를 지워도 오프라인 호환이 열리지 않도록 Run 및 관리 실행 필요 여부를 유지합니다.
    for (const TPair<TWeakObjectPtr<APartyPlayerController>, FRunAccountId>& Entry : Participants)
    {
        if (Entry.Key.IsValid())
        {
            Entry.Key->SetCombatParticipantBinding(FRunAccountId(), FGuid());
        }
    }
    UnitsById.Reset();
    CharacterIds.Reset();
    PartySlots.Reset();
    Participants.Reset();
    ParticipantBindingIds.Reset();
    CombatInstanceId.Invalidate();
    ManagedSessionId.Invalidate();
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
    if (!IsValid(Manager) || !Manager->HasAuthority() || !HasManagedExecutionAuthority(true))
    {
        return;
    }
    CombatInstanceId = FGuid::NewGuid();
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

bool UCombatActionAuthority::ConfigureRun(const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, const TMap<int32, TObjectPtr<AUnitBase>>& PartyActors, FText& OutError, bool bManaged)
{
    ACombatManager* Manager = GetManager();
    if (!IsValid(Manager) || !Manager->HasAuthority() || Manager->IsCombatActive() || Manager->GetCombatViewState().bSuspendedForRecovery || IsValid(Manager->GetRoundCoordinator()))
    {
        OutError = NSLOCTEXT("CombatRequest", "ConfigureContext", "Run 소유권은 서버에서 전투 시작 전에 설정해야 합니다.");
        return false;
    }
    bRequiresRunConfiguration = true;
    bRunConfigured = false;
    CharacterIds.Reset();
    PartySlots.Reset();
    const URunStateSubsystem* Run = Manager->GetGameInstance() ? Manager->GetGameInstance()->GetSubsystem<URunStateSubsystem>() : nullptr;
    bManagedExecution = bManagedExecution || bManaged || (Run && Run->IsManagedRun());
    ManagedSessionId.Invalidate();
    if (bManagedExecution)
    {
        OutError = NSLOCTEXT("CombatRequest", "ManagedConfigure", "관리 전투는 현재 Host의 유효한 실행 lease와 동일한 Run 데이터로만 설정할 수 있습니다.");
        if (!bManaged || !Run || !Run->IsManagedRun() || !Run->HasManagedLease() || Identity.Origin != ERunIdentityOrigin::LocalDevelopment || Run->GetLocalCaller() != Identity.HostAccountId || !FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Run->GetRunIdentity(), 0) || Members.Num() != Run->GetPartyMembers().Num())
        {
            return false;
        }
        for (int32 Index = 0; Index < Members.Num(); ++Index)
        {
            if (!FRunPartyMember::StaticStruct()->CompareScriptStruct(&Members[Index], &Run->GetPartyMembers()[Index], 0))
            {
                return false;
            }
        }
        if (!URunParticipationLibrary::Validate(Run->GetParticipation(), Identity, Members, OutError))
        {
            return false;
        }
    }
    if (!URunIdentityLibrary::ValidateIdentity(Identity, Members, OutError))
    {
        return false;
    }
    TMap<TWeakObjectPtr<AUnitBase>, FGuid> NewCharacterIds;
    TMap<TWeakObjectPtr<AUnitBase>, int32> NewPartySlots;
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
        NewPartySlots.Add(UnitKey, Member.SlotIndex);
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
    PartySlots = MoveTemp(NewPartySlots);
    bRunConfigured = true;
    ManagedSessionId = bManagedExecution ? Run->GetManagedStamp().SessionId : FGuid();
    OutError = FText::GetEmpty();
    return true;
}

bool UCombatActionAuthority::BindParticipant(APartyPlayerController* Controller, const FRunAccountId& AccountId)
{
    ACombatManager* Manager = GetManager();
    if (!IsValid(Manager) || !Manager->HasAuthority() || !IsValid(Controller) || Controller->GetWorld() != Manager->GetWorld() || !bRunConfigured || !HasManagedExecutionAuthority(true) || !IsManagedHumanParticipant(AccountId) || !URunIdentityLibrary::IsOriginalParticipant(RunIdentity, AccountId))
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
    return !bManagedExecution && IsValid(Manager) && IsValid(Controller) && Manager->GetNetMode() == NM_Standalone && Controller->IsLocalController() && RunIdentity.Origin == ERunIdentityOrigin::LegacyOffline && (!bRequiresRunConfiguration || bRunConfigured);
}

bool UCombatActionAuthority::CanControllerControl(const APartyPlayerController* Controller, const AUnitBase* Unit) const
{
    const ACombatManager* Manager = GetManager();
    if (!IsValid(Manager) || !Manager->HasAuthority() || !HasManagedExecutionAuthority(false) || !IsValid(Controller) || !IsValid(Unit) || Controller->GetWorld() != Manager->GetWorld() || Unit->GetWorld() != Manager->GetWorld() || Controller->GetCombatManager() != Manager || Unit->GetTeam() != ETeam::Player || !GetUnitId(Unit).IsValid() || !Manager->GetRegisteredUnits().Contains(Unit))
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
    return Participant && CharacterId && IsManagedHumanParticipant(*Participant) && GetParticipantBindingId(Controller).IsValid() && URunIdentityLibrary::IsCharacterOwner(RunIdentity, PartyMembers, *CharacterId, *Participant);
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

int32 UCombatActionAuthority::GetPartySlot(const AUnitBase* Unit) const
{
    if (!bRunConfigured || !IsValid(Unit)) return INDEX_NONE;
    const int32* Slot = PartySlots.Find(TWeakObjectPtr<AUnitBase>(const_cast<AUnitBase*>(Unit)));
    return Slot ? *Slot : INDEX_NONE;
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
    Response.Result = ECombatRequestResult::InvalidRequest;
    Response.Message = NSLOCTEXT("CombatRequest", "SequentialRemoved", "개별 턴 즉시 실행은 지원하지 않습니다. 라운드 행동 계획을 제출해 주세요.");
    return Response;
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
    if (!IsValid(Manager) || !Manager->HasAuthority() || !HasManagedExecutionAuthority(true) || Manager->IsCombatActive() || Manager->GetCombatViewState().bSuspendedForRecovery || IsValid(Manager->GetRoundCoordinator()) || !bRunConfigured || !IsValid(Unit) || Unit->GetWorld() != Manager->GetWorld() || Unit->GetTeam() != ETeam::Player || !HasOriginalOwner(Unit) || !GetUnitId(Unit).IsValid() || !Manager->GetRegisteredUnits().Contains(Unit) || (Mode != EPartyControlMode::Human && Mode != EPartyControlMode::ServerAI))
    {
        return false;
    }
    if (bManagedExecution && Mode != (IsManagedHumanParticipant(GetOwnerAccountId(Unit)) ? EPartyControlMode::Human : EPartyControlMode::ServerAI))
    {
        OutError = NSLOCTEXT("CombatRequest", "ManagedControlMode", "관리 Run의 영속 인간 참가 목록과 다른 조작 방식은 적용할 수 없습니다.");
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
    return Execute(nullptr, Request);
}
