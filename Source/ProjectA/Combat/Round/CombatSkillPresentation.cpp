#include "Combat/Round/CombatSkillPresentation.h"
#include "Combat/Round/CombatRoundTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

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
