#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/Commands/CombatActionTypes.h"
#include "Combat/AI/PartyControlTypes.h"
#include "Game/Run/RunTypes.h"
#include "CombatActionAuthority.generated.h"

class ACombatManager;
class APartyPlayerController;
class AUnitBase;
class APlayerUnit;

// Server-owned command validation is separate from UI state and internal AI execution.
// 서버 명령 검증을 UI 상태 및 내부 AI 실행과 분리합니다.
UCLASS()
class PROJECTA_API UCombatActionAuthority : public UObject
{
    GENERATED_BODY()

public:
    void Reset();
    void RegisterUnits(const TArray<AUnitBase*>& Units);
    void BeginCombat();
    bool ConfigureRun(const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, const TMap<int32, TObjectPtr<AUnitBase>>& PartyActors, FText& OutError);

    // Only trusted server code may bind a connection; no account claim arrives in the command payload.
    // 신뢰된 서버 코드만 연결을 바인딩하며 명령에는 계정 주장을 받지 않습니다.
    bool BindParticipant(APartyPlayerController* Controller, const FRunAccountId& AccountId);
    bool CanControllerControl(const APartyPlayerController* Controller, const AUnitBase* Unit) const;
    FCombatActionResponse Execute(APartyPlayerController* Controller, const FCombatActionRequest& Request);
    bool SetPartyControlMode(APlayerUnit* Unit, EPartyControlMode Mode, FText& OutError);
    FCombatActionResponse ExecuteServerAI(APlayerUnit* Unit, const FCombatActionRequest& Request, FGuid ControlSessionId);
    FGuid GetUnitId(const AUnitBase* Unit) const;
    FGuid GetCharacterId(const AUnitBase* Unit) const;
    FRunAccountId GetOwnerAccountId(const AUnitBase* Unit) const;
    AUnitBase* ResolveUnit(FGuid UnitId) const;
    FGuid GetCombatInstanceId() const { return CombatInstanceId; }
    FGuid GetParticipantBindingId(const APartyPlayerController* Controller) const;
    const FRunIdentityData& GetRunIdentity() const { return RunIdentity; }

private:
    ACombatManager* GetManager() const;
    bool AllowsStandaloneLegacy(const APartyPlayerController* Controller) const;
    bool HasAIConsent(const APlayerUnit* Unit) const;
    FCombatActionResponse ExecuteUnitAction(AUnitBase* Unit, const FCombatActionRequest& Request);

    UPROPERTY(Transient)
    FRunIdentityData RunIdentity;

    UPROPERTY(Transient)
    TArray<FRunPartyMember> PartyMembers;

    TMap<FGuid, TWeakObjectPtr<AUnitBase>> UnitsById;
    TMap<TWeakObjectPtr<AUnitBase>, FGuid> CharacterIds;
    TMap<TWeakObjectPtr<APartyPlayerController>, FRunAccountId> Participants;
    TMap<TWeakObjectPtr<APartyPlayerController>, FGuid> ParticipantBindingIds;
    TMap<TWeakObjectPtr<APartyPlayerController>, int64> LastRequestSequences;
    TMap<FGuid, int64> LastAIRequestSequences;
    FGuid CombatInstanceId;
    bool bRequiresRunConfiguration = false;
    bool bRunConfigured = false;
};
