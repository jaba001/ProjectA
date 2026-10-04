#include "MonsterAssetLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/Skeleton.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Unit/MonsterAnimInstance.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogMonsterAssetLibrary, Log, All);

bool UMonsterAssetLibrary::ValidateMonsterMontage(UAnimMontage* Montage, UAnimSequence* Source)
{
    if (!IsValid(Montage) || !IsValid(Source) || !Montage->GetOutermost()->GetName().StartsWith(TEXT("/Game/User_JeHoon/")) || Montage->GetSkeleton() != Source->GetSkeleton()) return false;
    if (Montage->SlotAnimTracks.Num() != 1 || Montage->SlotAnimTracks[0].SlotName != FAnimSlotGroup::DefaultSlotName || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1 || Montage->CompositeSections.Num() != 1 || !Montage->Notifies.IsEmpty()) return false;
    const FAnimSegment& Segment = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
    const FCompositeSection& Section = Montage->CompositeSections[0];
    return Segment.GetAnimReference() == Source && FMath::IsNearlyZero(Segment.StartPos) && FMath::IsNearlyZero(Segment.AnimStartTime) && FMath::IsNearlyEqual(Segment.AnimEndTime, Source->GetPlayLength()) && FMath::IsNearlyEqual(Segment.AnimPlayRate, 1.f) && Segment.LoopingCount == 1 && Section.SectionName == TEXT("Default") && FMath::IsNearlyZero(Section.GetTime()) && Section.NextSectionName.IsNone() && FMath::IsNearlyEqual(Montage->RateScale, 1.f) && FMath::IsNearlyEqual(Montage->GetPlayLength(), Source->GetPlayLength()) && Montage->bEnableAutoBlendOut;
}

namespace
{
    const FName AuthorKey(TEXT("ProjectA.MonsterAuthoring"));
    const TCHAR* BlendSpaceOwner = TEXT("BlendSpace1D.v1");
    const TCHAR* BlueprintOwner = TEXT("LocomotionAnimBlueprint.v1");

    bool IsProjectAsset(const UObject* Asset)
    {
        return IsValid(Asset) && Asset->GetOutermost()->GetName().StartsWith(TEXT("/Game/User_JeHoon/"));
    }

    bool HasOwner(const UObject* Asset, const TCHAR* Owner)
    {
        return IsProjectAsset(Asset) && Asset->GetOutermost()->GetMetaData().GetValue(Asset, AuthorKey) == Owner;
    }

    bool ReportMonsterAssetFailure(const TCHAR* Reason)
    {
        UE_LOG(LogMonsterAssetLibrary, Error, TEXT("%s"), Reason);
        return false;
    }

    bool ValidBlendSpaceInputs(UBlendSpace1D* BlendSpace, UAnimSequence* Idle, UAnimSequence* Walk, UAnimSequence* Run, float WalkSpeed, float RunSpeed)
    {
        if (!IsProjectAsset(BlendSpace) || !IsValid(Idle) || !IsValid(Walk) || !IsValid(Run) || !IsValid(Idle->GetSkeleton())) return false;
        if (!FMath::IsFinite(WalkSpeed) || !FMath::IsFinite(RunSpeed) || WalkSpeed <= UE_KINDA_SMALL_NUMBER || RunSpeed - WalkSpeed <= UE_KINDA_SMALL_NUMBER) return false;
        if (Walk->GetSkeleton() != Idle->GetSkeleton() || Run->GetSkeleton() != Idle->GetSkeleton() || (BlendSpace->GetSkeleton() && BlendSpace->GetSkeleton() != Idle->GetSkeleton())) return false;
        for (const UAnimSequence* Sequence : {Idle, Walk, Run})
        {
            if (!FMath::IsFinite(Sequence->GetPlayLength()) || Sequence->GetPlayLength() <= 0.f || Sequence->GetAdditiveAnimType() != AAT_None) return false;
        }
        return true;
    }

    bool CheckBlendSpace(UBlendSpace1D* BlendSpace, UAnimSequence* Idle, UAnimSequence* Walk, UAnimSequence* Run, float WalkSpeed, float RunSpeed)
    {
        if (!ValidBlendSpaceInputs(BlendSpace, Idle, Walk, Run, WalkSpeed, RunSpeed) || BlendSpace->GetSkeleton() != Idle->GetSkeleton()) return false;
        const FBlendParameter& Axis = BlendSpace->GetBlendParameter(0);
        if (Axis.DisplayName != TEXT("GroundSpeed") || !FMath::IsNearlyZero(Axis.Min) || !FMath::IsNearlyEqual(Axis.Max, RunSpeed) || Axis.GridNum != 8 || Axis.bSnapToGrid || Axis.bWrapInput) return false;
        const TArray<FBlendSample>& Samples = BlendSpace->GetBlendSamples();
        if (Samples.Num() != 3) return false;
        const UAnimSequence* Sequences[] = {Idle, Walk, Run};
        const float Speeds[] = {0.f, WalkSpeed, RunSpeed};
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const FBlendSample& Sample = Samples[Index];
            if (!Sample.bIsValid || Sample.Animation != Sequences[Index] || !Sample.SampleValue.Equals(FVector(Speeds[Index], 0.f, 0.f), 0.001) || !FMath::IsNearlyEqual(Sample.RateScale, 1.f)) return false;
        }
        return true;
    }

    UAnimGraphNode_Root* FindRoot(UAnimBlueprint* Blueprint)
    {
        if (!IsValid(Blueprint)) return nullptr;
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        UAnimGraphNode_Root* Result = nullptr;
        for (UEdGraph* Graph : Graphs)
        {
            if (!Graph || Graph->GetFName() != TEXT("AnimGraph") || !Cast<UAnimationGraphSchema>(Graph->GetSchema())) continue;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (UAnimGraphNode_Root* Root = Cast<UAnimGraphNode_Root>(Node))
                {
                    if (Result) return nullptr;
                    Result = Root;
                }
            }
        }
        return Result;
    }

    UEdGraphPin* PosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
    {
        if (!Node) return nullptr;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == Direction && UAnimationGraphSchema::IsPosePin(Pin->PinType)) return Pin;
        }
        return nullptr;
    }

    bool LinkedPair(const UEdGraphPin* Output, const UEdGraphPin* Input)
    {
        return Output && Input && Output->Direction == EGPD_Output && Input->Direction == EGPD_Input && Input->LinkedTo.Num() == 1 && Input->LinkedTo[0] == Output && Output->LinkedTo.Num() == 1 && Output->LinkedTo[0] == Input;
    }

    bool ValidBlueprintInputs(UAnimBlueprint* Blueprint, UBlendSpace* BlendSpace, FName SlotName, bool bReportErrors = false)
    {
        const TCHAR* Reason = nullptr;
        if (!IsProjectAsset(Blueprint)) Reason = TEXT("Monster AnimBlueprint destination must belong to /Game/User_JeHoon/ / 몬스터 AnimBlueprint 대상은 프로젝트 전용 경로에 있어야 합니다");
        else if (!IsValid(Blueprint->ParentClass) || !Blueprint->ParentClass->IsChildOf(UMonsterAnimInstance::StaticClass())) Reason = TEXT("Monster AnimBlueprint parent must derive from MonsterAnimInstance / 몬스터 AnimBlueprint 부모는 MonsterAnimInstance를 상속해야 합니다");
        else if (!IsValid(Blueprint->TargetSkeleton)) Reason = TEXT("Monster AnimBlueprint target skeleton is missing / 몬스터 AnimBlueprint 대상 스켈레톤이 없습니다");
        else if (!IsValid(BlendSpace) || Blueprint->TargetSkeleton != BlendSpace->GetSkeleton()) Reason = TEXT("Monster AnimBlueprint and BlendSpace skeletons differ / 몬스터 AnimBlueprint와 BlendSpace 스켈레톤이 다릅니다");
        else if (SlotName.IsNone()) Reason = TEXT("Monster AnimBlueprint slot name is empty / 몬스터 AnimBlueprint 슬롯 이름이 없습니다");
        else if (!Blueprint->TargetSkeleton->ContainsSlotName(SlotName))
        {
            // UE routes a missing DefaultSlot through DefaultGroup, including deterministic cook paths.
            // UE는 결정적 쿠킹 경로에서도 미등록 DefaultSlot을 DefaultGroup으로 처리합니다.
            if (SlotName != FAnimSlotGroup::DefaultSlotName) Reason = TEXT("Custom monster slots must already exist on the original skeleton / 사용자 정의 몬스터 슬롯은 원본 스켈레톤에 이미 있어야 합니다");
            else if (Blueprint->TargetSkeleton->GetSlotGroupName(SlotName) != FAnimSlotGroup::DefaultGroupName) Reason = TEXT("Missing DefaultSlot must resolve to the engine DefaultGroup / 미등록 DefaultSlot은 엔진 DefaultGroup으로 해석되어야 합니다");
        }
        return !Reason || (bReportErrors && ReportMonsterAssetFailure(Reason));
    }

    bool CheckBlueprint(UAnimBlueprint* Blueprint, UBlendSpace* BlendSpace, FName SlotName)
    {
        if (!ValidBlueprintInputs(Blueprint, BlendSpace, SlotName) || !Blueprint->IsUpToDate()) return false;
        UAnimGraphNode_Root* Root = FindRoot(Blueprint);
        if (!Root || Root->GetGraph()->Nodes.Num() != 4) return false;
        UAnimGraphNode_BlendSpacePlayer* Player = nullptr;
        UAnimGraphNode_Slot* Slot = nullptr;
        UK2Node_VariableGet* Speed = nullptr;
        for (UEdGraphNode* Node : Root->GetGraph()->Nodes)
        {
            if (UAnimGraphNode_BlendSpacePlayer* PlayerNode = Cast<UAnimGraphNode_BlendSpacePlayer>(Node)) Player = PlayerNode;
            else if (UAnimGraphNode_Slot* SlotNode = Cast<UAnimGraphNode_Slot>(Node)) Slot = SlotNode;
            else if (UK2Node_VariableGet* SpeedNode = Cast<UK2Node_VariableGet>(Node)) Speed = SpeedNode;
        }
        if (!Player || !Slot || !Speed || Player->Node.GetBlendSpace() != BlendSpace || !Player->Node.IsLooping() || Slot->Node.SlotName != SlotName || Speed->GetVarName() != GET_MEMBER_NAME_CHECKED(UMonsterAnimInstance, GroundSpeed) || !Speed->VariableReference.IsSelfContext()) return false;
        const UMonsterAnimInstance* Defaults = Blueprint->GeneratedClass ? Cast<UMonsterAnimInstance>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
        return Defaults && Defaults->RootMotionMode == ERootMotionMode::IgnoreRootMotion && LinkedPair(Speed->GetValuePin(), Player->FindPin(TEXT("X"), EGPD_Input)) && LinkedPair(PosePin(Player, EGPD_Output), PosePin(Slot, EGPD_Input)) && LinkedPair(PosePin(Slot, EGPD_Output), PosePin(Root, EGPD_Input));
    }
}

FCombatRoundSkill UMonsterAssetLibrary::GetResolvedMonsterSkill(USkillDefinitionDataAsset* SkillAsset)
{
    FCombatRoundSkill Resolved;
    FText Error;
    if (!IsValid(SkillAsset) || !SkillAsset->ResolveRoundSkill(Resolved, Error))
    {
        UE_LOG(LogMonsterAssetLibrary, Error, TEXT("Cannot resolve monster skill %s: %s / 몬스터 스킬을 해석할 수 없습니다"), *GetPathNameSafe(SkillAsset), *Error.ToString());
        return FCombatRoundSkill();
    }
    return Resolved;
}

bool UMonsterAssetLibrary::ValidateMonsterSkill(USkillDefinitionDataAsset* SkillAsset)
{
    FCombatRoundSkill Resolved;
    FText Error;
    return IsValid(SkillAsset) && SkillAsset->ResolveRoundSkill(Resolved, Error);
}

bool UMonsterAssetLibrary::ConfigureMonsterBlendSpace(UBlendSpace1D* BlendSpace, UAnimSequence* Idle, UAnimSequence* Walk, UAnimSequence* Run, float WalkSpeed, float RunSpeed)
{
    if (!ValidBlendSpaceInputs(BlendSpace, Idle, Walk, Run, WalkSpeed, RunSpeed)) return ReportMonsterAssetFailure(TEXT("Monster BlendSpace requires a project destination, matching skeletons, nonadditive sequences and increasing finite speeds / 몬스터 BlendSpace에는 프로젝트 대상과 일치하는 스켈레톤, 일반 시퀀스, 증가하는 유한 속도가 필요합니다"));
    if (HasOwner(BlendSpace, BlendSpaceOwner)) return CheckBlendSpace(BlendSpace, Idle, Walk, Run, WalkSpeed, RunSpeed) || ReportMonsterAssetFailure(TEXT("The saved monster BlendSpace differs; preserve it for explicit repair / 저장된 몬스터 BlendSpace가 달라 명시적 수정을 위해 보존합니다"));
    if (BlendSpace->GetOutermost()->GetMetaData().HasValue(BlendSpace, AuthorKey) || !BlendSpace->GetBlendSamples().IsEmpty()) return ReportMonsterAssetFailure(TEXT("Only a new empty monster BlendSpace may be configured / 새로운 빈 몬스터 BlendSpace만 구성할 수 있습니다"));
    FStructProperty* ParameterProperty = FindFProperty<FStructProperty>(UBlendSpace::StaticClass(), TEXT("BlendParameters"));
    if (!ParameterProperty || ParameterProperty->Struct != FBlendParameter::StaticStruct() || ParameterProperty->ArrayDim != 3) return ReportMonsterAssetFailure(TEXT("The engine BlendSpace axis property is unavailable / 엔진 BlendSpace 축 프로퍼티를 확인할 수 없습니다"));
    FBlendParameter* Axis = ParameterProperty->ContainerPtrToValuePtr<FBlendParameter>(BlendSpace, 0);
    const FBlendParameter PreviousAxis = *Axis;
    USkeleton* PreviousSkeleton = BlendSpace->GetSkeleton();
    const auto RestoreEmptyAsset = [&]()
    {
        while (!BlendSpace->GetBlendSamples().IsEmpty()) BlendSpace->DeleteSample(BlendSpace->GetBlendSamples().Num() - 1);
        *Axis = PreviousAxis;
        BlendSpace->SetSkeleton(PreviousSkeleton);
        BlendSpace->PostEditChange();
        BlendSpace->ResampleData();
    };
    BlendSpace->Modify();
    BlendSpace->SetSkeleton(Idle->GetSkeleton());
    // Use reflected editor data because UE 5.8 exposes no public axis mutation API.
    // UE 5.8에는 공개 축 변경 API가 없으므로 리플렉션으로 에디터 데이터를 설정합니다.
    Axis->DisplayName = TEXT("GroundSpeed");
    Axis->Min = 0.f;
    Axis->Max = RunSpeed;
    Axis->GridNum = 8;
    Axis->bSnapToGrid = false;
    Axis->bWrapInput = false;
    if (BlendSpace->AddSample(Idle, FVector::ZeroVector) == INDEX_NONE || BlendSpace->AddSample(Walk, FVector(WalkSpeed, 0.f, 0.f)) == INDEX_NONE || BlendSpace->AddSample(Run, FVector(RunSpeed, 0.f, 0.f)) == INDEX_NONE)
    {
        RestoreEmptyAsset();
        return ReportMonsterAssetFailure(TEXT("Could not add the monster locomotion samples; restored the empty asset / 몬스터 이동 표본 추가에 실패하여 빈 에셋을 복구했습니다"));
    }
    BlendSpace->ValidateSampleData();
    BlendSpace->PostEditChange();
    BlendSpace->ResampleData();
    if (!CheckBlendSpace(BlendSpace, Idle, Walk, Run, WalkSpeed, RunSpeed))
    {
        RestoreEmptyAsset();
        return ReportMonsterAssetFailure(TEXT("Authored monster BlendSpace validation failed; restored the empty asset / 작성된 몬스터 BlendSpace 검증에 실패하여 빈 에셋을 복구했습니다"));
    }
    BlendSpace->GetOutermost()->GetMetaData().SetValue(BlendSpace, AuthorKey, BlendSpaceOwner);
    BlendSpace->MarkPackageDirty();
    return true;
}

bool UMonsterAssetLibrary::ValidateMonsterBlendSpace(UBlendSpace1D* BlendSpace, UAnimSequence* Idle, UAnimSequence* Walk, UAnimSequence* Run, float WalkSpeed, float RunSpeed)
{
    return HasOwner(BlendSpace, BlendSpaceOwner) && CheckBlendSpace(BlendSpace, Idle, Walk, Run, WalkSpeed, RunSpeed);
}

bool UMonsterAssetLibrary::ConfigureLocomotionAnimBlueprint(UAnimBlueprint* Blueprint, UBlendSpace* BlendSpace, FName SlotName)
{
    if (!ValidBlueprintInputs(Blueprint, BlendSpace, SlotName, true)) return false;
    if (HasOwner(Blueprint, BlueprintOwner)) return CheckBlueprint(Blueprint, BlendSpace, SlotName) || ReportMonsterAssetFailure(TEXT("The saved monster graph differs; preserve it for explicit repair / 저장된 몬스터 그래프가 달라 명시적 수정을 위해 보존합니다"));
    UAnimGraphNode_Root* Root = FindRoot(Blueprint);
    UEdGraphPin* ResultPin = PosePin(Root, EGPD_Input);
    if (Blueprint->GetOutermost()->GetMetaData().HasValue(Blueprint, AuthorKey) || !Root || Root->GetGraph()->Nodes.Num() != 1 || !ResultPin || !ResultPin->LinkedTo.IsEmpty()) return ReportMonsterAssetFailure(TEXT("Only a new empty project AnimGraph may be configured / 새로운 빈 프로젝트 AnimGraph만 구성할 수 있습니다"));
    UEdGraph* Graph = Root->GetGraph();
    const UEdGraphSchema* Schema = Graph->GetSchema();
    Blueprint->Modify();
    Graph->Modify();
    Root->Modify();
    FGraphNodeCreator<UAnimGraphNode_BlendSpacePlayer> PlayerCreator(*Graph);
    UAnimGraphNode_BlendSpacePlayer* Player = PlayerCreator.CreateNode();
    Player->Node.SetBlendSpace(BlendSpace);
    Player->NodePosX = Root->NodePosX - 440;
    Player->NodePosY = Root->NodePosY;
    PlayerCreator.Finalize();
    FGraphNodeCreator<UAnimGraphNode_Slot> SlotCreator(*Graph);
    UAnimGraphNode_Slot* Slot = SlotCreator.CreateNode();
    Slot->Node.SlotName = SlotName;
    Slot->NodePosX = Root->NodePosX - 220;
    Slot->NodePosY = Root->NodePosY;
    SlotCreator.Finalize();
    FGraphNodeCreator<UK2Node_VariableGet> SpeedCreator(*Graph);
    UK2Node_VariableGet* Speed = SpeedCreator.CreateNode();
    Speed->VariableReference.SetSelfMember(GET_MEMBER_NAME_CHECKED(UMonsterAnimInstance, GroundSpeed));
    Speed->NodePosX = Root->NodePosX - 660;
    Speed->NodePosY = Root->NodePosY + 120;
    SpeedCreator.Finalize();
    UEdGraphPin* SpeedOutput = Speed->GetValuePin();
    UEdGraphPin* SpeedInput = Player->FindPin(TEXT("X"), EGPD_Input);
    UEdGraphPin* PlayerOutput = PosePin(Player, EGPD_Output);
    UEdGraphPin* SlotInput = PosePin(Slot, EGPD_Input);
    UEdGraphPin* SlotOutput = PosePin(Slot, EGPD_Output);
    const bool bConnected = SpeedOutput && SpeedInput && PlayerOutput && SlotInput && SlotOutput && Schema->TryCreateConnection(SpeedOutput, SpeedInput) && Schema->TryCreateConnection(PlayerOutput, SlotInput) && Schema->TryCreateConnection(SlotOutput, ResultPin);
    if (!bConnected)
    {
        Player->DestroyNode();
        Slot->DestroyNode();
        Speed->DestroyNode();
        return ReportMonsterAssetFailure(TEXT("Could not connect the monster graph; restored its empty root / 몬스터 그래프 연결에 실패하여 빈 최종 포즈를 복구했습니다"));
    }
    // UE may register DefaultSlot in memory during compilation; original skeleton packages are never saved here.
    // UE는 컴파일 중 DefaultSlot을 메모리에 등록할 수 있으며 여기서는 원본 스켈레톤 패키지를 저장하지 않습니다.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (!CheckBlueprint(Blueprint, BlendSpace, SlotName))
    {
        Player->DestroyNode();
        Slot->DestroyNode();
        Speed->DestroyNode();
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        return ReportMonsterAssetFailure(TEXT("Authored monster graph validation failed; restored its empty root / 작성된 몬스터 그래프 검증에 실패하여 빈 최종 포즈를 복구했습니다"));
    }
    Blueprint->GetOutermost()->GetMetaData().SetValue(Blueprint, AuthorKey, BlueprintOwner);
    Blueprint->MarkPackageDirty();
    return true;
}

bool UMonsterAssetLibrary::ValidateLocomotionAnimBlueprint(UAnimBlueprint* Blueprint, UBlendSpace* BlendSpace, FName SlotName)
{
    return HasOwner(Blueprint, BlueprintOwner) && CheckBlueprint(Blueprint, BlendSpace, SlotName);
}
