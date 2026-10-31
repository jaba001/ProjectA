#include "EquipmentSkillAssetLibrary.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "Animation/MirrorDataTable.h"
#include "Animation/Skeleton.h"
#include "AnimationRuntime.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/EquipmentCombatRules.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunTypes.h"
#include "Game/Run/RunEquipmentTypes.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "GAS/CombatGameplayTags.h"
#include "Misc/MemStack.h"
#include "UI/Gameplay/RunItemPresentation.h"

namespace
{
    struct FMeleeMotion
    {
        FFrameRate Rate;
        int32 Frames = 0;
        TArray<FName> Bones;
        TArray<TArray<FTransform>> Tracks;
    };

    bool SampleMeleeMotion(UAnimMontage* Source, bool bMirror, FMeleeMotion& Out)
    {
        if (!IsValid(Source)
            || !IsValid(Source->GetSkeleton())
            || Source->SlotAnimTracks.Num() != 1
            || !FMath::IsFinite(Source->RateScale)
            || Source->RateScale <= 0.f)
        {
            return false;
        }
        Out.Rate = Source->GetSamplingFrameRate();
        const double Duration = Source->GetPlayLength() / Source->RateScale;
        if (!Out.Rate.IsValid()
            || !FMath::IsFinite(Duration)
            || Duration <= 0.0
            || Duration > 10.0)
        {
            return false;
        }
        Out.Frames = FMath::RoundToInt(Duration * Out.Rate.AsDecimal());
        if (Out.Frames < 1
            || Out.Frames > 2400
            || !FMath::IsNearlyEqual(Out.Frames / Out.Rate.AsDecimal(), Duration, 0.0001))
        {
            return false;
        }
        const FReferenceSkeleton& Reference = Source->GetSkeleton()->GetReferenceSkeleton();
        if (Reference.GetNum() > MAX_uint16) return false;
        for (const FAnimSegment& Segment : Source->SlotAnimTracks[0].AnimTrack.AnimSegments)
        {
            const UAnimSequence* Sequence = Cast<UAnimSequence>(Segment.GetAnimReference());
            if (!IsValid(Sequence)
                || Sequence->GetSkeleton() != Source->GetSkeleton()
                || Sequence->AdditiveAnimType != AAT_None
                || Segment.LoopingCount != 1)
            {
                return false;
            }
        }
        FMemMark Mark(FMemStack::Get());
        TArray<FBoneIndexType> Indices;
        for (int32 Index = 0; Index < Reference.GetNum(); ++Index) Indices.Add(static_cast<FBoneIndexType>(Index));
        FBoneContainer Bones;
        Bones.InitializeTo(Indices, UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll), *Source->GetSkeleton());
        Bones.SetUseRAWData(true);
        Bones.SetUseSourceData(true);
        UMirrorDataTable* Mirror = NewObject<UMirrorDataTable>();
        Mirror->RowStruct = FMirrorTableRow::StaticStruct();
        Mirror->Skeleton = Source->GetSkeleton();
        // Manny's component-space X axis separates the hands; central bones mirror against themselves.
        // Manny의 컴포넌트 공간 X축을 기준으로 양손을 교환하고 중앙 본은 자신을 기준으로 반전합니다.
        Mirror->MirrorAxis = EAxis::X;
        for (int32 Index = 0; Index < Reference.GetNum(); ++Index)
        {
            FMirrorTableRow Row;
            Row.Name = Reference.GetBoneName(Index);
            FString Opposite = Row.Name.ToString();
            if (Opposite.EndsWith(TEXT("_l"))) Opposite = Opposite.LeftChop(2) + TEXT("_r");
            else if (Opposite.EndsWith(TEXT("_r"))) Opposite = Opposite.LeftChop(2) + TEXT("_l");
            Row.MirroredName = Reference.FindBoneIndex(FName(*Opposite)) == INDEX_NONE ? Row.Name : FName(*Opposite);
            Mirror->AddRow(Row.Name, Row);
        }
        TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex> MirrorBones;
        TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> ReferenceRotations;
        Mirror->FillCompactPoseAndComponentRefRotations(Bones, MirrorBones, ReferenceRotations);
        Out.Tracks.SetNum(Reference.GetRawBoneNum());
        for (int32 Index = 0; Index < Reference.GetRawBoneNum(); ++Index) Out.Bones.Add(Reference.GetBoneName(Index));
        for (int32 Frame = 0; Frame <= Out.Frames; ++Frame)
        {
            FMemMark FrameMark(FMemStack::Get());
            FCompactPose Pose;
            Pose.SetBoneContainer(&Bones);
            Pose.ResetToRefPose();
            FBlendedCurve Curve;
            Curve.InitFrom(Bones);
            UE::Anim::FStackAttributeContainer Attributes;
            FAnimationPoseData PoseData(Pose, Curve, Attributes);
            const double Seconds = FMath::Min(Frame / Out.Rate.AsDecimal() * Source->RateScale, static_cast<double>(Source->GetPlayLength()));
            Source->SlotAnimTracks[0].AnimTrack.GetAnimationPose(PoseData, FAnimExtractContext(Seconds, Source->HasRootMotion()));
            if (bMirror) FAnimationRuntime::MirrorPose(Pose, Mirror->MirrorAxis, MirrorBones, ReferenceRotations);
            for (int32 Index = 0; Index < Out.Bones.Num(); ++Index)
            {
                const FCompactPoseBoneIndex Compact = Bones.GetCompactPoseIndexFromSkeletonPoseIndex(FSkeletonPoseBoneIndex(Index));
                if (Compact.GetInt() == INDEX_NONE || !Pose[Compact].IsValid()) return false;
                Out.Tracks[Index].Add(Pose[Compact]);
            }
        }
        return true;
    }

    bool BuildMeleeMotion(UAnimMontage* Source, bool bMirror, float EndTime, UAnimSequence* TransitionTarget, float TransitionSeconds, FMeleeMotion& Out)
    {
        if (!FMath::IsFinite(EndTime)
            || EndTime < 0.f
            || !FMath::IsFinite(TransitionSeconds)
            || TransitionSeconds < 0.f
            || !SampleMeleeMotion(Source, bMirror, Out))
        {
            return false;
        }
        if (EndTime > 0.f)
        {
            const int32 Frames = FMath::RoundToInt(EndTime * Out.Rate.AsDecimal());
            if (Frames < 1
                || Frames > Out.Frames
                || !FMath::IsNearlyEqual(Frames / Out.Rate.AsDecimal(), EndTime, 0.0001))
            {
                return false;
            }
            Out.Frames = Frames;
            for (TArray<FTransform>& Track : Out.Tracks) Track.SetNum(Frames + 1);
        }
        if (!IsValid(TransitionTarget)) return !TransitionTarget && TransitionSeconds == 0.f;
        const IAnimationDataModel* Target = TransitionTarget->GetDataModel();
        const int32 BlendFrames = FMath::RoundToInt(TransitionSeconds * Out.Rate.AsDecimal());
        if (!Target
            || TransitionTarget->GetSkeleton() != Source->GetSkeleton()
            || TransitionTarget->AdditiveAnimType != AAT_None
            || BlendFrames < 1
            || BlendFrames > Out.Frames
            || !FMath::IsNearlyEqual(BlendFrames / Out.Rate.AsDecimal(), TransitionSeconds, 0.0001))
        {
            return false;
        }
        // Preserve the strike and blend only the shortened recovery into the next attack's first pose.
        // 타격 동작은 보존하고 단축한 복귀 구간만 다음 공격의 첫 포즈로 연결합니다.
        for (int32 Index = 0; Index < Out.Bones.Num(); ++Index)
        {
            TArray<FTransform> Keys;
            Target->GetBoneTrackTransforms(Out.Bones[Index], Keys);
            if (Keys.IsEmpty() || !Keys[0].IsValid()) return false;
            for (int32 Offset = 0; Offset <= BlendFrames; ++Offset)
            {
                const float Alpha = static_cast<float>(Offset) / BlendFrames;
                const int32 Frame = Out.Frames - BlendFrames + Offset;
                const FTransform Original = Out.Tracks[Index][Frame];
                Out.Tracks[Index][Frame].Blend(Original, Keys[0], Alpha * Alpha * (3.f - 2.f * Alpha));
            }
        }
        return true;
    }
}

bool UEquipmentSkillAssetLibrary::BakeMeleeMontageMotion(UAnimSequence* Destination, UAnimMontage* Source, bool bMirror, float EndTime, UAnimSequence* TransitionTarget, float TransitionSeconds)
{
    if (!IsValid(Destination)
        || Destination == TransitionTarget
        || !Destination->GetPathName().StartsWith(TEXT("/Game/User_JeHoon/")))
    {
        return false;
    }
    FMeleeMotion Motion;
    if (!BuildMeleeMotion(Source, bMirror, EndTime, TransitionTarget, TransitionSeconds, Motion)) return false;
    Destination->Modify();
    Destination->SetSkeleton(Source->GetSkeleton());
    IAnimationDataController& Controller = Destination->GetController();
    Controller.OpenBracket(FText::FromString(TEXT("Bake melee montage / 근접 공격 몽타주 굽기")), false);
    Controller.ResetModel(false);
    Controller.SetFrameRate(Motion.Rate, false);
    Controller.SetNumberOfFrames(FFrameNumber(Motion.Frames), false);
    bool bSucceeded = true;
    for (int32 Index = 0; Index < Motion.Bones.Num(); ++Index)
    {
        TArray<FVector3f> Positions;
        TArray<FQuat4f> Rotations;
        TArray<FVector3f> Scales;
        for (const FTransform& Key : Motion.Tracks[Index])
        {
            Positions.Add(FVector3f(Key.GetTranslation()));
            Rotations.Add(FQuat4f(Key.GetRotation()));
            Scales.Add(FVector3f(Key.GetScale3D()));
        }
        bSucceeded &= Controller.AddBoneCurve(Motion.Bones[Index], false);
        bSucceeded &= Controller.SetBoneTrackKeys(Motion.Bones[Index], Positions, Rotations, Scales, false);
    }
    Controller.NotifyPopulated();
    Controller.CloseBracket(false);
    Destination->bEnableRootMotion = false;
    Destination->bForceRootLock = false;
    Destination->PostEditChange();
    Destination->MarkPackageDirty();
    return bSucceeded && ValidateMeleeMontageMotion(Destination, Source, bMirror, EndTime, TransitionTarget, TransitionSeconds);
}

bool UEquipmentSkillAssetLibrary::ValidateMeleeMontageMotion(UAnimSequence* Sequence, UAnimMontage* Source, bool bMirror, float EndTime, UAnimSequence* TransitionTarget, float TransitionSeconds)
{
    FMeleeMotion Expected;
    if (!IsValid(Sequence)
        || Sequence == TransitionTarget
        || !BuildMeleeMotion(Source, bMirror, EndTime, TransitionTarget, TransitionSeconds, Expected))
    {
        return false;
    }
    const IAnimationDataModel* Model = Sequence->GetDataModel();
    if (!Model
        || Sequence->GetSkeleton() != Source->GetSkeleton()
        || Model->GetFrameRate() != Expected.Rate
        || Model->GetNumberOfKeys() != Expected.Frames + 1)
    {
        return false;
    }
    for (int32 Index = 0; Index < Expected.Bones.Num(); ++Index)
    {
        TArray<FTransform> Keys;
        Model->GetBoneTrackTransforms(Expected.Bones[Index], Keys);
        if (Keys.Num() != Expected.Tracks[Index].Num()) return false;
        for (int32 Frame = 0; Frame < Keys.Num(); ++Frame) if (!Keys[Frame].Equals(Expected.Tracks[Index][Frame], 0.001)) return false;
    }
    return true;
}

bool UEquipmentSkillAssetLibrary::ConfigureDoubleAttack(USkillDefinitionDataAsset* Skill, USkillDefinitionDataAsset* BasicAttack, UAnimMontage* Montage, float Windup, float StrikeInterval)
{
    if (!IsValid(Skill)
        || !IsValid(BasicAttack)
        || Skill == BasicAttack
        || !IsValid(Montage))
    {
        return false;
    }
    FCombatRoundSkill Profile;
    FText Error;
    if (!BasicAttack->ResolveRoundSkill(Profile, Error)) return false;
    Profile.CastMontage = Montage;
    Profile.WindupSeconds = Windup;
    Profile.Power = 10.f;
    Profile.bUseEquippedWeaponDamage = true;
    Profile.bUseWeaponTrace = false;
    Profile.bUseEffectCollision = false;
    Profile.bUseMeleeAreaCollision = false;
    Profile.MeleeArea = ESkillAreaType::Single;
    Profile.EffectTags.AddTag(ProjectACombatTags::Skill_Attack_Melee);
    Profile.RequiredEquipment.Reset();
    const FString RequirementKey = TEXT("Skill.") + Skill->GetPrimaryAssetId().ToString() + TEXT(".EquipmentRequirement");
    Profile.EquipmentRequirementText = FText::ChangeKey(TEXT("ProjectA.Content"), RequirementKey, FText::FromString(TEXT("쌍수 단검 장착 시 사용 가능")));
    Profile.MeleeStrikes.Reset();
    const FGameplayTag Dagger = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Dagger"));
    for (int32 SlotIndex : {1, 0})
    {
        FCombatEquipmentRequirement& Requirement = Profile.RequiredEquipment.AddDefaulted_GetRef();
        Requirement.Slot = URunEquipmentCatalog::GetWeaponSlot(SlotIndex);
        Requirement.ItemQuery = FGameplayTagQuery::MakeQuery_MatchTag(Dagger);
        FCombatMeleeStrike& Strike = Profile.MeleeStrikes.AddDefaulted_GetRef();
        Strike.WeaponSlot = Requirement.Slot;
        Strike.DelaySeconds = SlotIndex == 1 ? 0.f : StrikeInterval;
    }
    if (!CombatRoundRules::IsValidSkill(Profile)) return false;
    Skill->Modify();
    Skill->bUseRoundDefinition = true;
    Skill->RoundDefinition = Profile;
    Skill->AbilityClass = BasicAttack->AbilityClass;
    Skill->ActionPointCost = Profile.ActionPointCost;
    Skill->SkillIcon = BasicAttack->SkillIcon;
    Skill->SkillId = TEXT("DoubleAttack");
    Skill->SkillName = FText::FromString(TEXT("이중타격"));
    Skill->SkillDescription = FText::FromString(TEXT("단검의 기본 스킬입니다. 쌍수 단검 장착 시 사용할 수 있으며 왼손, 오른손 순서로 각각의 무기 피해량으로 한 번씩 공격합니다. AP 비용은 타격과 같습니다."));
    Skill->PostEditChange();
    Skill->MarkPackageDirty();
    return true;
}

bool UEquipmentSkillAssetLibrary::ValidateDoubleAttack(USkillDefinitionDataAsset* Skill, USkillDefinitionDataAsset* BasicAttack, FString& OutReport)
{
    OutReport = TEXT("Double attack asset contract failed / 더블어택 에셋 계약 불일치");
    FCombatRoundSkill Profile;
    FCombatRoundSkill Base;
    FText Error;
    if (!IsValid(Skill)
        || !IsValid(BasicAttack)
        || !Skill->ResolveRoundSkill(Profile, Error)
        || !BasicAttack->ResolveRoundSkill(Base, Error))
    {
        return false;
    }
    if (!Profile.bUseEquippedWeaponDamage
        || Profile.MeleeStrikes.Num() != 2
        || Profile.RequiredEquipment.Num() != 2
        || Profile.ActionPointCost != Base.ActionPointCost)
    {
        return false;
    }
    const FGameplayTag Main = URunEquipmentCatalog::GetWeaponSlot(0);
    const FGameplayTag Off = URunEquipmentCatalog::GetWeaponSlot(1);
    if (Profile.MeleeStrikes[0].WeaponSlot != Off || Profile.MeleeStrikes[1].WeaponSlot != Main) return false;
    TArray<FRunEquipmentVisual> Equipment;
    FRunEquipmentVisual Right;
    Right.SlotTag = Main;
    Right.ItemTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Dagger")));
    Right.WeaponDamage = 20.f;
    FRunEquipmentVisual Left = Right;
    Left.SlotTag = Off;
    Left.WeaponDamage = 10.f;
    Equipment = {Right, Left};
    float Damage = 0.f;
    if (!EquipmentCombatRules::Matches(Equipment, Profile.RequiredEquipment)
        || !EquipmentCombatRules::ResolveDamage(Equipment, FGameplayTag(), Damage)
        || Damage != 15.f)
    {
        return false;
    }
    if (!EquipmentCombatRules::ResolveDamage(Equipment, Off, Damage)
        || Damage != 10.f
        || !EquipmentCombatRules::ResolveDamage(Equipment, Main, Damage)
        || Damage != 20.f)
    {
        return false;
    }
    Equipment.Pop();
    if (EquipmentCombatRules::Matches(Equipment, Profile.RequiredEquipment)) return false;
    Left.ItemTags.Reset();
    Left.ItemTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Shield")));
    Equipment.Add(Left);
    if (EquipmentCombatRules::Matches(Equipment, Profile.RequiredEquipment)
        || !EquipmentCombatRules::ResolveDamage(Equipment, FGameplayTag(), Damage)
        || Damage != 20.f)
    {
        return false;
    }
    TArray<FRunItemDefinition> Weapons;
    if (!RunItemShopCatalog::Load(Weapons, Error)) return false;
    const FSoftObjectPath SkillPath(Skill);
    for (const FRunItemDefinition& Weapon : Weapons)
    {
        const bool bDagger = Weapon.Tags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Dagger")));
        if (Weapon.WeaponDamage != 10.f || EquipmentCombatRules::GetIntrinsicSkills(Weapon.Tags).Contains(SkillPath) != bDagger) return false;
        if (bDagger && (RunItemPresentation::GrantedSkills(Weapon).IsEmpty() || RunItemPresentation::SkillDetails(Weapon).IsEmpty())) return false;
        if (bDagger)
        {
            FRunItemDefinition Generated = Weapon;
            Generated.GenerationVersion = 1;
            Generated.GrantedSkills = {FSoftObjectPath(BasicAttack)};
            const auto Details = RunItemPresentation::SkillDetails(Generated);
            if (Details.Num() != 2
                || Details[0].SkillAsset != FSoftObjectPath(BasicAttack)
                || Details[1].SkillAsset != SkillPath)
            {
                return false;
            }
        }
    }
    FRunPartyMember Member;
    Member.bCreated = true;
    Member.bHasSkillLoadout = true;
    Member.ClassId = TEXT("Rogue");
    Member.InnateSkills = {FSoftObjectPath(BasicAttack)};
    RunEquipmentRules::InitializeStartingEquipment(Member, Weapons);
    if (Member.Items.Num() != 2
        || RunEquipmentRules::FindItemIndexAtSlot(Member, Main) != 0
        || RunEquipmentRules::FindItemIndexAtSlot(Member, Off) != 1)
    {
        return false;
    }
    TArray<FSoftObjectPath> Skills;
    if (!RunEquipmentRules::BuildEquippedSkills(Member, Skills, Error) || !Skills.Contains(FSoftObjectPath(Skill))) return false;
    Member.Equipment.Slots.Pop();
    if (!RunEquipmentRules::BuildEquippedSkills(Member, Skills, Error)
        || !Skills.Contains(SkillPath)
        || Member.Equipment.ConditionalSkillVersion != 2)
    {
        return false;
    }
    Member.Equipment.ConditionalSkillVersion = 1;
    if (!RunEquipmentRules::BuildEquippedSkills(Member, Skills, Error) || Skills.Contains(SkillPath)) return false;
    Member.Equipment.ConditionalSkillVersion = 0;
    if (!RunEquipmentRules::BuildEquippedSkills(Member, Skills, Error) || Skills.Contains(SkillPath)) return false;
    Member.Equipment.ConditionalSkillVersion = 2;
    Member.Equipment.Slots.Reset();
    if (!RunEquipmentRules::BuildEquippedSkills(Member, Skills, Error) || Skills.Contains(SkillPath)) return false;
    if (Profile.EquipmentRequirementText.IsEmpty() || Skill->SkillName.ToString() != TEXT("이중타격")) return false;
    OutReport = TEXT("Dual daggers required; single dagger grants skill; last removal clears; legacy v0/v1 preserved; item details; damage 10/20; mean 15; AP matches basic attack.");
    return true;
}
