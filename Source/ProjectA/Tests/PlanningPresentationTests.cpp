#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/Combat/CombatPlanningRefreshState.h"
#include "UI/MainMenu/CharacterPartyDraft.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PresentationTestBlocked, "ProjectA.Test.Presentation.Blocked");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterPartyDraftTest, "ProjectA.UI.PartyDraft.CapacityAndOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterPartyDraftTest::RunTest(const FString& Parameters)
{
    FCharacterPartyDraft Draft;
    const TArray<FName> ExpandedCatalogue = { TEXT("Warrior"), TEXT("Mage"), TEXT("Archer"), TEXT("Rogue"), TEXT("AdditionalProfession") };
    Draft.Reset(ExpandedCatalogue, false);
    TestEqual(TEXT("Adding a profession keeps the fixed party capacity."), Draft.ExportParty().Num(), 4);
    TestFalse(TEXT("An uncreated slot cannot receive direct control."), Draft.SelectControlled(0));
    TestTrue(TEXT("The extra profession is available within an existing party slot."), Draft.SetClass(3, ExpandedCatalogue.Last()) && Draft.Create(3));
    TestEqual(TEXT("The selected extra profession survives export."), Draft.ExportParty()[3].ClassId, ExpandedCatalogue.Last());
    TestTrue(TEXT("The final party slot remains controllable."), Draft.SelectControlled(3));
    TestTrue(TEXT("A second created member can replace direct control."), Draft.Create(0) && Draft.SelectControlled(0));
    TestEqual(TEXT("Only one exported member receives direct control."), Draft.ExportParty().FilterByPredicate([](const FRunPartyMember& Member) { return Member.bPlayerControlled; }).Num(), 1);
    Draft.SetName(0, FText::FromString(TEXT("Draft name")));
    TestTrue(TEXT("Deleting the selected member clears its control selection."), Draft.Clear(0) && Draft.GetControlledSlot() == INDEX_NONE);
    TestTrue(TEXT("Deletion removes the name while preserving the remaining companion."), Draft.GetSlot(0)->CharacterName.IsEmpty() && Draft.IsCreated(3));
    TestFalse(TEXT("A fifth slot cannot be created by a larger profession catalogue."), Draft.Create(4));
    Draft.Reset({ TEXT("OnlyProfession") }, false);
    TestEqual(TEXT("A smaller catalogue also preserves four party slots."), Draft.ExportParty().Num(), 4);
    TestTrue(TEXT("Reopening starts a clean draft with valid defaults in every slot."), Draft.ExportParty().ContainsByPredicate([](const FRunPartyMember& Member) { return Member.SlotIndex == 3 && Member.ClassId == TEXT("OnlyProfession") && !Member.bCreated && Member.CharacterName.IsEmpty(); }));
    Draft.Reset({}, true);
    TestFalse(TEXT("An empty catalogue cannot create a character without a profession."), Draft.Create(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatPlanningObservationTest, "ProjectA.UI.Planning.IndependentReplicationInvalidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatPlanningObservationTest::RunTest(const FString& Parameters)
{
    FCombatPlanningRefreshState Displayed;
    Displayed.View.CombatId = FGuid::NewGuid();
    Displayed.View.Phase = ECombatRoundPhase::Planning;
    Displayed.View.PlanRevision = 7;
    Displayed.View.Units.AddDefaulted_GetRef().SkillIds.Add(TEXT("DelayedSkill"));
    Displayed.Units.AddDefaulted_GetRef().bAlive = true;
    Displayed.Tiles.AddDefaulted_GetRef().Coord = FIntPoint(1, 0);
    Displayed.bActivated = true;
    Displayed.bInputEnabled = true;
    Displayed.OwnerSlot = 1;
    TestTrue(TEXT("An unchanged planning observation reuses the displayed result."), Displayed == FCombatPlanningRefreshState(Displayed));

    // Independent replication can change query inputs without advancing PlanRevision.
    // 독립적인 복제는 PlanRevision 증가 없이도 조회 입력을 변경할 수 있습니다.
    FCombatPlanningRefreshState Incoming = Displayed;
    Incoming.Skills.AddDefaulted_GetRef().SkillId = TEXT("DelayedSkill");
    TestFalse(TEXT("A skill catalogue arriving after unit skill IDs invalidates the display."), Displayed == Incoming);
    Incoming = Displayed;
    Incoming.Units[0].SAP = 1;
    TestFalse(TEXT("A separately replicated SAP change invalidates movement queries."), Displayed == Incoming);
    Incoming = Displayed;
    Incoming.Units[0].MoveRange = 3;
    TestFalse(TEXT("An independently replicated movement range invalidates reachable tiles at the same plan revision."), Displayed == Incoming);
    Incoming = Displayed;
    Incoming.Units[0].Tags.AddTag(TAG_PresentationTestBlocked);
    TestFalse(TEXT("A GAS tag change invalidates allowed skills even at the same plan revision."), Displayed == Incoming);
    Incoming = Displayed;
    Incoming.Units[0].BlockedAbilityTags.AddTag(TAG_PresentationTestBlocked);
    TestFalse(TEXT("Ability block tags invalidate skill eligibility independently of owned tags."), Displayed == Incoming);
    Incoming = Displayed;
    Incoming.Units[0].bAlive = false;
    TestFalse(TEXT("Death arriving before the round view invalidates input eligibility."), Displayed == Incoming);
    Incoming = Displayed;
    Incoming.Tiles[0].TileCoord = FIntPoint(2, 0);
    TestFalse(TEXT("A late grid index update invalidates cached movement destinations."), Displayed == Incoming);
    Incoming = Displayed;
    Incoming.bRequestPending = true;
    TestFalse(TEXT("A pending request disables commands without requiring a new server revision."), Displayed == Incoming);
    Incoming = Displayed;
    Incoming.SelectedTarget = 9;
    TestFalse(TEXT("A new target invalidates the skill eligibility view."), Displayed == Incoming);
    Incoming = Displayed;
    Incoming.bActivated = false;
    TestFalse(TEXT("Deactivation invalidates highlights even when the round is unchanged."), Displayed == Incoming);
    return true;
}

#endif
