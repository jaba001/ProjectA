#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "Game/Snapshot/PartySnapshotSaveGame.h"
#include "Game/Run/RunSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include <limits>

namespace
{
    FPartySnapshot MakeStorageTestSnapshot()
    {
        FPartySnapshot Snapshot;
        Snapshot.SnapshotId = TEXT("T14_Opponent");
        Snapshot.ContentVersion = 7;

        FPartySnapshotMember Member;
        Member.MemberId = TEXT("Member_01");
        Member.ClassId = TEXT("Hunter");
        Member.CharacterName = TEXT("저장된 사냥꾼");
        Member.Stats.MaxHP = 185.5f;
        Member.Stats.CurrentHP = 71.25f;
        Member.Stats.MaxActionPoints = 3;
        Member.Stats.MaxSubActionPoints = 2;
        Member.Stats.MoveRange = 4;
        Member.SkillIds = { TEXT("BasicAttack"), TEXT("Sweep") };
        Member.EquipmentIds = { TEXT("Equipment.Bow"), TEXT("Equipment.Ring") };
        Member.TacticsId = TEXT("Tactics.Aggressive");
        Member.FormationSlot = 3;
        Snapshot.Members.Add(Member);

        Member.MemberId = TEXT("Member_02");
        Member.ClassId = TEXT("Scholar");
        Member.CharacterName = TEXT("저장된 학자");
        Member.Stats.CurrentHP = 0.0f;
        Member.SkillIds = { TEXT("BasicAttack") };
        Member.EquipmentIds.Empty();
        Member.TacticsId = NAME_None;
        Member.FormationSlot = 1;
        Snapshot.Members.Add(Member);
        return Snapshot;
    }

    bool AreSnapshotsEqual(const FPartySnapshot& Left, const FPartySnapshot& Right)
    {
        return FPartySnapshot::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
    }

    struct FScopedSnapshotTestSlot
    {
        const FName Id = FName(*(TEXT("T14_Test_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));

        ~FScopedSnapshotTestSlot()
        {
            UGameplayStatics::DeleteGameInSlot(UPartySnapshotLibrary::GetSaveSlotName(Id), 0);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartySnapshotValidationTest, "ProjectA.Snapshot.Validation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPartySnapshotValidationTest::RunTest(const FString& Parameters)
{
    FText Error;
    const FPartySnapshot Valid = MakeStorageTestSnapshot();
    TestTrue(TEXT("Valid build accepts opaque equipment and tactics for later catalog resolution"), UPartySnapshotLibrary::ValidateSnapshot(Valid, Error));
    TestTrue(TEXT("Successful validation clears the error"), Error.IsEmpty());

    FPartySnapshot Invalid = Valid;
    const auto Reject = [this, &Valid, &Invalid, &Error](const TCHAR* Label)
    {
        TestFalse(Label, UPartySnapshotLibrary::ValidateSnapshot(Invalid, Error));
        TestFalse(FString(Label) + TEXT(" explains rejection"), Error.IsEmpty());
        Invalid = Valid;
    };

    Invalid.SchemaVersion = 0;
    Reject(TEXT("Old schema is rejected"));
    Invalid.SchemaVersion = 2;
    Reject(TEXT("Future schema is rejected"));
    Invalid.ContentVersion = 0;
    Reject(TEXT("Invalid content version is rejected"));
    Invalid.SnapshotId = NAME_None;
    Reject(TEXT("Empty snapshot identifier is rejected"));
    Invalid.Members[0].ClassId = TEXT("/Game/Arbitrary.Asset");
    Reject(TEXT("Asset path as identifier is rejected"));
    Invalid.Members.Empty();
    Reject(TEXT("Empty party is rejected"));
    Invalid.Members.SetNum(5);
    Reject(TEXT("Oversized party is rejected"));
    Invalid.Members[1].MemberId = Invalid.Members[0].MemberId;
    Reject(TEXT("Duplicate member is rejected"));
    Invalid.Members[0].MemberId = NAME_None;
    Reject(TEXT("Empty member identifier is rejected"));
    Invalid.Members[0].ClassId = NAME_None;
    Reject(TEXT("Empty class identifier is rejected"));
    Invalid.Members[0].CharacterName = TEXT("   ");
    Reject(TEXT("Whitespace name is rejected"));
    Invalid.Members[0].CharacterName = TEXT("Name\n");
    Reject(TEXT("Control character in name is rejected"));
    Invalid.Members[0].CharacterName = FString::ChrN(65, TEXT('a'));
    Reject(TEXT("Oversized name is rejected"));
    Invalid.Members[1].FormationSlot = Invalid.Members[0].FormationSlot;
    Reject(TEXT("Duplicate formation is rejected"));
    Invalid.Members[0].FormationSlot = -1;
    Reject(TEXT("Negative formation is rejected"));
    Invalid.Members[0].FormationSlot = 4;
    Reject(TEXT("Oversized formation is rejected"));
    Invalid.Members[0].Stats.MaxHP = 0.0f;
    Reject(TEXT("Zero max HP is rejected"));
    Invalid.Members[0].Stats.MaxHP = 1000001.0f;
    Reject(TEXT("Oversized max HP is rejected"));
    Invalid.Members[0].Stats.MaxHP = std::numeric_limits<float>::quiet_NaN();
    Reject(TEXT("NaN max HP is rejected"));
    Invalid.Members[0].Stats.CurrentHP = std::numeric_limits<float>::infinity();
    Reject(TEXT("Infinite current HP is rejected"));
    Invalid.Members[0].Stats.CurrentHP = -1.0f;
    Reject(TEXT("Negative current HP is rejected"));
    Invalid.Members[0].Stats.CurrentHP = Invalid.Members[0].Stats.MaxHP + 1.0f;
    Reject(TEXT("Current HP above max is rejected"));
    Invalid.Members[0].Stats.MaxActionPoints = 0;
    Reject(TEXT("Zero AP is rejected"));
    Invalid.Members[0].Stats.MaxActionPoints = 101;
    Reject(TEXT("Oversized AP is rejected"));
    Invalid.Members[0].Stats.MaxSubActionPoints = -1;
    Reject(TEXT("Negative SubAP is rejected"));
    Invalid.Members[0].Stats.MaxSubActionPoints = 101;
    Reject(TEXT("Oversized SubAP is rejected"));
    Invalid.Members[0].Stats.MoveRange = -1;
    Reject(TEXT("Negative move range is rejected"));
    Invalid.Members[0].Stats.MoveRange = 33;
    Reject(TEXT("Oversized move range is rejected"));
    Invalid.Members[0].SkillIds.Empty();
    Reject(TEXT("Missing skills are rejected"));
    Invalid.Members[0].SkillIds.SetNum(6);
    Reject(TEXT("Oversized skill list is rejected"));
    const FName DuplicateSkillId = Invalid.Members[0].SkillIds[0];
    Invalid.Members[0].SkillIds.Add(DuplicateSkillId);
    Reject(TEXT("Duplicate skills are rejected"));
    Invalid.Members[0].SkillIds[0] = NAME_None;
    Reject(TEXT("Empty skill identifier is rejected"));
    Invalid.Members[0].EquipmentIds.SetNum(17);
    Reject(TEXT("Oversized equipment list is rejected"));
    const FName DuplicateEquipmentId = Invalid.Members[0].EquipmentIds[0];
    Invalid.Members[0].EquipmentIds.Add(DuplicateEquipmentId);
    Reject(TEXT("Duplicate equipment is rejected"));
    Invalid.Members[0].TacticsId = TEXT("../Tactics");
    Reject(TEXT("Invalid tactics path is rejected"));

    FPartySnapshot Boundary = Valid;
    Boundary.Members[0].Stats.MaxHP = 1000000.0f;
    Boundary.Members[0].Stats.CurrentHP = 1000000.0f;
    Boundary.Members[0].Stats.MaxActionPoints = 100;
    Boundary.Members[0].Stats.MaxSubActionPoints = 100;
    Boundary.Members[0].Stats.MoveRange = 32;
    Boundary.Members[1].Stats.MaxSubActionPoints = 0;
    Boundary.Members[1].Stats.MoveRange = 0;
    TestTrue(TEXT("Inclusive stat bounds and zero HP are accepted"), UPartySnapshotLibrary::ValidateSnapshot(Boundary, Error));
    TestTrue(TEXT("Valid input clears an earlier validation error"), Error.IsEmpty());

    TestEqual(TEXT("Opponent slot has separate prefix"), UPartySnapshotLibrary::GetSaveSlotName(TEXT("Local_01")), FString(TEXT("ProjectA_Opponent_Local_01")));
    TestTrue(TEXT("None slot is rejected"), UPartySnapshotLibrary::GetSaveSlotName(NAME_None).IsEmpty());
    TestTrue(TEXT("Traversal slot is rejected"), UPartySnapshotLibrary::GetSaveSlotName(TEXT("../ProjectA_Run")).IsEmpty());
    TestTrue(TEXT("Absolute slot is rejected"), UPartySnapshotLibrary::GetSaveSlotName(TEXT("C:\\ProjectA_Run")).IsEmpty());
    TestTrue(TEXT("Unicode slot is rejected"), UPartySnapshotLibrary::GetSaveSlotName(TEXT("상대")).IsEmpty());
    TestTrue(TEXT("Oversized slot is rejected"), UPartySnapshotLibrary::GetSaveSlotName(FName(*FString::ChrN(65, TEXT('a')))).IsEmpty());
    TestFalse(TEXT("Maximum slot length is accepted"), UPartySnapshotLibrary::GetSaveSlotName(FName(*FString::ChrN(64, TEXT('a')))).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartySnapshotStorageTest, "ProjectA.Snapshot.SaveGameRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPartySnapshotStorageTest::RunTest(const FString& Parameters)
{
    const FScopedSnapshotTestSlot Slot;
    const FString SlotName = UPartySnapshotLibrary::GetSaveSlotName(Slot.Id);
    const FPartySnapshot Original = MakeStorageTestSnapshot();
    FPartySnapshot Restored = Original;
    Restored.SnapshotId = TEXT("PreserveMe");
    const FPartySnapshot BeforeLoad = Restored;
    FText Error;

    TestFalse(TEXT("Missing save load fails"), UPartySnapshotLibrary::LoadSnapshot(Slot.Id, Restored, Error));
    TestFalse(TEXT("Missing save explains failure"), Error.IsEmpty());
    TestTrue(TEXT("Missing load preserves all output fields"), AreSnapshotsEqual(Restored, BeforeLoad));
    TestFalse(TEXT("Invalid slot load fails"), UPartySnapshotLibrary::LoadSnapshot(TEXT("../Run"), Restored, Error));
    TestTrue(TEXT("Invalid slot preserves all output fields"), AreSnapshotsEqual(Restored, BeforeLoad));

    if (!TestTrue(TEXT("Valid snapshot saves through Unreal SaveGame"), UPartySnapshotLibrary::SaveSnapshot(Slot.Id, Original, Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    TestTrue(TEXT("Successful save clears previous error"), Error.IsEmpty());
    TestNotNull(TEXT("Serialized data uses the expected SaveGame class"), Cast<UPartySnapshotSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)));
    if (!TestTrue(TEXT("Snapshot loads from disk"), UPartySnapshotLibrary::LoadSnapshot(Slot.Id, Restored, Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    TestTrue(TEXT("Every snapshot field survives serialization including equipment, tactics and ordered skills"), AreSnapshotsEqual(Restored, Original));
    TestEqual(TEXT("Formation order remains distinct from array order"), Restored.Members[0].FormationSlot, 3);
    TestEqual(TEXT("Unicode character name survives serialization"), Restored.Members[0].CharacterName, FString(TEXT("저장된 사냥꾼")));
    TestEqual(TEXT("Fractional current HP survives serialization"), Restored.Members[0].Stats.CurrentHP, 71.25f);

    FPartySnapshot Invalid = Original;
    Invalid.SchemaVersion = 999;
    TestFalse(TEXT("Invalid write is rejected"), UPartySnapshotLibrary::SaveSnapshot(Slot.Id, Invalid, Error));
    TestFalse(TEXT("Invalid slot write is rejected"), UPartySnapshotLibrary::SaveSnapshot(TEXT("../Run"), Original, Error));
    TestTrue(TEXT("Previous save remains readable after rejected writes"), UPartySnapshotLibrary::LoadSnapshot(Slot.Id, Restored, Error));
    TestTrue(TEXT("Rejected writes preserve the previous file"), AreSnapshotsEqual(Restored, Original));

    UPartySnapshotSaveGame* InvalidSave = Cast<UPartySnapshotSaveGame>(UGameplayStatics::CreateSaveGameObject(UPartySnapshotSaveGame::StaticClass()));
    if (!TestNotNull(TEXT("Invalid save fixture is created"), InvalidSave))
    {
        return false;
    }
    InvalidSave->Snapshot = Invalid;
    TestTrue(TEXT("Unsupported version fixture writes directly"), UGameplayStatics::SaveGameToSlot(InvalidSave, SlotName, 0));
    TestFalse(TEXT("Unsupported version is rejected on read"), UPartySnapshotLibrary::LoadSnapshot(Slot.Id, Restored, Error));
    TestTrue(TEXT("Unsupported version preserves every output field"), AreSnapshotsEqual(Restored, Original));

    InvalidSave->Snapshot = Original;
    InvalidSave->Snapshot.Members[1].FormationSlot = InvalidSave->Snapshot.Members[0].FormationSlot;
    TestTrue(TEXT("Invalid data fixture writes directly"), UGameplayStatics::SaveGameToSlot(InvalidSave, SlotName, 0));
    TestFalse(TEXT("Invalid data is rejected on read"), UPartySnapshotLibrary::LoadSnapshot(Slot.Id, Restored, Error));
    TestTrue(TEXT("Invalid data preserves every output field"), AreSnapshotsEqual(Restored, Original));

    URunSaveGame* WrongSave = Cast<URunSaveGame>(UGameplayStatics::CreateSaveGameObject(URunSaveGame::StaticClass()));
    TestTrue(TEXT("Wrong SaveGame class fixture writes directly"), UGameplayStatics::SaveGameToSlot(WrongSave, SlotName, 0));
    TestFalse(TEXT("Wrong SaveGame class is rejected"), UPartySnapshotLibrary::LoadSnapshot(Slot.Id, Restored, Error));
    TestTrue(TEXT("Wrong SaveGame class preserves every output field"), AreSnapshotsEqual(Restored, Original));
    TestTrue(TEXT("Isolated test save is removed"), UGameplayStatics::DeleteGameInSlot(SlotName, 0));
    TestFalse(TEXT("Test save no longer exists"), UGameplayStatics::DoesSaveGameExist(SlotName, 0));
    return true;
}

#endif
