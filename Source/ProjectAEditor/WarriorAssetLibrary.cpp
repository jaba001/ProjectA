#include "WarriorAssetLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_ControlRig.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "RetargetEditor/IKRetargetBatchOperation.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogWarriorAssetLibrary, Log, All);

namespace
{
    bool IsProjectCopy(const UObject* Object)
    {
        return IsValid(Object) && Object->GetOutermost()->GetName().StartsWith(TEXT("/Game/User_JeHoon/"));
    }

    bool Fail(const TCHAR* Reason)
    {
        UE_LOG(LogWarriorAssetLibrary, Error, TEXT("%s"), Reason);
        return false;
    }

    USCS_Node* FindComponentNode(UBlueprint* Blueprint, FName ComponentName)
    {
        if (!IsValid(Blueprint) || !Blueprint->SimpleConstructionScript || ComponentName.IsNone()) return nullptr;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->GetVariableName() == ComponentName) return Node;
        }
        return nullptr;
    }

    UAnimGraphNode_Root* FindOutputNode(UAnimBlueprint* Blueprint)
    {
        if (!IsValid(Blueprint)) return nullptr;
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (UEdGraph* Graph : Graphs)
        {
            if (!Graph || Graph->GetFName() != TEXT("AnimGraph") || !Cast<UAnimationGraphSchema>(Graph->GetSchema())) continue;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (UAnimGraphNode_Root* Root = Cast<UAnimGraphNode_Root>(Node)) return Root;
            }
        }
        return nullptr;
    }

    UEdGraphPin* FindPosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
    {
        if (!Node) return nullptr;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == Direction && UAnimationGraphSchema::IsPosePin(Pin->PinType)) return Pin;
        }
        return nullptr;
    }

    bool IsCompatibleReferencePose(const USkeleton* Source, const USkeleton* Target)
    {
        if (!IsValid(Source) || !IsValid(Target)) return false;
        const FReferenceSkeleton& SourceBones = Source->GetReferenceSkeleton();
        const FReferenceSkeleton& TargetBones = Target->GetReferenceSkeleton();
        if (TargetBones.GetRawBoneNum() == 0) return false;
        for (int32 Index = 0; Index < TargetBones.GetRawBoneNum(); ++Index)
        {
            const FName BoneName = TargetBones.GetBoneName(Index);
            const int32 SourceIndex = SourceBones.FindBoneIndex(BoneName);
            if (SourceIndex == INDEX_NONE) return false;
            const int32 SourceParent = SourceBones.GetParentIndex(SourceIndex);
            const int32 TargetParent = TargetBones.GetParentIndex(Index);
            const FName SourceParentName = SourceParent == INDEX_NONE ? NAME_None : SourceBones.GetBoneName(SourceParent);
            const FName TargetParentName = TargetParent == INDEX_NONE ? NAME_None : TargetBones.GetBoneName(TargetParent);
            if (SourceParentName != TargetParentName || !SourceBones.GetRefBonePose()[SourceIndex].Equals(TargetBones.GetRefBonePose()[Index], 0.0001))
            {
                UE_LOG(LogWarriorAssetLibrary, Error, TEXT("Reference hierarchy or pose differs for bone %s / 본 계층 또는 기준 자세가 다릅니다"), *BoneName.ToString());
                return false;
            }
        }
        return true;
    }
}

UObject* UWarriorAssetLibrary::LoadSavedAssetReference(const FSoftObjectPath& Path)
{
    return Path.TryLoad();
}

bool UWarriorAssetLibrary::AssignMeshSkeleton(USkeletalMesh* Mesh, USkeleton* Skeleton)
{
    if (!IsProjectCopy(Mesh) || !IsProjectCopy(Skeleton) || !IsCompatibleReferencePose(Mesh->GetSkeleton(), Skeleton)) return Fail(TEXT("Mesh skeleton assignment requires compatible project copies / 호환되는 메시와 스켈레톤 작업 사본이 필요합니다"));
    Mesh->Modify();
    Mesh->SetSkeleton(Skeleton);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    return Mesh->GetSkeleton() == Skeleton;
}

bool UWarriorAssetLibrary::RetargetAnimations(const TArray<UObject*>& Assets, USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh, UIKRetargeter* Retargeter, const FString& Destination, const FString& Suffix)
{
    if (IsRunningCommandlet() || !SourceMesh || !IsProjectCopy(TargetMesh) || !IsProjectCopy(Retargeter) || !Destination.StartsWith(TEXT("/Game/User_JeHoon/")) || Destination.Contains(TEXT("..")) || Suffix.IsEmpty() || Suffix.Contains(TEXT("/"))) return Fail(TEXT("IK batch authoring requires the editor and a project-owned output directory / IK 일괄 작성에는 에디터와 작업 사본 출력 폴더가 필요합니다"));
    FIKRetargetBatchOperationContext Context;
    for (UObject* Asset : Assets)
    {
        if (!IsValid(Asset)) return false;
        Context.AssetsToRetarget.Add(Asset);
    }
    Context.SourceMesh = SourceMesh;
    Context.TargetMesh = TargetMesh;
    Context.IKRetargetAsset = Retargeter;
    // The Python convenience API defaults to /Game and invokes Slate; specify the owned folder explicitly.
    // Python 편의 API는 /Game을 기본값으로 사용하고 Slate를 호출하므로 작업 사본 폴더를 명시합니다.
    Context.NameRule.FolderPath = Destination;
    Context.NameRule.Suffix = Suffix;
    TStrongObjectPtr<UIKRetargetBatchOperation> Operation(NewObject<UIKRetargetBatchOperation>());
    Operation->RunRetarget(Context);
    for (UObject* Asset : Assets)
    {
        const FString Name = Asset->GetName() + Suffix;
        if (!LoadObject<UObject>(nullptr, *(Destination / Name + TEXT(".") + Name))) return false;
    }
    return !Assets.IsEmpty();
}

bool UWarriorAssetLibrary::SetWeaponAttachment(UBlueprint* Blueprint, FName ComponentName, FName SocketName)
{
    if (!IsProjectCopy(Blueprint) || !Blueprint->GeneratedClass || SocketName.IsNone()) return Fail(TEXT("Weapon attachment requires a project-owned Blueprint and named socket / 작업 사본 Blueprint와 소켓 이름이 필요합니다"));
    USCS_Node* Node = FindComponentNode(Blueprint, ComponentName);
    UStaticMeshComponent* Weapon = Node ? Cast<UStaticMeshComponent>(Node->ComponentTemplate) : nullptr;
    AActor* Defaults = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    USkeletalMeshComponent* Mesh = Defaults ? Defaults->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
    if (!Weapon || !Mesh || !Mesh->DoesSocketExist(SocketName) || !Node->bIsParentComponentNative || Node->ParentComponentOrVariableName != Mesh->GetFName()) return Fail(TEXT("Weapon must already be attached to the native skeletal mesh with a valid bone or socket / 무기는 유효한 본 또는 소켓을 가진 네이티브 스켈레탈 메시의 자식이어야 합니다"));
    Blueprint->Modify();
    Node->Modify();
    Weapon->Modify();
    // SCS attachment metadata is authoritative when the Blueprint creates a component instance.
    // Blueprint가 컴포넌트 인스턴스를 생성할 때 SCS 부착 메타데이터를 기준으로 사용합니다.
    Node->AttachToName = SocketName;
    Weapon->SetupAttachment(Mesh, SocketName);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status != BS_Error && GetWeaponAttachment(Blueprint, ComponentName) == SocketName;
}

FName UWarriorAssetLibrary::GetWeaponAttachment(UBlueprint* Blueprint, FName ComponentName)
{
    const USCS_Node* Node = FindComponentNode(Blueprint, ComponentName);
    return Node ? Node->AttachToName : NAME_None;
}

TArray<FName> UWarriorAssetLibrary::GetAnimationSlotNames(UAnimBlueprint* Blueprint)
{
    TArray<FName> Names;
    if (!IsValid(Blueprint)) return Names;
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (const UEdGraph* Graph : Graphs)
    {
        if (!Graph) continue;
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            if (const UAnimGraphNode_Slot* Slot = Cast<UAnimGraphNode_Slot>(Node)) Names.AddUnique(Slot->Node.SlotName);
        }
    }
    return Names;
}

bool UWarriorAssetLibrary::IsOutputSlotConnected(UAnimBlueprint* Blueprint, FName SlotName)
{
    UAnimGraphNode_Root* Root = FindOutputNode(Blueprint);
    if (!Root || SlotName.IsNone()) return false;
    TArray<const UEdGraphNode*> Pending = {Root};
    TSet<const UEdGraphNode*> Visited;
    while (!Pending.IsEmpty())
    {
        const UEdGraphNode* Node = Pending.Pop(EAllowShrinking::No);
        if (!Node || Visited.Contains(Node)) continue;
        Visited.Add(Node);
        if (const UAnimGraphNode_Slot* Slot = Cast<UAnimGraphNode_Slot>(Node); Slot && Slot->Node.SlotName == SlotName) return true;
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Input || !UAnimationGraphSchema::IsPosePin(Pin->PinType)) continue;
            for (const UEdGraphPin* Source : Pin->LinkedTo)
            {
                if (Source) Pending.Add(Source->GetOwningNode());
            }
        }
    }
    return false;
}

bool UWarriorAssetLibrary::EnsureOutputSlot(UAnimBlueprint* Blueprint, FName SlotName)
{
    if (!IsProjectCopy(Blueprint) || !IsProjectCopy(Blueprint->TargetSkeleton) || SlotName.IsNone()) return Fail(TEXT("Output slot changes require project-owned AnimBlueprint and Skeleton copies / 슬롯 변경에는 작업 사본 AnimBlueprint와 Skeleton이 필요합니다"));
    if (IsOutputSlotConnected(Blueprint, SlotName)) return true;
    if (GetAnimationSlotNames(Blueprint).Contains(SlotName)) return Fail(TEXT("An unconnected matching slot already exists; preserve it for explicit repair / 연결되지 않은 동일 슬롯이 있어 명시적 수정을 위해 보존합니다"));
    UAnimGraphNode_Root* Root = FindOutputNode(Blueprint);
    UEdGraphPin* ResultPin = FindPosePin(Root, EGPD_Input);
    if (!ResultPin || ResultPin->LinkedTo.Num() > 1) return Fail(TEXT("The main AnimGraph output has no unambiguous pose input / 기본 AnimGraph 출력의 포즈 입력을 확인할 수 없습니다"));
    UEdGraph* Graph = Root->GetGraph();
    const UEdGraphSchema* Schema = Graph->GetSchema();
    UEdGraphPin* PreviousSource = ResultPin->LinkedTo.IsEmpty() ? nullptr : ResultPin->LinkedTo[0];
    Blueprint->Modify();
    Graph->Modify();
    Root->Modify();
    FGraphNodeCreator<UAnimGraphNode_Slot> Creator(*Graph);
    UAnimGraphNode_Slot* Slot = Creator.CreateNode();
    Slot->Node.SlotName = SlotName;
    Slot->NodePosX = Root->NodePosX - 220;
    Slot->NodePosY = Root->NodePosY;
    Creator.Finalize();
    UEdGraphPin* SlotInput = FindPosePin(Slot, EGPD_Input);
    UEdGraphPin* SlotOutput = FindPosePin(Slot, EGPD_Output);
    if (PreviousSource) Schema->BreakSinglePinLink(PreviousSource, ResultPin);
    const bool bConnected = SlotInput && SlotOutput && (!PreviousSource || Schema->TryCreateConnection(PreviousSource, SlotInput)) && Schema->TryCreateConnection(SlotOutput, ResultPin);
    if (!bConnected)
    {
        Slot->DestroyNode();
        if (PreviousSource) Schema->TryCreateConnection(PreviousSource, ResultPin);
        return Fail(TEXT("Could not insert the montage slot; restored the original pose connection / 몽타주 슬롯 삽입 실패로 원래 포즈 연결을 복구했습니다"));
    }
    Blueprint->TargetSkeleton->Modify();
    Blueprint->TargetSkeleton->RegisterSlotNode(SlotName);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->Status != BS_Error && IsOutputSlotConnected(Blueprint, SlotName);
}

bool UWarriorAssetLibrary::RemoveLegacyFootIK(UAnimBlueprint* Blueprint)
{
    if (!IsProjectCopy(Blueprint)) return Fail(TEXT("Foot IK removal requires a project-owned AnimBlueprint copy / Foot IK 제거에는 AnimBlueprint 작업 사본이 필요합니다"));
    const FString LegacyRigPath(TEXT("/Game/Characters/Mannequins/Rigs/CR_Mannequin_FootIK.CR_Mannequin_FootIK_C"));
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    TArray<UAnimGraphNode_ControlRig*> LegacyNodes;
    for (UEdGraph* Graph : Graphs)
    {
        const UAnimationGraphSchema* Schema = Graph ? Cast<UAnimationGraphSchema>(Graph->GetSchema()) : nullptr;
        if (!Schema) continue;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            UAnimGraphNode_ControlRig* RigNode = Cast<UAnimGraphNode_ControlRig>(Node);
            const UClass* RigClass = RigNode ? RigNode->Node.GetControlRigClass().Get() : nullptr;
            if (!RigClass || RigClass->GetPathName() != LegacyRigPath) continue;
            UEdGraphPin* Input = FindPosePin(RigNode, EGPD_Input);
            UEdGraphPin* Output = FindPosePin(RigNode, EGPD_Output);
            if (!Input || !Output || Input->LinkedTo.Num() != 1 || Output->LinkedTo.Num() != 1) return Fail(TEXT("Legacy Foot IK has an ambiguous pose connection; preserved the graph / 기존 Foot IK 포즈 연결이 명확하지 않아 그래프를 보존합니다"));
            UEdGraphPin* Source = Input->LinkedTo[0];
            UEdGraphPin* Destination = Output->LinkedTo[0];
            if (!Source || !Destination || Source->LinkedTo.Num() != 1 || Destination->LinkedTo.Num() != 1 || !Schema->ArePinsCompatible(Source, Destination, nullptr)) return Fail(TEXT("Legacy Foot IK cannot be bypassed without changing other pose links / 다른 포즈 연결 변경 없이 기존 Foot IK를 우회할 수 없습니다"));
            LegacyNodes.Add(RigNode);
        }
    }
    if (LegacyNodes.IsEmpty()) return true;
    TArray<FName> ConnectedSlots;
    for (FName SlotName : GetAnimationSlotNames(Blueprint))
    {
        if (IsOutputSlotConnected(Blueprint, SlotName)) ConnectedSlots.Add(SlotName);
    }
    Blueprint->Modify();
    for (UAnimGraphNode_ControlRig* RigNode : LegacyNodes)
    {
        UEdGraph* Graph = RigNode->GetGraph();
        const UEdGraphSchema* Schema = Graph->GetSchema();
        UEdGraphPin* Input = FindPosePin(RigNode, EGPD_Input);
        UEdGraphPin* Output = FindPosePin(RigNode, EGPD_Output);
        UEdGraphPin* Source = Input->LinkedTo[0];
        UEdGraphPin* Destination = Output->LinkedTo[0];
        Graph->Modify();
        RigNode->Modify();
        // Bypass only the verified template rig, preserving the existing locomotion and montage slot chain.
        // 확인된 템플릿 리그만 우회하며 기존 이동 및 몽타주 슬롯 연결을 보존합니다.
        if (!Schema->TryCreateConnection(Source, Destination))
        {
            Schema->TryCreateConnection(Source, Input);
            Schema->TryCreateConnection(Output, Destination);
            return Fail(TEXT("Could not bypass the legacy Foot IK pose connection / 기존 Foot IK 포즈 연결 우회에 실패했습니다"));
        }
        FBlueprintEditorUtils::RemoveNode(Blueprint, RigNode, true);
        Graph->NotifyGraphChanged();
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    for (FName SlotName : ConnectedSlots)
    {
        if (!IsOutputSlotConnected(Blueprint, SlotName)) return Fail(TEXT("The existing montage slot connection was not preserved / 기존 몽타주 슬롯 연결이 보존되지 않았습니다"));
    }
    return Blueprint->Status != BS_Error;
}
