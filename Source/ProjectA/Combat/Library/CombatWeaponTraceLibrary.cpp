#include "Combat/Library/CombatWeaponTraceLibrary.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "Animation/Skeleton.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/MemStack.h"
#include "Unit/UnitBase.h"
#include "UObject/UnrealType.h"

namespace
{
    const UStaticMeshComponent* FindWeapon(const AUnitBase* Source, FName ComponentName)
    {
        if (ComponentName.IsNone()) return nullptr;
        // SCS variables bind generated component instances even when their object names acquire suffixes.
        // SCS 변수는 오브젝트 이름에 접미사가 붙어도 생성된 컴포넌트 인스턴스를 연결합니다.
        if (const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Source->GetClass(), ComponentName)) return Cast<UStaticMeshComponent>(Property->GetObjectPropertyValue_InContainer(Source));
        TArray<UStaticMeshComponent*> Components;
        Source->GetComponents(Components);
        const UStaticMeshComponent* Result = nullptr;
        for (const UStaticMeshComponent* Component : Components)
        {
            if (!IsValid(Component) || Component->GetFName() != ComponentName) continue;
            if (Result) return nullptr;
            Result = Component;
        }
        return Result;
    }

    bool IsFiniteBlade(const CombatWeaponTrace::FBladePose& Pose)
    {
        return !Pose.Base.ContainsNaN() && !Pose.Tip.ContainsNaN();
    }
}

bool CombatWeaponTrace::SampleBoneTransform(const USkeletalMesh* Mesh, const UAnimMontage* Montage, FName Slot, FName BoneOrSocket, double MontageSeconds, FTransform& OutComponentTransform)
{
    OutComponentTransform = FTransform::Identity;
    if (!IsValid(Mesh) || !IsValid(Mesh->GetSkeleton()) || !IsValid(Montage) || Montage->GetSkeleton() != Mesh->GetSkeleton() || !FMath::IsFinite(MontageSeconds) || !FMath::IsFinite(Montage->GetPlayLength()) || MontageSeconds < 0.0 || MontageSeconds > Montage->GetPlayLength() || Montage->SlotAnimTracks.Num() != 1 || Slot.IsNone() || BoneOrSocket.IsNone()) return false;
    const FSlotAnimationTrack& Track = Montage->SlotAnimTracks[0];
    if (Track.SlotName != Slot) return false;
    const FAnimSegment* Segment = Track.AnimTrack.GetSegmentAtTime(static_cast<float>(MontageSeconds));
    if (!Segment || !FMath::IsFinite(Segment->GetValidPlayRate()) || FMath::IsNearlyZero(Segment->AnimPlayRate) || Segment->LoopingCount != 1) return false;
    float SequenceSeconds = 0.f;
    const UAnimSequence* Sequence = Cast<UAnimSequence>(Segment->GetAnimationData(static_cast<float>(MontageSeconds), SequenceSeconds));
    if (!IsValid(Sequence) || Sequence->GetSkeleton() != Mesh->GetSkeleton() || Sequence->AdditiveAnimType != AAT_None || !FMath::IsFinite(Sequence->GetPlayLength()) || Sequence->GetPlayLength() <= 0.f || !FMath::IsFinite(Sequence->RateScale) || FMath::IsNearlyZero(Sequence->RateScale) || !FMath::IsFinite(SequenceSeconds) || SequenceSeconds < 0.f || SequenceSeconds > Sequence->GetPlayLength()) return false;

    FTransform AttachmentOffset = FTransform::Identity;
    if (const USkeletalMeshSocket* Socket = Mesh->FindSocket(BoneOrSocket))
    {
        BoneOrSocket = Socket->BoneName;
        AttachmentOffset = Socket->GetSocketLocalTransform();
    }
    const FReferenceSkeleton& ReferenceSkeleton = Mesh->GetRefSkeleton();
    const int32 AttachmentIndex = ReferenceSkeleton.FindBoneIndex(BoneOrSocket);
    if (AttachmentIndex == INDEX_NONE || ReferenceSkeleton.GetNum() > MAX_uint16) return false;

    // Engine pose extraction preserves mesh retargeting and root-lock behavior on servers without a rendered AnimInstance.
    // 엔진 포즈 추출로 화면 AnimInstance가 없는 서버에서도 메시 리타깃과 루트 잠금 동작을 유지합니다.
    FMemMark Mark(FMemStack::Get());
    TArray<FBoneIndexType> BoneIndices;
    BoneIndices.Reserve(ReferenceSkeleton.GetNum());
    for (int32 Index = 0; Index < ReferenceSkeleton.GetNum(); ++Index) BoneIndices.Add(static_cast<FBoneIndexType>(Index));
    FBoneContainer RequiredBones;
    RequiredBones.InitializeTo(BoneIndices, UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll), *Mesh);
    FCompactPose Pose;
    Pose.SetBoneContainer(&RequiredBones);
    Pose.ResetToRefPose();
    FBlendedCurve Curve;
    Curve.InitFrom(RequiredBones);
    UE::Anim::FStackAttributeContainer Attributes;
    FAnimationPoseData PoseData(Pose, Curve, Attributes);
    const FAnimExtractContext ExtractContext(static_cast<double>(SequenceSeconds), Montage->HasRootMotion());
    Sequence->GetAnimationPose(PoseData, ExtractContext);
    FTransform BoneTransform = FTransform::Identity;
    for (int32 Index = AttachmentIndex; Index != INDEX_NONE; Index = ReferenceSkeleton.GetParentIndex(Index))
    {
        const FCompactPoseBoneIndex PoseIndex = RequiredBones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index));
        if (PoseIndex.GetInt() == INDEX_NONE || !Pose[PoseIndex].IsValid()) return false;
        BoneTransform = BoneTransform * Pose[PoseIndex];
    }
    OutComponentTransform = AttachmentOffset * BoneTransform;
    return OutComponentTransform.IsValid();
}

bool CombatWeaponTrace::SampleBlade(const AUnitBase* Source, const FCombatRoundSkill& Skill, const UAnimMontage* Montage, double MontageSeconds, FBladePose& Out)
{
    Out = FBladePose();
    if (!IsValid(Source) || Skill.WeaponBaseSocket.IsNone() || Skill.WeaponTipSocket.IsNone()) return false;
    const USkeletalMeshComponent* Mesh = Source->GetMesh();
    const USkeletalMesh* MeshAsset = IsValid(Mesh) ? Mesh->GetSkeletalMeshAsset() : nullptr;
    const UStaticMeshComponent* Weapon = FindWeapon(Source, Skill.WeaponComponentName);
    if (!IsValid(MeshAsset) || !IsValid(Weapon) || Weapon->GetOwner() != Source || Weapon->GetAttachParent() != Mesh || Weapon->IsUsingAbsoluteLocation() || Weapon->IsUsingAbsoluteRotation() || Weapon->IsUsingAbsoluteScale()) return false;
    const UStaticMeshSocket* BaseSocket = Weapon->GetSocketByName(Skill.WeaponBaseSocket);
    const UStaticMeshSocket* TipSocket = Weapon->GetSocketByName(Skill.WeaponTipSocket);
    if (!IsValid(BaseSocket) || !IsValid(TipSocket)) return false;
    FTransform AttachmentTransform;
    if (!SampleBoneTransform(MeshAsset, Montage, Skill.WeaponMontageSlot, Weapon->GetAttachSocketName(), MontageSeconds, AttachmentTransform)) return false;
    const FTransform WeaponTransform = Weapon->GetRelativeTransform() * AttachmentTransform * Mesh->GetComponentTransform();
    if (!WeaponTransform.IsValid() || BaseSocket->RelativeLocation.ContainsNaN() || TipSocket->RelativeLocation.ContainsNaN()) return false;
    Out.Base = WeaponTransform.TransformPosition(BaseSocket->RelativeLocation);
    Out.Tip = WeaponTransform.TransformPosition(TipSocket->RelativeLocation);
    return IsFiniteBlade(Out) && !Out.Base.Equals(Out.Tip, UE_KINDA_SMALL_NUMBER);
}

AUnitBase* CombatWeaponTrace::FindFirstHit(UWorld* World, AUnitBase* Source, const TArray<FCombatRoundUnitView>& Units, const FBladePose& Previous, const FBladePose& Current, float Radius)
{
    if (!IsValid(World) || !IsValid(Source) || Source->GetWorld() != World || !Source->IsUnitAlive() || !IsFiniteBlade(Previous) || !IsFiniteBlade(Current) || !FMath::IsFinite(Radius) || Radius <= 0.f) return nullptr;
    const double Length = FMath::Max(FVector::Distance(Previous.Base, Previous.Tip), FVector::Distance(Current.Base, Current.Tip));
    const double Intervals = FMath::CeilToDouble(Length / Radius);
    if (!FMath::IsFinite(Intervals) || Intervals < 1.0 || Intervals > 1024.0) return nullptr;
    const int32 PointIntervals = static_cast<int32>(Intervals);
    const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatWeaponTrace), false, Source);
    Params.bFindInitialOverlaps = true;
    Params.bIgnoreTouches = true;
    for (TActorIterator<APawn> It(World); It; ++It) Params.AddIgnoredActor(*It);
    FCollisionResponseParams Responses(ECR_Ignore);
    Responses.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
    Responses.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);
    TArray<const FCombatRoundUnitView*> Candidates;
    const FVector Origin = Source->GetCapsuleComponent()->GetComponentLocation();
    for (const FCombatRoundUnitView& Candidate : Units)
    {
        AUnitBase* Unit = Candidate.Unit;
        if (!IsValid(Unit) || Unit == Source || Unit->GetWorld() != World || !Unit->IsUnitAlive() || Unit->GetTeam() == Source->GetTeam() || !Unit->GetActorEnableCollision()) continue;
        UCapsuleComponent* Capsule = Unit->GetCapsuleComponent();
        if (!IsValid(Capsule) || !Capsule->IsQueryCollisionEnabled()) continue;
        FVector Contact;
        // A blade that crossed a wall in an earlier sample must not hit an occluded target afterward.
        // 이전 표본에서 벽을 통과한 칼날도 이후에 벽으로 가려진 대상을 맞히지 못하게 합니다.
        if (Capsule->GetClosestPointOnCollision(Origin, Contact) < 0.f || World->LineTraceTestByChannel(Origin, Contact, ECC_WorldDynamic, Params, Responses)) continue;
        Candidates.Add(&Candidate);
    }
    if (Candidates.IsEmpty()) return nullptr;
    AUnitBase* FirstUnit = nullptr;
    int32 FirstUnitId = MAX_int32;
    float FirstTime = 1.f;
    float FirstWallTime = 1.f;
    bool bHitWall = false;
    // Closely spaced blade samples cover its length; the caller supplies intermediate animation times for curved swings.
    // 촘촘한 칼날 표본으로 길이를 덮으며 곡선 휘두르기의 중간 애니메이션 시각은 호출부가 제공합니다.
    for (int32 PointIndex = 0; PointIndex <= PointIntervals; ++PointIndex)
    {
        const double Alpha = static_cast<double>(PointIndex) / PointIntervals;
        const FVector Start = FMath::Lerp(Previous.Base, Previous.Tip, Alpha);
        const FVector End = FMath::Lerp(Current.Base, Current.Tip, Alpha);
        if (World->OverlapBlockingTestByChannel(Start, FQuat::Identity, ECC_WorldDynamic, Shape, Params, Responses)) return nullptr;
        FHitResult WallHit;
        if (World->SweepSingleByChannel(WallHit, Start, End, FQuat::Identity, ECC_WorldDynamic, Shape, Params, Responses))
        {
            bHitWall = true;
            FirstWallTime = FMath::Min(FirstWallTime, WallHit.bStartPenetrating ? 0.f : WallHit.Time);
        }
        for (const FCombatRoundUnitView* Candidate : Candidates)
        {
            AUnitBase* Unit = Candidate->Unit;
            UCapsuleComponent* Capsule = Unit->GetCapsuleComponent();
            FHitResult Hit;
            const bool bInitialOverlap = Capsule->OverlapComponent(Start, FQuat::Identity, Shape);
            if (!bInitialOverlap && !Capsule->SweepComponent(Hit, Start, End, FQuat::Identity, Shape)) continue;
            const float HitTime = bInitialOverlap || Hit.bStartPenetrating ? 0.f : Hit.Time;
            if (!FirstUnit || HitTime < FirstTime || (HitTime == FirstTime && Candidate->UnitId < FirstUnitId))
            {
                FirstUnit = Unit;
                FirstUnitId = Candidate->UnitId;
                FirstTime = HitTime;
            }
        }
    }
    return bHitWall && FirstWallTime <= FirstTime ? nullptr : FirstUnit;
}
