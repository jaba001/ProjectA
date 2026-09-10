#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/GameplayAbility.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/GameInstance.h"
#include "Game/Run/RunIdentityLibrary.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunStateSubsystem.h"
#include "Game/Snapshot/PartySnapshotSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"

namespace
{
    struct FRunIdentityFixture
    {
        FRunIdentityData Identity;
        TArray<FRunPartyMember> Party;

        FRunIdentityFixture()
        {
            Identity.Origin = ERunIdentityOrigin::AccountProvider;
            Identity.RunId = FGuid::NewGuid();
            Identity.HostEpoch = 1;
            for (int32 Index = 0; Index < 2; ++Index)
            {
                FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
                Participant.AccountId.Provider = TEXT("FixtureProvider");
                Participant.AccountId.Subject = Index == 0 ? TEXT("OwnerA") : TEXT("ownera");
                Participant.AIConsent = Index == 0 ? ERunAIConsent::Granted : ERunAIConsent::Declined;
                Participant.ConsentPolicyVersion = 1;

                FRunPartyMember& Member = Party.AddDefaulted_GetRef();
                Member.SlotIndex = Index == 0 ? 3 : 1;
                Member.CharacterName = FText::FromString(Index == 0 ? TEXT("Original Hunter") : TEXT("Original Scholar"));
                Member.ClassId = Index == 0 ? TEXT("Hunter") : TEXT("Scholar");
                Member.bCreated = true;
                Member.CharacterId = FGuid::NewGuid();
                Member.OwnerAccountId = Participant.AccountId;
            }
            Identity.HostAccountId = Identity.OriginalParticipants[0].AccountId;
            Party.AddDefaulted_GetRef().SlotIndex = 0;
        }
    };

    struct FScopedIdentityRun
    {
        TStrongObjectPtr<UGameInstance> Instance{NewObject<UGameInstance>()};
        TStrongObjectPtr<URunStateSubsystem> Run{NewObject<URunStateSubsystem>(Instance.Get())};
        FString Slot = TEXT("T14_Identity_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);

        FScopedIdentityRun()
        {
            Run->PartyDefinition = LoadObject<UPartyDefinitionDataAsset>(nullptr, TEXT("/Game/User_JeHoon/Blueprint/DataAsset/DA_VerticalSliceParty.DA_VerticalSliceParty"));
            Run->EnableCheckpointSaving(Slot);
        }

        ~FScopedIdentityRun()
        {
            UGameplayStatics::DeleteGameInSlot(Slot, 0);
        }
    };

    FRunIdentityFixture MakeNumberedIdentity()
    {
        FRunIdentityFixture Fixture;
        Fixture.Identity.SchemaVersion = URunIdentityLibrary::CurrentSchemaVersion;
        Fixture.Party.Pop();
        for (int32 Index = 0; Index < 4; ++Index)
        {
            if (Index >= 2)
            {
                FRunParticipantData Participant;
                Participant.AccountId.Provider = TEXT("FixtureProvider");
                Participant.AccountId.Subject = FString::Printf(TEXT("NumberedOwner%d"), Index + 1);
                Fixture.Identity.OriginalParticipants.Add(Participant);
                FRunPartyMember Member = Fixture.Party[0];
                Member.SlotIndex = Index == 2 ? 0 : 2;
                Member.CharacterId = FGuid::NewGuid();
                Member.OwnerAccountId = Participant.AccountId;
                Fixture.Party.Add(Member);
            }
            Fixture.Identity.OriginalParticipants[Index].JoinOrdinal = Index + 1;
        }
        return Fixture;
    }

    bool MakeIdentityCheckpoint(URunStateSubsystem* Run, FCombatCheckpointData& Checkpoint)
    {
        Checkpoint.AttemptId = FGuid::NewGuid();
        Checkpoint.Revision = 1;
        Checkpoint.Identity = Run->GetRunIdentity();
        Checkpoint.NodeId = Run->GetCurrentNodeId();
        Checkpoint.EncounterId = Run->GetCurrentEncounterId();
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
            Unit.HP = Profession.MaxHP;
            Unit.MaxHP = Profession.MaxHP;
            Unit.GridCoord = FIntPoint(Member.SlotIndex, 0);
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
        Enemy.UnitClass = FSoftObjectPath(AEnemyUnit::StaticClass());
        Enemy.GridCoord = FIntPoint(0, 2);
        Checkpoint.Units.Add(Enemy);
        return true;
    }

    bool SameIdentity(const FRunIdentityData& Left, const FRunIdentityData& Right)
    {
        return FRunIdentityData::StaticStruct()->CompareScriptStruct(&Left, &Right, 0);
    }

    bool SameParty(const TArray<FRunPartyMember>& Left, const TArray<FRunPartyMember>& Right)
    {
        if (Left.Num() != Right.Num())
        {
            return false;
        }
        for (int32 Index = 0; Index < Left.Num(); ++Index)
        {
            if (!FRunPartyMember::StaticStruct()->CompareScriptStruct(&Left[Index], &Right[Index], 0))
            {
                return false;
            }
        }
        return true;
    }

    // Exclude new properties from the serialized bytes, reproducing a genuinely metadata-free v1 save.
    // 새 프로퍼티를 저장 바이트에서 제외하여 실제로 식별 정보가 없는 v1 저장을 재현합니다.
    class FLegacyIdentityArchive : public FObjectAndNameAsStringProxyArchive
    {
    public:
        explicit FLegacyIdentityArchive(FArchive& Inner) : FObjectAndNameAsStringProxyArchive(Inner, false)
        {
        }

        virtual bool ShouldSkipProperty(const FProperty* Property) const override
        {
            const FName Name = Property->GetFName();
            if (Name == TEXT("Identity") || Name == TEXT("CharacterId") || Name == TEXT("OwnerAccountId"))
            {
                Skipped.Add(Name);
                return true;
            }
            return FObjectAndNameAsStringProxyArchive::ShouldSkipProperty(Property);
        }

        mutable TSet<FName> Skipped;
    };

    bool WriteLegacyIdentityFixture(URunSaveGame* Save, const FString& Slot)
    {
        Save->Version = 1;
        TArray<uint8> Bytes;
        if (!UGameplayStatics::SaveGameToMemory(Save, Bytes))
        {
            return false;
        }
        const int64 HeaderSize = UGameplayStatics::StripSaveGameHeader(Bytes).Tell();
        Bytes.SetNum(HeaderSize);
        FMemoryWriter Writer(Bytes, true);
        Writer.Seek(HeaderSize);
        FLegacyIdentityArchive Archive(Writer);
        Save->Serialize(Archive);
        return Archive.Skipped.Num() == 3 && !Archive.IsError() && UGameplayStatics::SaveDataToSlot(Bytes, Slot, 0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunIdentityValidationTest, "ProjectA.Run.Identity.Validation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRunIdentityValidationTest::RunTest(const FString& Parameters)
{
    const FRunIdentityFixture Valid;
    FText Error;
    TestTrue(TEXT("Two owners with case-distinct subjects are valid stored identities"), URunIdentityLibrary::ValidateIdentity(Valid.Identity, Valid.Party, Error));
    TestTrue(TEXT("Original owner is recognized"), URunIdentityLibrary::IsOriginalParticipant(Valid.Identity, Valid.Identity.HostAccountId));
    TestTrue(TEXT("Original owner owns their character"), URunIdentityLibrary::IsCharacterOwner(Valid.Identity, Valid.Party, Valid.Party[0].CharacterId, Valid.Identity.HostAccountId));
    TestFalse(TEXT("Host has no ownership exception for another character"), URunIdentityLibrary::IsCharacterOwner(Valid.Identity, Valid.Party, Valid.Party[1].CharacterId, Valid.Identity.HostAccountId));
    TestTrue(TEXT("Case-sensitive account subjects remain distinct"), Valid.Identity.OriginalParticipants[0].AccountId != Valid.Identity.OriginalParticipants[1].AccountId);
    FRunAccountId Stranger = Valid.Identity.HostAccountId;
    Stranger.Subject = TEXT("ReplacementPlayer");
    TestFalse(TEXT("Replacement participant is not in the original roster"), URunIdentityLibrary::IsOriginalParticipant(Valid.Identity, Stranger));
    TestFalse(TEXT("Replacement participant cannot claim a character"), URunIdentityLibrary::IsCharacterOwner(Valid.Identity, Valid.Party, Valid.Party[0].CharacterId, Stranger));

    FRunIdentityFixture Invalid = Valid;
    const auto Reject = [this, &Valid, &Invalid, &Error](const TCHAR* Label)
    {
        TestFalse(Label, URunIdentityLibrary::ValidateIdentity(Invalid.Identity, Invalid.Party, Error));
        TestFalse(FString(Label) + TEXT(" provides an error"), Error.IsEmpty());
        Invalid = Valid;
    };
    Invalid.Identity.SchemaVersion = 3;
    Reject(TEXT("Unknown identity schema is rejected"));
    Invalid.Identity.RunId.Invalidate();
    TestFalse(TEXT("Malformed Run cannot report a participant match"), URunIdentityLibrary::IsOriginalParticipant(Invalid.Identity, Invalid.Identity.HostAccountId));
    Reject(TEXT("Missing Run ID is rejected"));
    Invalid.Identity.OriginalParticipants.Empty();
    Reject(TEXT("Missing original roster is rejected"));
    Invalid.Identity.OriginalParticipants.SetNum(5);
    Reject(TEXT("More than four original participants are rejected"));
    Invalid.Identity.OriginalParticipants[1] = Invalid.Identity.OriginalParticipants[0];
    TestFalse(TEXT("Duplicate roster cannot report a participant match"), URunIdentityLibrary::IsOriginalParticipant(Invalid.Identity, Invalid.Identity.HostAccountId));
    Reject(TEXT("Duplicate original participants are rejected"));
    Invalid.Identity.OriginalParticipants[0].AccountId.Subject.Empty();
    Reject(TEXT("Partial account ID is rejected"));
    Invalid.Identity.HostAccountId = Stranger;
    Reject(TEXT("Host outside the original roster is rejected"));
    Invalid.Identity.HostEpoch = 0;
    Reject(TEXT("Identified Run requires a positive Host epoch"));
    Invalid.Identity.Origin = static_cast<ERunIdentityOrigin>(255);
    Reject(TEXT("Unknown identity origin is rejected"));
    Invalid.Identity.OriginalParticipants[0].AIConsent = ERunAIConsent::Unknown;
    Reject(TEXT("Unknown consent cannot carry a granted-policy version"));
    Invalid.Identity.OriginalParticipants[0].ConsentPolicyVersion = 0;
    Reject(TEXT("Explicit consent requires its policy version"));
    Invalid.Identity.OriginalParticipants[0].AIConsent = static_cast<ERunAIConsent>(255);
    Reject(TEXT("Unknown consent enum is rejected"));
    Invalid.Party[0].CharacterId.Invalidate();
    Reject(TEXT("Created character requires an ID"));
    Invalid.Party[1].CharacterId = Invalid.Party[0].CharacterId;
    Reject(TEXT("Duplicate character IDs are rejected"));
    Invalid.Party[0].OwnerAccountId = Stranger;
    Reject(TEXT("Character owner must be an original participant"));
    Invalid.Party[1].OwnerAccountId = Invalid.Party[0].OwnerAccountId;
    Reject(TEXT("Every original participant must own a created character"));
    Invalid.Party[2].CharacterId = FGuid::NewGuid();
    Reject(TEXT("Empty slot cannot carry a character ID"));
    Invalid.Party[2].OwnerAccountId = Invalid.Identity.HostAccountId;
    Reject(TEXT("Empty slot cannot carry an owner"));
    Invalid.Party[1].SlotIndex = Invalid.Party[0].SlotIndex;
    Reject(TEXT("Duplicate formation slots are rejected"));
    Invalid.Party[0].SlotIndex = 4;
    Reject(TEXT("Out-of-range party slot is rejected"));
    Invalid.Identity.Origin = ERunIdentityOrigin::LocalDevelopment;
    Reject(TEXT("Local development identity cannot use an account-provider namespace"));
    Invalid.Identity.OriginalParticipants[0].AccountId.Provider = TEXT("Development");
    Reject(TEXT("Account-provider identity cannot use the reserved development namespace"));

    FRunIdentityFixture Four;
    Four.Party.Pop();
    for (int32 Index = 2; Index < 4; ++Index)
    {
        FRunParticipantData Participant;
        Participant.AccountId.Provider = TEXT("FixtureProvider");
        Participant.AccountId.Subject = FString::Printf(TEXT("Owner%d"), Index);
        Four.Identity.OriginalParticipants.Add(Participant);
        FRunPartyMember Member = Four.Party[0];
        Member.SlotIndex = Index == 2 ? 0 : 2;
        Member.CharacterId = FGuid::NewGuid();
        Member.OwnerAccountId = Participant.AccountId;
        Four.Party.Add(Member);
    }
    TestTrue(TEXT("Four participants with four original characters are representable"), URunIdentityLibrary::ValidateIdentity(Four.Identity, Four.Party, Error));
    TestTrue(TEXT("Successful validation clears a previous error"), Error.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunIdentityLifecycleTest, "ProjectA.Run.Identity.Lifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRunIdentityLifecycleTest::RunTest(const FString& Parameters)
{
    FRunIdentityFixture Input;
    for (FRunPartyMember& Member : Input.Party)
    {
        Member.CharacterId.Invalidate();
        Member.OwnerAccountId = FRunAccountId();
    }
    FScopedIdentityRun Fixture;
    FText Error;
    if (!TestTrue(TEXT("Existing ID-free singleplayer initialization succeeds"), Fixture.Run->InitializeRun(Input.Party, Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    const FRunIdentityData Identity = Fixture.Run->GetRunIdentity();
    const TArray<FRunPartyMember> Party = Fixture.Run->GetPartyMembers();
    TestTrue(TEXT("Singleplayer initialization creates a local-development identity"), Identity.Origin == ERunIdentityOrigin::LocalDevelopment);
    TestEqual(TEXT("New singleplayer explicitly records identity schema two"), Identity.SchemaVersion, 2);
    TestTrue(TEXT("New Run ID is valid"), Identity.RunId.IsValid());
    TestEqual(TEXT("Singleplayer has one original participant"), Identity.OriginalParticipants.Num(), 1);
    TestEqual(TEXT("Initial Host epoch is one"), Identity.HostEpoch, 1);
    if (Identity.OriginalParticipants.Num() != 1)
    {
        return false;
    }
    TestEqual(TEXT("Development identity is explicitly namespaced"), Identity.HostAccountId.Provider, FName(TEXT("Development")));
    TestEqual(TEXT("New singleplayer Host has explicit original join number one"), Identity.OriginalParticipants[0].JoinOrdinal, 1);
    TestTrue(TEXT("Starting singleplayer never invents AI consent"), Identity.OriginalParticipants[0].AIConsent == ERunAIConsent::Unknown);
    TestEqual(TEXT("Unknown consent has no policy version"), Identity.OriginalParticipants[0].ConsentPolicyVersion, 0);
    TestFalse(TEXT("Sorted empty slot remains unidentified"), Party[0].CharacterId.IsValid());
    TestTrue(TEXT("Sorted empty slot has no owner"), Party[0].OwnerAccountId.IsEmpty());
    TestTrue(TEXT("All generated local character ownership is structurally valid"), URunIdentityLibrary::ValidateIdentity(Identity, Party, Error));

    int32 Events = 0;
    Fixture.Run->OnRunStateChanged.AddLambda([&Events]()
    {
        ++Events;
    });
    TArray<FRunPartyMember> InvalidParty = Input.Party;
    InvalidParty[1].SlotIndex = InvalidParty[0].SlotIndex;
    TestFalse(TEXT("Invalid new party is rejected"), Fixture.Run->InitializeRun(InvalidParty, Error));
    TestTrue(TEXT("Failed local initialization preserves identity"), SameIdentity(Fixture.Run->GetRunIdentity(), Identity));
    TestTrue(TEXT("Failed local initialization preserves party"), SameParty(Fixture.Run->GetPartyMembers(), Party));
    TestEqual(TEXT("Failed initialization emits no state change"), Events, 0);
    FRunIdentityFixture InvalidExplicit;
    InvalidExplicit.Identity.HostAccountId.Subject = TEXT("Replacement");
    TestFalse(TEXT("Invalid explicit identity is rejected"), Fixture.Run->InitializeRunWithIdentity(InvalidExplicit.Party, InvalidExplicit.Identity, Error));
    TestFalse(TEXT("New runs cannot claim legacy ownership absence"), Fixture.Run->InitializeRunWithIdentity(Input.Party, FRunIdentityData(), Error));
    TestTrue(TEXT("Rejected explicit initializations preserve the complete current identity"), SameIdentity(Fixture.Run->GetRunIdentity(), Identity));
    TestTrue(TEXT("Rejected explicit initializations preserve the complete current party"), SameParty(Fixture.Run->GetPartyMembers(), Party));
    TestEqual(TEXT("Rejected explicit initializations emit no state change"), Events, 0);
    TestFalse(TEXT("Reusing the active Run ID cannot reset progression or ownership"), Fixture.Run->InitializeRunWithIdentity(Party, Identity, Error));
    TestTrue(TEXT("Repeated Run ID preserves the current identity"), SameIdentity(Fixture.Run->GetRunIdentity(), Identity));
    TestTrue(TEXT("Repeated Run ID preserves the current party"), SameParty(Fixture.Run->GetPartyMembers(), Party));
    TestEqual(TEXT("Repeated Run ID emits no state change"), Events, 0);

    TestTrue(TEXT("First encounter begins"), Fixture.Run->BeginEncounter(TEXT("Combat_01")));
    TestTrue(TEXT("First combat starts"), Fixture.Run->MarkCombatStarted());
    Fixture.Run->UpdatePartyMemberHP(1, 72.0f);
    Fixture.Run->UpdatePartyMemberHP(3, 86.0f);
    TestTrue(TEXT("First victory completes"), Fixture.Run->CompleteEncounter(ECombatResult::Victory));
    TestTrue(TEXT("Continue returns to the next node"), Fixture.Run->ContinueRun());
    TestTrue(TEXT("Progression preserves Run, participant, Host and consent identity"), SameIdentity(Fixture.Run->GetRunIdentity(), Identity));
    for (int32 Index = 0; Index < Party.Num(); ++Index)
    {
        TestTrue(TEXT("Progression preserves original character ID"), Fixture.Run->GetPartyMembers()[Index].CharacterId == Party[Index].CharacterId);
        TestTrue(TEXT("Progression preserves original character owner"), Fixture.Run->GetPartyMembers()[Index].OwnerAccountId == Party[Index].OwnerAccountId);
    }
    TestTrue(TEXT("Starting another Run remains supported"), Fixture.Run->InitializeRun(Input.Party, Error));
    TestTrue(TEXT("A fresh Run receives a distinct ID"), Fixture.Run->GetRunIdentity().RunId != Identity.RunId);
    Fixture.Run->OnRunStateChanged.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunIdentitySaveIntegrityTest, "ProjectA.Run.Identity.SaveIntegrity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRunIdentitySaveIntegrityTest::RunTest(const FString& Parameters)
{
    const FRunIdentityFixture Input;
    FScopedIdentityRun Fixture;
    FText Error;
    if (!TestTrue(TEXT("Explicit participant ownership initializes"), Fixture.Run->InitializeRunWithIdentity(Input.Party, Input.Identity, Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    TestTrue(TEXT("Identified checkpoint saves"), Fixture.Run->SaveCheckpoint(Error));
    TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
    if (!TestNotNull(TEXT("Identified checkpoint loads as RunSaveGame"), Saved.Get()))
    {
        return false;
    }
    TestEqual(TEXT("New identified checkpoint uses outer save version two"), Saved->Version, 2);
    TestTrue(TEXT("SaveGame preserves every identity field and both consent states"), SameIdentity(Saved->Identity, Input.Identity));
    TestTrue(TEXT("SaveGame preserves all party data including sorted character ownership"), SameParty(Saved->Party, Fixture.Run->GetPartyMembers()));

    TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
    Restored->EnableCheckpointSaving(Fixture.Slot);
    int32 Events = 0;
    Restored->OnRunStateChanged.AddLambda([&Events]()
    {
        ++Events;
    });
    TArray<uint8> BytesBefore;
    TestTrue(TEXT("Stored bytes are available"), UGameplayStatics::LoadDataFromSlot(BytesBefore, Fixture.Slot, 0));
    TestTrue(TEXT("Menu can query identified Continue"), Restored->CanContinueSavedRun(Error));
    TestTrue(TEXT("Repeated menu queries remain valid"), Restored->CanContinueSavedRun(Error));
    TArray<uint8> BytesAfter;
    TestTrue(TEXT("Stored bytes remain available"), UGameplayStatics::LoadDataFromSlot(BytesAfter, Fixture.Slot, 0));
    TestTrue(TEXT("Continue queries never rewrite the checkpoint"), BytesBefore == BytesAfter);
    TestTrue(TEXT("Continue queries do not initialize the current Run"), Restored->GetPhase() == ERunPhase::None && !Restored->GetRunIdentity().RunId.IsValid());
    TestEqual(TEXT("Continue queries emit no state changes"), Events, 0);
    TestTrue(TEXT("Identified checkpoint restores"), Restored->LoadCheckpoint(Error));
    const FRunIdentityData BeforeIdentity = Restored->GetRunIdentity();
    const TArray<FRunPartyMember> BeforeParty = Restored->GetPartyMembers();
    TestTrue(TEXT("Loaded identity retains all original fields"), SameIdentity(BeforeIdentity, Input.Identity));
    TestTrue(TEXT("Loaded party retains original character identities"), SameParty(BeforeParty, Saved->Party));
    TestEqual(TEXT("Successful load emits one state change"), Events, 1);

    const FRunIdentityData SavedIdentity = Saved->Identity;
    const auto RejectStored = [this, &Fixture, &Restored, &Saved, &Error, &Events, &BeforeIdentity, &BeforeParty](const TCHAR* Label)
    {
        TestTrue(TEXT("Invalid checkpoint fixture writes directly"), UGameplayStatics::SaveGameToSlot(Saved.Get(), Fixture.Slot, 0));
        TArray<uint8> Before;
        UGameplayStatics::LoadDataFromSlot(Before, Fixture.Slot, 0);
        TestFalse(Label, Restored->LoadCheckpoint(Error));
        TestFalse(TEXT("Rejected checkpoint provides an explanation"), Error.IsEmpty());
        TestTrue(TEXT("Rejected load preserves complete identity"), SameIdentity(Restored->GetRunIdentity(), BeforeIdentity));
        TestTrue(TEXT("Rejected load preserves complete party"), SameParty(Restored->GetPartyMembers(), BeforeParty));
        TestTrue(TEXT("Rejected load preserves progression"), Restored->CanStartNode(TEXT("Combat_01")));
        TestEqual(TEXT("Rejected load emits no state changes"), Events, 1);
        TArray<uint8> After;
        UGameplayStatics::LoadDataFromSlot(After, Fixture.Slot, 0);
        TestTrue(TEXT("Rejected load does not rewrite the existing file"), Before == After);
    };
    Saved->Identity.HostAccountId.Subject = TEXT("ReplacementHost");
    RejectStored(TEXT("Invalid owner metadata cannot replace an active Run"));
    Saved->Identity = FRunIdentityData();
    RejectStored(TEXT("Version two without identity metadata is rejected"));
    Saved->Identity = SavedIdentity;
    Saved->Version = 1;
    RejectStored(TEXT("Version one carrying identity metadata is rejected"));
    Saved->Version = 999;
    RejectStored(TEXT("Unknown outer save version is rejected"));

    TStrongObjectPtr<UPartySnapshotSaveGame> WrongType(NewObject<UPartySnapshotSaveGame>());
    TestTrue(TEXT("Wrong SaveGame type fixture writes"), UGameplayStatics::SaveGameToSlot(WrongType.Get(), Fixture.Slot, 0));
    TestFalse(TEXT("Wrong SaveGame type cannot continue"), Restored->LoadCheckpoint(Error));
    TestTrue(TEXT("Wrong SaveGame type preserves the current Run"), SameIdentity(Restored->GetRunIdentity(), BeforeIdentity) && SameParty(Restored->GetPartyMembers(), BeforeParty));
    TestTrue(TEXT("Test checkpoint can be removed"), UGameplayStatics::DeleteGameInSlot(Fixture.Slot, 0));
    TestFalse(TEXT("Missing checkpoint cannot replace the current Run"), Restored->LoadCheckpoint(Error));
    TestTrue(TEXT("Missing load preserves the current Run"), SameIdentity(Restored->GetRunIdentity(), BeforeIdentity) && SameParty(Restored->GetPartyMembers(), BeforeParty));
    Restored->OnRunStateChanged.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunIdentityLegacyCompatibilityTest, "ProjectA.Run.Identity.LegacyCompatibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRunIdentityLegacyCompatibilityTest::RunTest(const FString& Parameters)
{
    const FRunIdentityFixture Input;
    FScopedIdentityRun Fixture;
    FText Error;
    if (!TestTrue(TEXT("Fixture starts an identified Run"), Fixture.Run->InitializeRunWithIdentity(Input.Party, Input.Identity, Error)))
    {
        return false;
    }
    TStrongObjectPtr<URunSaveGame> Save(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
    if (!TestNotNull(TEXT("Fixture checkpoint exists"), Save.Get()) || !TestTrue(TEXT("Legacy bytes omit all three newly added identity properties"), WriteLegacyIdentityFixture(Save.Get(), Fixture.Slot)))
    {
        return false;
    }
    TStrongObjectPtr<URunSaveGame> Legacy(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
    if (!TestNotNull(TEXT("Engine loads metadata-free legacy bytes"), Legacy.Get()))
    {
        return false;
    }
    TestEqual(TEXT("Legacy outer version survives loading"), Legacy->Version, 1);
    TestTrue(TEXT("Missing identity properties retain legacy defaults"), SameIdentity(Legacy->Identity, FRunIdentityData()));
    for (const FRunPartyMember& Member : Legacy->Party)
    {
        TestFalse(TEXT("Legacy characters acquire no invented ID"), Member.CharacterId.IsValid());
        TestTrue(TEXT("Legacy characters acquire no inferred owner"), Member.OwnerAccountId.IsEmpty());
    }

    TArray<uint8> Before;
    UGameplayStatics::LoadDataFromSlot(Before, Fixture.Slot, 0);
    const FRunIdentityData ActiveIdentity = Fixture.Run->GetRunIdentity();
    const TArray<FRunPartyMember> ActiveParty = Fixture.Run->GetPartyMembers();
    int32 Events = 0;
    Fixture.Run->OnRunStateChanged.AddLambda([&Events]()
    {
        ++Events;
    });
    TestTrue(TEXT("Menu accepts a supported legacy checkpoint"), Fixture.Run->CanContinueSavedRun(Error));
    TestTrue(TEXT("Repeated legacy queries remain valid"), Fixture.Run->CanContinueSavedRun(Error));
    TArray<uint8> After;
    UGameplayStatics::LoadDataFromSlot(After, Fixture.Slot, 0);
    TestTrue(TEXT("Legacy Continue queries never migrate or rewrite the file"), Before == After);
    TestTrue(TEXT("Legacy Continue queries preserve the active identity"), SameIdentity(Fixture.Run->GetRunIdentity(), ActiveIdentity));
    TestTrue(TEXT("Legacy Continue queries preserve the active party"), SameParty(Fixture.Run->GetPartyMembers(), ActiveParty));
    TestEqual(TEXT("Legacy queries emit no state changes"), Events, 0);
    TestTrue(TEXT("Metadata-free legacy checkpoint restores"), Fixture.Run->LoadCheckpoint(Error));
    TestTrue(TEXT("Restored legacy Run stays explicitly unidentified"), SameIdentity(Fixture.Run->GetRunIdentity(), FRunIdentityData()));
    TestTrue(TEXT("Restored legacy party retains its gameplay fields"), SameParty(Fixture.Run->GetPartyMembers(), Legacy->Party));
    TestTrue(TEXT("Legacy checkpoint can be saved without inventing ownership"), Fixture.Run->SaveCheckpoint(Error));
    Legacy.Reset(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
    if (TestNotNull(TEXT("Resaved legacy checkpoint loads"), Legacy.Get()))
    {
        TestEqual(TEXT("Resaved unidentified progress remains outer version one"), Legacy->Version, 1);
        TestTrue(TEXT("Resaving legacy data creates no Run identity"), SameIdentity(Legacy->Identity, FRunIdentityData()));
        TestTrue(TEXT("Resaving legacy data preserves character fields without ownership"), SameParty(Legacy->Party, Fixture.Run->GetPartyMembers()));
    }
    Fixture.Run->OnRunStateChanged.Clear();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunJoinOrderValidationTest, "ProjectA.Run.Identity.JoinOrderValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRunJoinOrderValidationTest::RunTest(const FString& Parameters)
{
    const FRunIdentityFixture Valid = MakeNumberedIdentity();
    FText Error;
    TestTrue(TEXT("Four explicit original join numbers validate"), URunIdentityLibrary::ValidateIdentity(Valid.Identity, Valid.Party, Error));
    FRunIdentityFixture Invalid = Valid;
    const auto Reject = [this, &Valid, &Invalid, &Error](const TCHAR* Label)
    {
        TestFalse(Label, URunIdentityLibrary::ValidateIdentity(Invalid.Identity, Invalid.Party, Error));
        TestFalse(TEXT("Invalid numbering reports a reason"), Error.IsEmpty());
        TestFalse(TEXT("Invalid numbering cannot report original participation"), URunIdentityLibrary::IsOriginalParticipant(Invalid.Identity, Invalid.Identity.HostAccountId));
        Invalid = Valid;
    };
    Invalid.Identity.OriginalParticipants[1].JoinOrdinal = 0;
    Reject(TEXT("Schema two never infers a missing number"));
    Invalid.Identity.OriginalParticipants[1].JoinOrdinal = -1;
    Reject(TEXT("Negative join number is rejected"));
    Invalid.Identity.OriginalParticipants[1].JoinOrdinal = 1;
    Reject(TEXT("Duplicate join numbers are rejected"));
    Invalid.Identity.OriginalParticipants[1].JoinOrdinal = 5;
    Reject(TEXT("Gapped or out-of-roster join numbers are rejected"));
    Invalid.Identity.HostAccountId = Invalid.Identity.OriginalParticipants[1].AccountId;
    Reject(TEXT("Initial Host must hold original join number one"));
    Invalid.Identity.SchemaVersion = 1;
    Reject(TEXT("Schema one cannot carry inferred join numbers"));
    FRunIdentityData Legacy;
    Legacy.SchemaVersion = 2;
    TestFalse(TEXT("Metadata-free legacy identity stays schema one"), URunIdentityLibrary::ValidateIdentity(Legacy, {}, Error));

    FRunIdentityFixture Reordered = Valid;
    Reordered.Identity.OriginalParticipants.Swap(0, 3);
    Reordered.Identity.OriginalParticipants.Swap(1, 2);
    TestTrue(TEXT("Stored array order is independent from original join order"), URunIdentityLibrary::ValidateIdentity(Reordered.Identity, Reordered.Party, Error));
    for (int32 Index = 0; Index < 4; ++Index)
    {
        int32 Ordinal = -77;
        TestTrue(TEXT("Each original account exposes its explicit number"), URunIdentityLibrary::TryGetJoinOrdinal(Reordered.Identity, Reordered.Party, Valid.Identity.OriginalParticipants[Index].AccountId, Ordinal, Error));
        TestEqual(TEXT("Number survives roster array reordering"), Ordinal, Index + 1);
    }
    Reordered.Identity.HostEpoch = 2;
    Reordered.Identity.HostAccountId = Valid.Identity.OriginalParticipants[2].AccountId;
    TestTrue(TEXT("A later Host epoch may name another original participant"), URunIdentityLibrary::ValidateIdentity(Reordered.Identity, Reordered.Party, Error));
    FRunAccountId Candidate;
    Candidate.Provider = TEXT("Sentinel");
    Candidate.Subject = TEXT("PreserveOutput");
    const FRunAccountId Sentinel = Candidate;
    const TArray<FRunAccountId> HumanAccounts{ Valid.Identity.OriginalParticipants[3].AccountId, Valid.Identity.OriginalParticipants[1].AccountId, Valid.Identity.OriginalParticipants[2].AccountId };
    TestTrue(TEXT("Explicit resuming humans yield a stored-order Host candidate"), URunIdentityLibrary::TrySelectHostCandidate(Reordered.Identity, Reordered.Party, HumanAccounts, Candidate, Error));
    TestTrue(TEXT("Lowest original number wins regardless of current Host and array order"), Candidate == Valid.Identity.OriginalParticipants[1].AccountId);
    TestTrue(TEXT("Successful candidate selection clears prior errors"), Error.IsEmpty());
    for (int32 Index = 1; Index < 4; ++Index)
    {
        const FRunAccountId& Account = Valid.Identity.OriginalParticipants[Index].AccountId;
        TestTrue(TEXT("Each original number two through four is a candidate when listed alone"), URunIdentityLibrary::TrySelectHostCandidate(Reordered.Identity, Reordered.Party, { Account }, Candidate, Error));
        TestTrue(TEXT("A singleton list selects its explicit account"), Candidate == Account);
    }

    const FRunIdentityFixture Unnumbered;
    const auto RejectCandidate = [this, &Candidate, &Sentinel, &Error](const FRunIdentityFixture& Fixture, const TArray<FRunAccountId>& Humans, const TCHAR* Label)
    {
        Candidate = Sentinel;
        TestFalse(Label, URunIdentityLibrary::TrySelectHostCandidate(Fixture.Identity, Fixture.Party, Humans, Candidate, Error));
        TestTrue(TEXT("Rejected candidate lookup preserves output"), Candidate == Sentinel);
        TestFalse(TEXT("Rejected candidate lookup reports a reason"), Error.IsEmpty());
    };
    RejectCandidate(Reordered, {}, TEXT("Empty resuming humans are rejected"));
    RejectCandidate(Reordered, { HumanAccounts[0], HumanAccounts[0] }, TEXT("Duplicate resuming humans are rejected"));
    RejectCandidate(Reordered, { HumanAccounts[0], Sentinel }, TEXT("An external account rejects the entire candidate request"));
    RejectCandidate(Unnumbered, { Unnumbered.Identity.HostAccountId }, TEXT("Old array positions cannot be used as inferred join numbers"));
    Invalid = Reordered;
    Invalid.Identity.RunId.Invalidate();
    RejectCandidate(Invalid, HumanAccounts, TEXT("Malformed Run identity rejects candidate lookup"));
    Invalid = Reordered;
    Invalid.Party[0].CharacterId.Invalidate();
    RejectCandidate(Invalid, HumanAccounts, TEXT("Invalid character ownership rejects candidate lookup"));

    int32 Ordinal = -77;
    TestFalse(TEXT("External account has no join number"), URunIdentityLibrary::TryGetJoinOrdinal(Reordered.Identity, Reordered.Party, Sentinel, Ordinal, Error));
    TestEqual(TEXT("Unknown account lookup preserves output"), Ordinal, -77);
    TestFalse(TEXT("Legacy schema has no inferred join number"), URunIdentityLibrary::TryGetJoinOrdinal(Unnumbered.Identity, Unnumbered.Party, Unnumbered.Identity.HostAccountId, Ordinal, Error));
    TestEqual(TEXT("Legacy lookup preserves output"), Ordinal, -77);
    TestFalse(TEXT("Invalid party rejects number lookup"), URunIdentityLibrary::TryGetJoinOrdinal(Invalid.Identity, Invalid.Party, Invalid.Identity.HostAccountId, Ordinal, Error));
    TestEqual(TEXT("Invalid party lookup preserves output"), Ordinal, -77);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunJoinOrderPersistenceTest, "ProjectA.Run.Identity.JoinOrderPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRunJoinOrderPersistenceTest::RunTest(const FString& Parameters)
{
    for (int32 IdentitySchema = 1; IdentitySchema <= 2; ++IdentitySchema)
    {
        FRunIdentityFixture Input = MakeNumberedIdentity();
        Input.Identity.SchemaVersion = IdentitySchema;
        Input.Identity.HostEpoch = 3;
        Input.Identity.HostAccountId = Input.Identity.OriginalParticipants[2].AccountId;
        if (IdentitySchema == 1)
        {
            for (FRunParticipantData& Participant : Input.Identity.OriginalParticipants)
            {
                Participant.JoinOrdinal = 0;
            }
        }
        Input.Identity.OriginalParticipants.Swap(0, 3);
        FScopedIdentityRun Fixture;
        FText Error;
        if (!TestTrue(TEXT("Explicit four-owner identity initializes without migration"), Fixture.Run->InitializeRunWithIdentity(Input.Party, Input.Identity, Error))) return false;
        if (!TestTrue(TEXT("Map checkpoint saves"), Fixture.Run->SaveCheckpoint(Error))) return false;
        TStrongObjectPtr<URunSaveGame> Saved(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
        if (!TestNotNull(TEXT("Map checkpoint loads through native SaveGame"), Saved.Get())) return false;
        TestEqual(TEXT("Both identity schemas retain outer map-save version two"), Saved->Version, 2);
        TestTrue(TEXT("Map save preserves schema, roster order, join numbers, Host and ownership metadata"), SameIdentity(Saved->Identity, Input.Identity));
        TStrongObjectPtr<URunStateSubsystem> Restored(NewObject<URunStateSubsystem>(Fixture.Instance.Get()));
        Restored->EnableCheckpointSaving(Fixture.Slot);
        if (!TestTrue(TEXT("Map checkpoint passes ordinary load validation"), Restored->LoadCheckpoint(Error))) return false;
        TestTrue(TEXT("Loaded map identity preserves explicit numbers or unknown old numbering"), SameIdentity(Restored->GetRunIdentity(), Input.Identity));

        if (!TestTrue(TEXT("Numbered and old-identity Runs can begin combat"), Fixture.Run->BeginEncounter(TEXT("Combat_01")) && Fixture.Run->MarkCombatStarted())) return false;
        FCombatCheckpointData Checkpoint;
        if (!TestTrue(TEXT("Real profession data creates a valid storage fixture"), MakeIdentityCheckpoint(Fixture.Run.Get(), Checkpoint))) return false;
        if (!TestTrue(TEXT("Combat checkpoint commits through production validation and atomic storage"), Fixture.Run->CommitCombatCheckpoint(Checkpoint, Error)))
        {
            AddError(Error.ToString());
            return false;
        }
        Saved.Reset(Cast<URunSaveGame>(UGameplayStatics::LoadGameFromSlot(Fixture.Slot, 0)));
        if (!TestNotNull(TEXT("Combat checkpoint loads through native SaveGame"), Saved.Get())) return false;
        TestEqual(TEXT("Both identity schemas retain outer combat-save version three"), Saved->Version, 3);
        TestTrue(TEXT("Combat file preserves every outer identity field"), SameIdentity(Saved->Identity, Input.Identity));
        TestTrue(TEXT("Nested combat identity retains the same schema and join numbers"), SameIdentity(Saved->CombatCheckpoint.Identity, Input.Identity));
        if (!TestTrue(TEXT("Combat checkpoint passes ordinary load validation"), Restored->LoadCheckpoint(Error))) return false;
        TestTrue(TEXT("Restored combat identity is never reordered or inferred"), SameIdentity(Restored->GetRunIdentity(), Input.Identity));
        TestTrue(TEXT("Restored checkpoint retains original Host and full nested identity"), SameIdentity(Restored->GetCombatCheckpoint().Identity, Input.Identity) && Restored->ValidateCheckpointHost(Input.Identity.HostAccountId, Error));

        const FRunIdentityData Before = Restored->GetRunIdentity();
        const TArray<FRunPartyMember> BeforeParty = Restored->GetPartyMembers();
        const FCombatCheckpointData BeforeCheckpoint = Restored->GetCombatCheckpoint();
        Saved->CombatCheckpoint.Identity.OriginalParticipants[0].JoinOrdinal = IdentitySchema == 1 ? 1 : 0;
        if (!TestTrue(TEXT("Corrupt nested numbering fixture writes"), UGameplayStatics::SaveGameToSlot(Saved.Get(), Fixture.Slot, 0))) return false;
        TArray<uint8> BeforeBytes;
        TestTrue(TEXT("Corrupt fixture bytes are readable"), UGameplayStatics::LoadDataFromSlot(BeforeBytes, Fixture.Slot, 0));
        TestFalse(TEXT("Mismatched or invalid nested join numbering cannot load"), Restored->LoadCheckpoint(Error));
        TestTrue(TEXT("Rejected numbering preserves the current Run and character ownership"), SameIdentity(Restored->GetRunIdentity(), Before) && SameParty(Restored->GetPartyMembers(), BeforeParty));
        TestTrue(TEXT("Rejected numbering preserves the confirmed checkpoint"), FCombatCheckpointData::StaticStruct()->CompareScriptStruct(&Restored->GetCombatCheckpoint(), &BeforeCheckpoint, 0));
        TArray<uint8> AfterBytes;
        TestTrue(TEXT("Rejected fixture bytes remain readable"), UGameplayStatics::LoadDataFromSlot(AfterBytes, Fixture.Slot, 0));
        TestTrue(TEXT("Rejected numbering never rewrites the save file"), BeforeBytes == AfterBytes);
    }
    return true;
}

#endif
