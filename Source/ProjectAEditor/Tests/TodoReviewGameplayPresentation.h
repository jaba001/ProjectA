#pragma once

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "Components/MeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Game/Encounter/CombatArena.h"
#include "GameFramework/PlayerController.h"
#include "Grid/Combat/CombatGridManager.h"
#include "Grid/Combat/CombatGridTile.h"
#include "Unit/UnitBase.h"

namespace TodoReviewGameplayPresentation
{
    // Observe normal frames and visible geometry; never repair a camera, mesh, grid or simulation for a screenshot.
    // 캡처를 위해 카메라·메시·그리드·시뮬레이션을 보정하지 않고 정상 프레임과 표시 지오메트리만 관측합니다.
    class FReadiness
    {
    public:
        bool Poll(UWorld* World, const ACombatRoundCoordinator* Round, TSharedRef<FJsonObject> Report)
        {
            if (!World || !Round) return false;
            if (LastEngineFrame == GFrameCounter) return bReady;
            LastEngineFrame = GFrameCounter;
            ++ObservedFrames;
            if (ObservedFrames == 1) FirstWorldTime = World->GetTimeSeconds();
            const float WorldTime = World->GetTimeSeconds();
            APlayerController* Controller = World->GetFirstPlayerController();
            const ACombatArena* Arena = Round->GetArena();
            AActor* Target = Controller ? Controller->GetViewTarget() : nullptr;
            const UCameraComponent* Camera = Target ? Target->FindComponentByClass<UCameraComponent>() : nullptr;
            const APlayerCameraManager* Manager = Controller ? Controller->PlayerCameraManager : nullptr;
            const bool bExpectedTarget = Arena && Target && Camera && Target != Controller && (Arena->CameraAnchor ? Target == Arena->CameraAnchor.Get() : Target->GetActorTransform().Equals(Arena->CameraTransform * Arena->GetActorTransform(), 1.f));
            const bool bExpectedPOV = bExpectedTarget && Manager && Manager->GetCameraLocation().Equals(Camera->GetComponentLocation(), 1.f) && Manager->GetCameraRotation().Equals(Camera->GetComponentRotation(), 0.1f);
            Report->SetStringField(TEXT("view_target"), GetPathNameSafe(Target));
            Report->SetStringField(TEXT("authored_camera_anchor"), Arena ? GetPathNameSafe(Arena->CameraAnchor) : TEXT("none"));
            Report->SetStringField(TEXT("actual_camera_location"), Manager ? Manager->GetCameraLocation().ToString() : TEXT("none"));
            Report->SetStringField(TEXT("actual_camera_rotation"), Manager ? Manager->GetCameraRotation().ToString() : TEXT("none"));
            Report->SetStringField(TEXT("target_camera_location"), Camera ? Camera->GetComponentLocation().ToString() : TEXT("none"));
            Report->SetStringField(TEXT("target_camera_rotation"), Camera ? Camera->GetComponentRotation().ToString() : TEXT("none"));
            Report->SetBoolField(TEXT("camera_pov_matches_authored_arena"), bExpectedPOV);
            TArray<TSharedPtr<FJsonValue>> Units;
            int32 Living = 0;
            int32 DrawnLiving = 0;
            for (const FCombatRoundUnitView& Entry : Round->GetView().Units)
            {
                AUnitBase* Unit = Entry.Unit;
                if (!Unit || !Unit->IsUnitAlive()) continue;
                ++Living;
                bool bDrawn = false;
                TSharedRef<FJsonObject> UnitReport = MakeShared<FJsonObject>();
                UnitReport->SetNumberField(TEXT("id"), Entry.UnitId);
                UnitReport->SetStringField(TEXT("actor"), Unit->GetPathName());
                UnitReport->SetStringField(TEXT("location"), Unit->GetActorLocation().ToString());
                UnitReport->SetBoolField(TEXT("actor_hidden"), Unit->IsHidden());
                TArray<TSharedPtr<FJsonValue>> Meshes;
                TInlineComponentArray<UMeshComponent*> Components(Unit);
                for (const UMeshComponent* Mesh : Components)
                {
                    const float LastOnScreen = Mesh->GetLastRenderTimeOnScreen();
                    const bool bVisible = !Unit->IsHidden() && Mesh->IsRegistered() && Mesh->IsVisible() && !Mesh->bHiddenInGame;
                    const bool bRecent = bVisible && LastOnScreen >= FirstWorldTime && WorldTime - LastOnScreen <= 0.25f;
                    bDrawn |= bRecent;
                    TSharedRef<FJsonObject> MeshReport = MakeShared<FJsonObject>();
                    MeshReport->SetStringField(TEXT("component"), Mesh->GetName());
                    MeshReport->SetBoolField(TEXT("visible_registered"), bVisible);
                    MeshReport->SetNumberField(TEXT("last_render_time_on_screen"), LastOnScreen);
                    MeshReport->SetBoolField(TEXT("recently_drawn_on_screen"), bRecent);
                    Meshes.Add(MakeShared<FJsonValueObject>(MeshReport));
                }
                UnitReport->SetArrayField(TEXT("meshes"), Meshes);
                UnitReport->SetBoolField(TEXT("has_recent_visible_mesh"), bDrawn);
                Units.Add(MakeShared<FJsonValueObject>(UnitReport));
                if (bDrawn) ++DrawnLiving;
            }
            int32 DrawnTiles = 0;
            if (Arena && Arena->Grid)
            {
                for (const TPair<FIntPoint, ACombatGridTile*>& Entry : Arena->Grid->TileMap)
                {
                    const ACombatGridTile* Tile = Entry.Value;
                    if (!Tile || Tile->IsHidden()) continue;
                    TInlineComponentArray<UMeshComponent*> Meshes(Tile);
                    if (Meshes.ContainsByPredicate([WorldTime, this](const UMeshComponent* Mesh)
                    {
                        const float LastOnScreen = Mesh->GetLastRenderTimeOnScreen();
                        return Mesh->IsRegistered() && Mesh->IsVisible() && !Mesh->bHiddenInGame && LastOnScreen >= FirstWorldTime && WorldTime - LastOnScreen <= 0.25f;
                    })) ++DrawnTiles;
                }
            }
            const bool bCurrentReady = bExpectedPOV && Living > 0 && DrawnLiving == Living && DrawnTiles > 0;
            if (ObservedFrames == 1)
            {
                TSharedRef<FJsonObject> First = MakeShared<FJsonObject>();
                First->SetStringField(TEXT("view_target"), GetPathNameSafe(Target));
                First->SetStringField(TEXT("camera_location"), Manager ? Manager->GetCameraLocation().ToString() : TEXT("none"));
                First->SetStringField(TEXT("camera_rotation"), Manager ? Manager->GetCameraRotation().ToString() : TEXT("none"));
                First->SetBoolField(TEXT("camera_pov_matches_authored_arena"), bExpectedPOV);
                First->SetNumberField(TEXT("recently_rendered_living_units"), DrawnLiving);
                First->SetNumberField(TEXT("recently_rendered_grid_tiles"), DrawnTiles);
                Report->SetObjectField(TEXT("first_observation"), First);
            }
            StableFrames = bCurrentReady ? StableFrames + 1 : 0;
            bReady = StableFrames >= 30 && WorldTime - FirstWorldTime >= 1.f;
            Report->SetArrayField(TEXT("living_unit_presentation"), Units);
            Report->SetNumberField(TEXT("living_units"), Living);
            Report->SetNumberField(TEXT("recently_rendered_living_units"), DrawnLiving);
            Report->SetNumberField(TEXT("recently_rendered_grid_tiles"), DrawnTiles);
            Report->SetNumberField(TEXT("observed_distinct_engine_frames"), ObservedFrames);
            Report->SetNumberField(TEXT("consecutive_ready_engine_frames"), StableFrames);
            Report->SetNumberField(TEXT("observed_world_seconds"), WorldTime - FirstWorldTime);
            Report->SetNumberField(TEXT("world_seconds"), WorldTime);
            Report->SetBoolField(TEXT("ready_for_evidence_capture"), bReady);
            Report->SetStringField(TEXT("scope"), TEXT("Natural engine frames only; expected arena view target and camera POV, every living unit has a visible registered mesh reported on-screen within 0.25 world seconds, and at least one grid tile is similarly rendered. Thirty consecutive ready engine frames and at least one elapsed world second precede evidence capture. No Tick, camera, visibility, transform, health or simulation mutations. Final PNG still requires visual inspection."));
            return bReady;
        }

    private:
        uint64 LastEngineFrame = MAX_uint64;
        int32 ObservedFrames = 0;
        int32 StableFrames = 0;
        float FirstWorldTime = 0.f;
        bool bReady = false;
    };
}
