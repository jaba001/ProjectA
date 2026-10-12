#pragma once

#include "CoreMinimal.h"

struct FEncounterDungeonFloorRegion
{
    FTransform Transform;
    FVector2D HalfExtent;
    bool bOpenForwardEdge = false;
};

namespace EncounterDungeonLayout
{
    constexpr int32 VariantCount = 8;
    constexpr float CellSize = 400.f;
    constexpr float WallHeight = 420.f;
    constexpr float EyeHeight = 170.f;
    constexpr float DoorOpenDuration = 0.65f;
    // Shorter corridors use gentler walking and turning without speeding up doorway transitions.
    // 짧아진 통로에서 보행과 회전을 완만하게 하고 문 전환 이동도 느리게 유지합니다.
    constexpr float ArrivalTravelDuration = 3.2f;
    constexpr float CombatWalkDuration = 4.2f;
    constexpr float TurnDuration = 0.28f;
    constexpr float ExitWalkSpeed = 500.f;
    constexpr float LoopEntryWalkDuration = 0.8f;
    constexpr float EntranceDoorApproachDistance = 120.f;
    constexpr int32 DoorsPerRoom = 3;
    constexpr int32 LoopEntranceDoorIndex = 3 * DoorsPerRoom;
    constexpr int32 ChoiceEntranceDoorIndex = LoopEntranceDoorIndex + 1;
    constexpr float DoorCoverDistance = 32.f;

    // Legacy APIs preserve variant zero; invalid variants return empty geometry or identity.
    // 기존 인자 형식은 변형 0을 유지하며 잘못된 변형은 빈 지형 또는 단위 변환을 반환합니다.
    PROJECTA_API TArray<FVector> GetPath(int32 Direction);
    PROJECTA_API TArray<FVector> GetPath(int32 Direction, int32 LayoutVariant);
    PROJECTA_API FQuat GetBranchRotation(int32 Direction);
    PROJECTA_API FVector GetChoiceDoorCenter();
    PROJECTA_API TArray<FEncounterDungeonFloorRegion> GetFloorRegions(int32 LayoutVariant = 0);
    PROJECTA_API bool HasFloorAt(const TArray<FEncounterDungeonFloorRegion>& Regions, const FVector& Point);
    PROJECTA_API TArray<FVector2D> GetExposedWallSpans(const TArray<FEncounterDungeonFloorRegion>& Regions, int32 RegionIndex, const FVector2D& LocalNormal);
    PROJECTA_API FTransform GetStageTransform(int32 Direction);
    PROJECTA_API FTransform GetStageTransform(int32 Direction, int32 LayoutVariant);
    PROJECTA_API FVector GetDoorCenter(int32 Direction, int32 LayoutVariant, bool bExit);
    PROJECTA_API FVector GetLoopDoorCenter(int32 Direction, int32 LayoutVariant);
    PROJECTA_API TArray<FVector> GetExitPath(int32 Direction, int32 LayoutVariant);
    PROJECTA_API FText GetDirectionLabel(int32 Index);
}
