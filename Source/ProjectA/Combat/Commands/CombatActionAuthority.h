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

// Retains authoritative Run ownership and connection bindings for round planning.
// 라운드 계획에 필요한 서버 Run 소유권과 연결 바인딩을 유지합니다.
UCLASS()
class PROJECTA_API UCombatActionAuthority : public UObject
{
    GENERATED_BODY()

public:
    void Reset();
    void RegisterUnits(const TArray<AUnitBase*>& Units);
    void BeginCombat();
    bool ConfigureRun(const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, const TMap<int32, TObjectPtr<AUnitBase>>& PartyActors, FText& OutError, bool bManaged = false);

    // Only trusted server code may bind a connection; no account claim arrives in the command payload.
    // 신뢰된 서버 코드만 연결을 바인딩하며 명령에는 계정 주장을 받지 않습니다.
    bool BindParticipant(APartyPlayerController* Controller, const FRunAccountId& AccountId);
    bool CanControllerControl(const APartyPlayerController* Controller, const AUnitBase* Unit) const;
    // Retained callers receive an explicit rejection instead of executing a sequential action.
    // 유지된 호출자는 순차 행동 실행 대신 명시적인 거절을 받습니다.
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

    // Revalidate the persistent Host lease before planning mutations and simulation steps.
    // 계획 변경과 시뮬레이션 간격 전에 영속 Host lease를 다시 검증합니다.
    bool HasManagedExecutionAuthority(bool bAllowResumePending) const;

private:
    ACombatManager* GetManager() const;
    bool IsManagedHumanParticipant(const FRunAccountId& AccountId) const;
    bool AllowsStandaloneLegacy(const APartyPlayerController* Controller) const;
    bool HasOriginalOwner(const APlayerUnit* Unit) const;

    UPROPERTY(Transient)
    FRunIdentityData RunIdentity;

    UPROPERTY(Transient)
    TArray<FRunPartyMember> PartyMembers;

    TMap<FGuid, TWeakObjectPtr<AUnitBase>> UnitsById;
    TMap<TWeakObjectPtr<AUnitBase>, FGuid> CharacterIds;
    TMap<TWeakObjectPtr<APartyPlayerController>, FRunAccountId> Participants;
    TMap<TWeakObjectPtr<APartyPlayerController>, FGuid> ParticipantBindingIds;
    FGuid CombatInstanceId;
    FGuid ManagedSessionId;
    bool bRequiresRunConfiguration = false;
    bool bManagedExecution = false;
    bool bRunConfigured = false;
};
