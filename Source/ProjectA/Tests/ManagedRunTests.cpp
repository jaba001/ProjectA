#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/GameplayAbility.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Run/Authority/LocalRunAuthorityRecord.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunIdentityLibrary.h"
#include "Game/Run/RunParticipationLibrary.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Game/Snapshot/PartySnapshotSaveGame.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    struct FManagedTestSession
    {
        TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
        TStrongObjectPtr<URunStateSubsystem> Run{NewObject<URunStateSubsystem>(Instance.Get())};
    };

    struct FManagedFixture
    {
        FString Namespace = TEXT("M_") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(28);
        FString OrdinarySlot = TEXT("T14_ManagedOrdinary_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        FRunIdentityData Identity;
        TArray<FRunPartyMember> Party;
        TArray<TUniquePtr<FManagedTestSession>> Sessions;
        FText Error;

        explicit FManagedFixture(int32 Count = 4)
        {
            Identity.SchemaVersion = URunIdentityLibrary::CurrentSchemaVersion;
            Identity.Origin = ERunIdentityOrigin::LocalDevelopment;
            Identity.RunId = FGuid::NewGuid();
            Identity.HostEpoch = 1;
            for (int32 Index = 0; Index < Count; ++Index)
            {
                FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
                Participant.AccountId.Provider = TEXT("Development");
                Participant.AccountId.Subject = FString::Printf(TEXT("ManagedOwner%d"), Index + 1);
                Participant.JoinOrdinal = Index + 1;
                FRunPartyMember& Member = Party.AddDefaulted_GetRef();
                Member.SlotIndex = Index;
                Member.CharacterName = FText::FromString(FString::Printf(TEXT("Managed Hunter %d"), Index + 1));
                Member.ClassId = TEXT("Hunter");
                Member.bCreated = true;
                Member.CharacterId = FGuid::NewGuid();
                Member.OwnerAccountId = Participant.AccountId;
            }
            Identity.HostAccountId = Account(1);
        }

        ~FManagedFixture()
        {
            for (const TUniquePtr<FManagedTestSession>& Session : Sessions)
            {
                Session->Run->OnRunStateChanged.Clear();
                Session->Run->CloseManagedRun();
            }
            const FString Slot = FLocalRunAuthorityStore(Namespace).GetSlotName(Identity.RunId);
            UGameplayStatics::DeleteGameInSlot(Slot, 0);
            UGameplayStatics::DeleteGameInSlot(OrdinarySlot, 0);
            const FString Path = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".sav"));
            IFileManager::Get().Delete(*(Path + TEXT(".lease")), false, true);
            IFileManager::Get().Delete(*(Path + TEXT(".txn")), false, true);
        }

        const FRunAccountId& Account(int32 Ordinal) const { return Identity.OriginalParticipants[Ordinal - 1].AccountId; }

        URunStateSubsystem* NewSession(int32 Ordinal)
        {
            TUniquePtr<FManagedTestSession> Session = MakeUnique<FManagedTestSession>();
            URunStateSubsystem* Run = Session->Run.Get();
            Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_VerticalSliceParty.DA_VerticalSliceParty"));
            FLocalDevelopmentCallerContext Context;
            Context.StoreNamespace = Namespace;
            Context.AccountId = Account(Ordinal);
            Sessions.Add(MoveTemp(Session));
            return Run->ConfigureLocalDevelopmentCaller(Context, Error) ? Run : nullptr;
        }

        TArray<uint8> FileBytes() const
        {
            TArray<uint8> Bytes;
            UGameplayStatics::LoadDataFromSlot(Bytes, FLocalRunAuthorityStore(Namespace).GetSlotName(Identity.RunId), 0);
            return Bytes;
        }

        TStrongObjectPtr<URunSaveGame> LoadPayload()
        {
            FRunAuthorityRecordData Record;
            if (FLocalRunAuthorityStore(Namespace).Read(Identity.RunId, Record, Error) != ERunAuthorityResult::Success) return TStrongObjectPtr<URunSaveGame>();
            return TStrongObjectPtr<URunSaveGame>(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromMemory(Record.Payload)));
        }

        bool MakeCheckpoint(URunStateSubsystem* Run, FCombatCheckpointData& Checkpoint)
        {
            Checkpoint.AttemptId = FGuid::NewGuid();
            Checkpoint.Revision = 1;
            Checkpoint.CompletedTurnSerial = 7;
            Checkpoint.Identity = Run->GetRunIdentity();
            Checkpoint.NodeId = Run->GetCurrentNodeId();
            Checkpoint.EncounterId = Run->GetCurrentEncounterId();
            Checkpoint.SchemaVersion = 2;
            for (const FRunPartyMember& Member : Run->GetPartyMembers())
            {
                FProfessionDefinition Profession;
                if (!Run->PartyDefinition || !Run->PartyDefinition->ResolveProfession(Member.ClassId, Profession) || Profession.StartingSkills.IsEmpty()) return false;
                FCombatCheckpointUnit& Unit = Checkpoint.Units.AddDefaulted_GetRef();
                Unit.UnitId = FGuid::NewGuid();
                Unit.CharacterId = Member.CharacterId;
                Unit.OwnerAccountId = Member.OwnerAccountId;
                Unit.PartySlot = Member.SlotIndex;
                Unit.UnitClass = FSoftObjectPath(Profession.CombatClass.Get());
                Unit.CharacterName = Member.CharacterName;
                Unit.HP = Profession.MaxHP - 5.0f;
                Unit.MaxHP = Profession.MaxHP;
                Unit.AP = 1;
                Unit.SubAP = 1;
                Unit.GridCoord = FIntPoint(Member.SlotIndex, 0);
                if (!URunParticipationLibrary::ResolveControlMode(Run->GetParticipation(), Run->GetRunIdentity(), Run->GetPartyMembers(), Unit.CharacterId, Unit.PartyControlMode, Error)) return false;
                for (USkillDefinitionDataAsset* Skill : Profession.StartingSkills)
                {
                    if (!Skill) return false;
                    Unit.Skills.Add(FSoftObjectPath(Skill));
                }
                Unit.DefaultAttackAbility = FSoftObjectPath(Profession.StartingSkills[0]->AbilityClass.Get());
            }
            FCombatCheckpointUnit Enemy = Checkpoint.Units[0];
            Enemy.UnitId = FGuid::NewGuid();
            Enemy.CharacterId.Invalidate();
            Enemy.OwnerAccountId = FRunAccountId();
            Enemy.PartySlot = INDEX_NONE;
            Enemy.Team = ETeam::Enemy;
            Enemy.PartyControlMode = EPartyControlMode::Human;
            Enemy.UnitClass = FSoftObjectPath(AEnemyUnit::StaticClass());
            Enemy.GridCoord = FIntPoint(0, 2);
            Checkpoint.Units.Add(Enemy);
            return true;
        }

        bool StartAndCommit(URunStateSubsystem* Run, FCombatCheckpointData& Checkpoint)
        {
            return Run->BeginEncounter(TEXT("Combat_01")) && Run->MarkCombatStarted() && MakeCheckpoint(Run, Checkpoint) && Run->CommitCombatCheckpoint(Checkpoint, Error);
        }
    };

    bool ManagedSameIdentity(const FRunIdentityData& Left, const FRunIdentityData& Right) { return FRunIdentityData::StaticStruct()->CompareScriptStruct(&Left, &Right, 0); }
    bool ManagedSameCheckpoint(const FCombatCheckpointData& Left, const FCombatCheckpointData& Right) { return FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Left, &Right, 0); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManagedRunContextTest, "ProjectA.Run.Managed.ContextAndIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FManagedRunContextTest::RunTest(const FString& Parameters)
{
    FManagedFixture Fixture(2);
    URunStateSubsystem* Host = Fixture.NewSession(1);
    if (!TestNotNull(TEXT("A dedicated C++ development caller configures"), Host)) return false;
    FLocalDevelopmentCallerContext Changed;
    Changed.StoreNamespace = Fixture.Namespace.ToUpper();
    Changed.AccountId = Fixture.Account(1);
    TestTrue(TEXT("Repeating the same normalized context is harmless"), Host->ConfigureLocalDevelopmentCaller(Changed, Fixture.Error));
    Changed.AccountId = Fixture.Account(2);
    TestFalse(TEXT("A GameInstance cannot change its assigned local caller"), Host->ConfigureLocalDevelopmentCaller(Changed, Fixture.Error));
    Changed.AccountId = Fixture.Account(1);
    Changed.StoreNamespace = TEXT("OtherStore");
    TestFalse(TEXT("A GameInstance cannot switch its authority namespace"), Host->ConfigureLocalDevelopmentCaller(Changed, Fixture.Error));
    TestTrue(TEXT("Failed context changes preserve the caller"), Host->GetLocalCaller() == Fixture.Account(1));

    int32 Events = 0;
    Host->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
    TStrongObjectPtr<UPartyDefinitionDataAsset> Catalog(Host->PartyDefinition.Get());
    Host->PartyDefinition = nullptr;
    TestFalse(TEXT("Managed creation rejects a missing party catalog"), Host->CreateManagedRun(Fixture.Party, Fixture.Identity, Fixture.Error));
    TestFalse(TEXT("Failure after successful participation validation retains an explanation"), Fixture.Error.IsEmpty());
    TestTrue(TEXT("Missing catalog creates no lease or partial Run"), !Host->IsManagedRun() && !Host->HasManagedLease() && Host->GetPhase() == ERunPhase::None);
    Host->PartyDefinition = Catalog.Get();
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Failed initial publication cannot create a managed session"), Host->CreateManagedRun(Fixture.Party, Fixture.Identity, Fixture.Error));
    TestTrue(TEXT("Failed creation preserves empty memory, target and lease"), !Host->IsManagedRun() && !Host->HasManagedLease() && Host->GetPhase() == ERunPhase::None && !Host->GetManagedResumeTarget().IsValid());
    TestEqual(TEXT("Failed creation emits no state change"), Events, 0);
    if (!TestTrue(TEXT("Two original owners create a managed v4 Run"), Host->CreateManagedRun(Fixture.Party, Fixture.Identity, Fixture.Error))) return false;
    TestTrue(TEXT("Creation acquires a lease and selects the new Run"), Host->HasManagedLease() && Host->IsManagedRun() && !Host->IsManagedResumePending() && Host->GetManagedResumeTarget() == Fixture.Identity.RunId);
    TestEqual(TEXT("Creation publishes once"), Events, 1);
    TestEqual(TEXT("Creation starts with all original humans"), Host->GetParticipation().HumanParticipants.Num(), 2);
    TStrongObjectPtr<URunSaveGame> Saved = Fixture.LoadPayload();
    if (!TestNotNull(TEXT("The authority record contains native RunSaveGame bytes"), Saved.Get())) return false;
    TestEqual(TEXT("Managed payload version is four"), Saved->Version, 4);
    TestTrue(TEXT("Creation preserves identity without authentication claims"), ManagedSameIdentity(Saved->Identity, Fixture.Identity));

    TStrongObjectPtr<UPartySnapshotSaveGame> Marker(NewObject<UPartySnapshotSaveGame>());
    if (!TestTrue(TEXT("An isolated ordinary-slot marker exists"), UGameplayStatics::SaveGameToSlot(Marker.Get(), Fixture.OrdinarySlot, 0))) return false;
    TArray<uint8> OrdinaryBefore;
    UGameplayStatics::LoadDataFromSlot(OrdinaryBefore, Fixture.OrdinarySlot, 0);
    Host->EnableCheckpointSaving(Fixture.OrdinarySlot);
    const FRunAuthorityStamp Before = Host->GetManagedStamp();
    TestTrue(TEXT("Managed saving uses the authority lease even after ordinary slot selection"), Host->SaveCheckpoint(Fixture.Error));
    TestTrue(TEXT("A managed save advances only its authority revision"), Host->GetManagedStamp().Revision == Before.Revision + 1 && Host->GetManagedStamp().SessionId == Before.SessionId);
    TArray<uint8> OrdinaryAfter;
    UGameplayStatics::LoadDataFromSlot(OrdinaryAfter, Fixture.OrdinarySlot, 0);
    TestTrue(TEXT("Managed saving leaves ordinary progress bytes untouched"), OrdinaryBefore == OrdinaryAfter);
    TestFalse(TEXT("Ordinary initialization cannot discard a held managed lease"), Host->InitializeRun(Fixture.Party, Fixture.Error));
    TestFalse(TEXT("Ordinary loading cannot replace an active managed Run"), Host->LoadCheckpoint(Fixture.Error));
    TestTrue(TEXT("Rejected replacement retains the managed identity and lease"), Host->HasManagedLease() && ManagedSameIdentity(Host->GetRunIdentity(), Fixture.Identity));

    URunStateSubsystem* Reader = Fixture.NewSession(2);
    if (!TestNotNull(TEXT("Another original owner has a separate development context"), Reader)) return false;
    FManagedRunPreview Preview;
    TestTrue(TEXT("An original owner can read the complete record while its Host is running"), Reader->ReadManagedRun(Fixture.Identity.RunId, Preview, Fixture.Error));
    TestTrue(TEXT("Read does not acquire a lease or initialize memory"), !Reader->HasManagedLease() && Reader->GetPhase() == ERunPhase::None);
    TestTrue(TEXT("A valid target can be selected without ownership entry fields"), Reader->SetManagedResumeTarget(Fixture.Identity.RunId, Fixture.Error));
    const FGuid Target = Reader->GetManagedResumeTarget();
    TestFalse(TEXT("Invalid target selection preserves the prior target"), Reader->SetManagedResumeTarget(FGuid(), Fixture.Error));
    TestTrue(TEXT("The previous target is retained"), Reader->GetManagedResumeTarget() == Target);
    const FRunAuthorityStamp PreviewStamp = Preview.Stamp;
    TestFalse(TEXT("Invalid read fails without overwriting the output preview"), Reader->ReadManagedRun(FGuid(), Preview, Fixture.Error));
    TestTrue(TEXT("Failed read preserves the preview stamp"), Preview.Stamp == PreviewStamp);
    Host->CloseManagedRun();
    TestTrue(TEXT("Closing releases active state but preserves caller and menu target"), !Host->IsManagedRun() && !Host->HasManagedLease() && Host->GetPhase() == ERunPhase::None && Host->GetLocalCaller() == Fixture.Account(1) && Host->GetManagedResumeTarget() == Fixture.Identity.RunId);
    TestTrue(TEXT("Closing retains the committed record"), Reader->ReadManagedRun(Target, Preview, Fixture.Error));
    Host->OnRunStateChanged.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManagedRunSoloTest, "ProjectA.Run.Managed.SoloAndPermanentAI", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FManagedRunSoloTest::RunTest(const FString& Parameters)
{
    for (int32 Ordinal = 2; Ordinal <= 4; ++Ordinal)
    {
        FManagedFixture Fixture;
        URunStateSubsystem* Host = Fixture.NewSession(1);
        if (!Host || !TestTrue(TEXT("Original Host creates the four-owner Run"), Host->CreateManagedRun(Fixture.Party, Fixture.Identity, Fixture.Error))) return false;
        FCombatCheckpointData OriginalCheckpoint;
        if (!TestTrue(TEXT("A complete original combat boundary commits"), Fixture.StartAndCommit(Host, OriginalCheckpoint))) return false;
        const FRunAuthorityStamp OriginalStamp = Host->GetManagedStamp();
        Host->CloseManagedRun();
        URunStateSubsystem* Solo = Fixture.NewSession(Ordinal);
        if (!TestNotNull(TEXT("The selected original member owns a fresh caller context"), Solo)) return false;
        if (!TestTrue(TEXT("Original member two, three or four can explicitly resume alone"), Solo->ResumeManagedRun(OriginalStamp, { Fixture.Account(Ordinal) }, Fixture.Error))) return false;
        TestTrue(TEXT("Solo resume changes Host epoch and execution nonce together"), Solo->GetRunIdentity().HostAccountId == Fixture.Account(Ordinal) && Solo->GetManagedStamp().HostEpoch == OriginalStamp.HostEpoch + 1 && Solo->GetManagedStamp().Revision == OriginalStamp.Revision + 1 && Solo->GetManagedStamp().SessionId != OriginalStamp.SessionId);
        TestTrue(TEXT("The resumed Run waits for successful gameplay restoration"), Solo->IsManagedResumePending() && Solo->HasManagedLease());
        FCombatCheckpointData Expected = OriginalCheckpoint;
        Expected.Identity = Solo->GetRunIdentity();
        for (FCombatCheckpointUnit& Unit : Expected.Units)
        {
            if (Unit.Team == ETeam::Player) Unit.PartyControlMode = Unit.OwnerAccountId == Fixture.Account(Ordinal) ? EPartyControlMode::Human : EPartyControlMode::ServerAI;
        }
        TestTrue(TEXT("Resume changes only Host identity and party control modes at the confirmed turn"), ManagedSameCheckpoint(Solo->GetCombatCheckpoint(), Expected));
        TStrongObjectPtr<URunSaveGame> Saved = Fixture.LoadPayload();
        if (!TestNotNull(TEXT("The resumed authority payload is readable"), Saved.Get())) return false;
        TestTrue(TEXT("Outer and nested identities commit the same Host epoch"), ManagedSameIdentity(Saved->Identity, Solo->GetRunIdentity()) && ManagedSameIdentity(Saved->CombatCheckpoint.Identity, Saved->Identity));
        TestTrue(TEXT("The persisted human roster contains only the selected caller"), Saved->Participation.HumanParticipants.Num() == 1 && Saved->Participation.HumanParticipants[0] == Fixture.Account(Ordinal));
        FCombatCheckpointData Next = Expected;
        ++Next.Revision;
        ++Next.CompletedTurnSerial;
        TestTrue(TEXT("Server restoration may commit a boundary before clearing pending state"), Solo->CommitCombatCheckpoint(Next, Fixture.Error));
        TestTrue(TEXT("An internal commit does not prematurely confirm gameplay entry"), Solo->IsManagedResumePending());
        TestTrue(TEXT("Successful actor restoration explicitly clears pending"), Solo->ConfirmManagedResumeStarted(Fixture.Error));
        TestFalse(TEXT("Gameplay entry is now confirmed"), Solo->IsManagedResumePending());
        const FRunAuthorityStamp Latest = Solo->GetManagedStamp();
        Solo->CloseManagedRun();

        URunStateSubsystem* ReturningOwner = Fixture.NewSession(1);
        const TArray<uint8> Before = Fixture.FileBytes();
        TestFalse(TEXT("An owner already converted to AI cannot restore Human control"), ReturningOwner->ResumeManagedRun(Latest, { Fixture.Account(1), Fixture.Account(Ordinal) }, Fixture.Error));
        TestTrue(TEXT("A rejected Human return acquires no lease and preserves the record"), !ReturningOwner->HasManagedLease() && Fixture.FileBytes() == Before);
        URunStateSubsystem* SameHost = Fixture.NewSession(Ordinal);
        TestTrue(TEXT("The current solo Host can resume again from a new menu session"), SameHost->ResumeManagedRun(Latest, { Fixture.Account(Ordinal) }, Fixture.Error));
        TestTrue(TEXT("Repeated solo resume retains permanent AI and advances the Host epoch"), SameHost->GetParticipation().HumanParticipants.Num() == 1 && SameHost->GetRunIdentity().HostEpoch == Latest.HostEpoch + 1);
        TestFalse(TEXT("The closed original Host cannot publish an old combat boundary"), Host->CommitCombatCheckpoint(OriginalCheckpoint, Fixture.Error));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManagedRunOrderedResumeTest, "ProjectA.Run.Managed.OrderedResumeAndProgression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FManagedRunOrderedResumeTest::RunTest(const FString& Parameters)
{
    FManagedFixture Fixture(3);
    URunStateSubsystem* Host = Fixture.NewSession(1);
    if (!Host || !TestTrue(TEXT("Three original participants create a managed Run"), Host->CreateManagedRun(Fixture.Party, Fixture.Identity, Fixture.Error))) return false;
    const FRunAuthorityStamp Initial = Host->GetManagedStamp();
    URunStateSubsystem* Third = Fixture.NewSession(3);
    const TArray<FRunAccountId> Humans{ Fixture.Account(3), Fixture.Account(2) };
    TestFalse(TEXT("Member three cannot bypass the lower numbered resuming member two"), Third->ResumeManagedRun(Initial, Humans, Fixture.Error));
    URunStateSubsystem* Second = Fixture.NewSession(2);
    const TArray<uint8> BeforeBusy = Fixture.FileBytes();
    TestFalse(TEXT("A second execution cannot acquire the still-running Host's lease"), Second->ResumeManagedRun(Initial, Humans, Fixture.Error));
    TestTrue(TEXT("Busy acquisition preserves canonical bytes and empty caller state"), Fixture.FileBytes() == BeforeBusy && !Second->IsManagedRun());
    Host->CloseManagedRun();
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Failed resume publication leaves the old Host record intact"), Second->ResumeManagedRun(Initial, Humans, Fixture.Error));
    TestTrue(TEXT("Failed resume leaves no lease, pending state or partial Human roster"), !Second->HasManagedLease() && !Second->IsManagedResumePending() && Second->GetParticipation().HumanParticipants.IsEmpty() && Fixture.FileBytes() == BeforeBusy);
    if (!TestTrue(TEXT("Original three can resume as humans two and three with Host two"), Second->ResumeManagedRun(Initial, Humans, Fixture.Error))) return false;
    TestFalse(TEXT("Pending menu restoration blocks starting another node"), Second->BeginEncounter(TEXT("Combat_01")));
    TestTrue(TEXT("Map restoration confirms gameplay entry"), Second->ConfirmManagedResumeStarted(Fixture.Error));
    TestTrue(TEXT("The confirmed resumed Host can prepare an encounter"), Second->BeginEncounter(TEXT("Combat_01")));
    const FRunAuthorityStamp BeforeAbort = Second->GetManagedStamp();
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Failed preparation rollback save remains in its previous phase"), Second->AbortEncounter());
    TestTrue(TEXT("Failed abort retains phase, node and authority revision"), Second->GetPhase() == ERunPhase::Preparing && Second->GetCurrentNodeId() == TEXT("Combat_01") && Second->GetManagedStamp() == BeforeAbort);
    TestTrue(TEXT("Preparation rollback can retry"), Second->AbortEncounter());
    FCombatCheckpointData Checkpoint;
    if (!TestTrue(TEXT("Resumed participation applies to the next new combat boundary"), Fixture.StartAndCommit(Second, Checkpoint))) return false;
    for (const FCombatCheckpointUnit& Unit : Checkpoint.Units)
    {
        if (Unit.Team == ETeam::Player)
        {
            TestTrue(TEXT("The absent first owner remains AI without changing ownership"), Unit.PartyControlMode == (Unit.OwnerAccountId == Fixture.Account(1) ? EPartyControlMode::ServerAI : EPartyControlMode::Human));
            Second->UpdatePartyMemberHP(Unit.PartySlot, Unit.HP);
        }
    }
    const FRunAuthorityStamp BeforeResult = Second->GetManagedStamp();
    const TArray<uint8> BeforeResultBytes = Fixture.FileBytes();
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Failed terminal save does not publish a result"), Second->CompleteEncounter(ECombatResult::Victory));
    TestTrue(TEXT("Failed result keeps combat, checkpoint and revision unchanged"), Second->GetPhase() == ERunPhase::Combat && Second->GetLastResult() == ECombatResult::None && Second->GetCompletedNodes().IsEmpty() && ManagedSameCheckpoint(Second->GetCombatCheckpoint(), Checkpoint) && Second->GetManagedStamp() == BeforeResult && Fixture.FileBytes() == BeforeResultBytes);
    if (!TestTrue(TEXT("The same terminal result retries through its lease"), Second->CompleteEncounter(ECombatResult::Victory))) return false;
    TStrongObjectPtr<URunSaveGame> ResultSave = Fixture.LoadPayload();
    if (!TestNotNull(TEXT("Result payload remains available"), ResultSave.Get())) return false;
    TestTrue(TEXT("Result v4 persists participation outside combat"), ResultSave->Version == 4 && ResultSave->Phase == ERunPhase::Result && ResultSave->CombatCheckpoint.Revision == 0 && ResultSave->Participation.HumanParticipants == Humans);
    const FRunAuthorityStamp ResultStamp = Second->GetManagedStamp();
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Failed Continue retains the result for retry"), Second->ContinueRun());
    TestTrue(TEXT("Failed Continue preserves the current encounter and authority stamp"), Second->GetPhase() == ERunPhase::Result && Second->GetCurrentEncounterId() == TEXT("DefaultEncounter") && Second->GetManagedStamp() == ResultStamp);
    TestTrue(TEXT("Continue succeeds without losing permanent AI state"), Second->ContinueRun());
    TestTrue(TEXT("The following node preserves the exact human roster"), Second->CanStartNode(TEXT("Combat_02")) && Second->GetParticipation().HumanParticipants == Humans);
    TestTrue(TEXT("The following encounter starts normally"), Second->BeginEncounter(TEXT("Combat_02")) && Second->MarkCombatStarted());
    FCombatCheckpointData Next;
    if (!Fixture.MakeCheckpoint(Second, Next)) return false;
    TestTrue(TEXT("New encounter state commits as v4 with unchanged original ownership"), Second->CommitCombatCheckpoint(Next, Fixture.Error));
    TestTrue(TEXT("Following combat keeps the resumed Host and original join numbers"), ManagedSameIdentity(Second->GetRunIdentity(), Checkpoint.Identity));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManagedRunCompatibilityTest, "ProjectA.Run.Managed.RejectionAndCompatibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FManagedRunCompatibilityTest::RunTest(const FString& Parameters)
{
    FManagedFixture Fixture;
    URunStateSubsystem* Host = Fixture.NewSession(1);
    if (!TestNotNull(TEXT("The local caller is configured"), Host)) return false;
    FRunIdentityData AccountIdentity = Fixture.Identity;
    AccountIdentity.Origin = ERunIdentityOrigin::AccountProvider;
    TestFalse(TEXT("Managed storage never promotes an account-provider identity"), Host->CreateManagedRun(Fixture.Party, AccountIdentity, Fixture.Error));
    FRunIdentityData OldIdentity = Fixture.Identity;
    OldIdentity.SchemaVersion = 1;
    for (FRunParticipantData& Participant : OldIdentity.OriginalParticipants) Participant.JoinOrdinal = 0;
    TestFalse(TEXT("Old identity order is never inferred during managed creation"), Host->CreateManagedRun(Fixture.Party, OldIdentity, Fixture.Error));
    if (!TestTrue(TEXT("A valid managed Run creates"), Host->CreateManagedRun(Fixture.Party, Fixture.Identity, Fixture.Error))) return false;
    FManagedRunPreview OldPreview;
    TestTrue(TEXT("A preview captures the current CAS stamp"), Host->ReadManagedRun(Fixture.Identity.RunId, OldPreview, Fixture.Error));
    TestTrue(TEXT("A later legitimate save advances the authority revision"), Host->SaveCheckpoint(Fixture.Error));
    const FRunAuthorityStamp Latest = Host->GetManagedStamp();
    Host->CloseManagedRun();
    URunStateSubsystem* Second = Fixture.NewSession(2);
    const TArray<uint8> Before = Fixture.FileBytes();
    TestFalse(TEXT("A stale menu preview cannot overwrite a newer record"), Second->ResumeManagedRun(OldPreview.Stamp, { Fixture.Account(2) }, Fixture.Error));
    TestFalse(TEXT("Duplicate humans are rejected"), Second->ResumeManagedRun(Latest, { Fixture.Account(2), Fixture.Account(2) }, Fixture.Error));
    FRunAccountId Outsider = Fixture.Account(2);
    Outsider.Subject = TEXT("ReplacementOwner");
    TestFalse(TEXT("Replacement participants cannot enter an original Run"), Second->ResumeManagedRun(Latest, { Fixture.Account(2), Outsider }, Fixture.Error));
    TestFalse(TEXT("Empty Human selection cannot resume"), Second->ResumeManagedRun(Latest, {}, Fixture.Error));
    TestTrue(TEXT("Rejected resume requests leave memory, lease and disk unchanged"), !Second->IsManagedRun() && !Second->HasManagedLease() && Second->GetPhase() == ERunPhase::None && Fixture.FileBytes() == Before);

    TStrongObjectPtr<URunSaveGame> Managed = Fixture.LoadPayload();
    if (!TestNotNull(TEXT("The v4 payload is available for ordinary-load bypass checks"), Managed.Get())) return false;
    if (!TestTrue(TEXT("An isolated ordinary slot receives a valid v4 payload"), UGameplayStatics::SaveGameToSlot(Managed.Get(), Fixture.OrdinarySlot, 0))) return false;
    Second->EnableCheckpointSaving(Fixture.OrdinarySlot);
    TestFalse(TEXT("Generic Continue does not open managed v4 without a lease"), Second->CanContinueSavedRun(Fixture.Error));
    TestFalse(TEXT("Standalone Continue does not open managed v4 without a lease"), Second->CanContinueStandaloneSavedRun(Fixture.Error));
    TestFalse(TEXT("Generic load cannot bypass managed acquisition"), Second->LoadCheckpoint(Fixture.Error));
    TestFalse(TEXT("Standalone load cannot bypass managed acquisition"), Second->LoadStandaloneCheckpoint(Fixture.Error));
    TestTrue(TEXT("Bypass attempts leave current memory uninitialized"), Second->GetPhase() == ERunPhase::None && !Second->IsManagedRun());

    Managed->Version = 2;
    TestTrue(TEXT("A downgraded payload fixture writes"), UGameplayStatics::SaveGameToSlot(Managed.Get(), Fixture.OrdinarySlot, 0));
    TestFalse(TEXT("Older save versions cannot carry hidden managed participation"), Second->LoadCheckpoint(Fixture.Error));
    Managed->Participation = FRunParticipationData();
    TestTrue(TEXT("An ordinary v2 fixture without managed fields writes"), UGameplayStatics::SaveGameToSlot(Managed.Get(), Fixture.OrdinarySlot, 0));
    TestTrue(TEXT("Ordinary v2 identity/party saves remain loadable without managed conversion"), Second->LoadCheckpoint(Fixture.Error));
    TestTrue(TEXT("Ordinary loading preserves its identity and has no managed lease"), ManagedSameIdentity(Second->GetRunIdentity(), Managed->Identity) && !Second->IsManagedRun() && !Second->HasManagedLease());
    return true;
}

#endif
