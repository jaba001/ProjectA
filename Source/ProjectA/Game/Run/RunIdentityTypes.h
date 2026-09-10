#pragma once

#include "CoreMinimal.h"
#include "RunIdentityTypes.generated.h"

// Identity origin describes stored data, not a successful account authentication.
// 식별 정보 출처는 저장 데이터의 분류이며 계정 인증 성공을 의미하지 않습니다.
UENUM(BlueprintType)
enum class ERunIdentityOrigin : uint8
{
    LegacyOffline,
    LocalDevelopment,
    AccountProvider
};

UENUM(BlueprintType)
enum class ERunAIConsent : uint8
{
    Unknown,
    Granted,
    Declined
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunAccountId
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Run|Identity")
    FName Provider;

    // The provider's subject is opaque and must never be used as a filesystem path.
    // 공급자의 Subject는 불투명한 식별값이며 파일시스템 경로로 사용하지 않습니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Run|Identity")
    FString Subject;

    bool operator==(const FRunAccountId& Other) const { return Provider == Other.Provider && Subject.Equals(Other.Subject, ESearchCase::CaseSensitive); }
    bool operator!=(const FRunAccountId& Other) const { return !(*this == Other); }
    bool IsEmpty() const { return Provider.IsNone() && Subject.IsEmpty(); }
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunParticipantData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Run|Identity")
    FRunAccountId AccountId;

    // Schema two records the original join order explicitly; zero preserves unknown order in older saves.
    // 스키마 2는 최초 합류 순서를 명시하며 0은 구버전 저장의 알 수 없는 순서를 보존합니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Run|Identity")
    int32 JoinOrdinal = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Run|Identity")
    ERunAIConsent AIConsent = ERunAIConsent::Unknown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Run|Identity")
    int32 ConsentPolicyVersion = 0;
};

USTRUCT(BlueprintType)
struct PROJECTA_API FRunIdentityData
{
    GENERATED_BODY()

    // Keep the legacy default so missing metadata never acquires an inferred join order.
    // 누락된 메타데이터에 합류 순서를 추정하여 부여하지 않도록 구버전 기본값을 유지합니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Run|Identity")
    int32 SchemaVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Run|Identity")
    ERunIdentityOrigin Origin = ERunIdentityOrigin::LegacyOffline;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Run|Identity")
    FGuid RunId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Run|Identity")
    TArray<FRunParticipantData> OriginalParticipants;

    // Hosting a session never grants ownership of another participant's characters.
    // 세션을 호스트해도 다른 참가자의 캐릭터 소유권을 얻지 않습니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Run|Identity")
    FRunAccountId HostAccountId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Run|Identity")
    int32 HostEpoch = 0;
};
