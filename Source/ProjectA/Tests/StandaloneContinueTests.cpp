#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"
#include "UObject/StrongObjectPtr.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
    struct FStandaloneContinueFixture
    {
        TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
        TStrongObjectPtr<URunStateSubsystem> Run{NewObject<URunStateSubsystem>(Instance.Get())};
        TStrongObjectPtr<UPartyDefinitionDataAsset> Catalog{LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"))};
        FString Slot = TEXT("T14_StandaloneContinue_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);

        FStandaloneContinueFixture()
        {
            Run->PartyDefinition = Catalog.Get();
            Run->EnableCheckpointSaving(Slot);
        }

        ~FStandaloneContinueFixture()
        {
            UGameplayStatics::DeleteGameInSlot(Slot, 0);
        }

        TStrongObjectPtr<URunSaveGame> MakeSave(ERunIdentityOrigin Origin, int32 Participants, int32 PartySize, bool bCombat) const
        {
            FProfessionDefinition Profession;
            if (!Catalog || !Catalog->ResolveProfession(TEXT("Archer"), Profession) || Profession.StartingSkills.IsEmpty() || !Profession.StartingSkills[0]) return TStrongObjectPtr<URunSaveGame>();
            TStrongObjectPtr<URunSaveGame> Save(NewObject<URunSaveGame>());
            Save->Version = bCombat ? 3 : Origin == ERunIdentityOrigin::LegacyOffline ? 1 : 2;
            Save->Catalog = FSoftObjectPath(Catalog.Get());
            Save->Phase = bCombat ? ERunPhase::Combat : ERunPhase::Map;
            Save->Identity.Origin = Origin;
            if (Origin != ERunIdentityOrigin::LegacyOffline)
            {
                Save->Identity.RunId = FGuid::NewGuid();
                Save->Identity.HostEpoch = 1;
                for (int32 Index = 0; Index < Participants; ++Index)
                {
                    FRunParticipantData& Participant = Save->Identity.OriginalParticipants.AddDefaulted_GetRef();
                    Participant.AccountId.Provider = Origin == ERunIdentityOrigin::LocalDevelopment ? TEXT("Development") : TEXT("MenuFixtureProvider");
                    Participant.AccountId.Subject = FString::Printf(TEXT("OriginalOwner%d"), Index);
                }
                Save->Identity.HostAccountId = Save->Identity.OriginalParticipants[0].AccountId;
            }
            for (int32 Index = 0; Index < PartySize; ++Index)
            {
                FRunPartyMember& Member = Save->Party.AddDefaulted_GetRef();
                Member.SlotIndex = Index;
                Member.bCreated = true;
                Member.ClassId = TEXT("Archer");
                Member.CharacterName = FText::FromString(FString::Printf(TEXT("Saved Archer %d"), Index));
                Member.CurrentHP = Profession.MaxHP;
                if (Origin != ERunIdentityOrigin::LegacyOffline)
                {
                    Member.CharacterId = FGuid::NewGuid();
                    Member.OwnerAccountId = Save->Identity.OriginalParticipants[Index % Participants].AccountId;
                }
            }
            for (int32 Index = 0; Index < 2; ++Index)
            {
                FRunNodeDefinition& Node = Save->Nodes.AddDefaulted_GetRef();
                Node.NodeId = FName(*FString::Printf(TEXT("Combat_%02d"), Index + 1));
                Node.EncounterId = TEXT("DefaultEncounter");
            }
            if (!bCombat) return Save;
            Save->CurrentNode = Save->Nodes[0].NodeId;
            Save->CurrentEncounter = TEXT("DefaultEncounter");
            FCombatCheckpointData& Checkpoint = Save->CombatCheckpoint;
            Checkpoint.AttemptId = FGuid::NewGuid();
            Checkpoint.Revision = 1;
            Checkpoint.Identity = Save->Identity;
            Checkpoint.NodeId = Save->CurrentNode;
            Checkpoint.EncounterId = Save->CurrentEncounter;
            Checkpoint.CompletedTurnSerial = 5;
            for (const FRunPartyMember& Member : Save->Party)
            {
                FCombatCheckpointUnit& Unit = Checkpoint.Units.AddDefaulted_GetRef();
                Unit.UnitId = FGuid::NewGuid();
                Unit.CharacterId = Member.CharacterId;
                Unit.OwnerAccountId = Member.OwnerAccountId;
                Unit.PartySlot = Member.SlotIndex;
                Unit.UnitClass = FSoftObjectPath(Profession.CombatClass.Get());
                Unit.CharacterName = Member.CharacterName;
                Unit.HP = Member.CurrentHP;
                Unit.MaxHP = Profession.MaxHP;
                Unit.MaxAP = Profession.ActionPoints;
                Unit.MaxSubAP = Profession.SubActionPoints;
                Unit.GridCoord = FIntPoint(Member.SlotIndex, 1);
                Unit.Transform = FTransform(FVector(-200.0f * Member.SlotIndex, 200.0f, 100.0f));
                Unit.Skills = {FSoftObjectPath(Profession.StartingSkills[0].Get())};
                Unit.DefaultAttackAbility = FSoftObjectPath(Profession.StartingSkills[0]->AbilityClass.Get());
            }
            FCombatCheckpointUnit Enemy = Checkpoint.Units[0];
            Enemy.UnitId = FGuid::NewGuid();
            Enemy.CharacterId.Invalidate();
            Enemy.OwnerAccountId = FRunAccountId();
            Enemy.PartySlot = INDEX_NONE;
            Enemy.Team = ETeam::Enemy;
            Enemy.UnitClass = FSoftObjectPath(AEnemyUnit::StaticClass());
            Enemy.CharacterName = FText::FromString(TEXT("Saved Opponent"));
            Enemy.GridCoord = FIntPoint(0, 2);
            Enemy.Transform = FTransform(FVector(0.0f, 400.0f, 100.0f));
            Checkpoint.Units.Add(Enemy);
            return Save;
        }

        TArray<uint8> ReadBytes() const
        {
            TArray<uint8> Bytes;
            FFileHelper::LoadFileToArray(Bytes, *(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".sav"))));
            return Bytes;
        }

        // Compare every public persisted Run value without requesting another save or changing its slot.
        // 추가 저장이나 슬롯 변경 없이 공개된 모든 영속 Run 값을 비교합니다.
        TArray<uint8> CaptureRuntime() const
        {
            TStrongObjectPtr<URunSaveGame> State(NewObject<URunSaveGame>());
            State->Identity = Run->GetRunIdentity();
            State->Participation = Run->GetParticipation();
            State->EncounterProgress = Run->GetEncounterProgress();
            State->SkillShopState = Run->GetSkillShopState();
            State->Party = Run->GetPartyMembers();
            State->Nodes = Run->GetNodes();
            State->CompletedNodes = Run->GetCompletedNodes();
            State->CurrentNode = Run->GetCurrentNodeId();
            State->CurrentEncounter = Run->GetCurrentEncounterId();
            State->Phase = Run->GetPhase();
            State->Result = Run->GetLastResult();
            State->Catalog = FSoftObjectPath(Run->PartyDefinition.Get());
            State->CombatCheckpoint = Run->GetCombatCheckpoint();
            TArray<uint8> Bytes;
            UGameplayStatics::SaveGameToMemory(State.Get(), Bytes);
            return Bytes;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStandaloneContinueEligibilityTest, "ProjectA.Persistence.StandaloneContinueEligibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStandaloneContinueEligibilityTest::RunTest(const FString& Parameters)
{
    FStandaloneContinueFixture Fixture;
    FText Error;
    int32 Events = 0;
    const FDelegateHandle Observer = Fixture.Run->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
    ON_SCOPE_EXIT { Fixture.Run->OnRunStateChanged.Remove(Observer); };
    struct FCase
    {
        ERunIdentityOrigin Origin;
        int32 Participants;
        int32 PartySize;
        bool bCombat;
        bool bStandalone;
    };
    const TArray<FCase> Cases = {
        {ERunIdentityOrigin::LegacyOffline, 0, 4, false, true},
        {ERunIdentityOrigin::LocalDevelopment, 1, 4, false, true},
        {ERunIdentityOrigin::LocalDevelopment, 1, 4, true, false},
        {ERunIdentityOrigin::AccountProvider, 1, 1, false, false},
        {ERunIdentityOrigin::AccountProvider, 1, 1, true, false},
        {ERunIdentityOrigin::AccountProvider, 2, 2, false, false},
        {ERunIdentityOrigin::AccountProvider, 2, 2, true, false},
        {ERunIdentityOrigin::LocalDevelopment, 2, 2, false, false},
        {ERunIdentityOrigin::LocalDevelopment, 2, 2, true, false}
    };
    for (const FCase& Case : Cases)
    {
        TStrongObjectPtr<URunSaveGame> Save = Fixture.MakeSave(Case.Origin, Case.Participants, Case.PartySize, Case.bCombat);
        if (!TestNotNull(TEXT("Eligibility uses an authored profession and valid native save"), Save.Get()) || !TestTrue(TEXT("The isolated native save is written"), FRunCheckpointStorage::Save(Save.Get(), Fixture.Slot, Error))) return false;
        const FString Label = FString::Printf(TEXT("v%d origin%d owners%d: "), Save->Version, static_cast<int32>(Case.Origin), Case.Participants);
        const TArray<uint8> BeforeRuntime = Fixture.CaptureRuntime();
        const TArray<uint8> BeforeDisk = Fixture.ReadBytes();
        const int32 BeforeEvents = Events;
        TestEqual(Label + TEXT("general eligibility rejects retired combat and preserves noncombat support"), Fixture.Run->CanContinueSavedRun(Error), !Case.bCombat);
        TestEqual(Label + TEXT("menu eligibility follows the existing Standalone binding support"), Fixture.Run->CanContinueStandaloneSavedRun(Error), Case.bStandalone);
        if (!Case.bStandalone) TestFalse(Label + TEXT("menu rejection explains the required session"), Error.IsEmpty());
        TestTrue(Label + TEXT("both queries preserve runtime, disk and event count"), Fixture.CaptureRuntime() == BeforeRuntime && Fixture.ReadBytes() == BeforeDisk && Events == BeforeEvents);
        TestEqual(Label + TEXT("actual menu load enforces the same eligibility"), Fixture.Run->LoadStandaloneCheckpoint(Error), Case.bStandalone);
        if (!Case.bStandalone)
        {
            TestTrue(Label + TEXT("rejected menu load preserves runtime, disk and events"), Fixture.CaptureRuntime() == BeforeRuntime && Fixture.ReadBytes() == BeforeDisk && Events == BeforeEvents);
        }
        else
        {
            TestEqual(Label + TEXT("one accepted menu load publishes exactly one event"), Events, BeforeEvents + 1);
            TestTrue(Label + TEXT("accepted menu load retains the complete identity"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Save->Identity, &Fixture.Run->GetRunIdentity(), 0));
        }
        TestEqual(Label + TEXT("general load enforces the same combat migration gate"), Fixture.Run->LoadCheckpoint(Error), !Case.bCombat);
        if (Case.bCombat)
        {
            TestTrue(Label + TEXT("retired combat is explained explicitly"), Error.ToString().Contains(TEXT("순차 턴")));
            TestTrue(Label + TEXT("general rejection preserves current Run, file and events"), Fixture.CaptureRuntime() == BeforeRuntime && Fixture.ReadBytes() == BeforeDisk && Events == BeforeEvents);
        }
        else
        {
            TestTrue(Label + TEXT("general noncombat load preserves complete identity"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Save->Identity, &Fixture.Run->GetRunIdentity(), 0));
        }
        TestTrue(Label + TEXT("all reads leave file bytes unchanged"), Fixture.ReadBytes() == BeforeDisk);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStandaloneContinueFileReplacementTest, "ProjectA.Persistence.StandaloneContinueFileReplacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStandaloneContinueFileReplacementTest::RunTest(const FString& Parameters)
{
    FStandaloneContinueFixture Fixture;
    FText Error;
    TStrongObjectPtr<URunSaveGame> Original = Fixture.MakeSave(ERunIdentityOrigin::LocalDevelopment, 1, 1, false);
    if (!TestNotNull(TEXT("A current local Map is available for preservation checks"), Original.Get())) return false;
    int32 Events = 0;
    const FDelegateHandle Observer = Fixture.Run->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
    ON_SCOPE_EXIT { Fixture.Run->OnRunStateChanged.Remove(Observer); };
    for (bool bReplacementCombat : {false, true})
    {
        if (!TestTrue(TEXT("The original local Map is installed"), FRunCheckpointStorage::Save(Original.Get(), Fixture.Slot, Error)) || !TestTrue(TEXT("The current runtime starts from the original local Map"), Fixture.Run->LoadStandaloneCheckpoint(Error))) return false;
        const TArray<uint8> BeforeRuntime = Fixture.CaptureRuntime();
        const FText BeforeSaveError = Fixture.Run->GetSaveError();
        const bool bBeforeSaving = Fixture.Run->IsCheckpointSavingEnabled();
        const int32 BeforeEvents = Events;
        TestTrue(TEXT("The menu initially enables Continue for the local save"), Fixture.Run->CanContinueStandaloneSavedRun(Error));
        TStrongObjectPtr<URunSaveGame> Replaced = Fixture.MakeSave(ERunIdentityOrigin::AccountProvider, 2, 2, bReplacementCombat);
        if (!TestNotNull(TEXT("The replacement is a cooperative native save"), Replaced.Get()) || !TestTrue(TEXT("The file changes after the menu eligibility check"), FRunCheckpointStorage::Save(Replaced.Get(), Fixture.Slot, Error))) return false;
        const TArray<uint8> ReplacedBytes = Fixture.ReadBytes();
        TestEqual(TEXT("General eligibility accepts only the noncombat replacement"), Fixture.Run->CanContinueSavedRun(Error), !bReplacementCombat);
        TestFalse(TEXT("Actual menu load rechecks the replacement instead of trusting the enabled button"), Fixture.Run->LoadStandaloneCheckpoint(Error));
        TestFalse(TEXT("The rejected click reports why this session cannot restore the save"), Error.IsEmpty());
        TestTrue(TEXT("Rejected click preserves every current Run field and its persistence status"), Fixture.CaptureRuntime() == BeforeRuntime && Fixture.Run->GetSaveError().EqualTo(BeforeSaveError) && Fixture.Run->IsCheckpointSavingEnabled() == bBeforeSaving);
        TestEqual(TEXT("Rejected click publishes no Run event"), Events, BeforeEvents);
        TestTrue(TEXT("Rejected click preserves the exact replacement file"), Fixture.ReadBytes() == ReplacedBytes);
        TestEqual(TEXT("Explicit load also rejects a retired combat replacement"), Fixture.Run->LoadCheckpoint(Error), !bReplacementCombat);
        if (bReplacementCombat)
        {
            TestTrue(TEXT("General rejection keeps all prior runtime state and events"), Fixture.CaptureRuntime() == BeforeRuntime && Events == BeforeEvents);
            TestTrue(TEXT("General rejection keeps the replaced file intact"), Fixture.ReadBytes() == ReplacedBytes);
        }
        else
        {
            TestEqual(TEXT("Supported general load retains the replacement's real Run ID"), Fixture.Run->GetRunIdentity().RunId, Replaced->Identity.RunId);
        }
    }
    if (!TestTrue(TEXT("The local save is restored for a second eligibility check"), FRunCheckpointStorage::Save(Original.Get(), Fixture.Slot, Error)) || !TestTrue(TEXT("The menu enables a local save again"), Fixture.Run->CanContinueStandaloneSavedRun(Error))) return false;
    TStrongObjectPtr<URunSaveGame> NewLocal = Fixture.MakeSave(ERunIdentityOrigin::LocalDevelopment, 1, 2, false);
    if (!TestNotNull(TEXT("The second replacement is a supported local save"), NewLocal.Get()) || !TestTrue(TEXT("A different eligible local save replaces the displayed file"), FRunCheckpointStorage::Save(NewLocal.Get(), Fixture.Slot, Error))) return false;
    TestTrue(TEXT("The menu may load a newly validated eligible file"), Fixture.Run->LoadStandaloneCheckpoint(Error));
    TestEqual(TEXT("Load applies the newly validated object instead of stale menu data"), Fixture.Run->GetRunIdentity().RunId, NewLocal->Identity.RunId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStandaloneSurrenderEligibilityTest, "ProjectA.Persistence.StandaloneSurrenderEligibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStandaloneSurrenderEligibilityTest::RunTest(const FString& Parameters)
{
    struct FCase
    {
        ERunIdentityOrigin Origin;
        int32 Participants;
        bool bCombat;
        bool bAllowed;
    };
    const TArray<FCase> Cases = {
        {ERunIdentityOrigin::LegacyOffline, 0, false, true},
        {ERunIdentityOrigin::LocalDevelopment, 1, false, true},
        {ERunIdentityOrigin::LocalDevelopment, 1, true, false},
        {ERunIdentityOrigin::LocalDevelopment, 2, false, false},
        {ERunIdentityOrigin::AccountProvider, 1, false, false},
        {ERunIdentityOrigin::AccountProvider, 2, false, false}
    };
    for (const FCase& Case : Cases)
    {
        FStandaloneContinueFixture Fixture;
        FText Error;
        TStrongObjectPtr<URunSaveGame> Save = Fixture.MakeSave(Case.Origin, Case.Participants, 4, Case.bCombat);
        if (!TestNotNull(TEXT("The surrender candidate is valid"), Save.Get()) || !TestTrue(TEXT("The isolated candidate is saved"), FRunCheckpointStorage::Save(Save.Get(), Fixture.Slot, Error))) return false;
        const TArray<uint8> BeforeDisk = Fixture.ReadBytes();
        const TArray<uint8> BeforeRuntime = Fixture.CaptureRuntime();
        int32 Events = 0;
        const FDelegateHandle Observer = Fixture.Run->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
        ON_SCOPE_EXIT { Fixture.Run->OnRunStateChanged.Remove(Observer); };
        FString Token = TEXT("StaleToken");
        TestEqual(TEXT("Surrender uses the normal Standalone Continue scope"), Fixture.Run->GetStandaloneSurrenderToken(Token, Error), Case.bAllowed);
        TestEqual(TEXT("Only an eligible save produces a confirmation token"), !Token.IsEmpty(), Case.bAllowed);
        if (!Case.bAllowed) TestFalse(TEXT("A rejected identity cannot surrender through a direct confirmation call"), Fixture.Run->SurrenderStandaloneSavedRun(TEXT("StaleToken"), Error));
        TestTrue(TEXT("Preview or rejected confirmation preserves disk memory autosave and events"), Fixture.ReadBytes() == BeforeDisk && Fixture.CaptureRuntime() == BeforeRuntime && Fixture.Run->IsCheckpointSavingEnabled() && Events == 0);
        if (Case.bAllowed && Case.Origin == ERunIdentityOrigin::LegacyOffline)
        {
            TestTrue(TEXT("A legacy offline save can be surrendered without inventing a Run ID"), Fixture.Run->SurrenderStandaloneSavedRun(Token, Error));
            TestFalse(TEXT("A surrendered legacy save cannot continue"), Fixture.Run->CanContinueStandaloneSavedRun(Error));
        }
    }
    for (int32 Case = 0; Case < 3; ++Case)
    {
        FStandaloneContinueFixture Fixture;
        FText Error;
        TStrongObjectPtr<URunSaveGame> Save = Fixture.MakeSave(ERunIdentityOrigin::LocalDevelopment, Case == 0 ? 2 : 1, 2, false);
        if (!Save) return false;
        if (Case == 0)
        {
            Save->Version = 4;
            Save->Identity.SchemaVersion = 2;
            for (int32 Index = 0; Index < Save->Identity.OriginalParticipants.Num(); ++Index)
            {
                Save->Identity.OriginalParticipants[Index].JoinOrdinal = Index + 1;
                Save->Participation.HumanParticipants.Add(Save->Identity.OriginalParticipants[Index].AccountId);
            }
        }
        else if (Case == 1)
        {
            Save->Phase = ERunPhase::Complete;
            Save->Result = ECombatResult::Victory;
            for (const FRunNodeDefinition& Node : Save->Nodes) Save->CompletedNodes.Add(Node.NodeId);
            Save->CurrentNode = Save->Nodes.Last().NodeId;
        }
        else
        {
            Save->Phase = ERunPhase::Defeat;
            Save->Result = ECombatResult::Defeat;
            Save->CurrentNode = Save->Nodes[0].NodeId;
            Save->CurrentEncounter = Save->Nodes[0].EncounterId;
            for (FRunPartyMember& Member : Save->Party) Member.CurrentHP = 0.0f;
        }
        if (!TestTrue(TEXT("The managed or completed fixture is saved"), FRunCheckpointStorage::Save(Save.Get(), Fixture.Slot, Error))) return false;
        const TArray<uint8> Before = Fixture.ReadBytes();
        FString Token;
        TestFalse(TEXT("Managed and already completed saves cannot be surrendered from the ordinary menu"), Fixture.Run->GetStandaloneSurrenderToken(Token, Error));
        TestFalse(TEXT("Managed and completed confirmation cannot bypass the preview guard"), Fixture.Run->SurrenderStandaloneSavedRun(TEXT("Unconfirmed"), Error));
        TestTrue(TEXT("The ineligible saved record remains unchanged"), Fixture.ReadBytes() == Before);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStandaloneSurrenderCommitTest, "ProjectA.Persistence.StandaloneSurrenderCommit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStandaloneSurrenderCommitTest::RunTest(const FString& Parameters)
{
    FStandaloneContinueFixture Fixture;
    FStandaloneContinueFixture OtherSlot;
    FText Error;
    TStrongObjectPtr<URunSaveGame> Save = Fixture.MakeSave(ERunIdentityOrigin::LocalDevelopment, 1, 4, false);
    if (!Save || !FRunCheckpointStorage::Save(Save.Get(), Fixture.Slot, Error) || !FRunCheckpointStorage::Save(Save.Get(), OtherSlot.Slot, Error) || !Fixture.Run->LoadStandaloneCheckpoint(Error)) return false;
    const TArray<uint8> BeforeDisk = Fixture.ReadBytes();
    const TArray<uint8> OtherBytes = OtherSlot.ReadBytes();
    const TArray<uint8> BeforeRuntime = Fixture.CaptureRuntime();
    const FText BeforeError = Fixture.Run->GetSaveError();
    int32 Events = 0;
    bool bPublishedClearedState = false;
    const FDelegateHandle Observer = Fixture.Run->OnRunStateChanged.AddLambda([&Fixture, &Events, &bPublishedClearedState]()
    {
        ++Events;
        bPublishedClearedState = Fixture.Run->GetPhase() == ERunPhase::None && Fixture.Run->GetPartyMembers().IsEmpty() && !Fixture.Run->IsCheckpointSavingEnabled();
    });
    ON_SCOPE_EXIT { Fixture.Run->OnRunStateChanged.Remove(Observer); };
    FString Token;
    if (!TestTrue(TEXT("An eligible save produces a confirmation token"), Fixture.Run->GetStandaloneSurrenderToken(Token, Error))) return false;
    TestTrue(TEXT("Opening and cancelling confirmation has no persistence side effects"), Fixture.ReadBytes() == BeforeDisk && Fixture.CaptureRuntime() == BeforeRuntime && Events == 0);
    TestFalse(TEXT("An empty token cannot confirm surrender"), Fixture.Run->SurrenderStandaloneSavedRun(FString(), Error));
    TestFalse(TEXT("A token cannot delete an identical save in a different slot"), FRunCheckpointStorage::DeleteIfUnchanged(OtherSlot.Slot, Token, Error));
    TestFalse(TEXT("Unsafe slot paths are rejected before deletion"), FRunCheckpointStorage::DeleteIfUnchanged(TEXT("../ProjectA_Run"), Token, Error));
    FRunCheckpointStorage::FailNextDeleteForTesting();
    TestFalse(TEXT("An injected storage failure leaves surrender retryable"), Fixture.Run->SurrenderStandaloneSavedRun(Token, Error));
    TestFalse(TEXT("Delete failure provides an actionable error"), Error.IsEmpty());
    TestTrue(TEXT("Failed deletion preserves every runtime value file and persistence flag"), Fixture.ReadBytes() == BeforeDisk && Fixture.CaptureRuntime() == BeforeRuntime && Fixture.Run->GetSaveError().EqualTo(BeforeError) && Fixture.Run->IsCheckpointSavingEnabled() && Events == 0);
#if PLATFORM_WINDOWS
    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Fixture.Slot + TEXT(".sav")));
    HANDLE Reader = ::CreateFileW(*Path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!TestTrue(TEXT("A reader can hold the isolated save without granting delete access"), Reader != INVALID_HANDLE_VALUE)) return false;
    TestFalse(TEXT("An incompatible open handle prevents deletion"), Fixture.Run->SurrenderStandaloneSavedRun(Token, Error));
    ::CloseHandle(Reader);
    TestTrue(TEXT("A sharing violation also preserves disk memory and events"), Fixture.ReadBytes() == BeforeDisk && Fixture.CaptureRuntime() == BeforeRuntime && Events == 0);
    HANDLE DeleteSharingReader = ::CreateFileW(*Path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!TestTrue(TEXT("A reader can hold the isolated save while permitting shared deletion"), DeleteSharingReader != INVALID_HANDLE_VALUE)) return false;
    TestFalse(TEXT("A delete-sharing reader cannot leave surrender pending after reported success"), Fixture.Run->SurrenderStandaloneSavedRun(Token, Error));
    TestTrue(TEXT("A delete-sharing reader rejection preserves disk memory autosave and events"), Fixture.ReadBytes() == BeforeDisk && Fixture.CaptureRuntime() == BeforeRuntime && Fixture.Run->GetSaveError().EqualTo(BeforeError) && Fixture.Run->IsCheckpointSavingEnabled() && Events == 0);
    ::CloseHandle(DeleteSharingReader);
#endif
    TestTrue(TEXT("Retry with the same unchanged confirmation commits surrender"), Fixture.Run->SurrenderStandaloneSavedRun(Token, Error));
    TestTrue(TEXT("Successful deletion clears its error and publishes cleared state once"), Error.IsEmpty() && Fixture.Run->GetSaveError().IsEmpty() && Events == 1 && bPublishedClearedState);
    TestTrue(TEXT("Surrender clears identity encounter progress and combat state"), !Fixture.Run->GetRunIdentity().RunId.IsValid() && Fixture.Run->GetNodes().IsEmpty() && Fixture.Run->GetCompletedNodes().IsEmpty() && Fixture.Run->GetCurrentNodeId().IsNone() && Fixture.Run->GetCurrentEncounterId().IsNone() && Fixture.Run->GetLastResult() == ECombatResult::None && !Fixture.Run->HasCombatCheckpoint());
    TestFalse(TEXT("The surrendered checkpoint no longer exists"), UGameplayStatics::DoesSaveGameExist(Fixture.Slot, 0));
    TestFalse(TEXT("Continue becomes unavailable after surrender"), Fixture.Run->CanContinueStandaloneSavedRun(Error));
    TestFalse(TEXT("An already consumed confirmation cannot publish another surrender"), Fixture.Run->SurrenderStandaloneSavedRun(Token, Error));
    TestEqual(TEXT("Repeated confirmation does not publish another event"), Events, 1);
    TStrongObjectPtr<URunSaveGame> NewSave = Fixture.MakeSave(ERunIdentityOrigin::LocalDevelopment, 1, 1, false);
    if (!TestNotNull(TEXT("A new Run can be prepared after surrender"), NewSave.Get())) return false;
    TestTrue(TEXT("The same slot immediately accepts a new save after the reader closes and surrender succeeds"), FRunCheckpointStorage::Save(NewSave.Get(), Fixture.Slot, Error));
    TestTrue(TEXT("The newly saved Run can continue from the reused slot"), Fixture.Run->CanContinueStandaloneSavedRun(Error));
    TestTrue(TEXT("The other isolated save remains byte-identical"), OtherSlot.ReadBytes() == OtherBytes);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStandaloneSurrenderReplacementTest, "ProjectA.Persistence.StandaloneSurrenderReplacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStandaloneSurrenderReplacementTest::RunTest(const FString& Parameters)
{
    FStandaloneContinueFixture Fixture;
    FText Error;
    TStrongObjectPtr<URunSaveGame> Original = Fixture.MakeSave(ERunIdentityOrigin::LocalDevelopment, 1, 1, false);
    if (!Original || !FRunCheckpointStorage::Save(Original.Get(), Fixture.Slot, Error) || !Fixture.Run->LoadStandaloneCheckpoint(Error)) return false;
    const TArray<uint8> BeforeRuntime = Fixture.CaptureRuntime();
    int32 Events = 0;
    const FDelegateHandle Observer = Fixture.Run->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
    ON_SCOPE_EXIT { Fixture.Run->OnRunStateChanged.Remove(Observer); };
    FString Token;
    if (!Fixture.Run->GetStandaloneSurrenderToken(Token, Error)) return false;
    Original->Party[0].CurrentHP = 75.0f;
    if (!FRunCheckpointStorage::Save(Original.Get(), Fixture.Slot, Error)) return false;
    TArray<uint8> ReplacedBytes = Fixture.ReadBytes();
    TestFalse(TEXT("Even the same Run ID with changed contents invalidates confirmation"), Fixture.Run->SurrenderStandaloneSavedRun(Token, Error));
    TestFalse(TEXT("The storage handle independently rejects changed bytes"), FRunCheckpointStorage::DeleteIfUnchanged(Fixture.Slot, Token, Error));
    TestTrue(TEXT("Changed contents are preserved along with current runtime"), Fixture.ReadBytes() == ReplacedBytes && Fixture.CaptureRuntime() == BeforeRuntime && Events == 0);
    TStrongObjectPtr<URunSaveGame> Cooperative = Fixture.MakeSave(ERunIdentityOrigin::AccountProvider, 2, 2, false);
    if (!Cooperative || !FRunCheckpointStorage::Save(Cooperative.Get(), Fixture.Slot, Error)) return false;
    ReplacedBytes = Fixture.ReadBytes();
    TestFalse(TEXT("A cooperative replacement fails a fresh ownership check"), Fixture.Run->SurrenderStandaloneSavedRun(Token, Error));
    TestTrue(TEXT("Cooperative replacement and local runtime remain untouched"), Fixture.ReadBytes() == ReplacedBytes && Fixture.CaptureRuntime() == BeforeRuntime && Events == 0);
    const FString Path = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Fixture.Slot + TEXT(".sav"));
    const TArray<uint8> CorruptBytes;
    if (!TestTrue(TEXT("The isolated corruption fixture is truncated"), FFileHelper::SaveArrayToFile(CorruptBytes, *Path))) return false;
    TestFalse(TEXT("Corrupted files cannot be discarded through stale confirmation"), Fixture.Run->SurrenderStandaloneSavedRun(Token, Error));
    FString ClearedToken = Token;
    TestFalse(TEXT("Corrupted files cannot produce a fresh token"), Fixture.Run->GetStandaloneSurrenderToken(ClearedToken, Error));
    TestTrue(TEXT("Corruption rejection clears the token without touching disk memory or events"), ClearedToken.IsEmpty() && Fixture.ReadBytes() == CorruptBytes && Fixture.CaptureRuntime() == BeforeRuntime && Events == 0);
    return true;
}

#endif
