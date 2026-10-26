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
}

bool UEquipmentSkillAssetLibrary::BakeMeleeMontageMotion(UAnimSequence* Destination, UAnimMontage* Source, bool bMirror)
{
    if (!IsValid(Destination) || !Destination->GetPathName().StartsWith(TEXT("/Game/User_JeHoon/"))) return false;
    FMeleeMotion Motion;
    if (!SampleMeleeMotion(Source, bMirror, Motion)) return false;
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
    return bSucceeded && ValidateMeleeMontageMotion(Destination, Source, bMirror);
}

bool UEquipmentSkillAssetLibrary::ValidateMeleeMontageMotion(UAnimSequence* Sequence, UAnimMontage* Source, bool bMirror)
{
    FMeleeMotion Expected;
    if (!IsValid(Sequence) || !SampleMeleeMotion(Source, bMirror, Expected)) return false;
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
    Skill->SkillName = FText::FromString(TEXT("더블어택"));
    Skill->SkillDescription = FText::FromString(TEXT("양손에 단검을 장착해야 합니다. 왼손, 오른손 순서로 각각의 무기 피해량으로 한 번씩 공격합니다. AP 비용은 기본 근접 공격과 같습니다."));
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
    for (const FRunItemDefinition& Weapon : Weapons) if (Weapon.WeaponDamage != 10.f) return false;
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
    if (!RunEquipmentRules::BuildEquippedSkills(Member, Skills, Error) || Skills.Contains(FSoftObjectPath(Skill))) return false;
    OutReport = TEXT("Dual daggers required; left/right damage 10/20; mean 15; single dagger and shield rejected; weapon catalog damage 10; AP matches basic attack.");
    return true;
}
