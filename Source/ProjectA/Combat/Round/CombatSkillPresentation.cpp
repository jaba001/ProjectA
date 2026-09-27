#include "Combat/Round/CombatSkillPresentation.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCombatSkillPresentation, Log, All);

namespace
{
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

void CombatSkillPresentation::Attach(AActor* Owner, const FCombatSkillVfx& Visual, TArray<TObjectPtr<UFXSystemComponent>>& Components)
{
    Destroy(Components);
    if (!IsValid(Owner) || !Owner->GetRootComponent() || Owner->GetNetMode() == NM_DedicatedServer || Visual.RelativeTransform.ContainsNaN()) return;
    const FVector Location = Visual.RelativeTransform.GetLocation();
    const FRotator Rotation = Visual.RelativeTransform.Rotator();
    const FVector Scale = Visual.RelativeTransform.GetScale3D();
    if (UNiagaraSystem* System = Visual.Niagara.LoadSynchronous())
    {
        UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Owner->GetRootComponent(), NAME_None, Location, Rotation, Scale, EAttachLocation::KeepRelativeOffset, false, ENCPoolMethod::None, true, false);
        if (Component) Components.Add(Component);
    }
    if (UParticleSystem* System = Visual.Cascade.LoadSynchronous())
    {
        UParticleSystemComponent* Component = UGameplayStatics::SpawnEmitterAttached(System, Owner->GetRootComponent(), NAME_None, Location, Rotation, Scale, EAttachLocation::KeepRelativeOffset, false, EPSCPoolMethod::None, true);
        if (Component) Components.Add(Component);
    }
    for (UFXSystemComponent* Component : Components)
    {
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
