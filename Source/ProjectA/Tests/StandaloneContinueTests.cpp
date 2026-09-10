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

namespace
{
    struct FStandaloneContinueFixture
    {
        TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
        TStrongObjectPtr<URunStateSubsystem> Run{NewObject<URunStateSubsystem>(Instance.Get())};
        TStrongObjectPtr<UPartyDefinitionDataAsset> Catalog{LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_VerticalSliceParty.DA_VerticalSliceParty"))};
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
            if (!Catalog || !Catalog->ResolveProfession(TEXT("Hunter"), Profession) || Profession.StartingSkills.IsEmpty() || !Profession.StartingSkills[0]) return TStrongObjectPtr<URunSaveGame>();
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
                Member.ClassId = TEXT("Hunter");
                Member.CharacterName = FText::FromString(FString::Printf(TEXT("Saved Hunter %d"), Index));
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
        {ERunIdentityOrigin::LocalDevelopment, 1, 4, true, true},
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
        TestTrue(Label + TEXT("general eligibility remains available to the network restore path"), Fixture.Run->CanContinueSavedRun(Error));
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
        TestTrue(Label + TEXT("the general network load still accepts the valid save"), Fixture.Run->LoadCheckpoint(Error));
        TestTrue(Label + TEXT("general load preserves the saved checkpoint body"), FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Save->CombatCheckpoint, &Fixture.Run->GetCombatCheckpoint(), 0));
        TestTrue(Label + TEXT("all reads leave file bytes unchanged"), Fixture.ReadBytes() == BeforeDisk);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStandaloneContinueFileReplacementTest, "ProjectA.Persistence.StandaloneContinueFileReplacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FStandaloneContinueFileReplacementTest::RunTest(const FString& Parameters)
{
    FStandaloneContinueFixture Fixture;
    FText Error;
    TStrongObjectPtr<URunSaveGame> Original = Fixture.MakeSave(ERunIdentityOrigin::LocalDevelopment, 1, 1, true);
    if (!TestNotNull(TEXT("A current local combat is available for preservation checks"), Original.Get())) return false;
    int32 Events = 0;
    const FDelegateHandle Observer = Fixture.Run->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
    ON_SCOPE_EXIT { Fixture.Run->OnRunStateChanged.Remove(Observer); };
    for (bool bReplacementCombat : {false, true})
    {
        if (!TestTrue(TEXT("The original local combat is installed"), FRunCheckpointStorage::Save(Original.Get(), Fixture.Slot, Error)) || !TestTrue(TEXT("The current runtime starts from the original local combat"), Fixture.Run->LoadStandaloneCheckpoint(Error))) return false;
        const TArray<uint8> BeforeRuntime = Fixture.CaptureRuntime();
        const FText BeforeSaveError = Fixture.Run->GetSaveError();
        const bool bBeforeSaving = Fixture.Run->IsCheckpointSavingEnabled();
        const int32 BeforeEvents = Events;
        TestTrue(TEXT("The menu initially enables Continue for the local save"), Fixture.Run->CanContinueStandaloneSavedRun(Error));
        TStrongObjectPtr<URunSaveGame> Replaced = Fixture.MakeSave(ERunIdentityOrigin::AccountProvider, 2, 2, bReplacementCombat);
        if (!TestNotNull(TEXT("The replacement is a cooperative native save"), Replaced.Get()) || !TestTrue(TEXT("The file changes after the menu eligibility check"), FRunCheckpointStorage::Save(Replaced.Get(), Fixture.Slot, Error))) return false;
        const TArray<uint8> ReplacedBytes = Fixture.ReadBytes();
        TestTrue(TEXT("The replacement is valid for the general network restore path"), Fixture.Run->CanContinueSavedRun(Error));
        TestFalse(TEXT("Actual menu load rechecks the replacement instead of trusting the enabled button"), Fixture.Run->LoadStandaloneCheckpoint(Error));
        TestFalse(TEXT("The rejected click reports why this session cannot restore the save"), Error.IsEmpty());
        TestTrue(TEXT("Rejected click preserves every current Run field and its persistence status"), Fixture.CaptureRuntime() == BeforeRuntime && Fixture.Run->GetSaveError().EqualTo(BeforeSaveError) && Fixture.Run->IsCheckpointSavingEnabled() == bBeforeSaving);
        TestEqual(TEXT("Rejected click publishes no Run event"), Events, BeforeEvents);
        TestTrue(TEXT("Rejected click preserves the exact replacement file"), Fixture.ReadBytes() == ReplacedBytes);
        TestTrue(TEXT("The same replacement remains available to an explicit network load"), Fixture.Run->LoadCheckpoint(Error));
        TestEqual(TEXT("General load retains the replacement's real Run ID"), Fixture.Run->GetRunIdentity().RunId, Replaced->Identity.RunId);
    }
    if (!TestTrue(TEXT("The local save is restored for a second eligibility check"), FRunCheckpointStorage::Save(Original.Get(), Fixture.Slot, Error)) || !TestTrue(TEXT("The menu enables a local save again"), Fixture.Run->CanContinueStandaloneSavedRun(Error))) return false;
    TStrongObjectPtr<URunSaveGame> NewLocal = Fixture.MakeSave(ERunIdentityOrigin::LocalDevelopment, 1, 2, false);
    if (!TestNotNull(TEXT("The second replacement is a supported local save"), NewLocal.Get()) || !TestTrue(TEXT("A different eligible local save replaces the displayed file"), FRunCheckpointStorage::Save(NewLocal.Get(), Fixture.Slot, Error))) return false;
    TestTrue(TEXT("The menu may load a newly validated eligible file"), Fixture.Run->LoadStandaloneCheckpoint(Error));
    TestEqual(TEXT("Load applies the newly validated object instead of stale menu data"), Fixture.Run->GetRunIdentity().RunId, NewLocal->Identity.RunId);
    return true;
}

#endif
