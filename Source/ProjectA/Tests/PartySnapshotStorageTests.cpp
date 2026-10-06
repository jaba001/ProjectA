#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Checkpoint/CombatCheckpointTypes.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "Game/Snapshot/PartySnapshotSaveGame.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/TargetRunTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Unit/UnitDataRules.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
    // Author legacy tagged bytes without changing reflected property flags or production asset data.
    // 반사 프로퍼티 플래그나 원본 에셋을 바꾸지 않고 이전 형식의 태그 바이트를 작성합니다.
    class FSpeedMigrationArchive : public FObjectAndNameAsStringProxyArchive
    {
    public:
        FSpeedMigrationArchive(FArchive& Inner, FName SkippedProperty, bool bWriteLegacy, bool bSaveGame) : FObjectAndNameAsStringProxyArchive(Inner, true), SkippedPropertyName(SkippedProperty)
        {
            if (bWriteLegacy) ArPortFlags |= PPF_UseDeprecatedProperties;
            ArIsSaveGame = bSaveGame;
        }

        virtual bool ShouldSkipProperty(const FProperty* Property) const override
        {
            return Property->GetFName() == SkippedPropertyName || FObjectAndNameAsStringProxyArchive::ShouldSkipProperty(Property);
        }

        virtual FArchive& operator<<(FName& Name) override
        {
            if (IsSaving()) SerializedNames.Add(Name);
            return FNameAsStringProxyArchive::operator<<(Name);
        }

        TSet<FName> SerializedNames;

    private:
        FName SkippedPropertyName;
    };

    FPartySnapshot MakeStorageTestSnapshot()
    {
        FPartySnapshot Snapshot;
        Snapshot.SnapshotId = TEXT("T14_Opponent");
        Snapshot.ContentVersion = 7;

        FPartySnapshotMember Member;
        Member.MemberId = TEXT("Member_01");
        Member.ClassId = TEXT("Archer");
        Member.CharacterName = TEXT("저장된 궁수");
        Member.Stats.MaxHP = 185.5f;
        Member.Stats.CurrentHP = 71.25f;
        Member.Stats.Speed = 21.25f;
        Member.Stats.MaxActionPoints = 3;
        Member.Stats.MaxSubActionPoints = 2;
        Member.Stats.MoveRange = 4;
        Member.SkillIds = { TEXT("BasicAttack"), TEXT("Sweep") };
        Member.EquipmentIds = { TEXT("Equipment.Bow"), TEXT("Equipment.Ring") };
        Member.TacticsId = TEXT("Tactics.Aggressive");
        Member.FormationSlot = 3;
        Snapshot.Members.Add(Member);

        Member.MemberId = TEXT("Member_02");
        Member.ClassId = TEXT("Mage");
        Member.CharacterName = TEXT("저장된 마법사");
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
    const FPartySnapshotStats DefaultStats;
    TestEqual(TEXT("Speed defaults to ten when serialized data supplies no value"), DefaultStats.Speed, 10.0f);

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
    for (float Value : {-1.0f, 1000001.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
    {
        Invalid.Members[0].Stats.Speed = Value;
        Reject(*FString::Printf(TEXT("Invalid speed %g is rejected"), Value));
    }
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
    for (FName DeletedId : {FName(TEXT("SweepingStrike")), FName(TEXT("AOE")), FName(TEXT("RangedAttack"))})
    {
        Invalid.Members[0].SkillIds[0] = DeletedId;
        Reject(*FString::Printf(TEXT("Removed prototype %s cannot be authored into a new snapshot"), *DeletedId.ToString()));
    }
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
    Boundary.Members[0].Stats.Speed = 1000000.0f;
    Boundary.Members[0].Stats.MaxActionPoints = 100;
    Boundary.Members[0].Stats.MaxSubActionPoints = 100;
    Boundary.Members[0].Stats.MoveRange = 32;
    Boundary.Members[1].Stats.MaxSubActionPoints = 0;
    Boundary.Members[1].Stats.MoveRange = 0;
    Boundary.Members[1].Stats.Speed = 0.0f;
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
    TestEqual(TEXT("Unicode character name survives serialization"), Restored.Members[0].CharacterName, FString(TEXT("저장된 궁수")));
    TestEqual(TEXT("Fractional current HP survives serialization"), Restored.Members[0].Stats.CurrentHP, 71.25f);
    TestEqual(TEXT("Authored speed survives serialization instead of reverting to ten"), Restored.Members[0].Stats.Speed, 21.25f);

    FPartySnapshot Invalid = Original;
    Invalid.SchemaVersion = 999;
    TestFalse(TEXT("Invalid write is rejected"), UPartySnapshotLibrary::SaveSnapshot(Slot.Id, Invalid, Error));
    FPartySnapshot InvalidStats = Original;
    InvalidStats.Members[0].Stats.Speed = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Invalid speed is rejected before replacing a valid save"), UPartySnapshotLibrary::SaveSnapshot(Slot.Id, InvalidStats, Error));
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

    InvalidSave->Snapshot = Original;
    InvalidSave->Snapshot.Members[0].Stats.Speed = std::numeric_limits<float>::quiet_NaN();
    TestTrue(TEXT("Corrupted speed fixture writes directly"), UGameplayStatics::SaveGameToSlot(InvalidSave, SlotName, 0));
    TestFalse(TEXT("Corrupted speed is rejected on read"), UPartySnapshotLibrary::LoadSnapshot(Slot.Id, Restored, Error));
    TestTrue(TEXT("Rejected speed preserves every output field"), AreSnapshotsEqual(Restored, Original));

    URunSaveGame* WrongSave = Cast<URunSaveGame>(UGameplayStatics::CreateSaveGameObject(URunSaveGame::StaticClass()));
    TestTrue(TEXT("Wrong SaveGame class fixture writes directly"), UGameplayStatics::SaveGameToSlot(WrongSave, SlotName, 0));
    TestFalse(TEXT("Wrong SaveGame class is rejected"), UPartySnapshotLibrary::LoadSnapshot(Slot.Id, Restored, Error));
    TestTrue(TEXT("Wrong SaveGame class preserves every output field"), AreSnapshotsEqual(Restored, Original));
    TestTrue(TEXT("Isolated test save is removed"), UGameplayStatics::DeleteGameInSlot(SlotName, 0));
    TestFalse(TEXT("Test save no longer exists"), UGameplayStatics::DoesSaveGameExist(SlotName, 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartySnapshotRemovedSkillMigrationTest, "ProjectA.Snapshot.RemovedSkillMigration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPartySnapshotRemovedSkillMigrationTest::RunTest(const FString& Parameters)
{
    const FScopedSnapshotTestSlot Slot;
    const FString SlotName = UPartySnapshotLibrary::GetSaveSlotName(Slot.Id);
    const FString Path = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (SlotName + TEXT(".sav"));
    FPartySnapshot Original = MakeStorageTestSnapshot();
    Original.Members[0].SkillIds = {TEXT("DefaultAttack"), TEXT("SweepingStrike"), TEXT("AOE"), TEXT("RangedAttack"), TEXT("Whirlwind")};
    Original.Members[1].SkillIds.Add(TEXT("Sweep"));
    Original.Members[0].Appearance.BodyId = TEXT("Body_01");
    Original.Members[0].Appearance.ItemIds = {TEXT("Armor_01")};
    UPartySnapshotSaveGame* Save = Cast<UPartySnapshotSaveGame>(UGameplayStatics::CreateSaveGameObject(UPartySnapshotSaveGame::StaticClass()));
    if (!TestNotNull(TEXT("Legacy snapshot fixture is created"), Save)) return false;
    Save->Snapshot = Original;
    if (!TestTrue(TEXT("Legacy snapshot writes before migration through the raw SaveGame API"), UGameplayStatics::SaveGameToSlot(Save, SlotName, 0))) return false;
    TArray<uint8> OriginalBytes;
    if (!TestTrue(TEXT("Legacy snapshot bytes are captured"), FFileHelper::LoadFileToArray(OriginalBytes, *Path))) return false;
    FPartySnapshot Expected = Original;
    Expected.Members[0].SkillIds = {TEXT("DefaultAttack"), TEXT("Whirlwind")};
    FPartySnapshot Restored;
    FText Error;
    if (!TestTrue(TEXT("Loading removes only the deleted skill from the candidate"), UPartySnapshotLibrary::LoadSnapshot(Slot.Id, Restored, Error))) return false;
    TestTrue(TEXT("Stats, names, versions, formation, appearance, equipment, tactics and remaining skill order are preserved"), AreSnapshotsEqual(Restored, Expected));
    TArray<uint8> CurrentBytes;
    TestTrue(TEXT("Snapshot bytes remain readable after migration"), FFileHelper::LoadFileToArray(CurrentBytes, *Path));
    TestTrue(TEXT("Loading never rewrites the original legacy snapshot file"), CurrentBytes == OriginalBytes);
    for (FName DeletedId : {FName(TEXT("SweepingStrike")), FName(TEXT("AOE")), FName(TEXT("RangedAttack"))})
    {
        FPartySnapshot NewInput = Expected;
        NewInput.Members[0].SkillIds.Add(DeletedId);
        TestFalse(TEXT("Saving a new snapshot cannot reintroduce any deleted prototype"), UPartySnapshotLibrary::SaveSnapshot(Slot.Id, NewInput, Error));
        TestFalse(TEXT("Each rejected deleted-skill write explains the failure"), Error.IsEmpty());
    }
    CurrentBytes.Reset();
    TestTrue(TEXT("Snapshot bytes remain readable after a rejected write"), FFileHelper::LoadFileToArray(CurrentBytes, *Path));
    TestTrue(TEXT("A rejected write leaves the legacy snapshot file unchanged"), CurrentBytes == OriginalBytes);
    for (FName DeletedId : {FName(TEXT("SweepingStrike")), FName(TEXT("AOE")), FName(TEXT("RangedAttack"))})
    {
        Save->Snapshot = Original;
        Save->Snapshot.Members[0].SkillIds = {DeletedId};
        if (!TestTrue(TEXT("Each deleted-only legacy fixture writes directly"), UGameplayStatics::SaveGameToSlot(Save, SlotName, 0))) return false;
        TArray<uint8> RejectedBytes;
        if (!TestTrue(TEXT("The deleted-only original bytes are captured"), FFileHelper::LoadFileToArray(RejectedBytes, *Path))) return false;
        FPartySnapshot FallbackExpected = Original;
        FallbackExpected.Members[0].SkillIds = {TEXT("DefaultAttack")};
        TestTrue(TEXT("Removing the only retired skill restores the existing unarmed alias"), UPartySnapshotLibrary::LoadSnapshot(Slot.Id, Restored, Error));
        TestTrue(TEXT("The unarmed fallback preserves every other snapshot field"), AreSnapshotsEqual(Restored, FallbackExpected));
        TestTrue(TEXT("A valid fallback clears the load explanation"), Error.IsEmpty());
        CurrentBytes.Reset();
        TestTrue(TEXT("Rejected migration leaves the original file readable"), FFileHelper::LoadFileToArray(CurrentBytes, *Path));
        TestTrue(TEXT("Rejected migration never rewrites deleted-only original bytes"), CurrentBytes == RejectedBytes);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacySpeedSerializationTest, "ProjectA.Snapshot.LegacySpeedAndGrowthTaggedSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLegacySpeedSerializationTest::RunTest(const FString& Parameters)
{
    const auto CheckMigration = [this](auto Original, FName LegacyName, FName CurrentName, bool bSaveGame)
    {
        using FValue = decltype(Original);
        UScriptStruct* Type = FValue::StaticStruct();
        const FFloatProperty* Legacy = FindFProperty<FFloatProperty>(Type, LegacyName);
        const FFloatProperty* Current = FindFProperty<FFloatProperty>(Type, CurrentName);
        if (!TestTrue(TEXT("Only the legacy field carries the deprecated serialization flag"), Legacy && Current && Legacy->HasAnyPropertyFlags(CPF_Deprecated) && !Current->HasAnyPropertyFlags(CPF_Deprecated))) return false;
        for (float Value : {0.0f, 21.25f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            Legacy->SetPropertyValue_InContainer(&Original, Value);
            Current->SetPropertyValue_InContainer(&Original, 45.0f);
            TArray<uint8> Bytes;
            {
                FMemoryWriter Writer(Bytes);
                FSpeedMigrationArchive Archive(Writer, CurrentName, true, bSaveGame);
                Type->SerializeItem(Archive, &Original, nullptr);
                if (!TestTrue(TEXT("The legacy payload contains the old tag and excludes the new speed tag"), !Archive.IsError() && Archive.SerializedNames.Contains(LegacyName) && !Archive.SerializedNames.Contains(CurrentName))) return false;
            }
            FValue Restored;
            {
                FMemoryReader Reader(Bytes);
                FSpeedMigrationArchive Archive(Reader, NAME_None, false, bSaveGame);
                Type->SerializeItem(Archive, &Restored, nullptr);
                const float Loaded = Current->GetPropertyValue_InContainer(&Restored);
                TestTrue(TEXT("Legacy tagged loading preserves fractional zero and invalid speed values without silently resetting them"), !Archive.IsError() && (FMath::IsNaN(Value) ? FMath::IsNaN(Loaded) : Loaded == Value));
                TestEqual(TEXT("The migrated legacy field resets to its internal sentinel"), Legacy->GetPropertyValue_InContainer(&Restored), -MAX_flt);
                TestEqual(TEXT("Migrated invalid values remain invalid for the normal speed validators"), UnitDataRules::IsValidSpeed(Loaded), FMath::IsFinite(Value) && Value >= 0.0f);
                Current->SetPropertyValue_InContainer(&Restored, 7.25f);
                Restored.PostSerialize(Archive);
                TestEqual(TEXT("A later load callback cannot reapply already migrated data"), Current->GetPropertyValue_InContainer(&Restored), 7.25f);
            }
            Legacy->SetPropertyValue_InContainer(&Restored, 99.0f);
            Bytes.Reset();
            {
                FMemoryWriter Writer(Bytes);
                FSpeedMigrationArchive Archive(Writer, NAME_None, false, bSaveGame);
                Type->SerializeItem(Archive, &Restored, nullptr);
                if (!TestTrue(TEXT("New tagged saves contain speed and omit the retired field even when its memory is populated"), !Archive.IsError() && Archive.SerializedNames.Contains(CurrentName) && !Archive.SerializedNames.Contains(LegacyName))) return false;
            }
            FValue NewSave;
            FMemoryReader Reader(Bytes);
            FSpeedMigrationArchive Archive(Reader, NAME_None, false, bSaveGame);
            Type->SerializeItem(Archive, &NewSave, nullptr);
            TestTrue(TEXT("A new save restores the explicit fractional speed without a legacy override"), !Archive.IsError() && Current->GetPropertyValue_InContainer(&NewSave) == 7.25f && Legacy->GetPropertyValue_InContainer(&NewSave) == -MAX_flt);
        }
        return true;
    };
    return CheckMigration(FPartySnapshotStats(), TEXT("Dexterity"), TEXT("Speed"), true) && CheckMigration(FCombatCheckpointUnit(), TEXT("Dexterity"), TEXT("Speed"), true) && CheckMigration(FTargetRunGroup(), TEXT("AttributeGrowth"), TEXT("SpeedGrowth"), false);
}

#endif
