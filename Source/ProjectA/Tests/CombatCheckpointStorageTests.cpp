#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Checkpoint/CombatCheckpointLibrary.h"
#include "DataAsset/OpponentSnapshotCatalogDataAsset.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Game/Snapshot/PartySnapshotSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    struct FCheckpointStorageFixture
    {
        TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
        TStrongObjectPtr<URunStateSubsystem> Run{NewObject<URunStateSubsystem>(Instance.Get())};
        FString Slot = TEXT("T14_CombatCheckpoint_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        FProfessionDefinition Profession;

        FCheckpointStorageFixture()
        {
            Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_VerticalSliceParty.DA_VerticalSliceParty"));
            Run->EnableCheckpointSaving(Slot);
        }

        ~FCheckpointStorageFixture()
        {
            UGameplayStatics::DeleteGameInSlot(Slot, 0);
        }

        bool Initialize(FText& OutError)
        {
            FRunPartyMember Member;
            Member.SlotIndex = 0;
            Member.CharacterName = FText::FromString(TEXT("Checkpoint Hunter"));
            Member.ClassId = TEXT("Hunter");
            Member.bCreated = true;
            return Run->PartyDefinition && Run->PartyDefinition->ResolveProfession(Member.ClassId, Profession) && Run->InitializeRun({ Member }, OutError) && Run->GetSaveError().IsEmpty() && Run->BeginEncounter(TEXT("Combat_01")) && Run->MarkCombatStarted();
        }

        FCombatCheckpointData MakeCheckpoint() const
        {
            FCombatCheckpointData Checkpoint;
            Checkpoint.AttemptId = FGuid::NewGuid();
            Checkpoint.Revision = 1;
            Checkpoint.Identity = Run->GetRunIdentity();
            Checkpoint.NodeId = Run->GetCurrentNodeId();
            Checkpoint.EncounterId = Run->GetCurrentEncounterId();
            Checkpoint.CompletedTurnSerial = 7;
            Checkpoint.NextTurnIndex = 1;
            FCombatCheckpointUnit& Player = Checkpoint.Units.AddDefaulted_GetRef();
            Player.UnitId = FGuid::NewGuid();
            Player.CharacterId = Run->GetPartyMembers()[0].CharacterId;
            Player.OwnerAccountId = Run->GetPartyMembers()[0].OwnerAccountId;
            Player.PartySlot = 0;
            Player.UnitClass = FSoftObjectPath(Profession.CombatClass.Get());
            Player.CharacterName = Run->GetPartyMembers()[0].CharacterName;
            Player.HP = 73.0f;
            Player.MaxHP = FMath::Max(100.0f, Profession.MaxHP);
            Player.MaxAP = Profession.ActionPoints;
            Player.AP = 0;
            Player.MaxSubAP = Profession.SubActionPoints;
            Player.SubAP = 0;
            Player.MoveRange = 2;
            Player.HealingItemCount = 1;
            Player.HealingItemAmount = 35.0f;
            Player.GridCoord = FIntPoint(2, 1);
            Player.Transform = FTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(-400.0f, 200.0f, 98.15f));
            for (USkillDefinitionDataAsset* Skill : Profession.StartingSkills)
            {
                Player.Skills.Add(FSoftObjectPath(Skill));
            }
            Player.DefaultAttackAbility = FSoftObjectPath(Profession.StartingSkills[0]->AbilityClass.Get());
            FCombatCheckpointUnit Enemy = Player;
            Enemy.UnitId = FGuid::NewGuid();
            Enemy.CharacterId.Invalidate();
            Enemy.OwnerAccountId = FRunAccountId();
            Enemy.PartySlot = INDEX_NONE;
            Enemy.Team = ETeam::Enemy;
            Enemy.UnitClass = FSoftObjectPath(AEnemyUnit::StaticClass());
            Enemy.CharacterName = FText::FromString(TEXT("Checkpoint Enemy"));
            Enemy.HP = 42.0f;
            Enemy.GridCoord = FIntPoint(1, 2);
            Enemy.Transform = FTransform(FRotator(0.0f, -90.0f, 0.0f), FVector(-200.0f, 400.0f, 98.15f));
            Enemy.HealingItemCount = 0;
            Checkpoint.Units.Add(Enemy);
            return Checkpoint;
        }

        TArray<uint8> ReadBytes() const
        {
            TArray<uint8> Bytes;
            FFileHelper::LoadFileToArray(Bytes, *(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".sav"))));
            return Bytes;
        }
    };

    bool SameCheckpoint(const FCombatCheckpointData& Left, const FCombatCheckpointData& Right)
    {
        return FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointValueTest, "ProjectA.Checkpoint.ValueValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointValueTest::RunTest(const FString& Parameters)
{
    FCheckpointStorageFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("Real profession fixture initializes"), Fixture.Initialize(Error)))
    {
        return false;
    }
    const FCombatCheckpointData Valid = Fixture.MakeCheckpoint();
    TestTrue(TEXT("Actor-free boundary validates against original ownership"), UCombatCheckpointLibrary::Validate(Valid, Fixture.Run->GetPartyMembers(), Error));
    FCombatCheckpointData WithDeadUnit = Valid;
    FCombatCheckpointUnit Dead = Valid.Units[1];
    Dead.UnitId = FGuid::NewGuid();
    Dead.HP = 0.0f;
    Dead.bDead = true;
    Dead.bHasTile = false;
    WithDeadUnit.Units.Add(Dead);
    TestTrue(TEXT("A dead unit remains in turn order without occupying a tile"), UCombatCheckpointLibrary::Validate(WithDeadUnit, Fixture.Run->GetPartyMembers(), Error));
    FCombatCheckpointData Invalid = Valid;
    const auto Reject = [this, &Fixture, &Valid, &Invalid, &Error](const TCHAR* Label)
    {
        TestFalse(Label, UCombatCheckpointLibrary::Validate(Invalid, Fixture.Run->GetPartyMembers(), Error));
        TestFalse(FString(Label) + TEXT(" reports why"), Error.IsEmpty());
        Invalid = Valid;
    };
    Invalid.SchemaVersion = 3;
    Reject(TEXT("Unknown checkpoint schema is rejected"));
    Invalid.ContentVersion = 2;
    Reject(TEXT("Unknown content version is rejected"));
    Invalid.Revision = 0;
    Reject(TEXT("Uncommitted data is rejected"));
    Invalid.Identity.HostEpoch = 0;
    Reject(TEXT("Missing Host epoch is rejected"));
    Invalid.Units[0].OwnerAccountId.Subject += TEXT("_replacement");
    Reject(TEXT("A replacement owner is rejected"));
    Invalid.Units[0].CharacterId = FGuid::NewGuid();
    Reject(TEXT("A replacement character is rejected"));
    Invalid.Units[1].UnitId = Invalid.Units[0].UnitId;
    Reject(TEXT("Duplicate persistent unit IDs are rejected"));
    Invalid.Units[1].GridCoord = Invalid.Units[0].GridCoord;
    Reject(TEXT("Conflicting tile occupancy is rejected"));
    Invalid.Units[0].UnitClass = FSoftObjectPath(AEnemyUnit::StaticClass());
    Reject(TEXT("Wrong team class is rejected"));
    Invalid.Units[0].UnitClass = FSoftObjectPath(TEXT("/Temp/Injected.Injected_C"));
    Reject(TEXT("Non-content object paths are rejected before load"));
    Invalid.Units[0].AP = Invalid.Units[0].MaxAP + 1;
    Reject(TEXT("AP above its maximum is rejected"));
    Invalid.Units[0].HP = -1.0f;
    Reject(TEXT("Uninitialized or negative battle HP is rejected"));
    Invalid.Units[0].bDead = true;
    Reject(TEXT("Death must agree with zero HP"));
    const FSoftObjectPath DuplicateSkill = Invalid.Units[0].Skills[0];
    Invalid.Units[0].Skills.Add(DuplicateSkill);
    Reject(TEXT("Duplicate equipped skills are rejected"));
    Invalid.Units[0].DefaultAttackAbility.Reset();
    Reject(TEXT("Missing default ability is rejected"));
    Invalid.Units[1].HP = 0.0f;
    Invalid.Units[1].bDead = true;
    Invalid.Units[1].bHasTile = false;
    Reject(TEXT("Terminal combat and a dead next turn are rejected"));
    Invalid.OpponentSnapshot.SnapshotId = TEXT("UnmarkedSnapshot");
    Reject(TEXT("Unmarked opponent metadata is rejected"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointRoundTripTest, "ProjectA.Checkpoint.RoundTripAndHost", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointRoundTripTest::RunTest(const FString& Parameters)
{
    FCheckpointStorageFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("Fixture initializes"), Fixture.Initialize(Error)))
    {
        return false;
    }
    FCombatCheckpointData Checkpoint = Fixture.MakeCheckpoint();
    TStrongObjectPtr<UOpponentSnapshotCatalogDataAsset> OpponentCatalog(LoadObject<UOpponentSnapshotCatalogDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_OpponentSnapshotCatalog.DA_OpponentSnapshotCatalog")));
    if (!TestTrue(TEXT("Frozen opponent uses real catalog content"), OpponentCatalog.IsValid() && !OpponentCatalog->EnemyClasses.IsEmpty() && !OpponentCatalog->Skills.IsEmpty()))
    {
        return false;
    }
    const auto ClassEntry = OpponentCatalog->EnemyClasses.CreateConstIterator();
    const auto SkillEntry = OpponentCatalog->Skills.CreateConstIterator();
    USkillDefinitionDataAsset* Skill = SkillEntry.Value();
    Checkpoint.bHasOpponentSnapshot = true;
    Checkpoint.OpponentCatalog = FSoftObjectPath(OpponentCatalog.Get());
    Checkpoint.OpponentSnapshot.SnapshotId = TEXT("FrozenOpponent");
    Checkpoint.OpponentSnapshot.ContentVersion = OpponentCatalog->ContentVersion;
    FPartySnapshotMember& Opponent = Checkpoint.OpponentSnapshot.Members.AddDefaulted_GetRef();
    Opponent.MemberId = TEXT("Enemy_01");
    Opponent.ClassId = ClassEntry.Key();
    Opponent.CharacterName = TEXT("Frozen Enemy");
    Opponent.SkillIds.Add(SkillEntry.Key());
    FCombatCheckpointUnit& Enemy = Checkpoint.Units[1];
    Enemy.UnitClass = FSoftObjectPath(ClassEntry.Value().Get());
    Enemy.CharacterName = FText::FromString(Opponent.CharacterName);
    Enemy.MaxHP = Opponent.Stats.MaxHP;
    Enemy.MaxAP = Opponent.Stats.MaxActionPoints;
    Enemy.MaxSubAP = Opponent.Stats.MaxSubActionPoints;
    Enemy.MoveRange = Opponent.Stats.MoveRange;
    Enemy.Skills = { FSoftObjectPath(Skill) };
    Enemy.DefaultAttackAbility = FSoftObjectPath(Skill->AbilityClass.Get());
    FCombatCheckpointData MismatchedOpponent = Checkpoint;
    MismatchedOpponent.Units[1].MaxHP += 1.0f;
    TestFalse(TEXT("Frozen opponent maximum stats cannot diverge from the Snapshot"), UCombatCheckpointLibrary::Validate(MismatchedOpponent, Fixture.Run->GetPartyMembers(), Error));
    TestFalse(TEXT("Frozen build mismatch explains why"), Error.IsEmpty());
    MismatchedOpponent = Checkpoint;
    MismatchedOpponent.Units[1].MoveRange += 1;
    TestFalse(TEXT("Frozen opponent move range cannot diverge"), UCombatCheckpointLibrary::Validate(MismatchedOpponent, Fixture.Run->GetPartyMembers(), Error));
    MismatchedOpponent = Checkpoint;
    const FPartySnapshotMember ExtraOpponent = MismatchedOpponent.OpponentSnapshot.Members[0];
    MismatchedOpponent.OpponentSnapshot.Members.Add(ExtraOpponent);
    TestFalse(TEXT("Frozen opponent count must match saved enemy entries"), UCombatCheckpointLibrary::Validate(MismatchedOpponent, Fixture.Run->GetPartyMembers(), Error));
    TestFalse(TEXT("Frozen opponent count mismatch explains why"), Error.IsEmpty());
    int32 Events = 0;
    Fixture.Run->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
    if (!TestTrue(TEXT("Complete v3 boundary atomically commits"), Fixture.Run->CommitCombatCheckpoint(Checkpoint, Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    TestTrue(TEXT("Committed checkpoint retains every value"), SameCheckpoint(Fixture.Run->GetCombatCheckpoint(), Checkpoint));
    TestEqual(TEXT("Boundary persistence does not publish a partial next turn"), Events, 0);
    TestEqual(TEXT("Persistence does not rewrite encounter-local runtime party HP"), Fixture.Run->GetPartyMembers()[0].CurrentHP, -1.0f);
    TestTrue(TEXT("CanContinue recognizes v3 combat without loading it into memory"), Fixture.Run->CanContinueSavedRun(Error));
    TestEqual(TEXT("Continue availability is read-only"), Events, 0);
    TestTrue(TEXT("Existing Host identity is accepted"), Fixture.Run->ValidateCheckpointHost(Checkpoint.Identity.HostAccountId, Error));
    FRunAccountId OtherHost = Checkpoint.Identity.HostAccountId;
    OtherHost.Subject += TEXT("_another");
    TestFalse(TEXT("Host succession is not inferred by restoration"), Fixture.Run->ValidateCheckpointHost(OtherHost, Error));
    TStrongObjectPtr<URunSaveGame> Disk(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
    if (!TestTrue(TEXT("Native SaveGame reader understands the committed format"), Disk.IsValid()))
    {
        return false;
    }
    TestEqual(TEXT("Combat stores version three"), Disk->Version, 3);
    TestEqual(TEXT("Persisted party HP comes from the actual boundary"), Disk->Party[0].CurrentHP, Checkpoint.Units[0].HP);
    TestTrue(TEXT("Native serialization preserves frozen Snapshot and all battle fields"), SameCheckpoint(Disk->CombatCheckpoint, Checkpoint));
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    TestTrue(TEXT("A fresh subsystem restores the battle boundary"), Restored->LoadCheckpoint(Error));
    TestTrue(TEXT("Restoration enters combat"), Restored->GetPhase() == ERunPhase::Combat);
    TestTrue(TEXT("Restoration preserves all checkpoint fields"), SameCheckpoint(Restored->GetCombatCheckpoint(), Checkpoint));
    TestTrue(TEXT("Restoration preserves complete original identity"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Restored->GetRunIdentity(), &Checkpoint.Identity, 0));
    TestTrue(TEXT("Restored Host remains the original Host"), Restored->ValidateCheckpointHost(Checkpoint.Identity.HostAccountId, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointAtomicTest, "ProjectA.Checkpoint.AtomicWriteAndRevision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointAtomicTest::RunTest(const FString& Parameters)
{
    FCheckpointStorageFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("Fixture initializes"), Fixture.Initialize(Error)))
    {
        return false;
    }
    FCombatCheckpointData First = Fixture.MakeCheckpoint();
    if (!TestTrue(TEXT("First boundary commits"), Fixture.Run->CommitCombatCheckpoint(First, Error)))
    {
        return false;
    }
    const TArray<uint8> OriginalBytes = Fixture.ReadBytes();
    TestFalse(TEXT("Committed file has data"), OriginalBytes.IsEmpty());
    TestFalse(TEXT("Abort cannot replace an already committed battle with map progress"), Fixture.Run->AbortEncounter());
    TestTrue(TEXT("Rejected Abort preserves combat phase"), Fixture.Run->GetPhase() == ERunPhase::Combat);
    TestTrue(TEXT("Rejected Abort preserves the complete battle checkpoint"), SameCheckpoint(Fixture.Run->GetCombatCheckpoint(), First));
    TestTrue(TEXT("Rejected Abort preserves the committed file"), Fixture.ReadBytes() == OriginalBytes);
    FCombatCheckpointData Next = First;
    ++Next.Revision;
    ++Next.CompletedTurnSerial;
    Next.Units[0].HP = 41.0f;
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Injected pre-replacement failure is reported"), Fixture.Run->CommitCombatCheckpoint(Next, Error));
    TestFalse(TEXT("Failure remains visible for retry"), Fixture.Run->GetSaveError().IsEmpty());
    TestTrue(TEXT("Failure preserves every previous file byte"), Fixture.ReadBytes() == OriginalBytes);
    TestTrue(TEXT("Failure preserves the committed in-memory revision"), SameCheckpoint(Fixture.Run->GetCombatCheckpoint(), First));
    TestTrue(TEXT("Same pending boundary can retry successfully"), Fixture.Run->CommitCombatCheckpoint(Next, Error));
    TestTrue(TEXT("Successful retry clears the storage error"), Fixture.Run->GetSaveError().IsEmpty());
    const TArray<uint8> NextBytes = Fixture.ReadBytes();
    TestFalse(TEXT("Duplicate revision is rejected"), Fixture.Run->CommitCombatCheckpoint(Next, Error));
    FCombatCheckpointData Invalid = Next;
    Invalid.Revision += 2;
    TestFalse(TEXT("Skipped revision is rejected"), Fixture.Run->CommitCombatCheckpoint(Invalid, Error));
    Invalid = Next;
    ++Invalid.Revision;
    Invalid.CompletedTurnSerial = Next.CompletedTurnSerial;
    TestFalse(TEXT("Same turn boundary cannot be committed again under a new revision"), Fixture.Run->CommitCombatCheckpoint(Invalid, Error));
    ++Invalid.CompletedTurnSerial;
    ++Invalid.Identity.HostEpoch;
    TestFalse(TEXT("Host epoch cannot change during commit"), Fixture.Run->CommitCombatCheckpoint(Invalid, Error));
    Invalid = Next;
    ++Invalid.Revision;
    ++Invalid.CompletedTurnSerial;
    Invalid.Units[0].HP = Invalid.Units[0].MaxHP + 1.0f;
    TestFalse(TEXT("Invalid values cannot overwrite a valid file"), Fixture.Run->CommitCombatCheckpoint(Invalid, Error));
    TestFalse(TEXT("Ordinary save cannot bypass explicit combat commit"), Fixture.Run->SaveCheckpoint(Error));
    TestTrue(TEXT("All rejected writes preserve the last committed file"), Fixture.ReadBytes() == NextBytes);
    TestTrue(TEXT("All rejected writes preserve the last committed data"), SameCheckpoint(Fixture.Run->GetCombatCheckpoint(), Next));
    Invalid = Next;
    Invalid.AttemptId = FGuid::NewGuid();
    TestFalse(TEXT("A new attempt cannot inherit another attempt's revision"), Fixture.Run->CommitCombatCheckpoint(Invalid, Error));
    Invalid.Revision = 1;
    TestFalse(TEXT("An active committed combat cannot be replaced with a new attempt"), Fixture.Run->CommitCombatCheckpoint(Invalid, Error));
    for (const FString& Slot : { FString(), FString(TEXT("../Outside")), FString(TEXT("C:\\Outside")), FString(TEXT("Account/Subject")), FString(TEXT("name.sav")), FString(TEXT("CON")), FString(TEXT("nul")), FString(TEXT("COM1")), FString(TEXT("LPT9")), FString::ChrN(129, TEXT('a')) })
    {
        TestFalse(TEXT("Unsafe slot names are rejected before file access"), FRunCheckpointStorage::IsSafeSlotName(Slot));
    }
    TestTrue(TEXT("Existing GUID slots remain supported"), FRunCheckpointStorage::IsSafeSlotName(TEXT("T11_Test_00000000-0000-0000-0000-000000000000")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointCorruptLoadTest, "ProjectA.Checkpoint.CorruptLoadAndLegacy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointCorruptLoadTest::RunTest(const FString& Parameters)
{
    FCheckpointStorageFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("Fixture initializes"), Fixture.Initialize(Error)))
    {
        return false;
    }
    const FCombatCheckpointData Checkpoint = Fixture.MakeCheckpoint();
    if (!TestTrue(TEXT("Boundary commits"), Fixture.Run->CommitCombatCheckpoint(Checkpoint, Error)))
    {
        return false;
    }
    TStrongObjectPtr<URunSaveGame> Disk(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
    if (!TestTrue(TEXT("Fixture disk record is readable"), Disk.IsValid()))
    {
        return false;
    }
    int32 Events = 0;
    Fixture.Run->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
    const auto RejectLoad = [this, &Fixture, &Disk, &Checkpoint, &Error, &Events](const TCHAR* Label)
    {
        TestTrue(TEXT("Write deliberately damaged semantic fixture"), UGameplayStatics::SaveGameToSlot(Disk.Get(), Fixture.Slot, 0));
        TestFalse(Label, Fixture.Run->CanContinueSavedRun(Error));
        TestFalse(Label, Fixture.Run->LoadCheckpoint(Error));
        TestFalse(TEXT("Rejected load explains the error"), Error.IsEmpty());
        TestTrue(TEXT("Rejected load preserves the current battle"), SameCheckpoint(Fixture.Run->GetCombatCheckpoint(), Checkpoint));
        TestTrue(TEXT("Rejected load preserves combat phase"), Fixture.Run->GetPhase() == ERunPhase::Combat);
        TestTrue(TEXT("Rejected load preserves original Run identity"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Fixture.Run->GetRunIdentity(), &Checkpoint.Identity, 0));
        TestEqual(TEXT("Rejected load preserves unsaved runtime party HP"), Fixture.Run->GetPartyMembers()[0].CurrentHP, -1.0f);
        TestEqual(TEXT("Rejected load publishes no state event"), Events, 0);
    };
    Disk->Version = 2;
    RejectLoad(TEXT("Version two cannot contain a combat payload"));
    Disk->Version = 3;
    Disk->CombatCheckpoint = FCombatCheckpointData();
    RejectLoad(TEXT("Version three requires a complete combat payload"));
    Disk->CombatCheckpoint = Checkpoint;
    Disk->CombatCheckpoint.Identity.HostEpoch += 1;
    RejectLoad(TEXT("Save and checkpoint Host metadata must agree"));
    Disk->CombatCheckpoint = Checkpoint;
    Disk->Party[0].CurrentHP += 1.0f;
    RejectLoad(TEXT("Saved party HP must agree with checkpoint HP"));
    Disk->Party[0].CurrentHP = Checkpoint.Units[0].HP;
    Disk->CurrentNode = TEXT("Combat_02");
    RejectLoad(TEXT("Checkpoint progression must match the next unfinished node"));

    // A legacy save stays explicitly offline, and cannot smuggle a v3 payload through version one.
    // 기존 저장은 명시적인 오프라인 상태를 유지하며 v1에 v3 데이터를 숨길 수 없습니다.
    Disk->Version = 1;
    Disk->Identity = FRunIdentityData();
    Disk->Party[0].CharacterId.Invalidate();
    Disk->Party[0].OwnerAccountId = FRunAccountId();
    Disk->Phase = ERunPhase::Map;
    Disk->CurrentNode = NAME_None;
    Disk->CurrentEncounter = NAME_None;
    RejectLoad(TEXT("Version one cannot contain identified combat metadata"));
    Disk->CombatCheckpoint = FCombatCheckpointData();
    TestTrue(TEXT("Write metadata-free legacy fixture"), UGameplayStatics::SaveGameToSlot(Disk.Get(), Fixture.Slot, 0));
    TestTrue(TEXT("Version one remains loadable outside combat"), Fixture.Run->LoadCheckpoint(Error));
    TestTrue(TEXT("Legacy load clears the prior combat record"), !Fixture.Run->HasCombatCheckpoint());
    TestTrue(TEXT("Legacy load remains offline"), Fixture.Run->GetRunIdentity().Origin == ERunIdentityOrigin::LegacyOffline);
    TestTrue(TEXT("Legacy save remains writable through the atomic adapter"), Fixture.Run->SaveCheckpoint(Error));
    Disk.Reset(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
    TestTrue(TEXT("Legacy resave retains version one"), Disk.IsValid() && Disk->Version == 1);
    TStrongObjectPtr<UPartySnapshotSaveGame> WrongSave(NewObject<UPartySnapshotSaveGame>());
    TestTrue(TEXT("Write a different native SaveGame class"), UGameplayStatics::SaveGameToSlot(WrongSave.Get(), Fixture.Slot, 0));
    TestFalse(TEXT("Wrong SaveGame type cannot load as a Run"), Fixture.Run->LoadCheckpoint(Error));
    TestFalse(TEXT("Wrong SaveGame type reports an error"), Error.IsEmpty());
    TestEqual(TEXT("Only the valid legacy load emitted an event"), Events, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointTerminalTest, "ProjectA.Checkpoint.TerminalCommitRollback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointTerminalTest::RunTest(const FString& Parameters)
{
    FCheckpointStorageFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("Fixture initializes"), Fixture.Initialize(Error)))
    {
        return false;
    }
    const FCombatCheckpointData Checkpoint = Fixture.MakeCheckpoint();
    if (!TestTrue(TEXT("Boundary commits"), Fixture.Run->CommitCombatCheckpoint(Checkpoint, Error)))
    {
        return false;
    }
    const TArray<uint8> CombatBytes = Fixture.ReadBytes();
    Fixture.Run->UpdatePartyMemberHP(0, 61.0f);
    int32 Events = 0;
    Fixture.Run->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Failed terminal write does not complete the encounter"), Fixture.Run->CompleteEncounter(ECombatResult::Victory));
    TestTrue(TEXT("Failed terminal write retains combat phase"), Fixture.Run->GetPhase() == ERunPhase::Combat);
    TestTrue(TEXT("Failed terminal write retains no published result"), Fixture.Run->GetLastResult() == ECombatResult::None);
    TestTrue(TEXT("Failed terminal write rolls back node completion"), Fixture.Run->GetCompletedNodes().IsEmpty());
    TestTrue(TEXT("Failed terminal write preserves the last committed checkpoint"), SameCheckpoint(Fixture.Run->GetCombatCheckpoint(), Checkpoint));
    TestTrue(TEXT("Failed terminal write preserves the combat file"), Fixture.ReadBytes() == CombatBytes);
    TestFalse(TEXT("Terminal storage error is retained"), Fixture.Run->GetSaveError().IsEmpty());
    TestEqual(TEXT("Failed result publishes no event"), Events, 0);
    TestTrue(TEXT("The same terminal result retries successfully"), Fixture.Run->CompleteEncounter(ECombatResult::Victory));
    TestFalse(TEXT("Confirmed terminal result clears combat recovery metadata"), Fixture.Run->HasCombatCheckpoint());
    TestEqual(TEXT("Successful result publishes exactly once"), Events, 1);
    TestFalse(TEXT("Duplicate terminal callback is rejected"), Fixture.Run->CompleteEncounter(ECombatResult::Victory));
    TStrongObjectPtr<URunSaveGame> Disk(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
    if (!TestTrue(TEXT("Terminal result file is readable"), Disk.IsValid()))
    {
        return false;
    }
    TestEqual(TEXT("Terminal result returns to version two"), Disk->Version, 2);
    TestTrue(TEXT("Terminal file contains no resumable battle"), SameCheckpoint(Disk->CombatCheckpoint, FCombatCheckpointData()));
    TestEqual(TEXT("Terminal file contains final actual HP"), Disk->Party[0].CurrentHP, 61.0f);
    const TArray<uint8> ResultBytes = Fixture.ReadBytes();
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Failed Continue write keeps the result screen retryable"), Fixture.Run->ContinueRun());
    TestTrue(TEXT("Failed Continue restores result phase"), Fixture.Run->GetPhase() == ERunPhase::Result);
    TestEqual(TEXT("Failed Continue restores the encounter ID"), Fixture.Run->GetCurrentEncounterId(), FName(TEXT("DefaultEncounter")));
    TestTrue(TEXT("Failed Continue keeps the result file"), Fixture.ReadBytes() == ResultBytes);
    TestEqual(TEXT("Failed Continue publishes no event"), Events, 1);
    TestTrue(TEXT("Continue retries successfully"), Fixture.Run->ContinueRun());
    TestTrue(TEXT("Committed Continue exposes the next node"), Fixture.Run->CanStartNode(TEXT("Combat_02")));
    TestEqual(TEXT("Committed Continue publishes once"), Events, 2);
    TestTrue(TEXT("Next encounter begins after terminal commit"), Fixture.Run->BeginEncounter(TEXT("Combat_02")) && Fixture.Run->MarkCombatStarted());
    TestTrue(TEXT("A new encounter can begin a new attempt at revision one"), Fixture.Run->CommitCombatCheckpoint(Fixture.MakeCheckpoint(), Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointAIControlTest, "ProjectA.Checkpoint.AIControlCompatibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointAIControlTest::RunTest(const FString& Parameters)
{
    FCheckpointStorageFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("Real-content fixture initializes"), Fixture.Initialize(Error)))
    {
        return false;
    }
    const FCombatCheckpointData Human = Fixture.MakeCheckpoint();
    TestEqual(TEXT("Default checkpoint schema preserves existing empty-payload compatibility"), Human.SchemaVersion, 1);
    if (!TestTrue(TEXT("Schema one Human boundary commits without AI consent"), Fixture.Run->CommitCombatCheckpoint(Human, Error)))
    {
        return false;
    }
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    if (!TestTrue(TEXT("Schema one Human boundary remains loadable"), Restored->LoadCheckpoint(Error)))
    {
        return false;
    }
    TestTrue(TEXT("Schema one load preserves every checkpoint field"), SameCheckpoint(Restored->GetCombatCheckpoint(), Human));
    TestTrue(TEXT("Schema one load retains Human party control"), Restored->GetCombatCheckpoint().Units[0].PartyControlMode == EPartyControlMode::Human);

    FCombatCheckpointData ValidAI = Human;
    ValidAI.Identity.OriginalParticipants[0].AIConsent = ERunAIConsent::Granted;
    ValidAI.Identity.OriginalParticipants[0].ConsentPolicyVersion = 1;
    ValidAI.Units[0].PartyControlMode = EPartyControlMode::ServerAI;
    TestFalse(TEXT("Schema one cannot smuggle AI mode even with valid consent"), UCombatCheckpointLibrary::Validate(ValidAI, Fixture.Run->GetPartyMembers(), Error));
    ValidAI.SchemaVersion = 2;
    TestTrue(TEXT("Schema two supports AI with the original owner's policy-one consent"), UCombatCheckpointLibrary::Validate(ValidAI, Fixture.Run->GetPartyMembers(), Error));
    FCombatCheckpointData Invalid = ValidAI;
    const auto Reject = [this, &Fixture, &ValidAI, &Invalid, &Error](const TCHAR* Label)
    {
        TestFalse(Label, UCombatCheckpointLibrary::Validate(Invalid, Fixture.Run->GetPartyMembers(), Error));
        TestFalse(FString(Label) + TEXT(" reports why"), Error.IsEmpty());
        Invalid = ValidAI;
    };
    Invalid.Identity.OriginalParticipants[0].AIConsent = ERunAIConsent::Unknown;
    Invalid.Identity.OriginalParticipants[0].ConsentPolicyVersion = 0;
    Reject(TEXT("Unknown consent cannot authorize stored AI control"));
    Invalid.Identity.OriginalParticipants[0].AIConsent = ERunAIConsent::Declined;
    Reject(TEXT("Declined consent cannot authorize stored AI control"));
    Invalid.Identity.OriginalParticipants[0].ConsentPolicyVersion = 0;
    Reject(TEXT("Granted consent requires its supported policy version"));
    Invalid.Units[0].PartyControlMode = static_cast<EPartyControlMode>(255);
    Reject(TEXT("Unknown control mode is rejected"));
    Invalid.Units[1].PartyControlMode = EPartyControlMode::ServerAI;
    Reject(TEXT("Party AI mode cannot be assigned to an enemy entry"));

    // Consent belongs to a new Run's original identity; do not mutate consent on the running fixture.
    // 동의는 새 Run의 원래 식별 정보에 속하며 실행 중인 테스트 Run의 동의를 바꾸지 않습니다.
    FRunIdentityData ConsentingIdentity = ValidAI.Identity;
    ConsentingIdentity.RunId = FGuid::NewGuid();
    const TArray<FRunPartyMember> Members = Fixture.Run->GetPartyMembers();
    if (!TestTrue(TEXT("New Run records granted consent at initialization"), Fixture.Run->InitializeRunWithIdentity(Members, ConsentingIdentity, Error)) || !TestTrue(TEXT("Consenting Run enters combat"), Fixture.Run->BeginEncounter(TEXT("Combat_01")) && Fixture.Run->MarkCombatStarted()))
    {
        return false;
    }
    FCombatCheckpointData AI = Fixture.MakeCheckpoint();
    AI.SchemaVersion = 2;
    AI.Units[0].PartyControlMode = EPartyControlMode::ServerAI;
    if (!TestTrue(TEXT("Consented schema two AI boundary commits to disk"), Fixture.Run->CommitCombatCheckpoint(AI, Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    TStrongObjectPtr<URunSaveGame> Disk(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
    if (!TestTrue(TEXT("Native SaveGame reader loads the AI boundary"), Disk.IsValid()))
    {
        return false;
    }
    TestEqual(TEXT("The Run save remains version three for combat"), Disk->Version, 3);
    TestEqual(TEXT("Stored combat schema is two"), Disk->CombatCheckpoint.SchemaVersion, 2);
    TestTrue(TEXT("Native serialization retains every AI checkpoint field"), SameCheckpoint(Disk->CombatCheckpoint, AI));
    TestTrue(TEXT("AI checkpoint can continue"), Restored->CanContinueSavedRun(Error));
    if (!TestTrue(TEXT("Run load restores the persisted AI control field"), Restored->LoadCheckpoint(Error)))
    {
        return false;
    }
    const FCombatCheckpointUnit& SavedPlayer = Restored->GetCombatCheckpoint().Units[0];
    TestTrue(TEXT("Restored party unit remains ServerAI controlled"), SavedPlayer.PartyControlMode == EPartyControlMode::ServerAI);
    TestTrue(TEXT("Restored enemy keeps the neutral party-control marker"), Restored->GetCombatCheckpoint().Units[1].PartyControlMode == EPartyControlMode::Human);
    TestEqual(TEXT("AI restoration keeps the original character ID"), SavedPlayer.CharacterId, Members[0].CharacterId);
    TestTrue(TEXT("AI restoration keeps the original unit owner"), SavedPlayer.OwnerAccountId == Members[0].OwnerAccountId);
    TestTrue(TEXT("AI restoration keeps persistent party ownership"), Restored->GetPartyMembers()[0].OwnerAccountId == Members[0].OwnerAccountId);
    TestTrue(TEXT("AI restoration keeps complete Run, Host and consent metadata"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Restored->GetRunIdentity(), &ConsentingIdentity, 0));
    return true;
}

#endif
