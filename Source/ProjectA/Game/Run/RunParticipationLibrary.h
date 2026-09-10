#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/AI/PartyControlTypes.h"
#include "Game/Run/RunParticipationTypes.h"
#include "Game/Run/RunTypes.h"
#include "RunParticipationLibrary.generated.h"

UCLASS()
class PROJECTA_API URunParticipationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Structural validation does not authenticate accounts or approve a resume, start or return policy.
    // 구조 검증은 계정 인증이나 재개·시작·복귀 정책의 승인을 대신하지 않습니다.
    UFUNCTION(BlueprintCallable, Category = "Run|Participation")
    static bool Validate(const FRunParticipationData& Participation, const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, FText& OutError);

    static bool ResolveControlMode(const FRunParticipationData& Participation, const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, FGuid CharacterId, EPartyControlMode& OutMode, FText& OutError);

    // Resume may remove human participants permanently; original ownership and join order never change.
    // 재개는 인간 참가자를 영구적으로 줄일 수 있지만 원래 소유권과 합류 번호는 변경하지 않습니다.
    static bool ValidateTransition(const FRunParticipationData& Previous, const FRunIdentityData& PreviousIdentity, const FRunParticipationData& Next, const FRunIdentityData& NextIdentity, const TArray<FRunPartyMember>& Members, FText& OutError);
};
