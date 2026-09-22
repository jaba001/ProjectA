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
        TArray<TObjectPtr<USkillDefinitionDataAsset>> MemberSkills;

        FCheckpointStorageFixture()
        {
            Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
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
            Member.CharacterName = FText::FromString(TEXT("Checkpoint Archer"));
            Member.ClassId = TEXT("Archer");
            Member.bCreated = true;
            return Run->PartyDefinition && Run->PartyDefinition->ResolveProfession(Member.ClassId, Profession) && Run->InitializeRun({ Member }, OutError) && Run->PartyDefinition->ResolveMemberSkills(Run->GetPartyMembers()[0], MemberSkills, OutError) && !MemberSkills.IsEmpty() && Run->GetSaveError().IsEmpty() && Run->BeginEncounter(TEXT("Combat_01")) && Run->MarkCombatStarted();
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
            for (USkillDefinitionDataAsset* Skill : MemberSkills)
            {
                Player.Skills.Add(FSoftObjectPath(Skill));
            }
            Player.DefaultAttackAbility = FSoftObjectPath(MemberSkills[0]->AbilityClass.Get());
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

        FCombatCheckpointData MakeRoundCheckpoint() const
        {
            FCombatCheckpointData Checkpoint = MakeCheckpoint();
            Checkpoint.SchemaVersion = UCombatCheckpointLibrary::CurrentSchemaVersion;
            Checkpoint.RoundNumber = 2;
            Checkpoint.PlanRevision = 7;
            Checkpoint.CompletedTurnSerial = 0;
            Checkpoint.NextTurnIndex = 0;
            for (int32 Index = 0; Index < Checkpoint.Units.Num(); ++Index)
            {
                FCombatCheckpointUnit& Unit = Checkpoint.Units[Index];
                Unit.RoundUnitId = Index + 1;
                Unit.AP = Unit.MaxAP;
                Unit.SubAP = Unit.MaxSubAP;
                FCombatCheckpointRoundPlan& Plan = Checkpoint.RoundPlans.AddDefaulted_GetRef();
                Plan.UnitId = Unit.RoundUnitId;
                Plan.Command.UnitId = Unit.RoundUnitId;
                Plan.Command.TargetCoord = Unit.GridCoord;
                Plan.Command.DestinationCoord = Unit.GridCoord;
                Plan.MoveDestinationCoord = Unit.GridCoord;
                Plan.bReady = Index != 0;
                if (Unit.Team == ETeam::Player)
                {
                    FCombatRoundSkill Skill;
                    FText Error;
                    if (MemberSkills[0]->ResolveRoundSkill(Skill, Error)) Plan.Command.SkillId = Skill.SkillId;
                    Plan.Command.TargetUnitId = 2;
                    Plan.Command.TargetCoord = Checkpoint.Units[1].GridCoord;
                }
            }
            return Checkpoint;
        }

        // Install an old serialized payload directly; production code must never publish it again.
        // 이전 직렬화 본문을 직접 준비하며 제품 코드는 이 형식을 다시 저장하면 안 됩니다.
        bool WriteRetiredCombat(const FCombatCheckpointData& Checkpoint, FText& OutError) const
        {
            TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)));
            if (!Save) return false;
            Save->Version = 3;
            Save->Phase = ERunPhase::Combat;
            Save->Identity = Checkpoint.Identity;
            Save->CurrentNode = Checkpoint.NodeId;
            Save->CurrentEncounter = Checkpoint.EncounterId;
            Save->CombatCheckpoint = Checkpoint;
            for (FRunPartyMember& Member : Save->Party)
            {
                const FCombatCheckpointUnit* Unit = Checkpoint.Units.FindByPredicate([&Member](const FCombatCheckpointUnit& Entry) { return Entry.Team == ETeam::Player && Entry.PartySlot == Member.SlotIndex; });
                if (!Unit) return false;
                Member.CurrentHP = Unit->HP;
            }
            return FRunCheckpointStorage::Save(Save.Get(), Slot, OutError);
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
    Invalid.SchemaVersion = UCombatCheckpointLibrary::CurrentSchemaVersion + 1;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointRoundTripTest, "ProjectA.Checkpoint.RetiredCombatReadRejection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointRoundTripTest::RunTest(const FString& Parameters)
{
    FCheckpointStorageFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("Fixture initializes"), Fixture.Initialize(Error)))
    {
        return false;
    }
    FCombatCheckpointData Checkpoint = Fixture.MakeCheckpoint();
    TStrongObjectPtr<UOpponentSnapshotCatalogDataAsset> OpponentCatalog(LoadObject<UOpponentSnapshotCatalogDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Snapshots/DA_OpponentSnapshotCatalog.DA_OpponentSnapshotCatalog")));
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
    if (!TestTrue(TEXT("Historical v3 bytes are installed without the retired production commit API"), Fixture.WriteRetiredCombat(Checkpoint, Error))) return false;
    const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
    const FRunIdentityData BeforeIdentity = Fixture.Run->GetRunIdentity();
    const FCombatCheckpointData BeforeCheckpoint = Fixture.Run->GetCombatCheckpoint();
    int32 Events = 0;
    Fixture.Run->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
    TStrongObjectPtr<URunSaveGame> Disk(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
    if (!TestTrue(TEXT("Native serialization still reads retired data without executing it"), Disk.IsValid())) return false;
    TestEqual(TEXT("Historical bytes retain version three"), Disk->Version, 3);
    TestTrue(TEXT("Native serialization preserves the frozen opponent and original Host"), SameCheckpoint(Disk->CombatCheckpoint, Checkpoint) && FRunIdentityData::StaticStruct()->CompareScriptStruct(&Disk->Identity, &Checkpoint.Identity, 0));
    TestFalse(TEXT("General Continue rejects retired sequential combat"), Fixture.Run->CanContinueSavedRun(Error));
    TestTrue(TEXT("The rejection identifies the retired combat contract"), Error.ToString().Contains(TEXT("순차 턴")));
    TestFalse(TEXT("Standalone Continue rejects the same payload"), Fixture.Run->CanContinueStandaloneSavedRun(Error));
    TestFalse(TEXT("General load cannot execute the old battle"), Fixture.Run->LoadCheckpoint(Error));
    TestFalse(TEXT("Standalone load cannot execute the old battle"), Fixture.Run->LoadStandaloneCheckpoint(Error));
    TestTrue(TEXT("Rejections preserve current identity, phase and checkpoint"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Fixture.Run->GetRunIdentity(), &BeforeIdentity, 0) && Fixture.Run->GetPhase() == ERunPhase::Combat && SameCheckpoint(Fixture.Run->GetCombatCheckpoint(), BeforeCheckpoint));
    TestEqual(TEXT("Rejections preserve encounter-local runtime HP"), Fixture.Run->GetPartyMembers()[0].CurrentHP, -1.0f);
    TestTrue(TEXT("Rejections never rewrite historical bytes"), Fixture.ReadBytes() == BeforeBytes);
    TestEqual(TEXT("Read and rejection publish no Run events"), Events, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCheckpointAtomicTest, "ProjectA.Checkpoint.RetiredCommitPreservesNoncombat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatCheckpointAtomicTest::RunTest(const FString& Parameters)
{
    FCheckpointStorageFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("Fixture initializes with a durable Map and active unsaved battle"), Fixture.Initialize(Error))) return false;
    const FCombatCheckpointData Candidate = Fixture.MakeCheckpoint();
    const FCombatCheckpointData BeforeCheckpoint = Fixture.Run->GetCombatCheckpoint();
    const FRunIdentityData BeforeIdentity = Fixture.Run->GetRunIdentity();
    const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
    TestFalse(TEXT("A valid sequential checkpoint cannot be published by the retired API"), Fixture.Run->CommitCombatCheckpoint(Candidate, Error));
    TestFalse(TEXT("The retired publication API explains the unsupported contract"), Error.IsEmpty());
    TestFalse(TEXT("Ordinary saving cannot bypass unsupported midcombat persistence"), Fixture.Run->SaveCheckpoint(Error));
    TestTrue(TEXT("Rejected writes preserve identity, current phase and all checkpoint values"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Fixture.Run->GetRunIdentity(), &BeforeIdentity, 0) && Fixture.Run->GetPhase() == ERunPhase::Combat && SameCheckpoint(Fixture.Run->GetCombatCheckpoint(), BeforeCheckpoint));
    TestTrue(TEXT("Rejected writes preserve the last durable noncombat file"), Fixture.ReadBytes() == BeforeBytes);
    TestEqual(TEXT("Rejected writes preserve current party HP"), Fixture.Run->GetPartyMembers()[0].CurrentHP, -1.0f);
    TStrongObjectPtr<URunSaveGame> Disk(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
    TestTrue(TEXT("The retained file is a v2 Map without a battle payload"), Disk.IsValid() && Disk->Version == 2 && Disk->Phase == ERunPhase::Map && SameCheckpoint(Disk->CombatCheckpoint, FCombatCheckpointData()));
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
    if (!TestTrue(TEXT("Historical boundary fixture is installed"), Fixture.WriteRetiredCombat(Checkpoint, Error)))
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
        TestTrue(TEXT("Rejected load preserves the current battle"), SameCheckpoint(Fixture.Run->GetCombatCheckpoint(), FCombatCheckpointData()));
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
    Disk->EncounterProgress = FRunEncounterProgress();
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
    const FCombatCheckpointData Checkpoint = Fixture.Run->GetCombatCheckpoint();
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
    TestTrue(TEXT("Failed terminal write preserves the precombat file"), Fixture.ReadBytes() == CombatBytes);
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
    TestEqual(TEXT("Terminal result retains noncombat version two"), Disk->Version, 2);
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
    TestEqual(TEXT("Committed Continue publishes once"), Events, 2);
    TestTrue(TEXT("Encounter entry and exit commit after retry"), Fixture.Run->SelectRunEncounter(TEXT("Shop_02")) && Fixture.Run->LeaveRunEncounter());
    TestTrue(TEXT("Committed Continue exposes the next node"), Fixture.Run->CanStartNode(TEXT("Combat_02")));
    TestEqual(TEXT("Shop entry and exit each publish once"), Events, 4);
    TestTrue(TEXT("Next encounter begins after terminal commit"), Fixture.Run->BeginEncounter(TEXT("Combat_02")) && Fixture.Run->MarkCombatStarted());
    const TArray<uint8> NextMapBytes = Fixture.ReadBytes();
    TestFalse(TEXT("The following timed battle also rejects sequential checkpoints"), Fixture.Run->CommitCombatCheckpoint(Fixture.MakeCheckpoint(), Error));
    TestTrue(TEXT("The following battle keeps its last noncombat record"), Fixture.ReadBytes() == NextMapBytes && !Fixture.Run->HasCombatCheckpoint());
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
    TestTrue(TEXT("Historical schema one Human data remains recognizable"), UCombatCheckpointLibrary::Validate(Human, Fixture.Run->GetPartyMembers(), Error));
    if (!TestTrue(TEXT("Historical Human fixture is installed"), Fixture.WriteRetiredCombat(Human, Error))) return false;
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    TestFalse(TEXT("Recognizable schema one data cannot resume retired combat"), Restored->LoadCheckpoint(Error));
    TestTrue(TEXT("Rejected Human recovery preserves empty runtime"), Restored->GetPhase() == ERunPhase::None && !Restored->HasCombatCheckpoint());

    FCombatCheckpointData ValidAI = Human;
    ValidAI.Units[0].PartyControlMode = EPartyControlMode::ServerAI;
    TestFalse(TEXT("Schema one cannot smuggle an unsupported AI mode"), UCombatCheckpointLibrary::Validate(ValidAI, Fixture.Run->GetPartyMembers(), Error));
    ValidAI.SchemaVersion = 2;
    TestTrue(TEXT("Schema two supports AI without the original owner's prior consent"), UCombatCheckpointLibrary::Validate(ValidAI, Fixture.Run->GetPartyMembers(), Error));
    for (const ERunAIConsent Consent : { ERunAIConsent::Unknown, ERunAIConsent::Granted, ERunAIConsent::Declined })
    {
        FCombatCheckpointData Compatible = ValidAI;
        Compatible.Identity.OriginalParticipants[0].AIConsent = Consent;
        Compatible.Identity.OriginalParticipants[0].ConsentPolicyVersion = Consent == ERunAIConsent::Unknown ? 0 : 1;
        TestTrue(TEXT("Every valid legacy consent value permits stored AI control"), UCombatCheckpointLibrary::Validate(Compatible, Fixture.Run->GetPartyMembers(), Error));
    }
    FCombatCheckpointData Invalid = ValidAI;
    const auto Reject = [this, &Fixture, &ValidAI, &Invalid, &Error](const TCHAR* Label)
    {
        TestFalse(Label, UCombatCheckpointLibrary::Validate(Invalid, Fixture.Run->GetPartyMembers(), Error));
        TestFalse(FString(Label) + TEXT(" reports why"), Error.IsEmpty());
        Invalid = ValidAI;
    };
    Invalid.Identity.OriginalParticipants[0].AIConsent = ERunAIConsent::Granted;
    Invalid.Identity.OriginalParticipants[0].ConsentPolicyVersion = 0;
    Reject(TEXT("Legacy Granted metadata still requires a valid serialized policy version"));
    Invalid.Identity.OriginalParticipants[0].ConsentPolicyVersion = 1;
    Reject(TEXT("Legacy Unknown metadata still requires its zero policy version"));
    Invalid.Units[0].PartyControlMode = static_cast<EPartyControlMode>(255);
    Reject(TEXT("Unknown control mode is rejected"));
    Invalid.Units[1].PartyControlMode = EPartyControlMode::ServerAI;
    Reject(TEXT("Party AI mode cannot be assigned to an enemy entry"));

    // Native historical data stays inspectable while both Human and AI recovery remain retired.
    // 기존 데이터는 네이티브로 조회할 수 있지만 인간·AI 전투 복구는 모두 폐기합니다.
    for (const ERunAIConsent Consent : { ERunAIConsent::Unknown, ERunAIConsent::Declined })
    {
        FCombatCheckpointData AI = ValidAI;
        AI.Identity.OriginalParticipants[0].AIConsent = Consent;
        AI.Identity.OriginalParticipants[0].ConsentPolicyVersion = Consent == ERunAIConsent::Unknown ? 0 : 1;
        if (!TestTrue(TEXT("Historical AI fixture is installed"), Fixture.WriteRetiredCombat(AI, Error))) return false;
        const TArray<uint8> BeforeBytes = Fixture.ReadBytes();
        TStrongObjectPtr<URunSaveGame> Disk(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
        TestTrue(TEXT("Native reading retains AI mode, ownership and consent fields"), Disk.IsValid() && SameCheckpoint(Disk->CombatCheckpoint, AI));
        TestFalse(TEXT("AI historical battle is unavailable to Continue"), Restored->CanContinueSavedRun(Error));
        TestFalse(TEXT("AI historical battle cannot replace a current Run"), Restored->LoadCheckpoint(Error));
        TestTrue(TEXT("Rejected AI load preserves empty runtime and exact file bytes"), Restored->GetPhase() == ERunPhase::None && !Restored->HasCombatCheckpoint() && Fixture.ReadBytes() == BeforeBytes);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundCheckpointReadyCommitTest, "ProjectA.Checkpoint.RoundReadyAtomicCommit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundCheckpointReadyCommitTest::RunTest(const FString& Parameters)
{
    FCheckpointStorageFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("Round fixture initializes"), Fixture.Initialize(Error))) return false;
    FCombatCheckpointData Draft = Fixture.MakeRoundCheckpoint();
    if (!TestTrue(TEXT("Planning checkpoint commits"), Fixture.Run->CommitCombatCheckpoint(Draft, Error))) return false;
    const TArray<uint8> DraftBytes = Fixture.ReadBytes();
    FCombatCheckpointData Ready = Draft;
    ++Ready.Revision;
    ++Ready.PlanRevision;
    Ready.RoundPlans[0].bReady = true;
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Failed storage cannot confirm ready"), Fixture.Run->CommitCombatCheckpoint(Ready, Error));
    TestTrue(TEXT("Failure keeps exact previous bytes and in-memory planning state"), Fixture.ReadBytes() == DraftBytes && SameCheckpoint(Fixture.Run->GetCombatCheckpoint(), Draft));
    if (!TestTrue(TEXT("Retry commits ready and the complete unit state"), Fixture.Run->CommitCombatCheckpoint(Ready, Error))) return false;
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    TestTrue(TEXT("Standalone Continue recognizes round readiness"), Restored->CanContinueStandaloneSavedRun(Error));
    TestTrue(TEXT("A fresh Run loads committed readiness after process loss"), Restored->LoadStandaloneCheckpoint(Error));
    TestTrue(TEXT("Readiness, plans, HP, costs and round numbers round-trip together"), SameCheckpoint(Restored->GetCombatCheckpoint(), Ready));
    TestEqual(TEXT("Snapshot retains AP before Ready-end payment"), Restored->GetCombatCheckpoint().Units[0].AP, Ready.Units[0].MaxAP);
    TestFalse(TEXT("A stale revision cannot overwrite saved readiness"), Fixture.Run->CommitCombatCheckpoint(Draft, Error));
    FCombatCheckpointData Invalidated = Ready;
    ++Invalidated.Revision;
    ++Invalidated.PlanRevision;
    Invalidated.RoundPlans[0].bReady = false;
    TestTrue(TEXT("Editing readiness commits its invalidation"), Fixture.Run->CommitCombatCheckpoint(Invalidated, Error));
    TestTrue(TEXT("Reload sees the invalidation instead of an obsolete ready plan"), Restored->LoadStandaloneCheckpoint(Error) && !Restored->GetCombatCheckpoint().RoundPlans[0].bReady);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundCheckpointSkipReadyTest, "ProjectA.Checkpoint.RoundSkipReadyRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundCheckpointSkipReadyTest::RunTest(const FString& Parameters)
{
    FCheckpointStorageFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("Skip fixture initializes"), Fixture.Initialize(Error))) return false;
    FCombatCheckpointData Ready = Fixture.MakeRoundCheckpoint();
    Ready.RoundPlans[0].bReady = true;
    Ready.RoundPlans[0].Command.SkillId = NAME_None;
    Ready.RoundPlans[0].Command.TargetUnitId = INDEX_NONE;
    Ready.RoundPlans[0].Command.TargetCoord = Ready.Units[0].GridCoord;
    Ready.Units[0].AP = 0;
    Ready.Units[0].SubAP = 0;
    if (!TestTrue(TEXT("Ready without a skill or action points commits"), Fixture.Run->CommitCombatCheckpoint(Ready, Error))) return false;
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    TestTrue(TEXT("Standalone Continue recognizes a ready skip"), Restored->CanContinueStandaloneSavedRun(Error));
    TestTrue(TEXT("A ready skip survives process loss without an invented skill"), Restored->LoadStandaloneCheckpoint(Error) && SameCheckpoint(Restored->GetCombatCheckpoint(), Ready));
    ++Ready.Revision;
    ++Ready.PlanRevision;
    Ready.RoundPlans[0].bHasMovePlan = true;
    Ready.RoundPlans[0].MoveDestinationCoord = FIntPoint(2, 0);
    Ready.Units[0].MaxSubAP = FMath::Max(1, Ready.Units[0].MaxSubAP);
    Ready.Units[0].SubAP = 1;
    if (!TestTrue(TEXT("A ready skip can retain a reserved SAP move"), Fixture.Run->CommitCombatCheckpoint(Ready, Error))) return false;
    TestTrue(TEXT("Skip readiness and the unpaid SAP move round-trip together"), Restored->LoadStandaloneCheckpoint(Error) && SameCheckpoint(Restored->GetCombatCheckpoint(), Ready));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatLegacyRoundCheckpointTest, "ProjectA.Checkpoint.LegacyOfflineReadyRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatLegacyRoundCheckpointTest::RunTest(const FString& Parameters)
{
    FCheckpointStorageFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("Legacy fixture initializes"), Fixture.Initialize(Error))) return false;
    TStrongObjectPtr<URunSaveGame> Legacy(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
    if (!TestNotNull(TEXT("The existing map save loads"), Legacy.Get())) return false;
    Legacy->Version = 1;
    Legacy->Identity = FRunIdentityData();
    Legacy->EncounterProgress = FRunEncounterProgress();
    for (FRunPartyMember& Member : Legacy->Party)
    {
        Member.CharacterId.Invalidate();
        Member.OwnerAccountId = FRunAccountId();
    }
    if (!TestTrue(TEXT("A metadata-free map checkpoint is installed"), FRunCheckpointStorage::Save(Legacy.Get(), Fixture.Slot, Error)) || !TestTrue(TEXT("Legacy Continue retains offline identity"), Fixture.Run->LoadStandaloneCheckpoint(Error)) || !TestTrue(TEXT("Legacy progression enters a new encounter"), Fixture.Run->BeginEncounter(TEXT("Combat_01")) && Fixture.Run->MarkCombatStarted())) return false;
    FCombatCheckpointData Ready = Fixture.MakeRoundCheckpoint();
    Ready.RoundPlans[0].bReady = true;
    const TArray<uint8> Before = Fixture.ReadBytes();
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Failed conversion does not confirm legacy readiness"), Fixture.Run->CommitCombatCheckpoint(Ready, Error));
    TestTrue(TEXT("Failed conversion preserves the original v1 file"), Fixture.ReadBytes() == Before && !Fixture.Run->HasCombatCheckpoint());
    if (!TestTrue(TEXT("Legacy ready state commits without inventing owners"), Fixture.Run->CommitCombatCheckpoint(Ready, Error))) return false;
    Legacy.Reset(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
    if (!TestNotNull(TEXT("Legacy combat save loads"), Legacy.Get())) return false;
    TestEqual(TEXT("Legacy round payload has a distinct combat-only outer version"), Legacy->Version, 6);
    TestTrue(TEXT("Legacy identity and player ownership remain absent"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Legacy->Identity, &Ready.Identity, 0) && !Legacy->Party[0].CharacterId.IsValid() && Legacy->Party[0].OwnerAccountId.IsEmpty());
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    TestTrue(TEXT("Standalone Continue recognizes durable legacy readiness"), Restored->CanContinueStandaloneSavedRun(Error));
    TestTrue(TEXT("Legacy round state survives process loss"), Restored->LoadStandaloneCheckpoint(Error) && SameCheckpoint(Restored->GetCombatCheckpoint(), Ready));
    FCombatCheckpointData Invalid = Ready;
    Invalid.Units[0].CharacterId = FGuid::NewGuid();
    TestFalse(TEXT("Legacy payload cannot acquire an inferred character identity"), UCombatCheckpointLibrary::Validate(Invalid, Restored->GetPartyMembers(), Error));
    Invalid = Ready;
    Invalid.Units[0].PartyControlMode = EPartyControlMode::ServerAI;
    TestFalse(TEXT("Legacy restore cannot silently transfer human control to AI"), UCombatCheckpointLibrary::Validate(Invalid, Restored->GetPartyMembers(), Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundCheckpointValidationTest, "ProjectA.Checkpoint.RoundPlanValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundCheckpointValidationTest::RunTest(const FString& Parameters)
{
    FCheckpointStorageFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("Round fixture initializes"), Fixture.Initialize(Error))) return false;
    const FCombatCheckpointData Valid = Fixture.MakeRoundCheckpoint();
    TestTrue(TEXT("Actor-free round boundary validates"), UCombatCheckpointLibrary::Validate(Valid, Fixture.Run->GetPartyMembers(), Error));
    FCombatCheckpointData Invalid = Valid;
    Invalid.RoundPlans[0].Command.TargetUnitId = 8;
    TestFalse(TEXT("Missing command target is rejected"), UCombatCheckpointLibrary::Validate(Invalid, Fixture.Run->GetPartyMembers(), Error));
    Invalid = Valid;
    Invalid.RoundPlans[1].UnitId = Invalid.RoundPlans[0].UnitId;
    TestFalse(TEXT("Duplicate command ownership is rejected"), UCombatCheckpointLibrary::Validate(Invalid, Fixture.Run->GetPartyMembers(), Error));
    Invalid = Valid;
    Invalid.RoundPlans[0].bHasMovePlan = true;
    Invalid.RoundPlans[0].MoveDestinationCoord = Invalid.Units[1].GridCoord;
    TestFalse(TEXT("Reserved movement cannot overwrite another home tile"), UCombatCheckpointLibrary::Validate(Invalid, Fixture.Run->GetPartyMembers(), Error));
    Invalid = Valid;
    Invalid.RoundPlans[0].Command.SkillId = TEXT("UnknownSkill");
    TestFalse(TEXT("An unequipped action cannot be recovered"), UCombatCheckpointLibrary::Validate(Invalid, Fixture.Run->GetPartyMembers(), Error));
    FCombatCheckpointData Skip = Valid;
    Skip.RoundPlans[0].bReady = true;
    Skip.RoundPlans[0].Command.SkillId = NAME_None;
    Skip.RoundPlans[0].Command.TargetUnitId = INDEX_NONE;
    Skip.Units[0].AP = 0;
    Skip.Units[0].SubAP = 0;
    Skip.Units[0].Skills.Reset();
    Skip.Units[0].DefaultAttackAbility.Reset();
    TestTrue(TEXT("A ready human may skip without skills, targets or action points"), UCombatCheckpointLibrary::Validate(Skip, Fixture.Run->GetPartyMembers(), Error));
    Skip.RoundPlans[0].bHasMovePlan = true;
    Skip.RoundPlans[0].MoveDestinationCoord = FIntPoint(2, 0);
    TestFalse(TEXT("Skipping a skill does not waive reserved movement SAP"), UCombatCheckpointLibrary::Validate(Skip, Fixture.Run->GetPartyMembers(), Error));
    Skip.Units[0].MaxSubAP = FMath::Max(1, Skip.Units[0].MaxSubAP);
    Skip.Units[0].SubAP = 1;
    TestTrue(TEXT("A ready skip with one SAP can retain a valid move reservation"), UCombatCheckpointLibrary::Validate(Skip, Fixture.Run->GetPartyMembers(), Error));
    Skip.RoundPlans[0].MoveDestinationCoord = Skip.Units[1].GridCoord;
    TestFalse(TEXT("Skipping a skill does not waive movement destination validation"), UCombatCheckpointLibrary::Validate(Skip, Fixture.Run->GetPartyMembers(), Error));
    FCombatCheckpointData IdleEnemy = Valid;
    IdleEnemy.Units[1].Skills.Reset();
    IdleEnemy.Units[1].DefaultAttackAbility.Reset();
    TestTrue(TEXT("A native idle enemy does not require retired GAS ability metadata"), UCombatCheckpointLibrary::Validate(IdleEnemy, Fixture.Run->GetPartyMembers(), Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRoundCheckpointSkillRenameTest, "ProjectA.Checkpoint.RoundSkillRenameCompatibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FCombatRoundCheckpointSkillRenameTest::RunTest(const FString& Parameters)
{
    const FName PreviousId(TEXT("SkillDefinitionDataAsset:DA_SweepingStrike"));
    const FName CurrentId(TEXT("SkillDefinitionDataAsset:BPDA_SweepingStrike"));
    TestEqual(TEXT("Saved commands use the configured primary asset redirect"), UCombatCheckpointLibrary::ResolveSavedSkillId(PreviousId), CurrentId);
    TestEqual(TEXT("The current command identifier remains unchanged"), UCombatCheckpointLibrary::ResolveSavedSkillId(CurrentId), CurrentId);
    TestEqual(TEXT("Internal wait and human skip remain empty"), UCombatCheckpointLibrary::ResolveSavedSkillId(NAME_None), NAME_None);
    const FName UnrelatedId(TEXT("SkillDefinitionDataAsset:BPDA_DefaulatAttack"));
    TestEqual(TEXT("Other existing skill identifiers are preserved"), UCombatCheckpointLibrary::ResolveSavedSkillId(UnrelatedId), UnrelatedId);
    const FSoftObjectPath CurrentPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_SweepingStrike.BPDA_SweepingStrike"));
    USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(CurrentPath.TryLoad());
    if (!TestNotNull(TEXT("The renamed skill asset exists"), Skill)) return false;
    const FSoftObjectPath PreviousPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DA_SweepingStrike.DA_SweepingStrike"));
    const FSoftObjectPath OriginalPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_SweepingStrike.DA_SweepingStrike"));
    TestTrue(TEXT("Both historical soft references load the renamed asset"), PreviousPath.TryLoad() == Skill && OriginalPath.TryLoad() == Skill);
    FCheckpointStorageFixture Fixture;
    FText Error;
    if (!TestTrue(TEXT("A real-content checkpoint fixture initializes"), Fixture.Initialize(Error))) return false;
    FCombatCheckpointData Checkpoint = Fixture.MakeRoundCheckpoint();
    Checkpoint.Units[0].Skills = {PreviousPath};
    Checkpoint.RoundPlans[0].Command.SkillId = PreviousId;
    Checkpoint.RoundPlans[0].Command.TargetUnitId = Checkpoint.Units[1].RoundUnitId;
    Checkpoint.RoundPlans[0].Command.TargetCoord = Checkpoint.Units[1].GridCoord;
    Checkpoint.RoundPlans[0].bReady = true;
    TestTrue(TEXT("An existing ready save validates with its old path and command identifier"), UCombatCheckpointLibrary::Validate(Checkpoint, Fixture.Run->GetPartyMembers(), Error));
    return true;
}

#endif
