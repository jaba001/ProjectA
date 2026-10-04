#include "CombatVfxAssetLibrary.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_Niagara.h"
#include "Engine/StaticMesh.h"
#include "Misc/App.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraDataInterfaceAudioPlayer.h"
#include "NiagaraGraph.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraSystem.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "Sound/SoundBase.h"
#include "UpgradeNiagaraScriptResults.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

namespace
{
    struct FCoordinateInput
    {
        FNiagaraVariable Variable;
        FNiagaraParameterHandle AliasedHandle;
        UEdGraphPin* OverridePin = nullptr;
        const UNiagaraScriptVariable* ScriptVariable = nullptr;
        bool bHidden = false;
        bool bValueKnown = false;
        int32 Value = INDEX_NONE;
    };

    bool ReadCoordinatePin(const UEdGraphPin* Pin, int32& Value)
    {
        if (!Pin || !Pin->LinkedTo.IsEmpty() || Pin->DefaultValue.IsEmpty()) return false;
        const FNiagaraVariable Variable = UEdGraphSchema_Niagara::PinToNiagaraVariable(Pin, true);
        if (!Variable.IsDataAllocated() || Variable.GetType().GetEnum() != StaticEnum<ENiagaraCoordinateSpace>() || Variable.GetSizeInBytes() != sizeof(int32)) return false;
        FMemory::Memcpy(&Value, Variable.GetData(), sizeof(Value));
        return StaticEnum<ENiagaraCoordinateSpace>()->IsValidEnumValue(Value);
    }

    UEdGraphPin* FindCoordinateOverridePin(UNiagaraNodeFunctionCall* Node, const FNiagaraParameterHandle& Handle)
    {
        // Follow the function's real parameter-map input rather than guessing an override node or pin name.
        // 재정의 노드나 핀 이름을 추정하지 않고 함수의 실제 파라미터 맵 입력 연결을 따라갑니다.
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Input || Pin->PinType.PinSubCategoryObject != FNiagaraTypeDefinition::GetParameterMapStruct() || Pin->LinkedTo.Num() != 1) continue;
            const UEdGraphNode* OverrideNode = Pin->LinkedTo[0]->GetOwningNode();
            for (UEdGraphPin* Candidate : OverrideNode->Pins)
            {
                if (Candidate && Candidate->Direction == EGPD_Input && Candidate->PinName == Handle.GetParameterHandleString()) return Candidate;
            }
        }
        return nullptr;
    }

    TArray<FCoordinateInput> ReadCoordinateInputs(const FVersionedNiagaraEmitter& Emitter, UNiagaraNodeFunctionCall* Node)
    {
        TArray<FCoordinateInput> Result;
        if (!IsValid(Node) || !IsValid(Node->FunctionScript)) return Result;
        FCompileConstantResolver Resolver(Emitter, FNiagaraStackGraphUtilities::GetOutputNodeUsage(*Node));
        TArray<FNiagaraVariable> Variables;
        TSet<FNiagaraVariable> Hidden;
        FNiagaraStackGraphUtilities::GetStackFunctionInputs(*Node, Variables, Hidden, Resolver, FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly);
        TArray<UEdGraphPin*> StaticPins;
        TSet<UEdGraphPin*> HiddenStaticPins;
        FNiagaraStackGraphUtilities::GetStackFunctionStaticSwitchPins(*Node, StaticPins, HiddenStaticPins, Resolver);
        for (UEdGraphPin* Pin : StaticPins)
        {
            if (!Pin || UEdGraphSchema_Niagara::PinToTypeDefinition(Pin).GetEnum() != StaticEnum<ENiagaraCoordinateSpace>()) continue;
            const FNiagaraVariable Variable = UEdGraphSchema_Niagara::PinToNiagaraVariable(Pin);
            Variables.AddUnique(Variable);
            if (HiddenStaticPins.Contains(Pin)) Hidden.Add(Variable);
        }
        const UNiagaraScriptSource* FunctionSource = Node->GetFunctionScriptSource();
        for (const FNiagaraVariable& Variable : Variables)
        {
            if (Variable.GetType().GetEnum() != StaticEnum<ENiagaraCoordinateSpace>() || Variable.GetSizeInBytes() != sizeof(int32)) continue;
            FCoordinateInput Input;
            Input.Variable = Variable;
            Input.AliasedHandle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle(Variable.GetName()), Node);
            Input.bHidden = Hidden.Contains(Variable);
            for (UEdGraphPin* Pin : StaticPins)
            {
                if (Pin && Pin->PinName == Variable.GetName()) Input.OverridePin = Pin;
            }
            if (!Input.OverridePin) Input.OverridePin = FindCoordinateOverridePin(Node, Input.AliasedHandle);
            if (IsValid(FunctionSource) && IsValid(FunctionSource->NodeGraph)) Input.ScriptVariable = FunctionSource->NodeGraph->GetScriptVariable(Variable.GetName());
            if (Input.OverridePin) Input.bValueKnown = ReadCoordinatePin(Input.OverridePin, Input.Value);
            else if (IsValid(Input.ScriptVariable) && Input.ScriptVariable->DefaultMode == ENiagaraDefaultMode::Value && Input.ScriptVariable->Variable.GetType() == Variable.GetType() && Input.ScriptVariable->GetDefaultValueVariant().GetNumBytes() == sizeof(int32) && Input.ScriptVariable->GetDefaultValueData())
            {
                FMemory::Memcpy(&Input.Value, Input.ScriptVariable->GetDefaultValueData(), sizeof(Input.Value));
                Input.bValueKnown = StaticEnum<ENiagaraCoordinateSpace>()->IsValidEnumValue(Input.Value);
            }
            Result.Add(Input);
        }
        return Result;
    }

    TSharedRef<FJsonObject> DescribeCoordinateInput(const FCoordinateInput& Input)
    {
        TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Input.Variable.GetName().ToString());
        Row->SetStringField(TEXT("alias"), Input.AliasedHandle.GetParameterHandleString().ToString());
        Row->SetStringField(TEXT("type"), Input.Variable.GetType().GetName());
        Row->SetBoolField(TEXT("static"), Input.Variable.GetType().IsStatic());
        Row->SetBoolField(TEXT("hidden"), Input.bHidden);
        Row->SetBoolField(TEXT("hasOverride"), Input.OverridePin != nullptr);
        Row->SetBoolField(TEXT("valueKnown"), Input.bValueKnown);
        if (Input.bValueKnown)
        {
            Row->SetNumberField(TEXT("value"), Input.Value);
            Row->SetStringField(TEXT("space"), StaticEnum<ENiagaraCoordinateSpace>()->GetNameStringByValue(Input.Value));
        }
        if (IsValid(Input.ScriptVariable))
        {
            Row->SetStringField(TEXT("defaultVariable"), Input.ScriptVariable->GetPathName());
            Row->SetStringField(TEXT("defaultMode"), StaticEnum<ENiagaraDefaultMode>()->GetNameStringByValue(static_cast<int64>(Input.ScriptVariable->DefaultMode)));
        }
        if (Input.OverridePin)
        {
            Row->SetStringField(TEXT("pin"), Input.OverridePin->PinName.ToString());
            Row->SetStringField(TEXT("pinOwner"), Input.OverridePin->GetOwningNode()->GetPathName());
            Row->SetStringField(TEXT("literal"), Input.OverridePin->DefaultValue);
            TArray<TSharedPtr<FJsonValue>> Linked;
            for (const UEdGraphPin* Pin : Input.OverridePin->LinkedTo) Linked.Add(MakeShared<FJsonValueString>(Pin->GetOwningNode()->GetPathName() + TEXT(" : ") + Pin->PinName.ToString()));
            Row->SetArrayField(TEXT("linked"), Linked);
        }
        return Row;
    }

    bool IsLocationEventModule(const UNiagaraNodeFunctionCall* Node)
    {
        if (!IsValid(Node) || !IsValid(Node->FunctionScript)) return false;
        const FString Path = Node->FunctionScript->GetPathName();
        return Path == TEXT("/Niagara/Modules/Events/GenerateLocationEvent.GenerateLocationEvent") || Path == TEXT("/Niagara/Modules/Events/ReceiveLocationEvent.ReceiveLocationEvent");
    }

    bool IsInitialVelocity(const FNiagaraVariableBase& Variable)
    {
        const FString Name = Variable.GetName().ToString();
        return Name.StartsWith(TEXT("Constants.")) && (Name.EndsWith(TEXT(".AddVelocity.Velocity")) || Name.EndsWith(TEXT(".InitializeParticle.Velocity")) || Name.EndsWith(TEXT(".InitializeParticle.InitialVelocity")) || Name.EndsWith(TEXT(".InitializeParticles.Velocity")) || Name.EndsWith(TEXT(".InitializeParticles.InitialVelocity")));
    }

    TSharedRef<FJsonObject> DescribeParameter(const FNiagaraParameterStore& Store, const FNiagaraVariableBase& Variable)
    {
        TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Variable.GetName().ToString());
        Row->SetStringField(TEXT("type"), Variable.GetType().GetName());
        Row->SetBoolField(TEXT("initialVelocity"), IsInitialVelocity(Variable));
        if (Variable.GetType() == FNiagaraTypeDefinition::GetVec3Def())
        {
            const FVector3f Value = Store.GetParameterValue<FVector3f>(Variable);
            Row->SetArrayField(TEXT("value"), {MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)});
        }
        else if (Variable.GetType() == FNiagaraTypeDefinition::GetQuatDef())
        {
            const FQuat4f Value = Store.GetParameterValue<FQuat4f>(Variable);
            Row->SetArrayField(TEXT("value"), {MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z), MakeShared<FJsonValueNumber>(Value.W)});
        }
        else if (Variable.GetType() == FNiagaraTypeDefinition::GetFloatDef()) Row->SetNumberField(TEXT("value"), Store.GetParameterValue<float>(Variable));
        else if (Variable.GetType() == FNiagaraTypeDefinition::GetIntDef()) Row->SetNumberField(TEXT("value"), Store.GetParameterValue<int32>(Variable));
        else if (Variable.GetType() == FNiagaraTypeDefinition::GetBoolDef()) Row->SetBoolField(TEXT("value"), Store.GetParameterValue<FNiagaraBool>(Variable).GetValue());
        else if (Variable.GetType() == FNiagaraTypeDefinition::GetPositionDef())
        {
            const FNiagaraPosition Value = Store.GetParameterValue<FNiagaraPosition>(Variable);
            Row->SetArrayField(TEXT("value"), {MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)});
        }
        return Row;
    }
}

FString UCombatVfxAssetLibrary::InspectNiagaraModuleInputSpaces(UNiagaraSystem* System)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetBoolField(TEXT("valid"), IsValid(System));
    TArray<TSharedPtr<FJsonValue>> Emitters;
    if (IsValid(System))
    {
        Root->SetStringField(TEXT("asset"), System->GetPathName());
        for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
        {
            TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
            const FVersionedNiagaraEmitter Instance = Handle.GetInstance();
            const FVersionedNiagaraEmitterData* Data = Instance.GetEmitterData();
            const UNiagaraScriptSource* Source = Data ? Cast<UNiagaraScriptSource>(Data->GraphSource) : nullptr;
            Row->SetStringField(TEXT("name"), Handle.GetName().ToString());
            Row->SetBoolField(TEXT("enabled"), Handle.GetIsEnabled());
            Row->SetStringField(TEXT("emitter"), GetPathNameSafe(Instance.Emitter));
            Row->SetStringField(TEXT("source"), GetPathNameSafe(Source));
            Row->SetStringField(TEXT("graph"), IsValid(Source) ? GetPathNameSafe(Source->NodeGraph) : TEXT("None"));
            TArray<TSharedPtr<FJsonValue>> Modules;
            if (IsValid(Source) && IsValid(Source->NodeGraph))
            {
                TArray<UNiagaraNodeFunctionCall*> Nodes;
                Source->NodeGraph->GetNodesOfClass(Nodes);
                for (UNiagaraNodeFunctionCall* Node : Nodes)
                {
                    const TArray<FCoordinateInput> Inputs = ReadCoordinateInputs(Instance, Node);
                    if (Inputs.IsEmpty()) continue;
                    TSharedRef<FJsonObject> Module = MakeShared<FJsonObject>();
                    Module->SetStringField(TEXT("name"), Node->GetFunctionName());
                    Module->SetStringField(TEXT("node"), Node->GetPathName());
                    Module->SetStringField(TEXT("script"), GetPathNameSafe(Node->FunctionScript));
                    Module->SetStringField(TEXT("versionGuid"), Node->SelectedScriptVersion.ToString(EGuidFormats::DigitsWithHyphens));
                    TArray<TSharedPtr<FJsonValue>> Descriptions;
                    for (const FCoordinateInput& Input : Inputs) Descriptions.Add(MakeShared<FJsonValueObject>(DescribeCoordinateInput(Input)));
                    Module->SetArrayField(TEXT("inputs"), Descriptions);
                    Modules.Add(MakeShared<FJsonValueObject>(Module));
                }
            }
            Row->SetArrayField(TEXT("modules"), Modules);
            Emitters.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    Root->SetArrayField(TEXT("emitters"), Emitters);
    FString Result;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Result));
    return Result;
}

FString UCombatVfxAssetLibrary::ConfigureNiagaraModuleInputSpace(UNiagaraSystem* System, FName EmitterName, FName FunctionName, FName InputName, ENiagaraCoordinateSpace Space)
{
    if (!IsValid(System) || !System->GetOutermost()->GetName().StartsWith(TEXT("/Game/User_JeHoon/"))) return FString::Printf(TEXT("Module space change requires a project derivative: %s / 모듈 공간 변경에는 프로젝트 전용 파생본이 필요합니다."), *GetPathNameSafe(System));
    if (EmitterName.IsNone() || FunctionName.IsNone() || InputName.IsNone() || Space != ENiagaraCoordinateSpace::Local) return TEXT("Explicit world-space-to-Local input selection is required. / 명시적인 월드 공간에서 Local로 변경할 입력 선택이 필요합니다.");
    FVersionedNiagaraEmitter Selected;
    int32 EmitterMatches = 0;
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        if (Handle.GetName() != EmitterName) continue;
        Selected = Handle.GetInstance();
        ++EmitterMatches;
    }
    FVersionedNiagaraEmitterData* Data = Selected.GetEmitterData();
    UNiagaraScriptSource* Source = Data ? Cast<UNiagaraScriptSource>(Data->GraphSource) : nullptr;
    if (EmitterMatches != 1 || !IsValid(Selected.Emitter) || !Selected.Emitter->IsIn(System) || !IsValid(Source) || !Source->IsIn(System) || !IsValid(Source->NodeGraph) || !Source->NodeGraph->IsIn(System)) return TEXT("Emitter or graph ownership is missing, shared or ambiguous. / 이미터 또는 그래프 소유권이 누락·공유·중복 상태입니다.");
    TArray<UNiagaraScript*> Scripts;
    Data->GetScripts(Scripts, false);
    if (Scripts.IsEmpty()) return TEXT("Selected emitter has no scripts. / 선택한 이미터에 스크립트가 없습니다.");
    for (const UNiagaraScript* Script : Scripts)
    {
        if (!IsValid(Script) || !Script->IsIn(System)) return FString::Printf(TEXT("Script ownership rejected: %s / 스크립트가 파생본에 속하지 않습니다."), *GetPathNameSafe(Script));
    }
    TArray<UNiagaraNodeFunctionCall*> Nodes;
    Source->NodeGraph->GetNodesOfClass(Nodes);
    UNiagaraNodeFunctionCall* Node = nullptr;
    int32 FunctionMatches = 0;
    for (UNiagaraNodeFunctionCall* Candidate : Nodes)
    {
        if (FName(*Candidate->GetFunctionName()) != FunctionName) continue;
        Node = Candidate;
        ++FunctionMatches;
    }
    if (FunctionMatches != 1 || !IsValid(Node) || !Node->IsIn(System) || Node->GetGraph() != Source->NodeGraph || !IsValid(Node->FunctionScript)) return TEXT("Module selection is missing, shared or ambiguous. / 모듈 선택이 누락·공유·중복 상태입니다.");
    FCoordinateInput Input;
    int32 InputMatches = 0;
    for (const FCoordinateInput& Candidate : ReadCoordinateInputs(Selected, Node))
    {
        if (Candidate.Variable.GetName() != InputName) continue;
        Input = Candidate;
        ++InputMatches;
    }
    if (InputMatches != 1 || Input.bHidden || !Input.bValueKnown) return TEXT("Coordinate input is missing, hidden, linked or ambiguous. / 좌표 공간 입력이 누락·숨김·연결·중복 상태입니다.");
    if (Input.OverridePin && (!Input.OverridePin->GetOwningNode()->IsIn(System) || Input.OverridePin->GetOwningNode()->GetGraph() != Source->NodeGraph || UEdGraphSchema_Niagara::PinToTypeDefinition(Input.OverridePin) != Input.Variable.GetType())) return TEXT("Override pin ownership or type is invalid. / 재정의 핀의 소유권 또는 자료형이 올바르지 않습니다.");
    if (Input.Value == static_cast<int32>(ENiagaraCoordinateSpace::Local)) return FString();
    const bool bWorldInput = Input.Value == static_cast<int32>(ENiagaraCoordinateSpace::World) || (Input.Value == static_cast<int32>(ENiagaraCoordinateSpace::Simulation) && !Data->bLocalSpace);
    if (!bWorldInput) return TEXT("Only a verified world-space input may be changed to Local. / 검증된 월드 공간 입력만 Local로 변경할 수 있습니다.");
    FNiagaraVariable Desired(Input.Variable.GetType(), Input.Variable.GetName());
    const int32 DesiredValue = static_cast<int32>(Space);
    Desired.SetData(reinterpret_cast<const uint8*>(&DesiredValue));
    FString Literal;
    const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
    if (!Schema->TryGetPinDefaultValueFromNiagaraVariable(Desired, Literal)) return TEXT("Coordinate enum literal cannot be represented. / 좌표 공간 enum 리터럴을 작성할 수 없습니다.");
    // World-space emitters resolve Simulation as World; validate that effective space before changing one override.
    // 월드 공간 이미터의 Simulation은 World로 해석되므로 실제 공간을 검사한 후 재정의 하나만 변경합니다.
    System->Modify();
    Selected.Emitter->Modify();
    Source->Modify();
    Source->NodeGraph->Modify();
    Node->Modify();
    const FGuid VariableGuid = IsValid(Input.ScriptVariable) ? Input.ScriptVariable->Metadata.GetVariableGuid() : FGuid();
    UEdGraphPin& Pin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Node, Input.AliasedHandle, Input.Variable.GetType(), VariableGuid, FGuid());
    if (!Pin.LinkedTo.IsEmpty() || !Pin.GetOwningNode()->IsIn(System) || Pin.GetOwningNode()->GetGraph() != Source->NodeGraph || UEdGraphSchema_Niagara::PinToTypeDefinition(&Pin) != Input.Variable.GetType()) return TEXT("Created override pin is unsafe; do not save. / 작성된 재정의 핀이 안전하지 않으므로 저장하지 마세요.");
    Pin.Modify();
    Schema->TrySetDefaultValue(Pin, Literal);
    int32 Written = INDEX_NONE;
    if (!ReadCoordinatePin(&Pin, Written) || Written != DesiredValue) return TEXT("Coordinate enum write failed; do not save. / 좌표 공간 enum 작성에 실패했으므로 저장하지 마세요.");
    if (UNiagaraNode* OverrideNode = Cast<UNiagaraNode>(Pin.GetOwningNode())) OverrideNode->MarkNodeRequiresSynchronization(TEXT("Project VFX module coordinate-space override changed."), true);
    Data->GraphSource->MarkNotSynchronized(TEXT("Project VFX direction module input changed."));
    System->PrepareRapidIterationParametersForCompilation();
    System->MarkPackageDirty();
    System->RequestCompile(true);
    System->WaitForCompilationComplete(true, false);
    if (!System->IsValid() || (FApp::CanEverRender() && !System->IsReadyToRun())) return FString::Printf(TEXT("Module space compilation failed: %s; do not save. / 모듈 공간 컴파일에 실패했으므로 저장하지 마세요."), *System->GetPathName());
    return FString();
}

FString UCombatVfxAssetLibrary::InspectNiagaraSpace(UNiagaraSystem* System)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetBoolField(TEXT("valid"), IsValid(System));
    if (IsValid(System))
    {
        System->WaitForCompilationComplete(true, false);
        Root->SetBoolField(TEXT("ready"), System->IsReadyToRun());
        Root->SetBoolField(TEXT("systemValid"), System->IsValid());
        Root->SetStringField(TEXT("asset"), System->GetPathName());
        // Inspect embedded audio interfaces so catalogs distinguish built-in SFX from optional external playback.
        // 카탈로그에서 내장 SFX와 선택적 외부 재생을 구분하도록 포함된 오디오 인터페이스를 검사합니다.
        TArray<TSharedPtr<FJsonValue>> AudioInterfaces;
        ForEachObjectWithOuter(System, [&AudioInterfaces](UObject* Object)
        {
            const UNiagaraDataInterfaceAudioPlayer* Audio = Cast<UNiagaraDataInterfaceAudioPlayer>(Object);
            if (!Audio) return;
            TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("object"), Audio->GetPathName());
            Row->SetStringField(TEXT("sound"), GetPathNameSafe(Audio->SoundToPlay));
            Row->SetBoolField(TEXT("stopOnDestroy"), Audio->bStopWhenComponentIsDestroyed);
            Row->SetBoolField(TEXT("allowLoopingOneShot"), Audio->bAllowLoopingOneShotSounds);
            Row->SetBoolField(TEXT("limitPlaysPerTick"), Audio->bLimitPlaysPerTick);
            Row->SetNumberField(TEXT("maxPlaysPerTick"), Audio->MaxPlaysPerTick);
            AudioInterfaces.Add(MakeShared<FJsonValueObject>(Row));
        }, EGetObjectsFlags::IncludeNestedObjects);
        Root->SetArrayField(TEXT("audioInterfaces"), AudioInterfaces);
        TArray<TSharedPtr<FJsonValue>> UserParameters;
        for (const FNiagaraVariableWithOffset& Variable : System->GetExposedParameters().ReadParameterVariables()) UserParameters.Add(MakeShared<FJsonValueObject>(DescribeParameter(System->GetExposedParameters(), Variable)));
        Root->SetArrayField(TEXT("userParameters"), UserParameters);
        TArray<TSharedPtr<FJsonValue>> Emitters;
        for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
        {
            TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("name"), Handle.GetName().ToString());
            Row->SetBoolField(TEXT("enabled"), Handle.GetIsEnabled());
            const FVersionedNiagaraEmitterData* Data = Handle.GetInstance().GetEmitterData();
            Row->SetBoolField(TEXT("stateful"), Data != nullptr);
            if (Data)
            {
                Row->SetBoolField(TEXT("localSpace"), Data->bLocalSpace);
                Row->SetStringField(TEXT("simTarget"), StaticEnum<ENiagaraSimTarget>()->GetNameStringByValue(static_cast<int64>(Data->SimTarget)));
                TArray<TSharedPtr<FJsonValue>> Renderers;
                for (const UNiagaraRendererProperties* Renderer : Data->GetRenderers())
                {
                    if (!IsValid(Renderer)) continue;
                    TSharedRef<FJsonObject> RendererRow = MakeShared<FJsonObject>();
                    RendererRow->SetStringField(TEXT("class"), Renderer->GetClass()->GetPathName());
                    RendererRow->SetBoolField(TEXT("enabled"), Renderer->GetIsEnabled());
                    if (const UNiagaraMeshRendererProperties* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer))
                    {
                        RendererRow->SetStringField(TEXT("facingMode"), StaticEnum<ENiagaraMeshFacingMode>()->GetNameStringByValue(static_cast<int64>(MeshRenderer->FacingMode)));
                        TArray<TSharedPtr<FJsonValue>> Meshes;
                        for (const FNiagaraMeshRendererMeshProperties& Mesh : MeshRenderer->Meshes) Meshes.Add(MakeShared<FJsonValueString>(GetPathNameSafe(Mesh.Mesh)));
                        RendererRow->SetArrayField(TEXT("meshes"), Meshes);
                    }
                    Renderers.Add(MakeShared<FJsonValueObject>(RendererRow));
                }
                Row->SetArrayField(TEXT("renderers"), Renderers);
                TArray<TSharedPtr<FJsonValue>> Scripts;
                TArray<UNiagaraScript*> EmitterScripts;
                Data->GetScripts(EmitterScripts, false);
                for (const UNiagaraScript* Script : EmitterScripts)
                {
                    if (!IsValid(Script)) continue;
                    TSharedRef<FJsonObject> ScriptRow = MakeShared<FJsonObject>();
                    ScriptRow->SetStringField(TEXT("script"), Script->GetPathName());
                    TArray<TSharedPtr<FJsonValue>> Parameters;
                    for (const FNiagaraVariableWithOffset& Variable : Script->RapidIterationParameters.ReadParameterVariables()) Parameters.Add(MakeShared<FJsonValueObject>(DescribeParameter(Script->RapidIterationParameters, Variable)));
                    ScriptRow->SetArrayField(TEXT("parameters"), Parameters);
                    Scripts.Add(MakeShared<FJsonValueObject>(ScriptRow));
                }
                Row->SetArrayField(TEXT("scripts"), Scripts);
                TArray<TSharedPtr<FJsonValue>> LocationEventModules;
                const UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Data->GraphSource);
                if (IsValid(Source) && IsValid(Source->NodeGraph))
                {
                    TArray<UNiagaraNodeFunctionCall*> Nodes;
                    Source->NodeGraph->GetNodesOfClass(Nodes);
                    for (const UNiagaraNodeFunctionCall* Node : Nodes)
                    {
                        if (!IsLocationEventModule(Node)) continue;
                        TSharedRef<FJsonObject> ModuleRow = MakeShared<FJsonObject>();
                        ModuleRow->SetStringField(TEXT("name"), Node->GetFunctionName());
                        ModuleRow->SetStringField(TEXT("script"), Node->FunctionScript->GetPathName());
                        ModuleRow->SetStringField(TEXT("node"), Node->GetPathName());
                        ModuleRow->SetStringField(TEXT("versionGuid"), Node->SelectedScriptVersion.ToString(EGuidFormats::DigitsWithHyphens));
                        if (const FVersionedNiagaraScriptData* Version = Node->FunctionScript->GetScriptData(Node->SelectedScriptVersion))
                        {
                            ModuleRow->SetNumberField(TEXT("major"), Version->Version.MajorVersion);
                            ModuleRow->SetNumberField(TEXT("minor"), Version->Version.MinorVersion);
                        }
                        LocationEventModules.Add(MakeShared<FJsonValueObject>(ModuleRow));
                    }
                }
                Row->SetArrayField(TEXT("locationEventModules"), LocationEventModules);
            }
            Emitters.Add(MakeShared<FJsonValueObject>(Row));
        }
        Root->SetArrayField(TEXT("emitters"), Emitters);
    }
    FString Result;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Result));
    return Result;
}

FString UCombatVfxAssetLibrary::ConfigureEmitterSpace(UNiagaraSystem* System, const TArray<FName>& EmitterNames, bool bLocalSpace, bool bClearVelocity)
{
    if (!IsValid(System)) return FString::Printf(TEXT("Invalid Niagara system: %s / Niagara 시스템이 올바르지 않습니다."), *GetPathNameSafe(System));
    if (!System->GetOutermost()->GetName().StartsWith(TEXT("/Game/User_JeHoon/"))) return FString::Printf(TEXT("System is outside /Game/User_JeHoon/: %s / 프로젝트 전용 파생본 경로가 아닙니다."), *System->GetPathName());
    if (EmitterNames.IsEmpty()) return FString::Printf(TEXT("No emitters selected in %s / 변경할 이미터를 명시적으로 선택하세요."), *System->GetPathName());
    System->WaitForCompilationComplete(true, false);
    TSet<FName> FoundNames;
    TArray<FNiagaraEmitterHandle*> Selected;
    for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        if (!EmitterNames.Contains(Handle.GetName())) continue;
        const FVersionedNiagaraEmitter Instance = Handle.GetInstance();
        FVersionedNiagaraEmitterData* Data = Instance.GetEmitterData();
        const UNiagaraScriptSourceBase* Graph = Data ? Data->GraphSource.Get() : nullptr;
        const bool bEmitterOwned = IsValid(Instance.Emitter) && Instance.Emitter->IsIn(System);
        const bool bGraphOwned = IsValid(Graph) && Graph->IsIn(System);
        const bool bDuplicateName = FoundNames.Contains(Handle.GetName());
        if (!bEmitterOwned || !Data || !bGraphOwned || bDuplicateName)
        {
            return FString::Printf(TEXT("Emitter ownership rejected: system=%s, handle=%s, emitter=%s, emitterOwned=%d, versionData=%d, graph=%s, graphOwned=%d, duplicateName=%d / 이미터 또는 그래프가 외부 공유, 누락 또는 중복 상태입니다."), *System->GetPathName(), *Handle.GetName().ToString(), *GetPathNameSafe(Instance.Emitter), bEmitterOwned, Data != nullptr, *GetPathNameSafe(Graph), bGraphOwned, bDuplicateName);
        }
        TArray<UNiagaraScript*> Scripts;
        Data->GetScripts(Scripts, false);
        for (UNiagaraScript* Script : Scripts)
        {
            if (!IsValid(Script)) continue;
            if (!Script->IsIn(System))
            {
                return FString::Printf(TEXT("Script ownership rejected: system=%s, handle=%s, emitter=%s, script=%s, outer=%s / 이미터 스크립트가 파생본에 속하지 않습니다."), *System->GetPathName(), *Handle.GetName().ToString(), *GetPathNameSafe(Instance.Emitter), *Script->GetPathName(), *GetPathNameSafe(Script->GetOuter()));
            }
            if (!bClearVelocity) continue;
            for (const FNiagaraVariableWithOffset& Variable : Script->RapidIterationParameters.ReadParameterVariables())
            {
                if (IsInitialVelocity(Variable) && Variable.GetType() != FNiagaraTypeDefinition::GetVec3Def())
                {
                    return FString::Printf(TEXT("Unsupported velocity parameter: system=%s, script=%s, parameter=%s, type=%s / 초기 속도 파라미터 형식이 지원되지 않습니다."), *System->GetPathName(), *Script->GetPathName(), *Variable.GetName().ToString(), *Variable.GetType().GetName());
                }
            }
        }
        FoundNames.Add(Handle.GetName());
        Selected.Add(&Handle);
    }
    for (FName Name : EmitterNames)
    {
        if (!FoundNames.Contains(Name))
        {
            return FString::Printf(TEXT("Emitter not found: system=%s, name=%s / 이미터를 찾지 못했습니다."), *System->GetPathName(), *Name.ToString());
        }
    }
    // Complete ownership and parameter checks before changing any derivative data.
    // 파생본 데이터를 변경하기 전에 모든 소유권과 파라미터 검사를 완료합니다.
    System->Modify();
    for (FNiagaraEmitterHandle* Handle : Selected)
    {
        const FVersionedNiagaraEmitter Instance = Handle->GetInstance();
        FVersionedNiagaraEmitterData* Data = Instance.GetEmitterData();
        Instance.Emitter->Modify();
        Data->bLocalSpace = bLocalSpace;
        if (bClearVelocity)
        {
            TArray<UNiagaraScript*> Scripts;
            Data->GetScripts(Scripts, false);
            for (UNiagaraScript* Script : Scripts)
            {
                if (!IsValid(Script)) continue;
                Script->Modify();
                for (const FNiagaraVariableWithOffset& Variable : Script->RapidIterationParameters.ReadParameterVariables())
                {
                    if (IsInitialVelocity(Variable)) Script->RapidIterationParameters.SetParameterValue(FVector3f::ZeroVector, FNiagaraVariable(Variable.GetType(), Variable.GetName()));
                }
            }
        }
        // Preserve authored angular motion and renderer bindings while recompiling the changed simulation space.
        // 시뮬레이션 공간을 다시 컴파일하며 작성된 회전 운동과 렌더러 바인딩은 보존합니다.
        Data->GraphSource->MarkNotSynchronized(TEXT("Project combat VFX simulation space changed."));
    }
    // Prepare dependent script parameters through the system-wide Unreal 5.8 authoring API.
    // Unreal 5.8의 시스템 단위 작성 API로 종속 스크립트 파라미터를 준비합니다.
    System->PrepareRapidIterationParametersForCompilation();
    System->MarkPackageDirty();
    System->RequestCompile(true);
    System->WaitForCompilationComplete(true, false);
    // NullRHI authoring compiles scripts without render readiness; combat preparation checks GPU readiness later.
    // NullRHI 작성은 렌더 준비 없이 스크립트를 컴파일하며 전투 준비에서 GPU 준비를 별도로 확인합니다.
    if (!System->IsValid() || (FApp::CanEverRender() && !System->IsReadyToRun()))
    {
        return FString::Printf(TEXT("Niagara compilation failed: system=%s, valid=%d, ready=%d; do not save it. / 파생본의 Niagara 컴파일이 완료되지 않았으므로 저장하지 마세요."), *System->GetPathName(), System->IsValid(), System->IsReadyToRun());
    }
    return FString();
}

FString UCombatVfxAssetLibrary::UpgradeLocationEventToWorldVersion(UNiagaraSystem* System)
{
    if (!IsValid(System) || !System->GetOutermost()->GetName().StartsWith(TEXT("/Game/User_JeHoon/"))) return FString::Printf(TEXT("Location event upgrade requires a project derivative: %s / 위치 이벤트 변경에는 프로젝트 전용 파생본이 필요합니다."), *GetPathNameSafe(System));
    System->WaitForCompilationComplete(true, false);
    struct FUpgrade
    {
        FVersionedNiagaraEmitter Emitter;
        UNiagaraNodeFunctionCall* Node = nullptr;
        FGuid TargetVersion;
    };
    TArray<FUpgrade> Upgrades;
    TSet<UNiagaraNodeFunctionCall*> SeenNodes;
    for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        const FVersionedNiagaraEmitter Instance = Handle.GetInstance();
        FVersionedNiagaraEmitterData* Data = Instance.GetEmitterData();
        if (!Data) continue;
        UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Data->GraphSource);
        if (!IsValid(Instance.Emitter) || !Instance.Emitter->IsIn(System) || !IsValid(Source) || !Source->IsIn(System) || !IsValid(Source->NodeGraph) || !Source->NodeGraph->IsIn(System)) return FString::Printf(TEXT("Location event graph ownership rejected: system=%s, emitter=%s, source=%s / 위치 이벤트 그래프가 파생본에 속하지 않습니다."), *System->GetPathName(), *GetPathNameSafe(Instance.Emitter), *GetPathNameSafe(Source));
        TArray<UNiagaraNodeFunctionCall*> Nodes;
        Source->NodeGraph->GetNodesOfClass(Nodes);
        for (UNiagaraNodeFunctionCall* Node : Nodes)
        {
            if (!IsLocationEventModule(Node) || SeenNodes.Contains(Node)) continue;
            if (!Node->IsIn(System)) return FString::Printf(TEXT("Location event node ownership rejected: %s / 위치 이벤트 노드가 파생본에 속하지 않습니다."), *Node->GetPathName());
            SeenNodes.Add(Node);
            const FVersionedNiagaraScriptData* Current = Node->FunctionScript->GetScriptData(Node->SelectedScriptVersion);
            if (!Current || Current->Version.MajorVersion != 1 || Current->Version.MinorVersion > 1) return FString::Printf(TEXT("Unsupported location event version: node=%s, guid=%s / 위치 이벤트의 현재 버전이 지원 범위를 벗어납니다."), *Node->GetPathName(), *Node->SelectedScriptVersion.ToString());
            FGuid TargetGuid;
            for (const FNiagaraAssetVersion& Version : Node->FunctionScript->GetAllAvailableVersions())
            {
                if (Version.MajorVersion == 1 && Version.MinorVersion == 1) TargetGuid = Version.VersionGuid;
            }
            const FVersionedNiagaraScriptData* Target = TargetGuid.IsValid() ? Node->FunctionScript->GetScriptData(TargetGuid) : nullptr;
            if (!Target || !Current->PythonUpdateScript.IsEmpty() || !Current->ScriptAsset.FilePath.IsEmpty() || !Target->PythonUpdateScript.IsEmpty() || !Target->ScriptAsset.FilePath.IsEmpty()) return FString::Printf(TEXT("Location event 1.1 is missing or requires input migration: %s / 위치 이벤트 1.1이 없거나 별도 입력 변환이 필요합니다."), *Node->FunctionScript->GetPathName());
            if (Node->SelectedScriptVersion != TargetGuid) Upgrades.Add({Instance, Node, TargetGuid});
        }
    }
    if (Upgrades.IsEmpty()) return FString();
    // Preflight both producers and receivers before applying the official graph version switch to any node.
    // 노드에 공식 그래프 버전 변경을 적용하기 전에 생성·수신 모듈을 모두 사전 검사합니다.
    System->Modify();
    for (const FUpgrade& Upgrade : Upgrades)
    {
        Upgrade.Emitter.Emitter->Modify();
        Upgrade.Node->GetGraph()->Modify();
        FNiagaraScriptVersionUpgradeContext Context;
        Context.bSkipPythonScript = true;
        Context.ConstantResolver = FCompileConstantResolver(Upgrade.Emitter, FNiagaraStackGraphUtilities::GetOutputNodeUsage(*Upgrade.Node));
        Upgrade.Node->ChangeScriptVersion(Upgrade.TargetVersion, Context);
        Upgrade.Node->RefreshFromExternalChanges();
        if (Upgrade.Node->SelectedScriptVersion != Upgrade.TargetVersion) return FString::Printf(TEXT("Location event version change failed: %s; do not save. / 위치 이벤트 버전 변경에 실패했으므로 저장하지 마세요."), *Upgrade.Node->GetPathName());
        Upgrade.Emitter.GetEmitterData()->GraphSource->MarkNotSynchronized(TEXT("Project location events upgraded to world-space version 1.1."));
    }
    // Refresh all dependent rapid iteration stores after changing the module versions.
    // 모듈 버전을 변경한 뒤 모든 종속 신속 반복 파라미터 저장소를 갱신합니다.
    System->PrepareRapidIterationParametersForCompilation();
    System->MarkPackageDirty();
    System->RequestCompile(true);
    System->WaitForCompilationComplete(true, false);
    if (!System->IsValid() || (FApp::CanEverRender() && !System->IsReadyToRun())) return FString::Printf(TEXT("Location event upgrade compilation failed: %s; do not save. / 위치 이벤트 업그레이드 컴파일에 실패했으므로 저장하지 마세요."), *System->GetPathName());
    return FString();
}
