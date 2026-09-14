#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunIdentityLibrary.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunParticipationLibrary.h"
#include "Combat/Checkpoint/CombatCheckpointLibrary.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/RunEncounterPoolDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"
#include "Game/Development/DevelopmentCoopSubsystem.h"

void URunStateSubsystem::ResetDevelopmentRun()
{
    if (!UDevelopmentCoopSubsystem::IsAvailable() || bManagedRun || ManagedLease) return;
    bCheckpointSaving = false;
    SaveSlot = ResolveCheckpointSlot(FCommandLine::Get());
    RunIdentity = FRunIdentityData();
    Participation = FRunParticipationData();
    EncounterProgress = FRunEncounterProgress();
    PartyMembers.Reset();
    Nodes.Reset();
    CompletedNodes.Reset();
    CurrentNodeId = CurrentEncounterId = NAME_None;
    Phase = ERunPhase::None;
    LastResult = ECombatResult::None;
    CombatCheckpoint = FCombatCheckpointData();
    SaveError = FText::GetEmpty();
    PartyDefinition = nullptr;
    OnRunStateChanged.Broadcast();
}

namespace
{
    bool IsValidLocalCaller(const FLocalDevelopmentCallerContext& Context)
    {
        if (!FLocalRunAuthorityStore(Context.StoreNamespace).IsValid() || Context.AccountId.Provider != TEXT("Development") || Context.AccountId.Subject.IsEmpty() || Context.AccountId.Subject.Len() > 256) return false;
        for (TCHAR Character : Context.AccountId.Subject)
        {
            if (Character < 33 || Character > 126) return false;
        }
        return true;
    }
}

URunStateSubsystem::URunStateSubsystem()
{
    SaveSlot = ResolveCheckpointSlot(FCommandLine::Get());
}

void URunStateSubsystem::Deinitialize()
{
    ClearManagedMenuTravel();
    OnRunStateChanged.Clear();
    CloseManagedRun();
    Super::Deinitialize();
}

FString URunStateSubsystem::ResolveCheckpointSlot(const TCHAR* CommandLine)
{
    FString ExplicitSlot;
    if (FParse::Value(CommandLine, TEXT("ProjectASaveSlot="), ExplicitSlot) && !ExplicitSlot.TrimStartAndEnd().IsEmpty())
    {
        return ExplicitSlot;
    }
    FString OpponentSlot;
    if (FParse::Value(CommandLine, TEXT("ProjectAOpponentSnapshot="), OpponentSlot) || FParse::Param(CommandLine, TEXT("ProjectAOpponentSnapshot")))
    {
        const FName SlotId = OpponentSlot.Len() <= 64 ? FName(*OpponentSlot) : NAME_None;
        if (!UPartySnapshotLibrary::GetSaveSlotName(SlotId).IsEmpty())
        {
            return TEXT("ProjectA_SnapshotRun_") + SlotId.ToString();
        }
        // Invalid launch arguments must remain isolated while gameplay reports the selection error.
        // 잘못된 실행 인수도 Gameplay에서 선택 오류를 알리는 동안 일반 진행과 분리합니다.
        return TEXT("ProjectA_RejectedSnapshotRun");
    }
    return TEXT("ProjectA_Run");
}

void URunStateSubsystem::EnableCheckpointSaving(const FString& Slot)
{
    if (!Slot.IsEmpty())
    {
        SaveSlot = Slot;
    }
    bCheckpointSaving = true;
}

void URunStateSubsystem::AutoSaveCheckpoint()
{
    if (bCheckpointSaving && Phase != ERunPhase::Preparing && Phase != ERunPhase::Combat)
    {
        SaveCheckpoint(SaveError);
    }
}

bool URunStateSubsystem::ValidateEncounterProgress(const URunSaveGame* Save) const
{
    const FRunEncounterProgress& Progress = Save->EncounterProgress;
    const bool bChoice = Save->Phase == ERunPhase::EncounterChoice;
    const bool bShop = Save->Phase == ERunPhase::Shop;
    if (Progress.SchemaVersion == 0)
    {
        return !bChoice && !bShop && Progress.Offers.IsEmpty() && Progress.SelectedEncounterId.IsNone() && !Progress.bCompleted;
    }
    if (Progress.SchemaVersion != 1 || Progress.Offers.Num() != 3 || Save->Identity.Origin == ERunIdentityOrigin::LegacyOffline) return false;
    TSet<FName> Ids;
    for (const FRunEncounterOffer& Offer : Progress.Offers)
    {
        if (Offer.EncounterId.IsNone() || Ids.Contains(Offer.EncounterId) || Offer.DisplayName.ToString().TrimStartAndEnd().IsEmpty() || Offer.Type != ERunEncounterType::Shop) return false;
        Ids.Add(Offer.EncounterId);
    }
    const bool bSelected = !Progress.SelectedEncounterId.IsNone();
    if ((bSelected && !Ids.Contains(Progress.SelectedEncounterId)) || (Progress.bCompleted && !bSelected)) return false;
    const int32 Completed = Save->CompletedNodes.Num();
    if (Completed == 0) return !bChoice && !bShop && !bSelected && !Progress.bCompleted;
    if (bChoice || bShop)
    {
        return Completed == 1 && Save->CurrentNode == Save->Nodes[0].NodeId && Save->CurrentEncounter.IsNone() && Save->Result == ECombatResult::Victory && !Progress.bCompleted && bSelected == bShop;
    }
    if (Save->Phase == ERunPhase::Result && Completed == 1) return !bSelected && !Progress.bCompleted;
    return Progress.bCompleted;
}

bool URunStateSubsystem::ValidateSave(const URunSaveGame* Save, FText& OutError) const
{
    OutError = FText::FromString(TEXT("저장 파일이 손상되었거나 현재 버전·직업 설정과 호환되지 않습니다."));
    if (!Save || (Save->Version != 1 && Save->Version != 2 && Save->Version != 3 && Save->Version != 4) || Save->Party.IsEmpty() || Save->Party.Num() > 4 || Save->Nodes.Num() != 2 || Save->CompletedNodes.Num() > 2)
    {
        return false;
    }
    if (!ValidateEncounterProgress(Save)) return false;
    // Legacy saves remain offline; missing or damaged ownership must never downgrade a new save.
    // 기존 저장은 오프라인으로 유지하며 새 저장의 누락·손상된 소유권을 구버전으로 우회하지 않습니다.
    if ((Save->Version == 1) != (Save->Identity.Origin == ERunIdentityOrigin::LegacyOffline))
    {
        return false;
    }
    const bool bManaged = Save->Version == 4;
    const FRunParticipationData EmptyParticipation;
    if (bManaged)
    {
        if (Save->Identity.Origin != ERunIdentityOrigin::LocalDevelopment || Save->Identity.SchemaVersion != URunIdentityLibrary::CurrentSchemaVersion || Save->Identity.OriginalParticipants.Num() < 2) return false;
        FText ParticipationError;
        if (!URunParticipationLibrary::Validate(Save->Participation, Save->Identity, Save->Party, ParticipationError))
        {
            OutError = ParticipationError;
            return false;
        }
    }
    else if (!FRunParticipationData::StaticStruct()->CompareScriptStruct(&Save->Participation, &EmptyParticipation, 0))
    {
        return false;
    }
    const FCombatCheckpointData EmptyCheckpoint;
    if (Save->Version != 3 && !(bManaged && Save->Phase == ERunPhase::Combat) && !FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Save->CombatCheckpoint, &EmptyCheckpoint, 0))
    {
        return false;
    }
    FText IdentityError;
    if (!URunIdentityLibrary::ValidateIdentity(Save->Identity, Save->Party, IdentityError))
    {
        OutError = IdentityError;
        return false;
    }
    UPartyDefinitionDataAsset* Catalog = Cast<UPartyDefinitionDataAsset>(Save->Catalog.TryLoad());
    if (!Catalog)
    {
        return false;
    }
    TSet<int32> Slots;
    int32 Created = 0;
    int32 Living = 0;
    for (const FRunPartyMember& Member : Save->Party)
    {
        if (Member.SlotIndex < 0 || Member.SlotIndex >= 4 || Slots.Contains(Member.SlotIndex) || !FMath::IsFinite(Member.CurrentHP) || Member.CurrentHP < -1.0f)
        {
            return false;
        }
        Slots.Add(Member.SlotIndex);
        if (Member.bCreated)
        {
            FProfessionDefinition Definition;
            const bool bInitialHP = Member.CurrentHP == -1.0f && Save->Phase == ERunPhase::Map && Save->CompletedNodes.IsEmpty();
            if (Member.CharacterName.ToString().TrimStartAndEnd().IsEmpty() || !Catalog->ResolveProfession(Member.ClassId, Definition) || (Member.CurrentHP < 0.0f && !bInitialHP))
            {
                return false;
            }
            ++Created;
            Living += Member.CurrentHP != 0.0f ? 1 : 0;
        }
    }
    for (int32 Index = 0; Index < Save->Nodes.Num(); ++Index)
    {
        const FName Expected(*FString::Printf(TEXT("Combat_%02d"), Index + 1));
        if (Save->Nodes[Index].NodeId != Expected || Save->Nodes[Index].EncounterId != TEXT("DefaultEncounter") || Save->Nodes[Index].NodeType != ERunNodeType::Combat || (Save->CompletedNodes.IsValidIndex(Index) && Save->CompletedNodes[Index] != Expected))
        {
            return false;
        }
    }
    const int32 Completed = Save->CompletedNodes.Num();
    const bool bMapEntry = Save->Result == ECombatResult::None && Save->CurrentNode.IsNone();
    const bool bMapContinue = Completed > 0 && Save->Result == ECombatResult::Victory && Save->CurrentNode == Save->Nodes[Completed - 1].NodeId;
    const bool bMap = Save->Phase == ERunPhase::Map && Completed < 2 && Save->CurrentEncounter.IsNone() && (bMapEntry || bMapContinue);
    const bool bResult = Save->Phase == ERunPhase::Result && Completed > 0 && Save->Result == ECombatResult::Victory && Save->CurrentNode == Save->Nodes[Completed - 1].NodeId && Save->CurrentEncounter == TEXT("DefaultEncounter");
    const bool bComplete = Save->Phase == ERunPhase::Complete && Completed == 2 && Save->Result == ECombatResult::Victory && Save->CurrentNode == Save->Nodes.Last().NodeId && Save->CurrentEncounter.IsNone();
    const bool bDefeat = Save->Phase == ERunPhase::Defeat && Completed < 2 && Save->Result == ECombatResult::Defeat && Living == 0 && Save->CurrentNode == Save->Nodes[Completed].NodeId && Save->CurrentEncounter == TEXT("DefaultEncounter");
    const bool bCombat = (Save->Version == 3 || bManaged) && Save->Phase == ERunPhase::Combat && Completed < 2 && Save->Result == ECombatResult::None && Save->CurrentNode == Save->Nodes[Completed].NodeId && Save->CurrentEncounter == TEXT("DefaultEncounter");
    const bool bRunEncounter = Save->Phase == ERunPhase::EncounterChoice || Save->Phase == ERunPhase::Shop;
    if (Created == 0 || (!bMap && !bResult && !bComplete && !bDefeat && !bCombat && !bRunEncounter) || (!bDefeat && Living == 0) || (Save->Version == 3 && !bCombat))
    {
        return false;
    }
    if (bCombat)
    {
        const FCombatCheckpointData& Checkpoint = Save->CombatCheckpoint;
        if (!FRunIdentityData::StaticStruct()->CompareScriptStruct(&Save->Identity, &Checkpoint.Identity, 0) || Checkpoint.NodeId != Save->CurrentNode || Checkpoint.EncounterId != Save->CurrentEncounter || !UCombatCheckpointLibrary::Validate(Checkpoint, Save->Party, OutError))
        {
            return false;
        }
        for (const FCombatCheckpointUnit& Unit : Checkpoint.Units)
        {
            if (Unit.Team == ETeam::Player)
            {
                const FRunPartyMember* Member = Save->Party.FindByPredicate([&Unit](const FRunPartyMember& Candidate) { return Candidate.bCreated && Candidate.SlotIndex == Unit.PartySlot; });
                if (!Member || Member->CurrentHP != Unit.HP)
                {
                    OutError = NSLOCTEXT("RunCheckpoint", "PartyHP", "전투 체크포인트와 파티의 체력이 일치하지 않습니다.");
                    return false;
                }
                EPartyControlMode ExpectedMode = EPartyControlMode::Human;
                if (bManaged && (!URunParticipationLibrary::ResolveControlMode(Save->Participation, Save->Identity, Save->Party, Unit.CharacterId, ExpectedMode, OutError) || Unit.PartyControlMode != ExpectedMode))
                {
                    OutError = NSLOCTEXT("RunCheckpoint", "ManagedMode", "전투 조작 방식이 관리 Run의 영구 참여 상태와 일치하지 않습니다.");
                    return false;
                }
            }
        }
    }
    OutError = FText::GetEmpty();
    return true;
}

URunSaveGame* URunStateSubsystem::CreateSaveData() const
{
    URunSaveGame* Save = NewObject<URunSaveGame>();
    Save->Version = bManagedRun ? 4 : RunIdentity.Origin == ERunIdentityOrigin::LegacyOffline ? 1 : 2;
    Save->Identity = RunIdentity;
    Save->Participation = Participation;
    Save->EncounterProgress = EncounterProgress;
    Save->Party = PartyMembers;
    Save->Nodes = Nodes;
    Save->CompletedNodes = CompletedNodes;
    Save->CurrentNode = CurrentNodeId;
    Save->CurrentEncounter = CurrentEncounterId;
    Save->Phase = Phase;
    Save->Result = LastResult;
    Save->Catalog = FSoftObjectPath(PartyDefinition);
    return Save;
}

bool URunStateSubsystem::WriteSaveData(URunSaveGame* Save, FText& OutError)
{
    if (!ValidateSave(Save, OutError)) return false;
    if (!bManagedRun)
    {
        if (Save->Version == 4) return false;
        return FRunCheckpointStorage::Save(Save, SaveSlot, OutError);
    }
    if (Save->Version != 4 || !HasManagedLease() || !FRunIdentityData::StaticStruct()->CompareScriptStruct(&Save->Identity, &RunIdentity, 0) || !FRunParticipationData::StaticStruct()->CompareScriptStruct(&Save->Participation, &Participation, 0))
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedLease", "현재 관리 Run Host의 유효한 실행 lease가 있어야 저장할 수 있습니다.");
        return false;
    }
    TArray<uint8> Payload;
    if (!UGameplayStatics::SaveGameToMemory(Save, Payload))
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedSerialize", "관리 Run을 저장 데이터로 변환하지 못했습니다.");
        return false;
    }
    FRunAuthorityRecordData Record;
    if (FLocalRunAuthorityStore(LocalCallerContext.StoreNamespace).Commit(*ManagedLease, ManagedStamp.Revision, Payload, Record, OutError) != ERunAuthorityResult::Success) return false;
    ManagedStamp = Record.Stamp;
    return true;
}

bool URunStateSubsystem::SaveCheckpoint(FText& OutError)
{
    if (Phase == ERunPhase::Combat)
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ExplicitBoundary", "시간 기반 라운드 전투의 중간 저장·복구는 아직 지원하지 않습니다. 마지막 전투 외 저장은 보존됩니다.");
        SaveError = OutError;
        return false;
    }
    TStrongObjectPtr<URunSaveGame> Save(CreateSaveData());
    if (!WriteSaveData(Save.Get(), OutError))
    {
        SaveError = OutError;
        return false;
    }
    SaveError = FText::GetEmpty();
    return true;
}

bool URunStateSubsystem::CommitCombatCheckpoint(const FCombatCheckpointData&, FText& OutError)
{
    // Sequential checkpoint publication is retired; preserve the last durable non-combat record.
    // 순차 턴 체크포인트 저장을 폐기하고 마지막 전투 외 확정 기록을 보존합니다.
    OutError = NSLOCTEXT("RunCheckpoint", "TimedRoundRecoveryUnsupported", "기존 순차 턴 체크포인트를 새 전투에 저장할 수 없습니다. 시간 기반 라운드의 중간 저장·복구는 아직 지원하지 않습니다.");
    SaveError = OutError;
    return false;
}

bool URunStateSubsystem::ValidateCheckpointHost(const FRunAccountId& AccountId, FText& OutError) const
{
    OutError = NSLOCTEXT("RunCheckpoint", "Host", "기존 Host만 이 전투 체크포인트를 복구할 수 있습니다.");
    if (!HasCombatCheckpoint() || AccountId.IsEmpty() || AccountId != RunIdentity.HostAccountId || !FRunIdentityData::StaticStruct()->CompareScriptStruct(&RunIdentity, &CombatCheckpoint.Identity, 0))
    {
        return false;
    }
    if (!URunIdentityLibrary::ValidateIdentity(RunIdentity, PartyMembers, OutError))
    {
        return false;
    }
    OutError = FText::GetEmpty();
    return true;
}

bool URunStateSubsystem::CanContinueSavedRun(FText& OutError) const
{
    return CanContinueSavedRunInternal(false, OutError);
}

bool URunStateSubsystem::CanContinueStandaloneSavedRun(FText& OutError) const
{
    return CanContinueSavedRunInternal(true, OutError);
}

bool URunStateSubsystem::CanContinueSavedRunInternal(bool bStandaloneOnly, FText& OutError) const
{
    if (bManagedRun || ManagedLease)
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedActive", "현재 관리 세션을 먼저 닫아야 다른 진행을 불러올 수 있습니다.");
        return false;
    }
    TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(FRunCheckpointStorage::Load(SaveSlot, OutError)));
    return ValidateContinuableSave(Save.Get(), bStandaloneOnly, OutError);
}

bool URunStateSubsystem::ValidateContinuableSave(const URunSaveGame* Save, bool bStandaloneOnly, FText& OutError) const
{
    if (!Save)
    {
        OutError = NSLOCTEXT("RunCheckpoint", "InvalidSave", "이어할 저장이 없거나 Run 저장 파일이 아닙니다.");
        return false;
    }
    if (!ValidateSave(Save, OutError))
    {
        return false;
    }
    if (Save->Phase == ERunPhase::Combat)
    {
        OutError = NSLOCTEXT("RunCheckpoint", "RetiredCombatSave", "기존 순차 턴 전투 저장은 새 라운드 전투에서 이어할 수 없습니다. 새 전투의 중간 저장·복구는 아직 지원하지 않으며 기존 저장 파일은 보존됩니다.");
        return false;
    }
    if (Save->Version == 4)
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedResumeRequired", "관리 Run은 기준 저장소에서 실행 lease를 획득하는 명시적 재개를 사용해야 합니다.");
        return false;
    }
    if (Save->Phase == ERunPhase::Defeat || Save->Phase == ERunPhase::Complete)
    {
        OutError = FText::FromString(TEXT("종료된 진행입니다. 새 게임을 시작해 주세요."));
        return false;
    }
    if (bStandaloneOnly && Save->Identity.Origin != ERunIdentityOrigin::LegacyOffline && (Save->Identity.Origin != ERunIdentityOrigin::LocalDevelopment || Save->Identity.OriginalParticipants.Num() != 1))
    {
        OutError = NSLOCTEXT("RunCheckpoint", "SessionRequired", "계정 연결 또는 협동 세션이 필요한 저장입니다. 현재 싱글플레이 이어하기 대신 기존 Host와 원래 참가자가 연결된 세션에서 복원해야 합니다.");
        return false;
    }
    return true;
}

bool URunStateSubsystem::LoadCheckpoint(FText& OutError)
{
    return LoadCheckpointInternal(false, OutError);
}

bool URunStateSubsystem::LoadStandaloneCheckpoint(FText& OutError)
{
    return LoadCheckpointInternal(true, OutError);
}

bool URunStateSubsystem::LoadCheckpointInternal(bool bStandaloneOnly, FText& OutError)
{
    if (bManagedRun || ManagedLease)
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedActive", "현재 관리 세션을 먼저 닫아야 다른 진행을 불러올 수 있습니다.");
        return false;
    }
    TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(FRunCheckpointStorage::Load(SaveSlot, OutError)));
    // Recheck the very same loaded object before changing memory, even if the menu checked an older file.
    // 메뉴가 이전 파일을 검사했더라도 메모리를 바꾸기 전에 방금 읽은 동일 객체를 다시 검증합니다.
    if (!ValidateContinuableSave(Save.Get(), bStandaloneOnly, OutError))
    {
        return false;
    }
    ApplySaveData(Save.Get());
    SaveError = FText::GetEmpty();
    bCheckpointSaving = true;
    OnRunStateChanged.Broadcast();
    return true;
}

void URunStateSubsystem::ApplySaveData(const URunSaveGame* Save)
{
    RunIdentity = Save->Identity;
    Participation = Save->Participation;
    EncounterProgress = Save->EncounterProgress;
    bManagedRun = Save->Version == 4;
    PartyMembers = Save->Party;
    Nodes = Save->Nodes;
    CompletedNodes = Save->CompletedNodes;
    CurrentNodeId = Save->CurrentNode;
    CurrentEncounterId = Save->CurrentEncounter;
    Phase = Save->Phase;
    LastResult = Save->Result;
    CombatCheckpoint = Save->CombatCheckpoint;
    PartyDefinition = Cast<UPartyDefinitionDataAsset>(Save->Catalog.ResolveObject());
}

bool URunStateSubsystem::ConfigureLocalDevelopmentCaller(const FLocalDevelopmentCallerContext& Context, FText& OutError)
{
    OutError = NSLOCTEXT("ManagedRun", "CallerContext", "유효한 개발용 호출자 문맥이 필요하며 같은 GameInstance에서 계정이나 저장소를 변경할 수 없습니다.");
    if (!IsValidLocalCaller(Context)) return false;
    if (bHasLocalCallerContext && (LocalCallerContext.StoreNamespace != Context.StoreNamespace.ToLower() || LocalCallerContext.AccountId != Context.AccountId)) return false;
    LocalCallerContext = Context;
    LocalCallerContext.StoreNamespace = Context.StoreNamespace.ToLower();
    bHasLocalCallerContext = true;
    OutError = FText::GetEmpty();
    return true;
}

bool URunStateSubsystem::HasManagedLease() const
{
    return bManagedRun && bHasLocalCallerContext && ManagedLease && ManagedLease->IsValid() && ManagedLease->GetStamp() == ManagedStamp && ManagedStamp.RunId == RunIdentity.RunId && ManagedStamp.HostEpoch == RunIdentity.HostEpoch && LocalCallerContext.AccountId == RunIdentity.HostAccountId;
}

bool URunStateSubsystem::CanMutateManagedRun() const { return !bManagedRun || HasManagedLease(); }

URunSaveGame* URunStateSubsystem::CreateInitialSaveData(const TArray<FRunPartyMember>& Members, const FRunIdentityData& Identity, FText& OutError) const
{
    URunSaveGame* Save = NewObject<URunSaveGame>();
    const URunEncounterPoolDataAsset* Pool = PartyDefinition && PartyDefinition->RunEncounterPool ? PartyDefinition->RunEncounterPool.Get() : GetDefault<URunEncounterPoolDataAsset>();
    if (!Pool->BuildFixedOffers(Save->EncounterProgress.Offers, OutError)) return nullptr;
    Save->EncounterProgress.SchemaVersion = 1;
    Save->Identity = Identity;
    Save->Party = Members;
    Save->Party.Sort([](const FRunPartyMember& Left, const FRunPartyMember& Right) { return Left.SlotIndex < Right.SlotIndex; });
    for (FRunPartyMember& Member : Save->Party)
    {
        Member.CurrentHP = -1.0f;
    }
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FRunNodeDefinition& Node = Save->Nodes.AddDefaulted_GetRef();
        Node.NodeId = FName(*FString::Printf(TEXT("Combat_%02d"), Index + 1));
        Node.DisplayName = FText::FromString(FString::Printf(TEXT("Combat %d / 전투 %d"), Index + 1, Index + 1));
        Node.EncounterId = TEXT("DefaultEncounter");
    }
    Save->Phase = ERunPhase::Map;
    Save->Catalog = FSoftObjectPath(PartyDefinition);
    return Save;
}

bool URunStateSubsystem::CreateManagedRun(const TArray<FRunPartyMember>& Members, const FRunIdentityData& Identity, FText& OutError)
{
    OutError = NSLOCTEXT("ManagedRun", "CreateContext", "관리 Run은 고정된 개발용 최초 Host가 번호를 가진 원래 참가자 2~4명으로 새로 생성해야 합니다.");
    if (!bHasLocalCallerContext || bManagedRun || ManagedLease || Phase == ERunPhase::Combat || Phase == ERunPhase::Preparing || Identity.Origin != ERunIdentityOrigin::LocalDevelopment || Identity.SchemaVersion != URunIdentityLibrary::CurrentSchemaVersion || Identity.HostEpoch != 1 || Identity.HostAccountId != LocalCallerContext.AccountId || Identity.OriginalParticipants.Num() < 2 || Identity.OriginalParticipants.Num() > 4 || Identity.RunId == RunIdentity.RunId) return false;
    if (!URunIdentityLibrary::ValidateIdentity(Identity, Members, OutError)) return false;
    TStrongObjectPtr<URunSaveGame> Save(CreateInitialSaveData(Members, Identity, OutError));
    if (!Save) return false;
    Save->Version = 4;
    for (const FRunParticipantData& Participant : Identity.OriginalParticipants)
    {
        Save->Participation.HumanParticipants.Add(Participant.AccountId);
    }
    if (!ValidateSave(Save.Get(), OutError)) return false;
    TArray<uint8> Payload;
    if (!UGameplayStatics::SaveGameToMemory(Save.Get(), Payload))
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedSerialize", "관리 Run을 저장 데이터로 변환하지 못했습니다.");
        return false;
    }
    TUniquePtr<FLocalRunAuthorityLease> Lease;
    FRunAuthorityRecordData Record;
    if (FLocalRunAuthorityStore(LocalCallerContext.StoreNamespace).Create(Identity.RunId, Identity.HostEpoch, Payload, Lease, Record, OutError) != ERunAuthorityResult::Success) return false;
    ApplySaveData(Save.Get());
    ManagedLease = MoveTemp(Lease);
    ManagedStamp = Record.Stamp;
    ManagedResumeTarget = Identity.RunId;
    bManagedResumePending = false;
    bCheckpointSaving = true;
    SaveError = FText::GetEmpty();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::ReadManagedSave(FGuid RunId, FRunAuthorityRecordData& OutRecord, TStrongObjectPtr<URunSaveGame>& OutSave, FText& OutError) const
{
    OutError = NSLOCTEXT("ManagedRun", "ReadContext", "관리 Run을 조회하려면 원래 참가자로 배정된 개발용 호출자 문맥이 필요합니다.");
    if (!bHasLocalCallerContext || !RunId.IsValid()) return false;
    FRunAuthorityRecordData Record;
    if (FLocalRunAuthorityStore(LocalCallerContext.StoreNamespace).Read(RunId, Record, OutError) != ERunAuthorityResult::Success) return false;
    TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Record.Payload)));
    if (!Save || Save->GetClass() != URunSaveGame::StaticClass() || Save->Version != 4 || !ValidateSave(Save.Get(), OutError))
    {
        OutError = NSLOCTEXT("ManagedRun", "InvalidPayload", "기준 저장소 본문이 유효한 개발용 v4 관리 Run이 아닙니다.");
        return false;
    }
    if (Record.Stamp.RunId != Save->Identity.RunId || Record.Stamp.HostEpoch != Save->Identity.HostEpoch || !URunIdentityLibrary::IsOriginalParticipant(Save->Identity, LocalCallerContext.AccountId))
    {
        OutError = NSLOCTEXT("ManagedRun", "RecordIdentity", "기준 저장의 Run·Host 세대 또는 조회자의 원래 참가 기록이 일치하지 않습니다.");
        return false;
    }
    OutSave = MoveTemp(Save);
    OutRecord = MoveTemp(Record);
    OutError = FText::GetEmpty();
    return true;
}

bool URunStateSubsystem::ReadManagedRun(FGuid RunId, FManagedRunPreview& OutPreview, FText& OutError) const
{
    FRunAuthorityRecordData Record;
    TStrongObjectPtr<URunSaveGame> Save;
    if (!ReadManagedSave(RunId, Record, Save, OutError)) return false;
    FManagedRunPreview Preview;
    Preview.Stamp = Record.Stamp;
    Preview.Identity = Save->Identity;
    Preview.Participation = Save->Participation;
    Preview.Phase = Save->Phase;
    Preview.bHasCombatCheckpoint = Save->CombatCheckpoint.Revision > 0;
    Preview.CompletedNodeCount = Save->CompletedNodes.Num();
    Preview.TotalNodeCount = Save->Nodes.Num();
    OutPreview = MoveTemp(Preview);
    return true;
}

bool URunStateSubsystem::SetManagedResumeTarget(FGuid RunId, FText& OutError)
{
    FManagedRunPreview Preview;
    if (!ReadManagedRun(RunId, Preview, OutError)) return false;
    if (bManagedRun && RunId != RunIdentity.RunId)
    {
        OutError = NSLOCTEXT("ManagedRun", "ActiveTarget", "활성 관리 Run을 닫기 전에는 재개 대상을 바꿀 수 없습니다.");
        return false;
    }
    ManagedResumeTarget = RunId;
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::ResumeManagedRun(const FRunAuthorityStamp& ExpectedStamp, const TArray<FRunAccountId>& HumanParticipants, FText& OutError)
{
    OutError = NSLOCTEXT("ManagedRun", "ResumeContext", "이전 실행을 닫고 최신 확정 기록으로 명시적으로 재개해야 합니다.");
    if (!bHasLocalCallerContext || bManagedRun || ManagedLease || Phase == ERunPhase::Combat || Phase == ERunPhase::Preparing || !ExpectedStamp.IsValid() || ExpectedStamp.HostEpoch == MAX_int32 || ExpectedStamp.Revision == MAX_int64) return false;
    FRunAuthorityRecordData Previous;
    TStrongObjectPtr<URunSaveGame> Save;
    if (!ReadManagedSave(ExpectedStamp.RunId, Previous, Save, OutError)) return false;
    if (Previous.Stamp != ExpectedStamp || Save->Phase == ERunPhase::Complete || Save->Phase == ERunPhase::Defeat)
    {
        OutError = NSLOCTEXT("ManagedRun", "StaleOrEnded", "기록이 변경되었거나 종료된 Run입니다. 최신 기록을 다시 확인해 주세요.");
        return false;
    }
    // Reject old combat payloads before changing participants, Host epochs, leases or stored bytes.
    // 참가자·Host 세대·lease·저장 바이트를 바꾸기 전에 기존 전투 본문을 거절합니다.
    if (Save->Phase == ERunPhase::Combat)
    {
        OutError = NSLOCTEXT("ManagedRun", "RetiredCombatSave", "기존 순차 턴 관리 전투는 새 라운드 전투에서 재개할 수 없습니다. 새 전투의 중간 복구는 아직 지원하지 않으며 기준 저장과 실행 권한은 변경하지 않습니다.");
        return false;
    }
    FRunParticipationData NextParticipation;
    NextParticipation.HumanParticipants = HumanParticipants;
    FRunIdentityData NextIdentity = Save->Identity;
    NextIdentity.HostAccountId = LocalCallerContext.AccountId;
    ++NextIdentity.HostEpoch;
    if (!URunParticipationLibrary::ValidateTransition(Save->Participation, Save->Identity, NextParticipation, NextIdentity, Save->Party, OutError)) return false;
    Save->Identity = NextIdentity;
    Save->Participation = NextParticipation;
    if (!ValidateSave(Save.Get(), OutError)) return false;
    TArray<uint8> Payload;
    if (!UGameplayStatics::SaveGameToMemory(Save.Get(), Payload))
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedSerialize", "관리 Run을 저장 데이터로 변환하지 못했습니다.");
        return false;
    }
    TUniquePtr<FLocalRunAuthorityLease> Lease;
    FRunAuthorityRecordData Record;
    if (FLocalRunAuthorityStore(LocalCallerContext.StoreNamespace).Acquire(ExpectedStamp, NextIdentity.HostEpoch, Payload, Lease, Record, OutError) != ERunAuthorityResult::Success) return false;
    ApplySaveData(Save.Get());
    ManagedLease = MoveTemp(Lease);
    ManagedStamp = Record.Stamp;
    ManagedResumeTarget = ExpectedStamp.RunId;
    bManagedResumePending = true;
    bCheckpointSaving = true;
    SaveError = FText::GetEmpty();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::ConfirmManagedResumeStarted(FText& OutError)
{
    OutError = NSLOCTEXT("ManagedRun", "ConfirmLease", "재개를 확정하려면 현재 Host의 실행 lease가 필요합니다.");
    if (!HasManagedLease()) return false;
    ClearManagedMenuTravel();
    const bool bWasPending = bManagedResumePending;
    bManagedResumePending = false;
    OutError = FText::GetEmpty();
    if (bWasPending) OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::BeginManagedMenuTravel(FText& OutError)
{
    OutError = NSLOCTEXT("ManagedRun", "TravelContext", "현재 GameInstance의 대기 중인 관리 Run만 메뉴에서 이동할 수 있습니다.");
    UGameInstance* Instance = GetGameInstance();
    const FWorldContext* Context = Instance ? Instance->GetWorldContext() : nullptr;
    UEngine* Engine = Instance ? Instance->GetEngine() : nullptr;
    if (!bManagedResumePending || !HasManagedLease() || !Engine || !Context || Context->OwningGameInstance != Instance || !Context->World() || Context->World()->GetNetMode() != NM_Standalone) return false;
    ClearManagedMenuTravel();
    ManagedTravelSessionId = ManagedStamp.SessionId;
    ManagedTravelContextHandle = Context->ContextHandle;
    ManagedTravelEngine = Engine;
    ManagedTravelFailureHandle = Engine->OnTravelFailure().AddUObject(this, &URunStateSubsystem::HandleManagedTravelFailure);
    OutError = FText::GetEmpty();
    return true;
}

void URunStateSubsystem::ClearManagedMenuTravel()
{
    if (ManagedTravelEngine.IsValid() && ManagedTravelFailureHandle.IsValid()) ManagedTravelEngine->OnTravelFailure().Remove(ManagedTravelFailureHandle);
    ManagedTravelFailureHandle.Reset();
    ManagedTravelEngine.Reset();
    ManagedTravelSessionId.Invalidate();
    ManagedTravelContextHandle = NAME_None;
}

void URunStateSubsystem::HandleManagedTravelFailure(UWorld* World, ETravelFailure::Type, const FString&)
{
    UGameInstance* Instance = GetGameInstance();
    const FWorldContext* Context = Instance ? Instance->GetWorldContext() : nullptr;
    if (!bManagedResumePending || !HasManagedLease() || !ManagedTravelSessionId.IsValid() || ManagedTravelSessionId != ManagedStamp.SessionId || !IsValid(World) || World->GetGameInstance() != Instance || !Context || Context->ContextHandle != ManagedTravelContextHandle || Context->World() != World) return;
    // Travel may already have replaced the menu controller; keep the committed Host and AI record for retry.
    // 이동이 메뉴 컨트롤러를 이미 교체했을 수 있으므로 확정된 Host와 AI 기록을 재시도용으로 유지합니다.
    CloseManagedRun();
}

void URunStateSubsystem::CloseManagedRun()
{
    ClearManagedMenuTravel();
    if (!bManagedRun && !ManagedLease) return;
    ManagedLease.Reset();
    ManagedStamp = FRunAuthorityStamp();
    bManagedRun = false;
    bManagedResumePending = false;
    bCheckpointSaving = false;
    RunIdentity = FRunIdentityData();
    Participation = FRunParticipationData();
    EncounterProgress = FRunEncounterProgress();
    PartyMembers.Reset();
    Nodes.Reset();
    CompletedNodes.Reset();
    CurrentNodeId = NAME_None;
    CurrentEncounterId = NAME_None;
    Phase = ERunPhase::None;
    LastResult = ECombatResult::None;
    CombatCheckpoint = FCombatCheckpointData();
    SaveError = FText::GetEmpty();
    OnRunStateChanged.Broadcast();
}

bool URunStateSubsystem::InitializeRun(const TArray<FRunPartyMember>& Members, FText& OutError)
{
    // Standalone runs use a per-run development identity until an authenticated provider is integrated.
    // 인증 공급자 연동 전까지 싱글플레이는 Run마다 별도의 개발용 식별자를 사용합니다.
    FRunIdentityData Identity;
    Identity.SchemaVersion = URunIdentityLibrary::CurrentSchemaVersion;
    Identity.Origin = ERunIdentityOrigin::LocalDevelopment;
    Identity.RunId = FGuid::NewGuid();
    Identity.HostEpoch = 1;
    FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
    Participant.JoinOrdinal = 1;
    Participant.AccountId.Provider = TEXT("Development");
    Participant.AccountId.Subject = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Identity.HostAccountId = Participant.AccountId;
    TArray<FRunPartyMember> OwnedMembers = Members;
    for (FRunPartyMember& Member : OwnedMembers)
    {
        Member.CharacterId = Member.bCreated ? FGuid::NewGuid() : FGuid();
        Member.OwnerAccountId = Member.bCreated ? Participant.AccountId : FRunAccountId();
    }
    return InitializeRunWithIdentity(OwnedMembers, Identity, OutError);
}

bool URunStateSubsystem::InitializeRunWithIdentity(const TArray<FRunPartyMember>& Members, const FRunIdentityData& Identity, FText& OutError)
{
    OutError = FText::GetEmpty();
    if (bManagedRun || ManagedLease)
    {
        OutError = NSLOCTEXT("RunCheckpoint", "ManagedActive", "현재 관리 세션을 먼저 닫아야 다른 진행을 불러올 수 있습니다.");
        return false;
    }
    TSet<int32> UsedSlots;
    int32 CreatedCount = 0;

    if (Members.Num() > 4)
    {
        OutError = FText::FromString(TEXT("A party supports up to four slots. / 파티는 최대 4개 슬롯입니다."));
        return false;
    }

    for (const FRunPartyMember& Member : Members)
    {
        if (Member.SlotIndex < 0 || Member.SlotIndex >= 4 || UsedSlots.Contains(Member.SlotIndex))
        {
            OutError = FText::FromString(TEXT("Party slot indices must be unique and between 0 and 3. / 파티 슬롯 번호는 0~3이며 중복될 수 없습니다."));
            return false;
        }

        UsedSlots.Add(Member.SlotIndex);

        if (Member.bCreated)
        {
            if (Member.ClassId.IsNone() || Member.CharacterName.ToString().TrimStartAndEnd().IsEmpty())
            {
                OutError = FText::FromString(TEXT("Created characters need a name and class. / 생성한 캐릭터의 이름과 직업이 필요합니다."));
                return false;
            }

            ++CreatedCount;
        }
    }

    if (CreatedCount == 0)
    {
        OutError = FText::FromString(TEXT("Create at least one character to start. / 캐릭터를 1명 이상 생성해 주세요."));
        return false;
    }

    if (Identity.Origin == ERunIdentityOrigin::LegacyOffline)
    {
        OutError = FText::FromString(TEXT("새 진행에는 Run·참가자·캐릭터 식별 정보가 필요합니다."));
        return false;
    }
    if (!URunIdentityLibrary::ValidateIdentity(Identity, Members, OutError))
    {
        return false;
    }
    if (RunIdentity.RunId.IsValid() && RunIdentity.RunId == Identity.RunId)
    {
        OutError = FText::FromString(TEXT("현재 Run의 참가자와 소유권을 새 게임 생성으로 교체할 수 없습니다. 복원은 이어하기를 사용해 주세요."));
        return false;
    }

    TStrongObjectPtr<URunSaveGame> Save(CreateInitialSaveData(Members, Identity, OutError));
    if (!Save) return false;
    ApplySaveData(Save.Get());
    AutoSaveCheckpoint();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::CanStartNode(FName NodeId) const
{
    if (!CanMutateManagedRun() || bManagedResumePending || Phase != ERunPhase::Map || !Nodes.IsValidIndex(CompletedNodes.Num()))
    {
        return false;
    }

    return Nodes[CompletedNodes.Num()].NodeId == NodeId && !CompletedNodes.Contains(NodeId);
}

bool URunStateSubsystem::BeginEncounter(FName NodeId)
{
    if (!CanStartNode(NodeId))
    {
        return false;
    }

    const FRunNodeDefinition& Node = Nodes[CompletedNodes.Num()];
    CurrentNodeId = Node.NodeId;
    CurrentEncounterId = Node.EncounterId;
    LastResult = ECombatResult::None;
    Phase = ERunPhase::Preparing;
    AutoSaveCheckpoint();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::MarkCombatStarted()
{
    if (!CanMutateManagedRun() || Phase != ERunPhase::Preparing)
    {
        return false;
    }

    Phase = ERunPhase::Combat;
    AutoSaveCheckpoint();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::CompleteEncounter(ECombatResult Result)
{
    if (!CanMutateManagedRun() || Phase != ERunPhase::Combat || (Result != ECombatResult::Victory && Result != ECombatResult::Defeat))
    {
        return false;
    }

    const ECombatResult PreviousResult = LastResult;
    const TArray<FName> PreviousCompletedNodes = CompletedNodes;
    LastResult = Result;

    if (Result == ECombatResult::Victory)
    {
        CompletedNodes.AddUnique(CurrentNodeId);
        Phase = ERunPhase::Result;
    }
    else
    {
        Phase = ERunPhase::Defeat;
    }

    // Commit the terminal result before exposing it; a failed write remains retryable in combat.
    // 종료 결과를 공개하기 전에 저장하며 쓰기 실패 시 전투 상태에서 재시도할 수 있습니다.
    if (bCheckpointSaving && !SaveCheckpoint(SaveError))
    {
        LastResult = PreviousResult;
        CompletedNodes = PreviousCompletedNodes;
        Phase = ERunPhase::Combat;
        return false;
    }
    CombatCheckpoint = FCombatCheckpointData();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::AbortEncounter()
{
    if (!CanMutateManagedRun() || HasCombatCheckpoint() || (Phase != ERunPhase::Preparing && (Phase != ERunPhase::Combat || LastResult != ECombatResult::None)))
    {
        return false;
    }

    const FName PreviousNode = CurrentNodeId;
    const FName PreviousEncounter = CurrentEncounterId;
    const ERunPhase PreviousPhase = Phase;
    CurrentNodeId = NAME_None;
    CurrentEncounterId = NAME_None;
    Phase = ERunPhase::Map;
    if (bManagedRun)
    {
        if (!SaveCheckpoint(SaveError))
        {
            CurrentNodeId = PreviousNode;
            CurrentEncounterId = PreviousEncounter;
            Phase = PreviousPhase;
            return false;
        }
    }
    else
    {
        AutoSaveCheckpoint();
    }
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::ContinueRun()
{
    if (!CanMutateManagedRun() || bManagedResumePending || Phase != ERunPhase::Result || LastResult != ECombatResult::Victory)
    {
        return false;
    }

    const FName PreviousEncounterId = CurrentEncounterId;
    CurrentEncounterId = NAME_None;
    Phase = ERunPhase::Map;

    if (CompletedNodes.Num() >= Nodes.Num())
    {
        Phase = ERunPhase::Complete;
    }
    else if (EncounterProgress.SchemaVersion == 1 && !EncounterProgress.bCompleted)
    {
        Phase = ERunPhase::EncounterChoice;
    }

    if (bCheckpointSaving && !SaveCheckpoint(SaveError))
    {
        CurrentEncounterId = PreviousEncounterId;
        Phase = ERunPhase::Result;
        return false;
    }
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::SelectRunEncounter(FName EncounterId)
{
    if (!CanMutateManagedRun() || bManagedResumePending || Phase != ERunPhase::EncounterChoice || EncounterProgress.bCompleted || !EncounterProgress.SelectedEncounterId.IsNone()) return false;
    const FRunEncounterOffer* Offer = EncounterProgress.Offers.FindByPredicate([EncounterId](const FRunEncounterOffer& Candidate) { return Candidate.EncounterId == EncounterId; });
    if (!Offer || Offer->Type != ERunEncounterType::Shop) return false;
    EncounterProgress.SelectedEncounterId = EncounterId;
    Phase = ERunPhase::Shop;
    if (bCheckpointSaving && !SaveCheckpoint(SaveError))
    {
        EncounterProgress.SelectedEncounterId = NAME_None;
        Phase = ERunPhase::EncounterChoice;
        return false;
    }
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::LeaveRunEncounter()
{
    if (!CanMutateManagedRun() || bManagedResumePending || Phase != ERunPhase::Shop || EncounterProgress.bCompleted || EncounterProgress.SelectedEncounterId.IsNone()) return false;
    EncounterProgress.bCompleted = true;
    Phase = ERunPhase::Map;
    if (bCheckpointSaving && !SaveCheckpoint(SaveError))
    {
        EncounterProgress.bCompleted = false;
        Phase = ERunPhase::Shop;
        return false;
    }
    OnRunStateChanged.Broadcast();
    return true;
}

void URunStateSubsystem::UpdatePartyMemberHP(int32 SlotIndex, float CurrentHP)
{
    if (!CanMutateManagedRun() || Phase != ERunPhase::Combat || !FMath::IsFinite(CurrentHP))
    {
        return;
    }

    for (FRunPartyMember& Member : PartyMembers)
    {
        if (Member.SlotIndex == SlotIndex && Member.bCreated)
        {
            Member.CurrentHP = FMath::Max(0.0f, CurrentHP);
            return;
        }
    }
}
