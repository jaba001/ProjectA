#include "Misc/AutomationTest.h"
#include "Game/Development/DevelopmentCoopSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDevelopmentCoopAddressTest, "ProjectA.DevelopmentCoop.AddressValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDevelopmentCoopAddressTest::RunTest(const FString& Parameters)
{
    FString Address;
    TestTrue(TEXT("Loopback without a port receives the default listen port."), UDevelopmentCoopSubsystem::NormalizeAddress(TEXT(" localhost "), Address));
    TestEqual(TEXT("Loopback is a numeric endpoint."), Address, FString(TEXT("127.0.0.1:7777")));
    TestTrue(TEXT("A LAN address accepts an explicit port."), UDevelopmentCoopSubsystem::NormalizeAddress(TEXT("192.168.001.020:7778"), Address));
    TestEqual(TEXT("Address octets are normalized."), Address, FString(TEXT("192.168.1.20:7778")));
    const TArray<FString> Invalid{ TEXT(""), TEXT("/Game/User_JeHoon/LEVEL/Gameplay"), TEXT("127.0.0.1?listen"), TEXT("127.0.0.1:7777?game=Other"), TEXT("127.0.0.1/Map"), TEXT("256.0.0.1"), TEXT("1.2.3"), TEXT("1.2..4"), TEXT("1.2.3.4:"), TEXT("1.2.3.4:0"), TEXT("1.2.3.4:65536"), TEXT("1.2.3.4:+7777"), TEXT("1.2.3.4:1.5"), TEXT("1.2.3.4:7777:1") };
    for (const FString& Input : Invalid)
    {
        TestFalse(FString::Printf(TEXT("Reject non-endpoint input: %s"), *Input), UDevelopmentCoopSubsystem::NormalizeAddress(Input, Address));
        TestTrue(TEXT("Rejected input exposes no partial travel URL."), Address.IsEmpty());
    }
    return true;
}

#endif
