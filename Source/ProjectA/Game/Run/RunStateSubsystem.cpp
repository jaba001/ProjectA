#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunIdentityLibrary.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Combat/Checkpoint/CombatCheckpointLibrary.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"

URunStateSubsystem::URunStateSubsystem()
{
    SaveSlot = ResolveCheckpointSlot(FCommandLine::Get());
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

bool URunStateSubsystem::ValidateSave(const URunSaveGame* Save, FText& OutError) const
{
    OutError = FText::FromString(TEXT("저장 파일이 손상되었거나 현재 버전·직업 설정과 호환되지 않습니다."));
    if (!Save || (Save->Version != 1 && Save->Version != 2 && Save->Version != 3) || Save->Party.IsEmpty() || Save->Party.Num() > 4 || Save->Nodes.Num() != 2 || Save->CompletedNodes.Num() > 2)
    {
        return false;
    }
    // Legacy saves remain offline; missing or damaged ownership must never downgrade a new save.
    // 기존 저장은 오프라인으로 유지하며 새 저장의 누락·손상된 소유권을 구버전으로 우회하지 않습니다.
    if ((Save->Version == 1) != (Save->Identity.Origin == ERunIdentityOrigin::LegacyOffline))
    {
        return false;
    }
    const FCombatCheckpointData EmptyCheckpoint;
    if (Save->Version != 3 && !FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Save->CombatCheckpoint, &EmptyCheckpoint, 0))
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
    const bool bCombat = Save->Version == 3 && Save->Phase == ERunPhase::Combat && Completed < 2 && Save->Result == ECombatResult::None && Save->CurrentNode == Save->Nodes[Completed].NodeId && Save->CurrentEncounter == TEXT("DefaultEncounter");
    if (Created == 0 || (!bMap && !bResult && !bComplete && !bDefeat && !bCombat) || (!bDefeat && Living == 0) || (Save->Version == 3 && !bCombat))
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
            }
        }
    }
    OutError = FText::GetEmpty();
    return true;
}

URunSaveGame* URunStateSubsystem::CreateSaveData() const
{
    URunSaveGame* Save = NewObject<URunSaveGame>();
    Save->Version = RunIdentity.Origin == ERunIdentityOrigin::LegacyOffline ? 1 : 2;
    Save->Identity = RunIdentity;
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

bool URunStateSubsystem::SaveCheckpoint(FText& OutError)
{
    TStrongObjectPtr<URunSaveGame> Save(CreateSaveData());
    if (!ValidateSave(Save.Get(), OutError) || !FRunCheckpointStorage::Save(Save.Get(), SaveSlot, OutError))
    {
        SaveError = OutError;
        return false;
    }
    SaveError = FText::GetEmpty();
    return true;
}

bool URunStateSubsystem::CommitCombatCheckpoint(const FCombatCheckpointData& Checkpoint, FText& OutError)
{
    OutError = NSLOCTEXT("RunCheckpoint", "Boundary", "현재 Run·Host·전투 경계와 체크포인트가 일치하지 않습니다.");
    const bool bSameAttempt = HasCombatCheckpoint() && CombatCheckpoint.AttemptId == Checkpoint.AttemptId;
    if (HasCombatCheckpoint() && (!bSameAttempt || CombatCheckpoint.Revision >= MAX_int64 - 1 || Checkpoint.CompletedTurnSerial <= CombatCheckpoint.CompletedTurnSerial))
    {
        SaveError = OutError;
        return false;
    }
    const int64 ExpectedRevision = bSameAttempt ? CombatCheckpoint.Revision + 1 : 1;
    if (!bCheckpointSaving || Phase != ERunPhase::Combat || LastResult != ECombatResult::None || Checkpoint.Revision != ExpectedRevision || Checkpoint.NodeId != CurrentNodeId || Checkpoint.EncounterId != CurrentEncounterId || !FRunIdentityData::StaticStruct()->CompareScriptStruct(&RunIdentity, &Checkpoint.Identity, 0))
    {
        SaveError = OutError;
        return false;
    }
    TStrongObjectPtr<URunSaveGame> Save(CreateSaveData());
    Save->Version = 3;
    Save->CombatCheckpoint = Checkpoint;
    for (const FCombatCheckpointUnit& Unit : Checkpoint.Units)
    {
        if (Unit.Team == ETeam::Player)
        {
            FRunPartyMember* Member = Save->Party.FindByPredicate([&Unit](const FRunPartyMember& Candidate) { return Candidate.bCreated && Candidate.SlotIndex == Unit.PartySlot; });
            if (Member)
            {
                Member->CurrentHP = Unit.HP;
            }
        }
    }
    if (!ValidateSave(Save.Get(), OutError) || !FRunCheckpointStorage::Save(Save.Get(), SaveSlot, OutError))
    {
        SaveError = OutError;
        return false;
    }
    // Publish persistence metadata only; the combat owner publishes the next runtime turn afterward.
    // 저장 메타데이터만 확정하며 다음 런타임 턴은 전투 소유자가 이후에 공개합니다.
    CombatCheckpoint = Checkpoint;
    SaveError = FText::GetEmpty();
    return true;
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
    TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(FRunCheckpointStorage::Load(SaveSlot, OutError)));
    if (!Save.IsValid())
    {
        OutError = NSLOCTEXT("RunCheckpoint", "InvalidSave", "이어할 저장이 없거나 Run 저장 파일이 아닙니다.");
        return false;
    }
    if (!ValidateSave(Save.Get(), OutError))
    {
        return false;
    }
    if (Save->Phase == ERunPhase::Defeat || Save->Phase == ERunPhase::Complete)
    {
        OutError = FText::FromString(TEXT("종료된 진행입니다. 새 게임을 시작해 주세요."));
        return false;
    }
    return true;
}

bool URunStateSubsystem::LoadCheckpoint(FText& OutError)
{
    TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(FRunCheckpointStorage::Load(SaveSlot, OutError)));
    if (!Save.IsValid())
    {
        OutError = NSLOCTEXT("RunCheckpoint", "InvalidSave", "이어할 저장이 없거나 Run 저장 파일이 아닙니다.");
        return false;
    }
    if (!ValidateSave(Save.Get(), OutError))
    {
        return false;
    }
    if (Save->Phase == ERunPhase::Defeat || Save->Phase == ERunPhase::Complete)
    {
        OutError = FText::FromString(TEXT("종료된 진행입니다. 새 게임을 시작해 주세요."));
        return false;
    }
    RunIdentity = Save->Identity;
    PartyMembers = Save->Party;
    Nodes = Save->Nodes;
    CompletedNodes = Save->CompletedNodes;
    CurrentNodeId = Save->CurrentNode;
    CurrentEncounterId = Save->CurrentEncounter;
    Phase = Save->Phase;
    LastResult = Save->Result;
    CombatCheckpoint = Save->CombatCheckpoint;
    PartyDefinition = Cast<UPartyDefinitionDataAsset>(Save->Catalog.ResolveObject());
    SaveError = FText::GetEmpty();
    bCheckpointSaving = true;
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::InitializeRun(const TArray<FRunPartyMember>& Members, FText& OutError)
{
    // Standalone runs use a per-run development identity until an authenticated provider is integrated.
    // 인증 공급자 연동 전까지 싱글플레이는 Run마다 별도의 개발용 식별자를 사용합니다.
    FRunIdentityData Identity;
    Identity.Origin = ERunIdentityOrigin::LocalDevelopment;
    Identity.RunId = FGuid::NewGuid();
    Identity.HostEpoch = 1;
    FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
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

    RunIdentity = Identity;
    PartyMembers = Members;
    PartyMembers.Sort([](const FRunPartyMember& Left, const FRunPartyMember& Right) { return Left.SlotIndex < Right.SlotIndex; });

    for (FRunPartyMember& Member : PartyMembers)
    {
        Member.CurrentHP = -1.0f;
    }

    Nodes.Reset();

    for (int32 Index = 0; Index < 2; ++Index)
    {
        FRunNodeDefinition& Node = Nodes.AddDefaulted_GetRef();
        Node.NodeId = FName(*FString::Printf(TEXT("Combat_%02d"), Index + 1));
        Node.DisplayName = FText::FromString(FString::Printf(TEXT("Combat %d / 전투 %d"), Index + 1, Index + 1));
        Node.EncounterId = TEXT("DefaultEncounter");
    }

    CompletedNodes.Reset();
    CurrentNodeId = NAME_None;
    CurrentEncounterId = NAME_None;
    LastResult = ECombatResult::None;
    Phase = ERunPhase::Map;
    CombatCheckpoint = FCombatCheckpointData();
    AutoSaveCheckpoint();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::CanStartNode(FName NodeId) const
{
    if (Phase != ERunPhase::Map || !Nodes.IsValidIndex(CompletedNodes.Num()))
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
    if (Phase != ERunPhase::Preparing)
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
    if (Phase != ERunPhase::Combat || (Result != ECombatResult::Victory && Result != ECombatResult::Defeat))
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
    if (HasCombatCheckpoint() || (Phase != ERunPhase::Preparing && (Phase != ERunPhase::Combat || LastResult != ECombatResult::None)))
    {
        return false;
    }

    CurrentNodeId = NAME_None;
    CurrentEncounterId = NAME_None;
    Phase = ERunPhase::Map;
    AutoSaveCheckpoint();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::ContinueRun()
{
    if (Phase != ERunPhase::Result || LastResult != ECombatResult::Victory)
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

    if (bCheckpointSaving && !SaveCheckpoint(SaveError))
    {
        CurrentEncounterId = PreviousEncounterId;
        Phase = ERunPhase::Result;
        return false;
    }
    OnRunStateChanged.Broadcast();
    return true;
}

void URunStateSubsystem::UpdatePartyMemberHP(int32 SlotIndex, float CurrentHP)
{
    if (Phase != ERunPhase::Combat || !FMath::IsFinite(CurrentHP))
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
