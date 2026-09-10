#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Game/Run/RunIdentityTypes.h"
#include "Game/Run/RunTypes.h"
#include "RunIdentityLibrary.generated.h"

UCLASS()
class PROJECTA_API URunIdentityLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static constexpr int32 CurrentSchemaVersion = 2;

    UFUNCTION(BlueprintCallable, Category = "Run|Identity")
    static bool ValidateIdentity(const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, FText& OutError);

    // Queries compare stored identities; they do not authenticate a caller or authorize an RPC.
    // 조회는 저장된 식별자를 비교하며 호출자 인증이나 RPC 권한 검증을 수행하지 않습니다.
    UFUNCTION(BlueprintPure, Category = "Run|Identity")
    static bool IsOriginalParticipant(const FRunIdentityData& Identity, const FRunAccountId& AccountId);

    // Character ownership has no Host exception and is separate from live action eligibility.
    // 캐릭터 소유권에는 Host 예외가 없으며 현재 행동 가능 여부와 별개입니다.
    UFUNCTION(BlueprintPure, Category = "Run|Identity")
    static bool IsCharacterOwner(const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, FGuid CharacterId, const FRunAccountId& AccountId);

    // Queries require explicit stored ordinals and preserve the output when validation fails.
    // 조회에는 명시적으로 저장된 번호가 필요하며 검증 실패 시 출력값을 보존합니다.
    UFUNCTION(BlueprintPure, Category = "Run|Identity")
    static bool TryGetJoinOrdinal(const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, const FRunAccountId& AccountId, int32& OutOrdinal, FText& OutError);

    // Select only by original join order; callers must separately verify approval, presence and permanent AI history.
    // 최초 합류 순서로만 선택하며 승인, 접속 여부, 영구 AI 전환 이력은 호출자가 별도로 검증해야 합니다.
    UFUNCTION(BlueprintPure, Category = "Run|Identity")
    static bool TrySelectHostCandidate(const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, const TArray<FRunAccountId>& HumanParticipants, FRunAccountId& OutAccountId, FText& OutError);
};
