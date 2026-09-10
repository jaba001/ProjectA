#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunSaveGame.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunSnapshotCheckpointIsolationTest, "ProjectA.Persistence.SnapshotCheckpointIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRunSnapshotCheckpointIsolationTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Ordinary launches retain the PvE checkpoint"), URunStateSubsystem::ResolveCheckpointSlot(TEXT("")), FString(TEXT("ProjectA_Run")));
    TestEqual(TEXT("Snapshot selection receives a dedicated checkpoint"), URunStateSubsystem::ResolveCheckpointSlot(TEXT("-ProjectAOpponentSnapshot=Local_01")), FString(TEXT("ProjectA_SnapshotRun_Local_01")));
    TestEqual(TEXT("Explicit checkpoint selection takes precedence"), URunStateSubsystem::ResolveCheckpointSlot(TEXT("-ProjectAOpponentSnapshot=Local_01 -ProjectASaveSlot=ChosenRun")), FString(TEXT("ChosenRun")));
    TestEqual(TEXT("Empty explicit selection cannot remove Snapshot isolation"), URunStateSubsystem::ResolveCheckpointSlot(TEXT("-ProjectASaveSlot=\"\" -ProjectAOpponentSnapshot=Local_01")), FString(TEXT("ProjectA_SnapshotRun_Local_01")));
    const TArray<FString> InvalidSelections = { TEXT("-ProjectAOpponentSnapshot"), TEXT("-ProjectAOpponentSnapshot="), TEXT("-ProjectAOpponentSnapshot=\"\""), TEXT("-ProjectAOpponentSnapshot=\"   \""), TEXT("-ProjectAOpponentSnapshot=../ProjectA_Run"), TEXT("-ProjectAOpponentSnapshot=None"), TEXT("-ProjectAOpponentSnapshot=상대") };
    for (const FString& CommandLine : InvalidSelections)
    {
        TestEqual(*FString::Printf(TEXT("Rejected selection preserves PvE isolation: %s"), *CommandLine), URunStateSubsystem::ResolveCheckpointSlot(*CommandLine), FString(TEXT("ProjectA_RejectedSnapshotRun")));
    }
    TestEqual(TEXT("Oversized selection is rejected before FName creation"), URunStateSubsystem::ResolveCheckpointSlot(*(TEXT("-ProjectAOpponentSnapshot=") + FString::ChrN(2048, TEXT('a')))), FString(TEXT("ProjectA_RejectedSnapshotRun")));

    struct FScopedCheckpointSlots
    {
        TArray<FString> Names;

        ~FScopedCheckpointSlots()
        {
            for (const FString& Name : Names)
            {
                UGameplayStatics::DeleteGameInSlot(Name, 0);
            }
        }
    } Slots;
    const FString Token = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString FirstSelection = TEXT("-ProjectAOpponentSnapshot=T14_A_") + Token;
    const FString SecondSelection = TEXT("-ProjectAOpponentSnapshot=T14_B_") + Token;
    const FString ExplicitSelection = FirstSelection + TEXT(" -ProjectASaveSlot=T14_Explicit_") + Token;
    UGameInstance* Instance = NewObject<UGameInstance>();
    UPartyDefinitionDataAsset* Catalog = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    if (!TestNotNull(TEXT("Checkpoint isolation uses the real profession catalog"), Catalog))
    {
        return false;
    }
    const auto CreateRun = [Instance, Catalog](const FString& Arguments)
    {
        // Restore process arguments immediately after construction, before any world or file operation.
        // 월드나 파일 작업 전에 생성 직후 프로세스 인수를 즉시 복원합니다.
        const FString PreviousArguments = FCommandLine::Get();
        FCommandLine::Set(*Arguments);
        URunStateSubsystem* Run = NewObject<URunStateSubsystem>(Instance);
        FCommandLine::Set(*PreviousArguments);
        Run->PartyDefinition = Catalog;
        Run->EnableCheckpointSaving();
        return Run;
    };
    FRunPartyMember Member;
    Member.SlotIndex = 0;
    Member.ClassId = TEXT("Hunter");
    Member.bCreated = true;
    FText Error;
    const TArray<FString> Selections = { FirstSelection, SecondSelection, ExplicitSelection };
    for (int32 Index = 0; Index < Selections.Num(); ++Index)
    {
        const FString Slot = URunStateSubsystem::ResolveCheckpointSlot(*Selections[Index]);
        Slots.Names.Add(Slot);
        URunStateSubsystem* Run = CreateRun(Selections[Index]);
        Member.CharacterName = FText::FromString(FString::Printf(TEXT("Isolated Hunter %d"), Index));
        TestTrue(TEXT("Selected run initializes and saves"), Run->InitializeRun({ Member }, Error));
        TestTrue(TEXT("Constructor writes to the selected isolated checkpoint"), UGameplayStatics::DoesSaveGameExist(Slot, 0));
    }
    for (int32 Index = 0; Index < Selections.Num(); ++Index)
    {
        URunStateSubsystem* Restored = CreateRun(Selections[Index]);
        if (TestTrue(TEXT("Each launch selection restores its own checkpoint"), Restored->LoadCheckpoint(Error)))
        {
            TestEqual(TEXT("Other Snapshot runs and explicit overrides do not replace this party"), Restored->GetPartyMembers()[0].CharacterName.ToString(), FString::Printf(TEXT("Isolated Hunter %d"), Index));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunPersistenceTest, "ProjectA.Persistence.Checkpoints", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRunPersistenceTest::RunTest(const FString& Parameters)
{
    const FString Slot = TEXT("T11_Test_") + FGuid::NewGuid().ToString();
    UGameInstance* Instance = NewObject<UGameInstance>();
    URunStateSubsystem* Run = NewObject<URunStateSubsystem>(Instance);
    Run->EnableCheckpointSaving(Slot);
    FText Error;
    TestFalse(TEXT("Missing save cannot continue"), Run->CanContinueSavedRun(Error));
    TestFalse(TEXT("Missing save explains why"), Error.IsEmpty());
    Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    FRunPartyMember Member;
    Member.SlotIndex = 2;
    Member.CharacterName = FText::FromString(TEXT("Saved Hunter"));
    Member.ClassId = TEXT("Hunter");
    Member.bCreated = true;
    TestTrue(TEXT("New run initializes"), Run->InitializeRun({ Member }, Error));
    TestTrue(TEXT("Initial checkpoint is on disk"), Run->CanContinueSavedRun(Error));
    Run->BeginEncounter(TEXT("Combat_01"));
    Run->MarkCombatStarted();
    TestFalse(TEXT("Mid-combat save is rejected"), Run->SaveCheckpoint(Error));
    URunStateSubsystem* Restored = NewObject<URunStateSubsystem>(Instance);
    Restored->EnableCheckpointSaving(Slot);
    TestTrue(TEXT("Combat interruption restores pre-combat checkpoint"), Restored->LoadCheckpoint(Error));
    TestTrue(TEXT("Restored checkpoint is map"), Restored->GetPhase() == ERunPhase::Map);
    Run->UpdatePartyMemberHP(2, 73.0f);
    Run->CompleteEncounter(ECombatResult::Victory);
    TestTrue(TEXT("Victory checkpoint loads"), Restored->LoadCheckpoint(Error));
    TestEqual(TEXT("HP survives disk serialization"), Restored->GetPartyMembers()[0].CurrentHP, 73.0f);
    TestEqual(TEXT("Name survives disk serialization"), Restored->GetPartyMembers()[0].CharacterName.ToString(), FString(TEXT("Saved Hunter")));
    TestEqual(TEXT("Slot survives disk serialization"), Restored->GetPartyMembers()[0].SlotIndex, 2);
    TestEqual(TEXT("Catalog survives disk serialization"), Restored->PartyDefinition.Get(), Run->PartyDefinition.Get());
    TestTrue(TEXT("Loaded result can Continue"), Restored->ContinueRun());
    TestTrue(TEXT("Next node is available"), Restored->CanStartNode(TEXT("Combat_02")));
    URunSaveGame* Invalid = Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    Invalid->Version = 999;
    UGameplayStatics::SaveGameToSlot(Invalid, Slot, 0);
    TestFalse(TEXT("Unsupported version is rejected"), Restored->LoadCheckpoint(Error));
    TestTrue(TEXT("Rejected load preserves current run"), Restored->CanStartNode(TEXT("Combat_02")));
    Invalid->Version = 1;
    Invalid->CompletedNodes.Add(TEXT("Combat_01"));
    UGameplayStatics::SaveGameToSlot(Invalid, Slot, 0);
    TestFalse(TEXT("Invalid progression is rejected"), Restored->LoadCheckpoint(Error));
    Restored->BeginEncounter(TEXT("Combat_02"));
    Restored->MarkCombatStarted();
    Restored->UpdatePartyMemberHP(2, 0.0f);
    Restored->CompleteEncounter(ECombatResult::Defeat);
    TestFalse(TEXT("Defeat checkpoint prevents replay"), Restored->CanContinueSavedRun(Error));
    TestTrue(TEXT("Defeat checkpoint was saved"), Restored->GetSaveError().IsEmpty());
    TestTrue(TEXT("Test save cleaned"), UGameplayStatics::DeleteGameInSlot(Slot, 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunRestartTest, "ProjectA.Persistence.ProcessRestart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRunRestartTest::RunTest(const FString& Parameters)
{
    // Opt-in fixture supports two independent game processes without touching the player's save.
    // 플레이어 저장을 건드리지 않고 독립 프로세스 두 개에서 선택적으로 복원을 검증합니다.
    if (!FParse::Param(FCommandLine::Get(), TEXT("T11WriteCheckpoint")) && !FParse::Param(FCommandLine::Get(), TEXT("T11ReadCheckpoint")))
    {
        AddInfo(TEXT("Cross-process fixture is opt-in; Checkpoints covers normal disk round trips."));
        return true;
    }
    UGameInstance* Instance = NewObject<UGameInstance>();
    URunStateSubsystem* Run = NewObject<URunStateSubsystem>(Instance);
    Run->EnableCheckpointSaving(TEXT("T11_ProcessRestart"));
    FText Error;
    if (FParse::Param(FCommandLine::Get(), TEXT("T11WriteCheckpoint")))
    {
        Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_VerticalSliceParty.DA_VerticalSliceParty"));
        FRunPartyMember Member;
        Member.SlotIndex = 1;
        Member.CharacterName = FText::FromString(TEXT("Restart Scholar"));
        Member.ClassId = TEXT("Scholar");
        Member.bCreated = true;
        Run->InitializeRun({ Member }, Error);
        Run->BeginEncounter(TEXT("Combat_01"));
        Run->MarkCombatStarted();
        Run->UpdatePartyMemberHP(1, 61.0f);
        Run->CompleteEncounter(ECombatResult::Victory);
        TestTrue(TEXT("Restart fixture saved"), Run->GetSaveError().IsEmpty());
    }
    else
    {
        if (!TestTrue(TEXT("Independent process restores checkpoint"), Run->LoadCheckpoint(Error)))
        {
            return false;
        }
        TestEqual(TEXT("Restored name"), Run->GetPartyMembers()[0].CharacterName.ToString(), FString(TEXT("Restart Scholar")));
        TestEqual(TEXT("Restored HP"), Run->GetPartyMembers()[0].CurrentHP, 61.0f);
        TestEqual(TEXT("Restored profession"), Run->GetPartyMembers()[0].ClassId, FName(TEXT("Scholar")));
        TestTrue(TEXT("Restored result continues"), Run->ContinueRun());
        TestTrue(TEXT("Restored progress opens second node"), Run->CanStartNode(TEXT("Combat_02")));
        UGameplayStatics::DeleteGameInSlot(TEXT("T11_ProcessRestart"), 0);
    }
    return true;
}

#endif
