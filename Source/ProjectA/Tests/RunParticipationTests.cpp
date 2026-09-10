#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/Run/RunParticipationLibrary.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunParticipationValidationTest, "ProjectA.Recovery.ParticipationValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRunParticipationValidationTest::RunTest(const FString& Parameters)
{
    FRunIdentityData Identity;
    Identity.Origin = ERunIdentityOrigin::AccountProvider;
    Identity.RunId = FGuid::NewGuid();
    Identity.HostEpoch = 1;
    TArray<FRunPartyMember> Members;
    FRunParticipationData Participation;
    FText Error;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FRunParticipantData& Participant = Identity.OriginalParticipants.AddDefaulted_GetRef();
        Participant.AccountId.Provider = TEXT("RecoveryFixture");
        Participant.AccountId.Subject = FString::Printf(TEXT("Owner%d"), Index);
        Participation.HumanParticipants.Add(Participant.AccountId);
        FRunPartyMember& Member = Members.AddDefaulted_GetRef();
        Member.SlotIndex = Index;
        Member.CharacterId = FGuid::NewGuid();
        Member.OwnerAccountId = Participant.AccountId;
        Member.bCreated = true;
    }
    Identity.HostAccountId = Identity.OriginalParticipants[0].AccountId;
    TestTrue(TEXT("An all-human roster is structurally valid with Unknown legacy consent"), URunParticipationLibrary::Validate(Participation, Identity, Members, Error));
    Participation.HumanParticipants.Pop();
    TestTrue(TEXT("An omitted original owner needs no prior consent in the AI roster data"), URunParticipationLibrary::Validate(Participation, Identity, Members, Error));
    EPartyControlMode Mode = EPartyControlMode::Human;
    TestTrue(TEXT("Unknown legacy consent resolves the omitted owner's character as AI"), URunParticipationLibrary::ResolveControlMode(Participation, Identity, Members, Members[1].CharacterId, Mode, Error) && Mode == EPartyControlMode::ServerAI);
    Identity.OriginalParticipants[1].AIConsent = ERunAIConsent::Granted;
    Identity.OriginalParticipants[1].ConsentPolicyVersion = 1;
    TestTrue(TEXT("Legacy Granted metadata remains compatible with the AI roster"), URunParticipationLibrary::Validate(Participation, Identity, Members, Error));
    const FRunIdentityData OriginalIdentity = Identity;
    const TArray<FRunPartyMember> OriginalMembers = Members;
    TestTrue(TEXT("AI mode resolves from the original character owner"), URunParticipationLibrary::ResolveControlMode(Participation, Identity, Members, Members[1].CharacterId, Mode, Error) && Mode == EPartyControlMode::ServerAI);
    TestTrue(TEXT("Host character remains Human"), URunParticipationLibrary::ResolveControlMode(Participation, Identity, Members, Members[0].CharacterId, Mode, Error) && Mode == EPartyControlMode::Human);
    Members[0].CurrentHP = 0.0f;
    TestTrue(TEXT("Death does not remove a Human participant from the approved roster"), URunParticipationLibrary::Validate(Participation, Identity, Members, Error) && Participation.HumanParticipants.Contains(Identity.HostAccountId));
    Members = OriginalMembers;
    FRunParticipationData Invalid = Participation;
    const FRunAccountId DuplicateHuman = Invalid.HumanParticipants[0];
    Invalid.HumanParticipants.Add(DuplicateHuman);
    TestFalse(TEXT("Duplicate Human entries are rejected"), URunParticipationLibrary::Validate(Invalid, Identity, Members, Error));
    Invalid = Participation;
    Invalid.HumanParticipants[0] = Identity.OriginalParticipants[1].AccountId;
    TestFalse(TEXT("An AI Host cannot own the active session"), URunParticipationLibrary::Validate(Invalid, Identity, Members, Error));
    Invalid = Participation;
    Invalid.HumanParticipants[0].Subject = TEXT("ReplacementPlayer");
    TestFalse(TEXT("A replacement player is not an original participant"), URunParticipationLibrary::Validate(Invalid, Identity, Members, Error));
    Invalid = Participation;
    Invalid.SchemaVersion = 2;
    TestFalse(TEXT("An unsupported participation schema is rejected"), URunParticipationLibrary::Validate(Invalid, Identity, Members, Error));
    Identity.OriginalParticipants[1].AIConsent = ERunAIConsent::Declined;
    TestTrue(TEXT("Legacy Declined metadata also permits the AI roster"), URunParticipationLibrary::Validate(Participation, Identity, Members, Error));
    TestTrue(TEXT("Declined legacy consent resolves the original owner's character as AI"), URunParticipationLibrary::ResolveControlMode(Participation, Identity, Members, Members[1].CharacterId, Mode, Error) && Mode == EPartyControlMode::ServerAI);
    Identity.OriginalParticipants[1].ConsentPolicyVersion = 0;
    TestFalse(TEXT("Legacy consent metadata must still have a valid serialized form"), URunParticipationLibrary::Validate(Participation, Identity, Members, Error));
    Identity = OriginalIdentity;
    Mode = EPartyControlMode::ServerAI;
    TestFalse(TEXT("Unknown character lookup fails"), URunParticipationLibrary::ResolveControlMode(Participation, Identity, Members, FGuid::NewGuid(), Mode, Error));
    TestTrue(TEXT("Failed lookup does not publish a replacement mode"), Mode == EPartyControlMode::ServerAI);

    TArray<uint8> Bytes;
    {
        FMemoryWriter Writer(Bytes);
        FObjectAndNameAsStringProxyArchive Archive(Writer, false);
        FRunParticipationData::StaticStruct()->SerializeItem(Archive, &Participation, nullptr);
    }
    FRunParticipationData Restored;
    {
        FMemoryReader Reader(Bytes);
        FObjectAndNameAsStringProxyArchive Archive(Reader, true);
        FRunParticipationData::StaticStruct()->SerializeItem(Archive, &Restored, nullptr);
    }
    TestTrue(TEXT("Native struct serialization preserves the complete participation roster"), FRunParticipationData::StaticStruct()->CompareScriptStruct(&Restored, &Participation, 0));
    TestTrue(TEXT("The restored AI roster retains the same owner mapping"), URunParticipationLibrary::ResolveControlMode(Restored, Identity, Members, Members[1].CharacterId, Mode, Error) && Mode == EPartyControlMode::ServerAI);
    TestTrue(TEXT("Validation and serialization do not change Run, Host or legacy consent metadata"), FRunIdentityData::StaticStruct()->CompareScriptStruct(&Identity, &OriginalIdentity, 0));
    for (int32 Index = 0; Index < Members.Num(); ++Index)
    {
        TestTrue(TEXT("Validation and serialization never transfer a character"), FRunPartyMember::StaticStruct()->CompareScriptStruct(&Members[Index], &OriginalMembers[Index], 0));
    }
    return true;
}

#endif
