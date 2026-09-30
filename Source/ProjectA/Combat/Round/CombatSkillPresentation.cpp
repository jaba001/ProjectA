#include "Combat/Round/CombatSkillPresentation.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "Distributions/DistributionFloatParticleParameter.h"
#include "Distributions/DistributionVectorParticleParameter.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCombatSkillPresentation, Log, All);

namespace
{
    bool FindParameterInput(double Output, double MinInput, double MaxInput, double MinOutput, double MaxOutput, DistributionParamMode Mode, double& Input)
    {
        if (!FMath::IsFinite(Output) || !FMath::IsFinite(MinInput) || !FMath::IsFinite(MaxInput) || !FMath::IsFinite(MinOutput) || !FMath::IsFinite(MaxOutput)) return false;
        if (Mode == DPM_Direct)
        {
            Input = Output;
            return true;
        }
        if (Mode != DPM_Normal && Mode != DPM_Abs) return false;
        if (FMath::IsNearlyEqual(MinOutput, MaxOutput) || MaxInput <= MinInput)
        {
            Input = MinInput;
            return FMath::IsNearlyEqual(Output, MinOutput) && (Mode != DPM_Abs || Input >= 0.0);
        }
        if (Output < FMath::Min(MinOutput, MaxOutput) || Output > FMath::Max(MinOutput, MaxOutput)) return false;
        Input = MinInput + (Output - MinOutput) * (MaxInput - MinInput) / (MaxOutput - MinOutput);
        return FMath::IsFinite(Input) && (Mode != DPM_Abs || Input >= 0.0);
    }

    void BindProjectileParameters(UParticleSystemComponent* Component, UParticleSystem* System, const CombatSkillPresentation::FProjectileParameters& Projectile)
    {
        if (!IsValid(Component) || !IsValid(System) || Projectile.WorldVelocity.ContainsNaN() || !FMath::IsFinite(Projectile.Lifetime) || Projectile.Lifetime <= 0.f) return;
        TMap<FName, FVector> VectorInputs;
        TMap<FName, float> FloatInputs;
        TSet<FName> UnboundParameters;
        // Inspect module semantics instead of guessing parameter names or modifying source distributions.
        // 파라미터 이름을 추정하거나 원본 분포를 수정하지 않고 모듈의 의미를 확인합니다.
        for (const UParticleEmitter* Emitter : System->Emitters)
        {
            if (!IsValid(Emitter)) continue;
            for (const UParticleLODLevel* LOD : Emitter->LODLevels)
            {
                if (!IsValid(LOD) || !IsValid(LOD->RequiredModule)) continue;
                for (const UParticleModule* Module : LOD->Modules)
                {
                    if (!IsValid(Module) || !Module->bEnabled) continue;
                    if (const UParticleModuleVelocity* Velocity = Cast<UParticleModuleVelocity>(Module))
                    {
                        const UDistributionVectorParticleParameter* Parameter = Cast<UDistributionVectorParticleParameter>(Velocity->StartVelocity.Distribution);
                        if (!Parameter || Parameter->ParameterName.IsNone()) continue;
                        // Local particles already follow the actor; world particles need its launch velocity exactly once.
                        // 로컬 입자는 액터를 이미 따라가며 월드 입자는 액터의 발사 속도를 한 번만 적용합니다.
                        FVector Desired = LOD->RequiredModule->bUseLocalSpace ? FVector::ZeroVector : Projectile.WorldVelocity;
                        if (Velocity->bApplyOwnerScale) Desired /= Component->GetComponentScale();
                        if (!Velocity->bInWorldSpace) Desired = (Component->GetComponentQuat() * LOD->RequiredModule->EmitterRotation.Quaternion()).UnrotateVector(Desired);
                        FVector Input;
                        bool bValid = !Desired.ContainsNaN();
                        for (int32 Axis = 0; Axis < 3 && bValid; ++Axis) bValid = FindParameterInput(Desired[Axis], Parameter->MinInput[Axis], Parameter->MaxInput[Axis], Parameter->MinOutput[Axis], Parameter->MaxOutput[Axis], Parameter->ParamModes[Axis], Input[Axis]);
                        const FVector* Existing = VectorInputs.Find(Parameter->ParameterName);
                        if (!bValid || (Existing && !Existing->Equals(Input, 0.0001))) UnboundParameters.Add(Parameter->ParameterName);
                        else VectorInputs.Add(Parameter->ParameterName, Input);
                    }
                    else if (const UParticleModuleLifetime* Lifetime = Cast<UParticleModuleLifetime>(Module))
                    {
                        const UDistributionFloatParticleParameter* Parameter = Cast<UDistributionFloatParticleParameter>(Lifetime->Lifetime.Distribution);
                        if (!Parameter || Parameter->ParameterName.IsNone()) continue;
                        double Input = 0.0;
                        const bool bValid = FindParameterInput(Projectile.Lifetime, Parameter->MinInput, Parameter->MaxInput, Parameter->MinOutput, Parameter->MaxOutput, Parameter->ParamMode, Input);
                        const float* Existing = FloatInputs.Find(Parameter->ParameterName);
                        if (!bValid || (Existing && !FMath::IsNearlyEqual(*Existing, static_cast<float>(Input)))) UnboundParameters.Add(Parameter->ParameterName);
                        else FloatInputs.Add(Parameter->ParameterName, static_cast<float>(Input));
                    }
                }
            }
        }
        // One instance parameter may feed several emitters or LODs; conflicting mappings keep authored values.
        // 하나의 인스턴스 파라미터를 여러 이미터나 LOD가 공유하면 서로 충돌하는 매핑은 원본 값을 유지합니다.
        for (const TPair<FName, FVector>& Entry : VectorInputs)
        {
            if (FloatInputs.Contains(Entry.Key)) UnboundParameters.Add(Entry.Key);
        }
        for (const TPair<FName, FVector>& Entry : VectorInputs)
        {
            if (!UnboundParameters.Contains(Entry.Key)) Component->SetVectorParameter(Entry.Key, Entry.Value);
        }
        for (const TPair<FName, float>& Entry : FloatInputs)
        {
            if (!UnboundParameters.Contains(Entry.Key)) Component->SetFloatParameter(Entry.Key, Entry.Value);
        }
        for (FName Name : UnboundParameters) UE_LOG(LogCombatSkillPresentation, Verbose, TEXT("Projectile parameter retains authored mapping Asset=%s Parameter=%s / 투사체 파라미터의 원본 매핑 유지"), *System->GetPathName(), *Name.ToString());
    }

    void LimitImpactLifetime(UWorld* World, UFXSystemComponent* Component)
    {
        if (!IsValid(Component)) return;
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
        // Imported looping systems must not outlive a completed impact indefinitely.
        // 임포트한 반복 시스템이 완료된 피격 이후 무기한 남지 않도록 합니다.
        FTimerHandle Handle;
        World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Component, [Component]() { Component->DestroyComponent(); }), 5.f, false);
    }
}

bool CombatSkillPresentation::Prepare(UWorld* World, const TArray<FCombatRoundSkill>& Skills, TArray<TObjectPtr<UObject>>& PreparedAssets, FText& OutError)
{
    OutError = FText::GetEmpty();
    if (!IsValid(World))
    {
        OutError = FText::FromString(TEXT("스킬 이펙트를 준비할 월드가 없습니다."));
        return false;
    }
    if (!FApp::CanEverRender() || World->GetNetMode() == NM_DedicatedServer)
    {
        PreparedAssets.Reset();
        return true;
    }
    TArray<TObjectPtr<UObject>> Assets;
    const auto Fail = [&OutError](const FCombatRoundSkill& Skill, const FString& Path, const TCHAR* Reason)
    {
        OutError = FText::FromString(FString::Printf(TEXT("%s: %s\n%s"), *Skill.Name.ToString(), Reason, *Path));
        UE_LOG(LogCombatSkillPresentation, Warning, TEXT("Effect preparation failed Skill=%s Asset=%s Reason=%s / 스킬 효과 준비 실패"), *Skill.SkillId.ToString(), *Path, Reason);
        return false;
    };
    for (const FCombatRoundSkill& Skill : Skills)
    {
        for (const FCombatSkillVfx* Visual : {&Skill.Vfx, &Skill.ImpactVfx})
        {
            if (!Visual->Niagara.IsNull())
            {
                UNiagaraSystem* System = Visual->Niagara.LoadSynchronous();
                if (!IsValid(System)) return Fail(Skill, Visual->Niagara.ToString(), TEXT("Niagara 이펙트 에셋을 불러오지 못했습니다."));
                if (!Assets.Contains(System))
                {
#if WITH_EDITORONLY_DATA
                    // Loading the package starts asynchronous Niagara compilation; include GPU shaders before accepting the loadout.
                    // 패키지 로드가 비동기 Niagara 컴파일을 시작하므로 장착 승인 전에 GPU 셰이더까지 준비합니다.
                    System->WaitForCompilationComplete(true, false);
#endif
                    if (!System->IsReadyToRun()) return Fail(Skill, System->GetPathName(), TEXT("이펙트 실행 준비가 끝나지 않았습니다. 잠시 후 다시 시도하고 계속 실패하면 에셋 컴파일 로그를 확인하세요."));
                    if (!System->IsValid()) return Fail(Skill, System->GetPathName(), TEXT("Niagara 이펙트의 스크립트 또는 이미터 구성이 유효하지 않습니다. 에셋 컴파일 로그를 확인하세요."));
                    Assets.Add(System);
                }
            }
            if (!Visual->Cascade.IsNull())
            {
                UParticleSystem* System = Visual->Cascade.LoadSynchronous();
                if (!IsValid(System)) return Fail(Skill, Visual->Cascade.ToString(), TEXT("Cascade 이펙트 에셋을 불러오지 못했습니다."));
                Assets.AddUnique(System);
            }
        }
    }
    for (UObject* Asset : Assets)
    {
        if (!PreparedAssets.Contains(Asset)) UE_LOG(LogCombatSkillPresentation, Log, TEXT("Effect ready before combat execution Asset=%s / 전투 실행 전 스킬 효과 준비 완료"), *Asset->GetPathName());
    }
    PreparedAssets = MoveTemp(Assets);
    return true;
}

void CombatSkillPresentation::Attach(AActor* Owner, const FCombatSkillVfx& Visual, TArray<TObjectPtr<UFXSystemComponent>>& Components, bool bAutoDestroy, const FProjectileParameters* Projectile)
{
    Destroy(Components);
    if (!IsValid(Owner) || !Owner->GetRootComponent() || Owner->GetNetMode() == NM_DedicatedServer || Visual.RelativeTransform.ContainsNaN()) return;
    const FVector Location = Visual.RelativeTransform.GetLocation();
    const FRotator Rotation = Visual.RelativeTransform.Rotator();
    const FVector Scale = Visual.RelativeTransform.GetScale3D();
    if (UNiagaraSystem* System = Visual.Niagara.LoadSynchronous())
    {
        UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Owner->GetRootComponent(), NAME_None, Location, Rotation, Scale, EAttachLocation::KeepRelativeOffset, bAutoDestroy, ENCPoolMethod::None, true, false);
        if (IsValid(Component)) Components.Add(Component);
    }
    if (UParticleSystem* System = Visual.Cascade.LoadSynchronous())
    {
        UParticleSystemComponent* Component = UGameplayStatics::SpawnEmitterAttached(System, Owner->GetRootComponent(), NAME_None, Location, Rotation, Scale, EAttachLocation::KeepRelativeOffset, bAutoDestroy, EPSCPoolMethod::None, false);
        if (IsValid(Component))
        {
            if (Projectile) BindProjectileParameters(Component, System, *Projectile);
            Component->ActivateSystem(true);
            if (IsValid(Component)) Components.Add(Component);
        }
    }
    for (UFXSystemComponent* Component : Components)
    {
        if (!IsValid(Component)) continue;
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
    }
}

void CombatSkillPresentation::Destroy(TArray<TObjectPtr<UFXSystemComponent>>& Components)
{
    for (UFXSystemComponent* Component : Components)
    {
        if (IsValid(Component)) Component->DestroyComponent();
    }
    Components.Reset();
}

void CombatSkillPresentation::Impact(UWorld* World, const FCombatSkillVfx& Visual, const FTransform& Transform)
{
    if (!World || World->GetNetMode() == NM_DedicatedServer || Visual.RelativeTransform.ContainsNaN() || Transform.ContainsNaN()) return;
    const FTransform WorldTransform = Visual.RelativeTransform * Transform;
    if (UNiagaraSystem* System = Visual.Niagara.LoadSynchronous())
    {
        LimitImpactLifetime(World, UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, WorldTransform.GetLocation(), WorldTransform.Rotator(), WorldTransform.GetScale3D(), true, true, ENCPoolMethod::None, false));
    }
    if (UParticleSystem* System = Visual.Cascade.LoadSynchronous())
    {
        LimitImpactLifetime(World, UGameplayStatics::SpawnEmitterAtLocation(World, System, WorldTransform, true, EPSCPoolMethod::None, true));
    }
}
