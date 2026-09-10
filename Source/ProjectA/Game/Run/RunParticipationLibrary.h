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
};
