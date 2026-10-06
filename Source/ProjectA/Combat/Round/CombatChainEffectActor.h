#pragma once

#include "CoreMinimal.h"
#include "Combat/Round/CombatSkillEffectActor.h"
#include "CombatChainEffectActor.generated.h"

class UCapsuleComponent;

// Store chain progress as identifiers and values; actor lookup remains a transient server concern.
// 연쇄 진행은 식별자와 값으로 저장하며 액터 조회는 서버의 임시 처리로 유지합니다.
USTRUCT(BlueprintType)
struct PROJECTA_API FCombatChainRuntimeData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 SourceUnitId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly)
    int32 PreviousUnitId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly)
    int32 TargetUnitId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly)
    TArray<int32> HitUnitIds;

    UPROPERTY(BlueprintReadOnly)
    int32 HopIndex = 0;

    UPROPERTY(BlueprintReadOnly)
    double ElapsedSeconds = 0.0;

    UPROPERTY(BlueprintReadOnly)
    double NextHopTime = 0.0;

    UPROPERTY(BlueprintReadOnly)
    double SegmentStartedElapsed = 0.0;

    UPROPERTY(BlueprintReadOnly)
    FVector SegmentSourcePosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FVector LastHitPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FQuat InitialRotation = FQuat::Identity;

    UPROPERTY(BlueprintReadOnly)
    bool bContactWindowStarted = false;
};

// Replicate ordered segment history while presenting only the latest received connection.
// 구간 이력은 순서대로 복제하며 수신한 최신 연결만 표시합니다.
USTRUCT()
struct PROJECTA_API FCombatChainSegment
{
    GENERATED_BODY()

    UPROPERTY()
    int32 Sequence = INDEX_NONE;

    UPROPERTY()
    int32 SourceUnitId = INDEX_NONE;

    UPROPERTY()
    int32 TargetUnitId = INDEX_NONE;

    UPROPERTY()
    FVector SourcePosition = FVector::ZeroVector;

    UPROPERTY()
    FVector TargetPosition = FVector::ZeroVector;

    UPROPERTY()
    double ServerStartedAt = 0.0;
};

USTRUCT()
struct PROJECTA_API FCombatChainPresentation
{
    GENERATED_BODY()

    UPROPERTY()
    FCombatSkillVfx Vfx;

    UPROPERTY()
    FCombatSkillVfx ImpactVfx;

    UPROPERTY()
    FVector EffectOffset = FVector::ZeroVector;

    // Bound imported loops after the contact window while natural FX and audio completion may release the holder sooner.
    // 접촉 구간 뒤 임포트한 반복 효과를 제한하며 FX·사운드 자연 완료 시 보관 액터를 먼저 해제할 수 있습니다.
    UPROPERTY()
    float VisualLifetime = 0.f;

    UPROPERTY()
    int32 MaxTargets = 0;

    UPROPERTY()
    bool bReady = false;
};

// Retire old segment particles immediately while allowing separate audio to finish on its local holder.
// 이전 구간 파티클은 즉시 제거하고 별도 사운드는 로컬 보관 액터에서 완료하도록 유지합니다.
USTRUCT()
struct FCombatChainSegmentPresentation
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<AActor> Holder = nullptr;

    UPROPERTY()
    TArray<TObjectPtr<UFXSystemComponent>> Components;

    int32 Sequence = INDEX_NONE;
    CombatSkillPresentation::FAudioState Audio;
};

// The coordinator advances authoritative hits; actor Tick only maintains and releases segment visuals.
// 조정자가 권한 있는 피격을 진행하며 액터 Tick은 구간 연출의 유지와 해제만 처리합니다.
UCLASS()
class PROJECTA_API ACombatChainEffectActor : public ACombatSkillEffectActor
{
    GENERATED_BODY()

public:
    ACombatChainEffectActor();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void InitializeEffect(AUnitBase* Source, AUnitBase* Target, FVector AimLocation, const FCombatRoundSkill& Skill, const TArray<FCombatRoundUnitView>& Units, double PresentationTime = -1.0) override;
    virtual void AdvanceEffect(float DeltaSeconds, double PresentationTime = -1.0) override;
    const FCombatChainRuntimeData& GetChainRuntimeData() const { return Runtime; }

private:
    enum class EContactResult : uint8
    {
        Pending,
        Ready,
        Invalid
    };

    AUnitBase* ResolveRegisteredUnit(int32 UnitId) const;
    AUnitBase* ResolveVisualUnit(int32 UnitId) const;
    bool IsEligibleTarget(AUnitBase* Target, UCapsuleComponent*& OutCapsule) const;
    bool IsUnblockedContact(FVector Origin, UCapsuleComponent* Capsule, FVector& OutContact, bool* bOriginBlocked = nullptr) const;
    EContactResult CheckCurrentContact(AUnitBase*& OutTarget, FVector& OutContact) const;
    int32 FindNextTarget(FVector Origin) const;
    void BeginSegment(int32 PreviousUnitId, int32 TargetUnitId, FVector SourcePosition, double PresentationTime);
    void ResolveChain();
    double GetServerTime() const;
    void UpdatePresentations();
    void DestroyPresentation(FCombatChainSegmentPresentation& Presentation);

    UFUNCTION()
    void OnRep_ChainPresentation();

    UFUNCTION()
    void HandleChainOwnerDestroyed(AActor* DestroyedActor);

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastChainImpact(const FCombatSkillVfx& ImpactVisual, const FTransform& Transform);

    UPROPERTY(ReplicatedUsing = OnRep_ChainPresentation)
    FCombatChainPresentation ChainPresentation;

    UPROPERTY(ReplicatedUsing = OnRep_ChainPresentation)
    TArray<FCombatChainSegment> Segments;

    UPROPERTY(Transient)
    FCombatRoundSkill ChainDefinition;

    UPROPERTY(Transient)
    FCombatChainRuntimeData Runtime;

    UPROPERTY(Transient)
    TArray<FCombatChainSegmentPresentation> Presentations;

    TMap<int32, TWeakObjectPtr<AUnitBase>> RegisteredUnits;
    int32 LatestPresentedSequence = INDEX_NONE;
    double SegmentPresentationStartedAt = -1.0;
    bool bChainInitialized = false;
};
