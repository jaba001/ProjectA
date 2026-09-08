#include "Game/Run/RunStateSubsystem.h"

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
