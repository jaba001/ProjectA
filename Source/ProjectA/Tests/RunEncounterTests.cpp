#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/RunEncounterPoolDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunCheckpointStorage.h"
#include "Game/Run/RunParticipationLibrary.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    FRunPartyMember MakeShopMember()
    {
        FRunPartyMember Member;
        Member.SlotIndex = 2;
        Member.CharacterName = FText::FromString(TEXT("Shop Archer"));
        Member.ClassId = TEXT("Archer");
        Member.bCreated = true;
        return Member;
    }

    bool WinFirstBattle(URunStateSubsystem* Run)
    {
        if (!Run->BeginEncounter(TEXT("Combat_01")) || !Run->MarkCombatStarted()) return false;
        Run->UpdatePartyMemberHP(2, 73.f);
        return Run->CompleteEncounter(ECombatResult::Victory);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunEncounterFlowTest, "ProjectA.Run.Encounter.ChoicesAndProgression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunEncounterFlowTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    FText Error;
    for (int32 Index = 1; Index <= 3; ++Index)
    {
        TStrongObjectPtr<URunStateSubsystem> Run(NewObject<URunStateSubsystem>(Instance.Get()));
        if (!TestTrue(TEXT("A fresh Run initializes"), Run->InitializeRun({MakeShopMember()}, Error))) return false;
        TestEqual(TEXT("A fresh Run contains three fixed offers"), Run->GetEncounterProgress().Offers.Num(), 3);
        const FName Choice(*FString::Printf(TEXT("Shop_%02d"), Index));
        TestEqual(TEXT("The choice has its numbered shop label"), Run->GetEncounterProgress().Offers[Index - 1].DisplayName.ToString(), FString::Printf(TEXT("상점%d"), Index));
        const FRunIdentityData Identity = Run->GetRunIdentity();
        TestFalse(TEXT("Shops cannot be entered before victory"), Run->SelectRunEncounter(Choice));
        if (!TestTrue(TEXT("Victory reaches the selection screen through Continue"), WinFirstBattle(Run.Get()) && Run->ContinueRun())) return false;
        TestTrue(TEXT("Continue opens encounter choices"), Run->GetPhase() == ERunPhase::EncounterChoice);
        TestFalse(TEXT("The second battle cannot skip the encounter"), Run->BeginEncounter(TEXT("Combat_02")));
        TestFalse(TEXT("An unknown choice is rejected"), Run->SelectRunEncounter(TEXT("Missing")));
        TestFalse(TEXT("A choice cannot be exited before entry"), Run->LeaveRunEncounter());
        TestTrue(TEXT("Each of the three offers is enterable"), Run->SelectRunEncounter(Choice));
        TestTrue(TEXT("Selection opens the shop"), Run->GetPhase() == ERunPhase::Shop);
        TestEqual(TEXT("Selection records the exact offer"), Run->GetEncounterProgress().SelectedEncounterId, Choice);
        TestFalse(TEXT("Another choice cannot replace an entered shop"), Run->SelectRunEncounter(TEXT("Shop_01")));
        TestFalse(TEXT("Continue cannot bypass the shop exit"), Run->ContinueRun());
        TestTrue(TEXT("Shop exit unlocks the next map"), Run->LeaveRunEncounter() && Run->CanStartNode(TEXT("Combat_02")));
        TestFalse(TEXT("Repeated exit is rejected"), Run->LeaveRunEncounter());
        TestFalse(TEXT("Unselected shops cannot be visited afterward"), Run->SelectRunEncounter(TEXT("Shop_03")));
        TestEqual(TEXT("An empty shop preserves HP"), Run->GetPartyMembers()[0].CurrentHP, 73.f);
        TestTrue(TEXT("An empty shop preserves every identity field"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Run->GetRunIdentity(), 0));
        TestTrue(TEXT("The next battle starts"), Run->BeginEncounter(TEXT("Combat_02")) && Run->MarkCombatStarted());
        TestTrue(TEXT("The final victory finishes without another shop"), Run->CompleteEncounter(ECombatResult::Victory) && Run->ContinueRun() && Run->GetPhase() == ERunPhase::Complete);
    }
    TStrongObjectPtr<URunEncounterPoolDataAsset> Pool(NewObject<URunEncounterPoolDataAsset>());
    TArray<FRunEncounterOffer> Offers;
    TestTrue(TEXT("The default pool is valid"), Pool->BuildFixedOffers(Offers, Error));
    Pool->FixedOffers[1].EncounterId = Pool->FixedOffers[0].EncounterId;
    TestFalse(TEXT("Duplicate identifiers are rejected"), Pool->BuildFixedOffers(Offers, Error));
    TestEqual(TEXT("An invalid pool does not replace generated offers"), Offers[1].EncounterId, FName(TEXT("Shop_02")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunEncounterPersistenceTest, "ProjectA.Run.Encounter.PersistenceAndLegacy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunEncounterPersistenceTest::RunTest(const FString& Parameters)
{
    struct FScopedSlot
    {
        FString Name = TEXT("ShopTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        ~FScopedSlot() { UGameplayStatics::DeleteGameInSlot(Name, 0); }
        TArray<uint8> Read() const
        {
            TArray<uint8> Bytes;
            FFileHelper::LoadFileToArray(Bytes, *(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Name + TEXT(".sav"))));
            return Bytes;
        }
    } Slot;
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<URunStateSubsystem> Run(NewObject<URunStateSubsystem>(Instance.Get()));
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Instance.Get()));
    Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    Run->EnableCheckpointSaving(Slot.Name);
    Restored->EnableCheckpointSaving(Slot.Name);
    FText Error;
    if (!TestNotNull(TEXT("Persistence uses the authored party catalog"), Run->PartyDefinition.Get()) || !TestTrue(TEXT("Initial Run is saved"), Run->InitializeRun({MakeShopMember()}, Error) && Run->GetSaveError().IsEmpty())) return false;
    TStrongObjectPtr<URunSaveGame> Legacy(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot.Name, Error)));
    if (!TestTrue(TEXT("Victory opens saved choices"), WinFirstBattle(Run.Get()) && Run->ContinueRun())) return false;
    if (!TestTrue(TEXT("Choices restore without being regenerated"), Restored->LoadStandaloneCheckpoint(Error) && Restored->GetPhase() == ERunPhase::EncounterChoice)) return false;
    TestEqual(TEXT("All offers survive loading"), Restored->GetEncounterProgress().Offers.Num(), 3);
    const TArray<uint8> ChoiceBytes = Slot.Read();
    int32 Events = 0;
    const FDelegateHandle Handle = Restored->OnRunStateChanged.AddLambda([&Events]() { ++Events; });
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Failed shop entry is rejected"), Restored->SelectRunEncounter(TEXT("Shop_03")));
    TestTrue(TEXT("Failed entry preserves choices, selection and file"), Restored->GetPhase() == ERunPhase::EncounterChoice && Restored->GetEncounterProgress().SelectedEncounterId.IsNone() && Slot.Read() == ChoiceBytes);
    TestEqual(TEXT("Failed entry emits no success event"), Events, 0);
    TestTrue(TEXT("Entry can be retried"), Restored->SelectRunEncounter(TEXT("Shop_03")));
    TestTrue(TEXT("An entered shop survives loading"), Run->LoadStandaloneCheckpoint(Error) && Run->GetPhase() == ERunPhase::Shop && Run->GetEncounterProgress().SelectedEncounterId == TEXT("Shop_03"));
    const TArray<uint8> ShopBytes = Slot.Read();
    FRunCheckpointStorage::FailNextWriteForTesting();
    TestFalse(TEXT("Failed exit stays in the shop"), Restored->LeaveRunEncounter());
    TestTrue(TEXT("Failed exit preserves the selected shop and file"), Restored->GetPhase() == ERunPhase::Shop && !Restored->GetEncounterProgress().bCompleted && Slot.Read() == ShopBytes);
    TestEqual(TEXT("Only the successful entry emitted an event"), Events, 1);
    TestTrue(TEXT("Exit can be retried"), Restored->LeaveRunEncounter());
    TestTrue(TEXT("The completed encounter survives loading"), Run->LoadStandaloneCheckpoint(Error) && Run->CanStartNode(TEXT("Combat_02")) && Run->GetEncounterProgress().bCompleted);
    TestFalse(TEXT("Restoring cannot reopen an unchosen shop"), Run->SelectRunEncounter(TEXT("Shop_01")));
    TestEqual(TEXT("Only successful entry and exit emitted events"), Events, 2);
    Restored->OnRunStateChanged.Remove(Handle);
    TStrongObjectPtr<URunSaveGame> Invalid(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot.Name, Error)));
    Invalid->EncounterProgress.SelectedEncounterId = TEXT("Missing");
    TestTrue(TEXT("Malformed fixture is written to the isolated test slot"), FRunCheckpointStorage::Save(Invalid.Get(), Slot.Name, Error));
    TestFalse(TEXT("Unknown saved selection is rejected"), Run->LoadCheckpoint(Error));
    TestTrue(TEXT("Rejected load preserves current progress"), Run->CanStartNode(TEXT("Combat_02")));
    Invalid->EncounterProgress.SelectedEncounterId = TEXT("Shop_03");
    Invalid->EncounterProgress.bCompleted = false;
    FRunCheckpointStorage::Save(Invalid.Get(), Slot.Name, Error);
    TestFalse(TEXT("A map save cannot bypass an unfinished shop"), Run->LoadCheckpoint(Error));
    Legacy->EncounterProgress = FRunEncounterProgress();
    if (!TestTrue(TEXT("Pre-feature defaults remain loadable"), FRunCheckpointStorage::Save(Legacy.Get(), Slot.Name, Error) && Run->LoadStandaloneCheckpoint(Error))) return false;
    TestTrue(TEXT("An old Run preserves its original route without new encounters"), WinFirstBattle(Run.Get()) && Run->ContinueRun() && Run->CanStartNode(TEXT("Combat_02")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunStandaloneControlPersistenceTest, "ProjectA.Run.Encounter.StandaloneControlSelectionPersists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunStandaloneControlPersistenceTest::RunTest(const FString& Parameters)
{
    struct FScopedSlot
    {
        FString Name = TEXT("StandaloneControl_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        ~FScopedSlot() { UGameplayStatics::DeleteGameInSlot(Name, 0); }
    } Slot;
    FRunPartyMember Selected = MakeShopMember();
    Selected.SlotIndex = 3;
    Selected.bPlayerControlled = true;
    FRunPartyMember Companion = MakeShopMember();
    Companion.SlotIndex = 1;
    FRunPartyMember Empty;
    Empty.SlotIndex = 0;
    TArray<FRunPartyMember> Members{Selected, Empty, Companion};
    FText Error;
    int32 SelectedSlot = INDEX_NONE;
    TestTrue(TEXT("Explicit selection can choose a later slot regardless of input order"), URunParticipationLibrary::ResolveStandalonePlayerSlot(Members, SelectedSlot, Error) && SelectedSlot == 3);
    Members[0].CurrentHP = 0.0f;
    TestTrue(TEXT("A dead selected character never transfers control to its living companion"), URunParticipationLibrary::ResolveStandalonePlayerSlot(Members, SelectedSlot, Error) && SelectedSlot == 3);
    Members[2].bPlayerControlled = true;
    SelectedSlot = 42;
    TestFalse(TEXT("Multiple direct-control selections are rejected"), URunParticipationLibrary::ResolveStandalonePlayerSlot(Members, SelectedSlot, Error));
    TestEqual(TEXT("A rejected selection preserves the caller's previous result"), SelectedSlot, 42);
    Members[2].bPlayerControlled = false;
    Members[0].bPlayerControlled = false;
    Members[1].bPlayerControlled = true;
    TestFalse(TEXT("An empty party slot cannot be selected for direct control"), URunParticipationLibrary::ResolveStandalonePlayerSlot(Members, SelectedSlot, Error));
    Members[1].bPlayerControlled = false;
    Members[2].CurrentHP = 0.0f;
    TestTrue(TEXT("An older unselected party keeps its lowest created slot even if that character died"), URunParticipationLibrary::ResolveStandalonePlayerSlot(Members, SelectedSlot, Error) && SelectedSlot == 1);

    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<URunStateSubsystem> Run(NewObject<URunStateSubsystem>(Instance.Get()));
    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Instance.Get()));
    Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
    Run->EnableCheckpointSaving(Slot.Name);
    Restored->EnableCheckpointSaving(Slot.Name);
    if (!TestNotNull(TEXT("Selection persistence uses the saved profession catalog"), Run->PartyDefinition.Get()) || !TestTrue(TEXT("A selected single-player party initializes and saves"), Run->InitializeRun({Selected, Empty, Companion}, Error) && Run->GetSaveError().IsEmpty())) return false;
    const FRunIdentityData Identity = Run->GetRunIdentity();
    const TArray<FRunPartyMember> OriginalParty = Run->GetPartyMembers();
    if (!TestTrue(TEXT("Standalone Continue loads the explicitly selected character"), Restored->LoadStandaloneCheckpoint(Error) && URunParticipationLibrary::ResolveStandalonePlayerSlot(Restored->GetPartyMembers(), SelectedSlot, Error) && SelectedSlot == 3)) return false;
    for (const FRunPartyMember& Member : Restored->GetPartyMembers())
    {
        TestEqual(TEXT("The selection flag survives native SaveGame serialization"), Member.bPlayerControlled, Member.bCreated && Member.SlotIndex == 3);
        if (Member.bCreated) TestTrue(TEXT("AI companions retain the same original local owner"), Member.OwnerAccountId == Identity.HostAccountId);
    }
    if (!TestTrue(TEXT("The selected party enters its first combat"), Restored->BeginEncounter(TEXT("Combat_01")) && Restored->MarkCombatStarted())) return false;
    Restored->UpdatePartyMemberHP(3, 0.0f);
    Restored->UpdatePartyMemberHP(1, 73.0f);
    if (!TestTrue(TEXT("A surviving companion carries the same party through victory and the shop"), Restored->CompleteEncounter(ECombatResult::Victory) && Restored->ContinueRun() && Restored->SelectRunEncounter(TEXT("Shop_02")) && Restored->LeaveRunEncounter())) return false;
    if (!TestTrue(TEXT("The next battle's checkpoint restores after the chosen character died"), Run->LoadStandaloneCheckpoint(Error) && Run->CanStartNode(TEXT("Combat_02")))) return false;
    TestTrue(TEXT("The next encounter keeps the dead selected slot instead of promoting its companion"), URunParticipationLibrary::ResolveStandalonePlayerSlot(Run->GetPartyMembers(), SelectedSlot, Error) && SelectedSlot == 3);
    TestTrue(TEXT("Selection and death preserve the complete Run identity"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Run->GetRunIdentity(), 0));
    for (int32 Index = 0; Index < OriginalParty.Num(); ++Index)
    {
        const FRunPartyMember& Member = Run->GetPartyMembers()[Index];
        TestTrue(TEXT("The next encounter preserves character identity ownership and control selection"), Member.CharacterId == OriginalParty[Index].CharacterId && Member.OwnerAccountId == OriginalParty[Index].OwnerAccountId && Member.bPlayerControlled == OriginalParty[Index].bPlayerControlled);
        if (Member.SlotIndex == 3) TestEqual(TEXT("The selected character remains dead after reload"), Member.CurrentHP, 0.0f);
    }
    TestTrue(TEXT("The AI-only surviving party can enter the second combat"), Run->BeginEncounter(TEXT("Combat_02")) && Run->MarkCombatStarted());

    // Old single-player saves have no explicit flag; restore their stable slot without rewriting ownership.
    // 명시 플래그가 없는 기존 싱글 저장은 소유권을 다시 쓰지 않고 고정 슬롯을 복원합니다.
    TStrongObjectPtr<URunSaveGame> OlderSave(Cast<URunSaveGame>(FRunCheckpointStorage::Load(Slot.Name, Error)));
    if (!TestNotNull(TEXT("The pre-combat map checkpoint remains readable"), OlderSave.Get())) return false;
    for (FRunPartyMember& Member : OlderSave->Party)
    {
        Member.bPlayerControlled = false;
        if (Member.SlotIndex == 1) Member.CurrentHP = 0.0f;
        if (Member.SlotIndex == 3) Member.CurrentHP = 73.0f;
    }
    if (!TestTrue(TEXT("A flag-free historical single-player save remains loadable"), FRunCheckpointStorage::Save(OlderSave.Get(), Slot.Name, Error) && Restored->LoadStandaloneCheckpoint(Error))) return false;
    TestTrue(TEXT("Historical selection stays with the lowest created dead slot"), URunParticipationLibrary::ResolveStandalonePlayerSlot(Restored->GetPartyMembers(), SelectedSlot, Error) && SelectedSlot == 1);
    for (const FRunPartyMember& Member : Restored->GetPartyMembers()) TestEqual(TEXT("Historical selection is normalized to exactly one stored flag"), Member.bPlayerControlled, Member.bCreated && Member.SlotIndex == 1);
    TestTrue(TEXT("Compatibility selection does not replace participant identity"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &Restored->GetRunIdentity(), 0));
    return true;
}

#endif
