#include "CombatContentAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Game/Run/RunContentMigration.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Package.h"

bool UCombatContentAssetLibrary::RemoveRetiredSkillPins(UBlueprint* Blueprint, int32& OutRemovedPins, TArray<FString>& OutDetails)
{
    OutRemovedPins = 0;
    OutDetails.Reset();
    if (!IsInGameThread() || !IsValid(Blueprint) || !Blueprint->GetOutermost()->GetName().StartsWith(TEXT("/Game/User_JeHoon/"))) return false;
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    TArray<UEdGraphPin*> Pins;
    bool bSupported = true;
    for (UEdGraph* Graph : Graphs)
    {
        if (!IsValid(Graph)) continue;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!IsValid(Node)) continue;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin || !RunContentMigration::IsRemovedSkill(FSoftObjectPath(Pin->DefaultObject.Get()))) continue;
                const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
                const FName Function = Call ? Call->FunctionReference.GetMemberName() : NAME_None;
                OutDetails.Add(FString::Printf(TEXT("%s | %s | %s | %s"), *Node->GetPathName(), *Function.ToString(), *Pin->PinName.ToString(), *Pin->DefaultObject->GetPathName()));
                bSupported &= Pin->Direction == EGPD_Input && Function == TEXT("EnterSkillMode") && Pin->PinName == TEXT("SkillData") && Graph->GetSchema() != nullptr;
                Pins.Add(Pin);
            }
        }
    }
    if (!bSupported) return false;
    if (!Pins.IsEmpty()) Blueprint->Modify();
    for (UEdGraphPin* Pin : Pins)
    {
        UEdGraphNode* Node = Pin->GetOwningNode();
        Node->Modify();
        Node->GetGraph()->GetSchema()->TrySetDefaultObject(*Pin, nullptr);
        if (Pin->DefaultObject != nullptr) return false;
        ++OutRemovedPins;
    }
    if (OutRemovedPins > 0) FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    return true;
}
