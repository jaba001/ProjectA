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
};
