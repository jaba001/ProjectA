#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunIdentityLibrary.h"
#include "Tests/RunRewardTestHelpers.h"
#include "Game/Run/RunSaveGame.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunInitialPublicationTest, "ProjectA.Persistence.InitialRunAtomicRetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunInitialPublicationTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UPartyDefinitionDataAsset> Catalog(LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty")));
    if (!TestNotNull(TEXT("Initial publication uses the authored party catalog"), Catalog.Get())) return false;
    for (const int32 ParticipantCount : { 2, 4 })
    {
        const FString Slot = TEXT("ProjectA_Automation_InitialPublication_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
        TStrongObjectPtr<URunStateSubsystem> Run(NewObject<URunStateSubsystem>(Instance.Get()));
        Run->PartyDefinition = Catalog.Get();
        Run->EnableCheckpointSaving(Slot);
        int32 Events = 0;
        Run->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
        ON_SCOPE_EXIT
        {
            Run->OnRunStateChanged.Clear();
            UGameplayStatics::DeleteGameInSlot(Slot, 0);
        };
        FRunIdentityData Identity;
        Identity.SchemaVersion = URunIdentityLibrary::CurrentSchemaVersion;
        Identity.Origin = ERunIdentityOrigin::LocalDevelopment;
        Identity.RunId = FGuid::NewGuid();
        Identity.HostEpoch = 1;
        TArray<FRunPartyMember> Members;
        for (int32 Index = 0; Index < ParticipantCount; ++Index)
        {
            FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
            Participant.AccountId.Provider = TEXT("Development");
            Participant.AccountId.Subject = FString::Printf(TEXT("InitialPublicationOwner%d"), Index + 1);
            Participant.JoinOrdinal = Index + 1;
            FRunPartyMember& Member = Members.AddDefaulted_GetRef();
            Member.SlotIndex = Index;
            Member.CharacterName = FText::FromString(FString::Printf(TEXT("Initial Archer %d"), Index + 1));
            Member.ClassId = TEXT("Archer");
            Member.CharacterId = FGuid::NewGuid();
            Member.OwnerAccountId = Participant.AccountId;
            Member.bCreated = true;
        }
        Identity.HostAccountId = Identity.OriginalParticipants[0].AccountId;
        FText Error;
        FRunCheckpointStorage::FailNextWriteForTesting();
        TestFalse(TEXT("The first cooperative checkpoint failure is reported to the lobby caller"), Run->InitializeRunWithIdentity(Members, Identity, Error));
        TestFalse(TEXT("Initial save failure returns an actionable explanation"), Error.IsEmpty());
        TestTrue(TEXT("A failed first checkpoint publishes no Run, party or event"), Run->GetPhase() == ERunPhase::None && !Run->GetRunIdentity().RunId.IsValid() && Run->GetPartyMembers().IsEmpty() && Events == 0);
        TestFalse(TEXT("A failed first checkpoint leaves no save file"), UGameplayStatics::DoesSaveGameExist(Slot, 0));
        if (!TestTrue(TEXT("The exact same cooperative identity retries successfully"), Run->InitializeRunWithIdentity(Members, Identity, Error))) return false;
        TestTrue(TEXT("A successful retry publishes the intended Run once and clears the storage error"), Run->GetPhase() == ERunPhase::Map && Run->GetRunIdentity().RunId == Identity.RunId && Events == 1 && Run->GetSaveError().IsEmpty());
        TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)));
        if (!TestNotNull(TEXT("Successful retry has a durable native checkpoint"), Saved.Get())) return false;
        TestTrue(TEXT("Durable creation preserves original participant numbers and Host"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Saved->Identity, &Identity, 0));
        TestEqual(TEXT("Durable creation stores one character per original participant"), Saved->Party.Num(), ParticipantCount);
        TArray<uint8> BeforeBytes;
        if (!TestTrue(TEXT("The previous checkpoint bytes can be captured"), UGameplayStatics::LoadDataFromSlot(BeforeBytes, Slot, 0))) return false;
        const FRunIdentityData BeforeIdentity = Run->GetRunIdentity();
        const TArray<FRunPartyMember> BeforeParty = Run->GetPartyMembers();
        FRunIdentityData Replacement = Identity;
        Replacement.RunId = FGuid::NewGuid();
        if (ParticipantCount == 2)
        {
            TArray<FRunPartyMember> ExtraParty = Members;
            FRunPartyMember ExtraMember = Members[0];
            ExtraMember.SlotIndex = 2;
            ExtraMember.CharacterId = FGuid::NewGuid();
            ExtraParty.Add(ExtraMember);
            TestFalse(TEXT("New ordinary cooperative creation rejects an extra character before publication"), Run->InitializeRunWithIdentity(ExtraParty, Replacement, Error));
        }
        FRunCheckpointStorage::FailNextWriteForTesting();
        TestFalse(TEXT("A replacement Run cannot succeed when its first checkpoint write fails"), Run->InitializeRunWithIdentity(Members, Replacement, Error));
        TestTrue(TEXT("Rejected replacement preserves the complete previous identity"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&BeforeIdentity, &Run->GetRunIdentity(), 0));
        TestEqual(TEXT("Rejected replacement publishes no extra event"), Events, 1);
        TestEqual(TEXT("Rejected replacement preserves the previous party size"), Run->GetPartyMembers().Num(), BeforeParty.Num());
        for (int32 Index = 0; Index < BeforeParty.Num(); ++Index)
        {
            TestTrue(TEXT("Rejected replacement preserves every previous character field"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&BeforeParty[Index], &Run->GetPartyMembers()[Index], 0));
        }
        TArray<uint8> AfterBytes;
        TestTrue(TEXT("The previous checkpoint remains readable after rejection"), UGameplayStatics::LoadDataFromSlot(AfterBytes, Slot, 0));
        TestTrue(TEXT("Rejected creation preserves exact previous durable bytes"), BeforeBytes == AfterBytes);
        if (!TestTrue(TEXT("Failed replacement retries with the same identity without resetting the Run"), Run->InitializeRunWithIdentity(Members, Replacement, Error))) return false;
        TestTrue(TEXT("Replacement retry publishes once"), Events == 2 && Run->GetRunIdentity().RunId == Replacement.RunId && Run->GetSaveError().IsEmpty());

        if (ParticipantCount == 2)
        {
            // Earlier cooperative saves may contain extra owned characters; loading must retain their roster.
            // 이전 협동 저장에는 추가 소유 캐릭터가 있을 수 있으므로 불러올 때 기존 편성을 보존해야 합니다.
            FRunPartyMember ExtraMember = Saved->Party[0];
            ExtraMember.SlotIndex = 2;
            ExtraMember.CharacterId = FGuid::NewGuid();
            ExtraMember.Items.Reset();
            ExtraMember.Equipment = FRunEquipmentState();
            Saved->Party.Add(ExtraMember);
            if (!TestTrue(TEXT("An older compatible multi-character fixture is serialized"), UGameplayStatics::SaveGameToSlot(Saved.Get(), Slot, 0))) return false;
            TStrongObjectPtr<URunStateSubsystem> Reader(NewObject<URunStateSubsystem>(Instance.Get()));
            Reader->EnableCheckpointSaving(Slot);
            if (!TestTrue(TEXT("Creation-only limits do not reject an existing cooperative save"), Reader->LoadCheckpoint(Error)))
            {
                AddError(Error.ToString());
                return false;
            }
            TestTrue(TEXT("Existing extra characters retain their identity and original owner"), Reader->GetPartyMembers().Num() == 3 && Reader->GetPartyMembers()[2].CharacterId == ExtraMember.CharacterId && Reader->GetPartyMembers()[2].OwnerAccountId == ExtraMember.OwnerAccountId);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunLegacyCatalogPathTest, "ProjectA.Persistence.LegacyCatalogPath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunLegacyCatalogPathTest::RunTest(const FString& Parameters)
{
    const FString Slot = TEXT("ProjectA_Automation_LegacyCatalog_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    ON_SCOPE_EXIT { UGameplayStatics::DeleteGameInSlot(Slot, 0); };
    UGameInstance* Instance = NewObject<UGameInstance>();
    URunStateSubsystem* Run = NewObject<URunStateSubsystem>(Instance);
    Run->EnableCheckpointSaving(Slot);
    Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    FRunPartyMember Member;
    Member.SlotIndex = 0;
    Member.ClassId = TEXT("Warrior");
    Member.CharacterName = FText::FromString(TEXT("Legacy Warrior"));
    Member.bCreated = true;
    Member.bPlayerControlled = true;
    FText Error;
    if (!TestTrue(TEXT("Create a supported noncombat checkpoint."), Run->InitializeRun({ Member }, Error))) return false;
    URunSaveGame* Saved = Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    if (!TestNotNull(TEXT("Load the isolated legacy fixture."), Saved)) return false;
    Saved->Catalog = FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    if (!TestTrue(TEXT("Write the original pre-folder-move catalog path."), UGameplayStatics::SaveGameToSlot(Saved, Slot, 0))) return false;
    TArray<uint8> Before;
    UGameplayStatics::LoadDataFromSlot(Before, Slot, 0);
    URunStateSubsystem* Restored = NewObject<URunStateSubsystem>(Instance);
    Restored->EnableCheckpointSaving(Slot);
    if (!TestTrue(TEXT("The supported legacy catalog path continues successfully."), Restored->LoadStandaloneCheckpoint(Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    TestEqual(TEXT("Redirect resolves the authored party catalog."), Restored->PartyDefinition.Get(), Run->PartyDefinition.Get());
    TestTrue(TEXT("Map and direct-control selection survive legacy loading."), Restored->GetPhase() == ERunPhase::Map && Restored->GetPartyMembers()[0].bPlayerControlled);
    TArray<uint8> After;
    UGameplayStatics::LoadDataFromSlot(After, Slot, 0);
    TestTrue(TEXT("Loading the legacy save preserves its exact durable bytes."), Before == After);
    return true;
}

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
    UPartyDefinitionDataAsset* Catalog = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
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
    Member.ClassId = TEXT("Archer");
    Member.bCreated = true;
    FText Error;
    const TArray<FString> Selections = { FirstSelection, SecondSelection, ExplicitSelection };
    for (int32 Index = 0; Index < Selections.Num(); ++Index)
    {
        const FString Slot = URunStateSubsystem::ResolveCheckpointSlot(*Selections[Index]);
        Slots.Names.Add(Slot);
        URunStateSubsystem* Run = CreateRun(Selections[Index]);
        Member.CharacterName = FText::FromString(FString::Printf(TEXT("Isolated Archer %d"), Index));
        TestTrue(TEXT("Selected run initializes and saves"), Run->InitializeRun({ Member }, Error));
        TestTrue(TEXT("Constructor writes to the selected isolated checkpoint"), UGameplayStatics::DoesSaveGameExist(Slot, 0));
    }
    for (int32 Index = 0; Index < Selections.Num(); ++Index)
    {
        URunStateSubsystem* Restored = CreateRun(Selections[Index]);
        if (TestTrue(TEXT("Each launch selection restores its own checkpoint"), Restored->LoadCheckpoint(Error)))
        {
            TestEqual(TEXT("Other Snapshot runs and explicit overrides do not replace this party"), Restored->GetPartyMembers()[0].CharacterName.ToString(), FString::Printf(TEXT("Isolated Archer %d"), Index));
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
    Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    FRunPartyMember Member;
    Member.SlotIndex = 2;
    Member.CharacterName = FText::FromString(TEXT("Saved Archer"));
    Member.ClassId = TEXT("Archer");
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
    TestEqual(TEXT("Name survives disk serialization"), Restored->GetPartyMembers()[0].CharacterName.ToString(), FString(TEXT("Saved Archer")));
    TestEqual(TEXT("Slot survives disk serialization"), Restored->GetPartyMembers()[0].SlotIndex, 2);
    TestEqual(TEXT("Catalog survives disk serialization"), Restored->PartyDefinition.Get(), Run->PartyDefinition.Get());
    TestTrue(TEXT("Loaded result can Continue after collecting its reward"), RunRewardTests::CollectPendingGoldRewards(Restored) && Restored->ContinueRun());
    TestTrue(TEXT("Loaded choices lead through a shop"), Restored->SelectRunEncounter(TEXT("Shop_02")) && Restored->LeaveRunEncounter());
    TestTrue(TEXT("Next node is available"), Restored->CanStartNode(TEXT("Combat_02")));
    URunSaveGame* Invalid = Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    const FName SupportedClassId = Invalid->Party[0].ClassId;
    Invalid->Party[0].ClassId = TEXT("Hunter");
    if (!TestTrue(TEXT("The legacy profession fixture is stored"), UGameplayStatics::SaveGameToSlot(Invalid, Slot, 0))) return false;
    TestFalse(TEXT("An old test profession is rejected without mapping it to a new job"), Restored->LoadCheckpoint(Error));
    TestTrue(TEXT("The incompatible save reports its old profession ID"), Error.ToString().Contains(TEXT("Hunter")));
    const URunSaveGame* Preserved = Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    if (TestNotNull(TEXT("The incompatible save remains readable on disk"), Preserved)) TestEqual(TEXT("The old profession ID is not rewritten"), Preserved->Party[0].ClassId, FName(TEXT("Hunter")));
    TestEqual(TEXT("Rejected profession load preserves the current party"), Restored->GetPartyMembers()[0].ClassId, SupportedClassId);
    Invalid->Party[0].ClassId = SupportedClassId;
    Invalid->Version = 999;
    UGameplayStatics::SaveGameToSlot(Invalid, Slot, 0);
    TestFalse(TEXT("Unsupported version is rejected"), Restored->LoadCheckpoint(Error));
    TestTrue(TEXT("Rejected load preserves current run"), Restored->CanStartNode(TEXT("Combat_02")));
    Invalid->Version = 2;
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
    FString RestartSlot = TEXT("T11_ProcessRestart");
    FParse::Value(FCommandLine::Get(), TEXT("T11CheckpointSlot="), RestartSlot);
    if (!TestTrue(TEXT("Cross-process tests use an isolated fixture slot."), RestartSlot == TEXT("T11_ProcessRestart") || RestartSlot.StartsWith(TEXT("ProjectA_Automation_Restart_")))) return false;
    Run->EnableCheckpointSaving(RestartSlot);
    FText Error;
    if (FParse::Param(FCommandLine::Get(), TEXT("T11WriteCheckpoint")))
    {
        if (!TestFalse(TEXT("The writer cannot overwrite a pre-existing fixture."), UGameplayStatics::DoesSaveGameExist(RestartSlot, 0))) return false;
        Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
        FRunPartyMember Member;
        Member.SlotIndex = 1;
        Member.CharacterName = FText::FromString(TEXT("Restart Mage"));
        Member.ClassId = TEXT("Mage");
        Member.bCreated = true;
        if (!TestTrue(TEXT("Restart fixture initializes"), Run->InitializeRun({ Member }, Error)) || !TestTrue(TEXT("Initial restart checkpoint saves"), Run->GetSaveError().IsEmpty()))
        {
            AddError(Error.IsEmpty() ? Run->GetSaveError().ToString() : Error.ToString());
            return false;
        }
        if (!TestTrue(TEXT("Restart fixture begins its encounter"), Run->BeginEncounter(TEXT("Combat_01"))) || !TestTrue(TEXT("Restart fixture starts combat"), Run->MarkCombatStarted()))
        {
            return false;
        }
        Run->UpdatePartyMemberHP(1, 61.0f);
        if (!TestTrue(TEXT("Restart fixture reaches its result"), Run->CompleteEncounter(ECombatResult::Victory)) || !TestTrue(TEXT("Restart fixture saved"), Run->GetSaveError().IsEmpty()))
        {
            if (!Run->GetSaveError().IsEmpty())
            {
                AddError(Run->GetSaveError().ToString());
            }
            return false;
        }
    }
    else
    {
        const URunSaveGame* Saved = Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(RestartSlot, 0));
        if (!TestNotNull(TEXT("Independent process reads the writer's SaveGame"), Saved) || !TestEqual(TEXT("Restart fixture uses save version two"), Saved->Version, 2))
        {
            return false;
        }
        const FRunIdentityData SavedIdentity = Saved->Identity;
        const TArray<FRunPartyMember> SavedParty = Saved->Party;
        if (!TestTrue(TEXT("Independent process restores checkpoint"), Run->LoadCheckpoint(Error)))
        {
            AddError(Error.ToString());
            return false;
        }
        if (!TestEqual(TEXT("Writer saved one party member"), SavedParty.Num(), 1) || !TestEqual(TEXT("Reader restores one party member"), Run->GetPartyMembers().Num(), 1))
        {
            return false;
        }
        const FRunIdentityData& RestoredIdentity = Run->GetRunIdentity();
        const FRunPartyMember& RestoredMember = Run->GetPartyMembers()[0];
        TestTrue(TEXT("Independent process retains every identity field"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&SavedIdentity, &RestoredIdentity, 0));
        TestTrue(TEXT("Run ID survives process restart"), SavedIdentity.RunId.IsValid() && RestoredIdentity.RunId == SavedIdentity.RunId);
        TestTrue(TEXT("Host account survives process restart"), RestoredIdentity.HostAccountId == SavedIdentity.HostAccountId);
        TestEqual(TEXT("Host epoch survives process restart"), RestoredIdentity.HostEpoch, SavedIdentity.HostEpoch);
        TestTrue(TEXT("Independent process retains every party field"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&SavedParty[0], &RestoredMember, 0));
        TestTrue(TEXT("Character ID survives process restart"), SavedParty[0].CharacterId.IsValid() && RestoredMember.CharacterId == SavedParty[0].CharacterId);
        TestTrue(TEXT("Original character owner survives process restart"), RestoredMember.OwnerAccountId == SavedParty[0].OwnerAccountId);
        TestEqual(TEXT("Restored name"), Run->GetPartyMembers()[0].CharacterName.ToString(), FString(TEXT("Restart Mage")));
        TestEqual(TEXT("Restored HP"), Run->GetPartyMembers()[0].CurrentHP, 61.0f);
        TestEqual(TEXT("Restored profession"), Run->GetPartyMembers()[0].ClassId, FName(TEXT("Mage")));
        TestTrue(TEXT("Restored result continues after collecting its reward"), RunRewardTests::CollectPendingGoldRewards(Run) && Run->ContinueRun());
        TestTrue(TEXT("Restored choices lead through a shop"), Run->SelectRunEncounter(TEXT("Shop_02")) && Run->LeaveRunEncounter());
        TestTrue(TEXT("Restored progress opens second node"), Run->CanStartNode(TEXT("Combat_02")));
        UGameplayStatics::DeleteGameInSlot(RestartSlot, 0);
    }
    return true;
}

#endif
