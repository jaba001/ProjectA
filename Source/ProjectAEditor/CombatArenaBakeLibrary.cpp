#include "CombatArenaBakeLibrary.h"
#include "Game/Encounter/CombatArena.h"
#include "Game/Encounter/CombatArenaEnvironment.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/ScopeExit.h"
#include "ObjectTools.h"

namespace CombatArenaEnvironmentInternal
{
    const FName VisualOwnerTag(TEXT("ProjectAUnifiedGameplayVisual"));
    const FName CentralRegionTag(TEXT("PineRidge"));

    struct FRenderBatch
    {
        UStaticMeshComponent* Source = nullptr;
        TArray<FTransform> Transforms;
        TArray<int32> SourceIndices;
    };
}

bool UCombatArenaBakeLibrary::BakeCombatArenaEnvironments(bool bOverwriteExisting, FString& OutReport)
{
    OutReport.Reset();
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || World->GetOutermost()->GetName() != TEXT("/Game/User_JeHoon/LEVEL/Core/Gameplay")) return false;
    ACombatArena* Arena = nullptr;
    for (TActorIterator<ACombatArena> It(World); It; ++It) Arena = *It;
    if (!Arena) return false;
    TArray<UPackage*> Packages;
    for (const FCombatArenaEnvironmentProfile& Definition : CombatArenaEnvironment::GetProfiles())
    {
        const FCombatArenaEnvironmentProfile* Profile = &Definition;
        const FString Name = FString::Printf(TEXT("BP_CombatArena_%s"), *Profile->ArenaId.ToString());
        const FString PackageName = TEXT("/Game/User_JeHoon/Blueprint/Environment/") + Name;
        UBlueprint* Existing = LoadObject<UBlueprint>(nullptr, *(PackageName + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
        if (Existing && !bOverwriteExisting)
        {
            OutReport += Name + TEXT(": kept existing / 기존 편집 보존\n");
            continue;
        }
        if (Existing && ObjectTools::DeleteObjectsUnchecked({Existing}) != 1)
        {
            OutReport += Name + TEXT(": replacement failed / 교체 실패\n");
            return false;
        }
        const auto Bake = [&]() -> bool
        {
            FVector CameraPosition = IsValid(Arena->CameraAnchor) ? Arena->CameraAnchor->GetActorLocation() : (Arena->CameraTransform * Arena->GetActorTransform()).GetLocation();
            if (const ACameraActor* CameraActor = Cast<ACameraActor>(Arena->CameraAnchor)) CameraPosition = CameraActor->GetCameraComponent()->GetComponentLocation();

            // The native region templates share this unchanged 4x4 combat core and physical floor.
            // 기본 지역 템플릿은 변경하지 않는 동일한 4x4 전투 코어와 물리 바닥을 공유합니다.
            const FBox CombatClearance(FVector(-790.f, -190.f, 3.f), FVector(190.f, 990.f, 600.f));
            TArray<FVector> CombatSamples;
            for (int32 Row = 0; Row < 4; ++Row)
            {
                for (int32 Column = 0; Column < 4; ++Column)
                {
                    for (float Height : {5.f, 150.f, 300.f}) CombatSamples.Add(FVector(-Row * 200.f, Column * 200.f + (Column >= 2 ? 200.f : 0.f), Height));
                }
            }
            TArray<AActor*> CentralActors;
            TArray<CombatArenaEnvironmentInternal::FRenderBatch> Batches;
            int32 GroundCount = 0;
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                AActor* Actor = *It;
                if (!Actor->ActorHasTag(CombatArenaEnvironmentInternal::VisualOwnerTag)) continue;
                if (Actor->ActorHasTag(CombatArenaEnvironmentInternal::CentralRegionTag)) CentralActors.Add(Actor);
                if (!Actor->ActorHasTag(Profile->SourceRegionTag)) continue;
                TInlineComponentArray<UStaticMeshComponent*> Components(Actor);
                for (UStaticMeshComponent* Source : Components)
                {
                    if (!IsValid(Source)
                        || !Source->GetStaticMesh()
                        || !Source->IsVisible()) continue;
                    CombatArenaEnvironmentInternal::FRenderBatch Batch;
                    Batch.Source = Source;
                    const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Source);
                    const int32 Count = Instances ? Instances->GetInstanceCount() : 1;
                    if (Instances && (Instances->NumCustomDataFloats < 0 || static_cast<int64>(Instances->NumCustomDataFloats) * Count != Instances->PerInstanceSMCustomData.Num())) return false;
                    for (int32 Index = 0; Index < Count; ++Index)
                    {
                        FTransform Transform = Source->GetComponentTransform();
                        if (Instances && !Instances->GetInstanceTransform(Index, Transform, true)) return false;
                        Transform = CombatArenaEnvironment::RebaseTransform(Transform, *Profile);
                        if (Transform.ContainsNaN()) return false;
                        const FBox Bounds = Source->GetStaticMesh()->GetBoundingBox().TransformBy(Transform);
                        if (!CombatArenaEnvironment::ShouldKeepBounds(Bounds, CombatClearance, CameraPosition, CombatSamples)) continue;
                        GroundCount += CombatArenaEnvironment::IsGroundBounds(Bounds) ? 1 : 0;
                        Batch.Transforms.Add(Transform);
                        Batch.SourceIndices.Add(Index);
                    }
                    if (!Batch.Transforms.IsEmpty()) Batches.Add(MoveTemp(Batch));
                }
            }
            if (CentralActors.IsEmpty()
                || Batches.IsEmpty()
                || GroundCount == 0) return false;
            OutReport += FString::Printf(TEXT("%s: batches=%d ground=%d\n"), *Profile->ArenaId.ToString(), Batches.Num(), GroundCount);

            // Capture a temporary editor actor as serialized Blueprint components without changing the source scene.
            // 원본 장면을 바꾸지 않고 임시 에디터 액터를 직렬화된 Blueprint 컴포넌트로 캡처합니다.
            FActorSpawnParameters Params;
            Params.Owner = nullptr;
            Params.ObjectFlags |= RF_Transient;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AActor* Candidate = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
            if (!Candidate) return false;
            Candidate->SetReplicates(false);
            Candidate->SetActorEnableCollision(false);
            Candidate->SetActorHiddenInGame(false);
            USceneComponent* Root = NewObject<USceneComponent>(Candidate, TEXT("EnvironmentRoot"), RF_Transient);
            Candidate->SetRootComponent(Root);
            Candidate->AddInstanceComponent(Root);
            Root->SetMobility(EComponentMobility::Static);
            Root->RegisterComponent();
            ON_SCOPE_EXIT
            {
                Candidate->Destroy();
            };
            if (!Root->IsRegistered()) return false;
            int32 ComponentIndex = 0;
            for (const CombatArenaEnvironmentInternal::FRenderBatch& Batch : Batches)
            {
                UStaticMeshComponent* Source = Batch.Source;
                const FName ComponentName(*FString::Printf(TEXT("ArenaMesh%03d"), ComponentIndex++));
                UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Candidate, Source->GetClass(), ComponentName, RF_Transient);
                if (!Component) return false;
                Component->ComponentTags.Add(FName(*Source->GetOwner()->GetActorLabel()));
                Candidate->AddInstanceComponent(Component);
                Component->SetupAttachment(Root);
                Component->SetMobility(EComponentMobility::Static);
                if (!Component->SetStaticMesh(Source->GetStaticMesh()) && Component->GetStaticMesh() != Source->GetStaticMesh()) return false;
                for (int32 Index = 0; Index < Source->GetNumMaterials(); ++Index) Component->SetMaterial(Index, Source->GetMaterial(Index));
                Component->SetCastShadow(Source->CastShadow);
                Component->SetReceivesDecals(Source->bReceivesDecals);
                Component->SetAffectDistanceFieldLighting(Source->bAffectDistanceFieldLighting);
                Component->SetVisibleInRayTracing(Source->bVisibleInRayTracing);
                Component->SetRenderInMainPass(Source->bRenderInMainPass);
                Component->SetRenderInDepthPass(Source->bRenderInDepthPass);
                Component->SetRenderCustomDepth(Source->bRenderCustomDepth);
                Component->SetCustomDepthStencilValue(Source->CustomDepthStencilValue);
                Component->SetCustomDepthStencilWriteMask(Source->CustomDepthStencilWriteMask);
                Component->SetForcedLodModel(Source->GetForcedLodModel());
                Component->SetHiddenInGame(Source->bHiddenInGame);
                Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Component->SetGenerateOverlapEvents(false);
                Component->SetCanEverAffectNavigation(false);
                Component->SetComponentTickEnabled(false);
                Component->SetCullDistance(18000.f);
                Component->RegisterComponent();
                if (!Component->IsRegistered()) return false;
                if (UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
                {
                    // Keep ISM and ordinary component types intact; unsupported source materials never gain ISM usage.
                    // ISM과 일반 컴포넌트 종류를 유지하며 미지원 원본 재질에 ISM 사용을 강제하지 않습니다.
                    const UInstancedStaticMeshComponent* Original = CastChecked<UInstancedStaticMeshComponent>(Source);
                    Instances->SetNumCustomDataFloats(Original->NumCustomDataFloats);
                    Instances->SetCullDistances(15000, 18000);
                    for (int32 Index = 0; Index < Batch.Transforms.Num(); ++Index)
                    {
                        if (Instances->AddInstance(Batch.Transforms[Index], true) != Index) return false;
                        if (Original->NumCustomDataFloats > 0)
                        {
                            const int32 Offset = Batch.SourceIndices[Index] * Original->NumCustomDataFloats;
                            const TArrayView<const float> Data(Original->PerInstanceSMCustomData.GetData() + Offset, Original->NumCustomDataFloats);
                            if (!Instances->SetCustomData(Index, Data, true)) return false;
                        }
                    }
                }
                else
                {
                    Component->SetWorldTransform(Batch.Transforms[0]);
                }
            }
            FKismetEditorUtilities::FCreateBlueprintFromActorParams BlueprintParams;
            BlueprintParams.bReplaceActor = false;
            BlueprintParams.bKeepMobility = true;
            BlueprintParams.bOpenBlueprint = false;
            UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor(PackageName, Candidate, BlueprintParams);
            if (!Blueprint || !Blueprint->GeneratedClass) return false;
            Packages.Add(Blueprint->GetPackage());
            return true;
        };
        if (!Bake())
        {
            OutReport += Name + TEXT(": bake failed / 굽기 실패\n");
            return false;
        }
    }
    return Packages.IsEmpty() || UEditorLoadingAndSavingUtils::SavePackages(Packages, false);
}
