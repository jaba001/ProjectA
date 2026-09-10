#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunSaveGame.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

URunStateSubsystem::URunStateSubsystem()
{
    FParse::Value(FCommandLine::Get(), TEXT("ProjectASaveSlot="), SaveSlot);
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
    if (!Save || Save->Version != 1 || Save->Party.IsEmpty() || Save->Party.Num() > 4 || Save->Nodes.Num() != 2 || Save->CompletedNodes.Num() > 2)
    {
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
    if (Created == 0 || (!bMap && !bResult && !bComplete && !bDefeat) || (!bDefeat && Living == 0))
    {
        return false;
    }
    OutError = FText::GetEmpty();
    return true;
}

bool URunStateSubsystem::SaveCheckpoint(FText& OutError)
{
    URunSaveGame* Save = NewObject<URunSaveGame>();
    Save->Party = PartyMembers;
    Save->Nodes = Nodes;
    Save->CompletedNodes = CompletedNodes;
    Save->CurrentNode = CurrentNodeId;
    Save->CurrentEncounter = CurrentEncounterId;
    Save->Phase = Phase;
    Save->Result = LastResult;
    Save->Catalog = FSoftObjectPath(PartyDefinition);
    if (!ValidateSave(Save, OutError))
    {
        return false;
    }
    if (!UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0))
    {
        OutError = FText::FromString(TEXT("진행을 저장하지 못했습니다. 저장 공간과 쓰기 권한을 확인해 주세요."));
        return false;
    }
    return true;
}

bool URunStateSubsystem::CanContinueSavedRun(FText& OutError) const
{
    if (!UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))
    {
        OutError = FText::FromString(TEXT("이어할 저장 기록이 없습니다."));
        return false;
    }
    const URunSaveGame* Save = Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
    if (!ValidateSave(Save, OutError))
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
    if (!CanContinueSavedRun(OutError))
    {
        return false;
    }
    const URunSaveGame* Save = Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
    if (!ValidateSave(Save, OutError))
    {
        return false;
    }
    PartyMembers = Save->Party;
    Nodes = Save->Nodes;
    CompletedNodes = Save->CompletedNodes;
    CurrentNodeId = Save->CurrentNode;
    CurrentEncounterId = Save->CurrentEncounter;
    Phase = Save->Phase;
    LastResult = Save->Result;
    PartyDefinition = Cast<UPartyDefinitionDataAsset>(Save->Catalog.ResolveObject());
    SaveError = FText::GetEmpty();
    bCheckpointSaving = true;
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::InitializeRun(const TArray<FRunPartyMember>& Members, FText& OutError)
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
    if (Phase != ERunPhase::Combat || Result == ECombatResult::None)
    {
        return false;
    }

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

    AutoSaveCheckpoint();
    OnRunStateChanged.Broadcast();
    return true;
}

bool URunStateSubsystem::AbortEncounter()
{
    if (Phase != ERunPhase::Preparing && (Phase != ERunPhase::Combat || LastResult != ECombatResult::None))
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

    CurrentEncounterId = NAME_None;
    Phase = ERunPhase::Map;

    if (CompletedNodes.Num() >= Nodes.Num())
    {
        Phase = ERunPhase::Complete;
    }

    AutoSaveCheckpoint();
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
