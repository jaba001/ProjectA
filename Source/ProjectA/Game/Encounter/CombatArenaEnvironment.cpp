#include "Game/Encounter/CombatArenaEnvironment.h"
#include "Game/Run/RunPveDifficulty.h"

const TArray<FCombatArenaEnvironmentProfile>& CombatArenaEnvironment::GetProfiles()
{
    static const TArray<FCombatArenaEnvironmentProfile> Profiles = []()
    {
        TArray<FCombatArenaEnvironmentProfile> Result;
        FCombatArenaEnvironmentProfile Meadow;
        Meadow.ArenaId = TEXT("MeadowBloom");
        Meadow.SourceRegionTag = Meadow.ArenaId;
        Meadow.SourceOffset = FVector(-12000.f, 0.f, 0.f);
        Meadow.DisplayName = NSLOCTEXT("CombatArenaEnvironment", "MeadowName", "꽃 초원");
        Meadow.Description = NSLOCTEXT("CombatArenaEnvironment", "MeadowDescription", "풀과 꽃이 둘러싼 탁 트인 전장");
        Meadow.DifficultyQuery = FGameplayTagQuery::MakeQuery_MatchTag(RunPveDifficulty::GetLowTag());
        Result.Add(MoveTemp(Meadow));
        FCombatArenaEnvironmentProfile Dungeon;
        Dungeon.ArenaId = TEXT("DungeonStone");
        Dungeon.SourceRegionTag = Dungeon.ArenaId;
        Dungeon.SourceOffset = FVector(24000.f, 24000.f, 0.f);
        Dungeon.DisplayName = NSLOCTEXT("CombatArenaEnvironment", "DungeonName", "석조 던전");
        Dungeon.Description = NSLOCTEXT("CombatArenaEnvironment", "DungeonDescription", "석재 바닥과 오래된 벽으로 둘러싸인 전장");
        Dungeon.DifficultyQuery = FGameplayTagQuery::MakeQuery_MatchTag(RunPveDifficulty::GetMediumTag());
        Result.Add(MoveTemp(Dungeon));
        FCombatArenaEnvironmentProfile Ice;
        Ice.ArenaId = TEXT("IceCitadel");
        Ice.SourceRegionTag = Ice.ArenaId;
        Ice.SourceOffset = FVector(0.f, 24000.f, 0.f);
        Ice.DisplayName = NSLOCTEXT("CombatArenaEnvironment", "IceName", "얼음 성채");
        Ice.Description = NSLOCTEXT("CombatArenaEnvironment", "IceDescription", "눈과 얼음 요새의 잔해가 펼쳐진 전장");
        Ice.DifficultyQuery = FGameplayTagQuery::MakeQuery_MatchTag(RunPveDifficulty::GetHighTag());
        Result.Add(MoveTemp(Ice));
        return Result;
    }();
    return Profiles;
}

const FCombatArenaEnvironmentProfile* CombatArenaEnvironment::Find(FName ArenaId)
{
    return GetProfiles().FindByPredicate([ArenaId](const FCombatArenaEnvironmentProfile& Profile) { return Profile.ArenaId == ArenaId; });
}

FTransform CombatArenaEnvironment::RebaseTransform(const FTransform& Source, const FCombatArenaEnvironmentProfile& Profile)
{
    FTransform Result = Source;
    Result.AddToTranslation(-Profile.SourceOffset);
    return Result;
}

bool CombatArenaEnvironment::IsGroundBounds(const FBox& Bounds)
{
    // Authored ground tops are at Z=1, above the unchanged physical floor; preserve that exact height.
    // 작성된 지면 윗면은 원래 물리 바닥 위 Z=1이며 해당 높이를 그대로 유지합니다.
    return Bounds.IsValid && !Bounds.Min.ContainsNaN() && !Bounds.Max.ContainsNaN() && Bounds.Max.Z >= -1.f && Bounds.Max.Z <= 3.f && Bounds.Min.Z <= 1.5f;
}

bool CombatArenaEnvironment::ShouldKeepBounds(const FBox& Bounds, const FBox& CombatClearance, const FVector& CameraPosition, const TArray<FVector>& CombatSamples)
{
    if (!Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN() || !CombatClearance.IsValid || CameraPosition.ContainsNaN() || CombatSamples.IsEmpty()) return false;
    if (IsGroundBounds(Bounds)) return true;
    if (Bounds.Intersect(CombatClearance)) return false;
    const FBox Padded = Bounds.ExpandBy(FVector(60.f, 60.f, 20.f));
    for (const FVector& Sample : CombatSamples)
    {
        if (Sample.ContainsNaN() || FMath::LineBoxIntersection(Padded, CameraPosition, Sample, Sample - CameraPosition)) return false;
    }
    return true;
}
