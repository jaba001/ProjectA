#include "DataAsset/EncounterStageVisualCatalog.h"

#include "Game/Run/RunEncounterTypes.h"
#include "Misc/PackageName.h"

namespace EncounterStageVisualCatalogDefaults
{
    FSoftObjectPath ObjectPath(const TCHAR* Package)
    {
        return FSoftObjectPath(FString::Printf(TEXT("%s.%s"), Package, *FPackageName::GetShortName(Package)));
    }

    FEncounterStageProp Prop(const TCHAR* Package, FVector Location, FVector MaxSize, float Yaw = 0.f)
    {
        FEncounterStageProp Value;
        Value.Mesh = TSoftObjectPtr<UStaticMesh>(ObjectPath(Package));
        Value.Location = Location;
        Value.MaxSize = MaxSize;
        Value.Rotation = FRotator(0.f, Yaw, 0.f);
        return Value;
    }

    FEncounterStageVisualProfile Profile(FName Id, const FGameplayTagQuery& Query, int32 Priority, const TCHAR* Character, const TCHAR* Idle)
    {
        FEncounterStageVisualProfile Value;
        Value.ProfileId = Id;
        Value.StageQuery = Query;
        Value.Priority = Priority;
        Value.CharacterMesh = TSoftObjectPtr<USkeletalMesh>(ObjectPath(Character));
        Value.IdleAnimation = TSoftObjectPtr<UAnimSequence>(ObjectPath(Idle));
        return Value;
    }

    FGameplayTagQuery MarketQuery()
    {
        FGameplayTagQueryExpression Root;
        Root.AllExprMatch();
        FGameplayTagQueryExpression Shops;
        Shops.AllTagsMatch().AddTag(FRunEncounterOffer::GetItemShopTag().RequestDirectParent());
        Root.AddExpr(Shops);
        FGameplayTagQueryExpression Specialized;
        Specialized.NoTagsMatch().AddTag(FRunEncounterOffer::GetTagItemShopTag()).AddTag(FRunEncounterOffer::GetConsumableShopTag());
        Root.AddExpr(Specialized);
        return FGameplayTagQuery::BuildQuery(Root);
    }
}

UEncounterStageVisualCatalog::UEncounterStageVisualCatalog()
{
    using namespace EncounterStageVisualCatalogDefaults;
    const TCHAR* PrimitiveIdle = TEXT("/Game/Primitive_Characters_Pack/Demoscene_UE5/Animations/MM_Idle");

    // Reference complete original characters and their matching-skeleton idle without tinting source materials.
    // 원본 재질을 착색하지 않고 완성형 원본 캐릭터와 같은 스켈레톤의 대기 애니메이션을 참조합니다.
    FEncounterStageVisualProfile Market = Profile(TEXT("Market"), MarketQuery(), 0, TEXT("/Game/Primitive_Characters_Pack/Mesh/Primitive_01/Mesh_UE5/Full/SKM_Primitive_Charater_01_01"), PrimitiveIdle);
    Market.Props = {
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_table_dungeon_01"), FVector(-100.f, -145.f, 12.f), FVector(210.f, 118.f, 78.f), 90.f),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/small_deco/SM_PROP_book_dungeon_07"), FVector(-100.f, -180.f, 88.5f), FVector(38.f, 30.f, 10.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/small_deco/SM_PROP_bottle_dungeon_04"), FVector(-110.f, -80.f, 88.5f), FVector(20.f, 20.f, 30.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/container/SM_PROP_crate_dungeon_01"), FVector(90.f, 15.f, 12.f), FVector(78.f, 78.f, 68.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_bookshelf_dungeon_02"), FVector(175.f, -295.f, 12.f), FVector(132.f, 70.f, 200.f), 90.f),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/container/SM_PROP_barrel_dungeon_01"), FVector(-120.f, -325.f, 12.f), FVector(55.f, 55.f, 72.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_stool_01_dungeon"), FVector(140.f, 25.f, 12.f), FVector(45.f, 45.f, 45.f))
    };
    Profiles.Add(MoveTemp(Market));

    FEncounterStageVisualProfile Forge = Profile(TEXT("Forge"), FGameplayTagQuery::MakeQuery_MatchTag(FRunEncounterOffer::GetTagItemShopTag()), 10, TEXT("/Game/Fantasy_Pack/Characters/Dwarf/Mesh/SK_Dwarf"), TEXT("/Game/Fantasy_Pack/Animations/2Without_Weapon/Anim_Idle_Without_Weapon"));
    Forge.CharacterHeight = 155.f;
    Forge.LightColor = FLinearColor(1.f, 0.58f, 0.28f);
    Forge.Props = {
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_table_dungeon_01"), FVector(-100.f, -145.f, 12.f), FVector(185.f, 108.f, 68.f), 90.f),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/light/SM_PROP_fireplace_dungeon"), FVector(120.f, -290.f, 12.f), FVector(150.f, 98.f, 185.f), 90.f),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/light/SM_PROP_brazier_dungeon_02"), FVector(-140.f, 30.f, 12.f), FVector(55.f, 55.f, 70.f)),
        Prop(TEXT("/Game/Dungeon_Modular_V1/Meshes/SM_Barrel_01"), FVector(150.f, -35.f, 12.f), FVector(70.f, 70.f, 75.f)),
        Prop(TEXT("/Game/PurePoly/FreeLowPolyFantasyRPGWeapons/Meshes/SM_PP_Theme_11_Sword_One-Handed_003"), FVector(150.f, -35.f, 87.f), FVector(95.f, 75.f, 135.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/container/SM_PROP_crate_dungeon_01"), FVector(-135.f, -325.f, 12.f), FVector(65.f, 65.f, 60.f))
    };
    Profiles.Add(MoveTemp(Forge));

    // A bench, campfire and remedies form a rest area without inventing an unavailable bed asset.
    // 설치되지 않은 침대 에셋을 가정하지 않고 벤치와 모닥불, 회복 물품으로 휴식 공간을 구성합니다.
    FEncounterStageVisualProfile Camp = Profile(TEXT("Camp"), FGameplayTagQuery::MakeQuery_MatchTag(FRunEncounterOffer::GetRecoveryTag()), 10, TEXT("/Game/Primitive_Characters_Pack/Mesh/Primitive_02/Mesh_UE5/Full/SKM_Primitive_02_Full"), PrimitiveIdle);
    Camp.LightColor = FLinearColor(0.65f, 1.f, 0.74f);
    Camp.Props = {
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_chair_03_dungeon"), FVector(80.f, -260.f, 12.f), FVector(160.f, 58.f, 56.f), 90.f),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/light/SM_PROP_campfire_dungeon_01"), FVector(-150.f, 5.f, 12.f), FVector(85.f, 85.f, 30.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_table_dungeon_02"), FVector(-120.f, -215.f, 12.f), FVector(80.f, 80.f, 68.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/small_deco/SM_PROP_bottle_dungeon_01"), FVector(-120.f, -238.f, 80.f), FVector(18.f, 18.f, 27.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/small_deco/SM_PROP_book_dungeon_07"), FVector(-120.f, -197.f, 80.f), FVector(28.f, 22.f, 8.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/container/SM_PROP_crate_dungeon_01"), FVector(165.f, 30.f, 12.f), FVector(65.f, 65.f, 65.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_stool_01_dungeon"), FVector(-90.f, -330.f, 12.f), FVector(40.f, 40.f, 40.f))
    };
    Profiles.Add(MoveTemp(Camp));

    FEncounterStageVisualProfile Alchemy = Profile(TEXT("Alchemy"), FGameplayTagQuery::MakeQuery_MatchTag(FRunEncounterOffer::GetConsumableShopTag()), 10, TEXT("/Game/Primitive_Characters_Pack/Mesh/Primitive_04/Mesh_UE5/Full/SKM_Primitive_04_Full_02"), PrimitiveIdle);
    Alchemy.LightColor = FLinearColor(0.64f, 0.8f, 1.f);
    Alchemy.Props = {
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_bookshelf_dungeon_02"), FVector(170.f, -255.f, 12.f), FVector(140.f, 65.f, 205.f), 90.f),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_table_dungeon_01"), FVector(-115.f, -145.f, 12.f), FVector(208.f, 114.f, 76.f), 90.f),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/small_deco/SM_PROP_book_dungeon_06"), FVector(-110.f, -212.f, 87.7f), FVector(40.f, 32.f, 10.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/small_deco/SM_PROP_bottle_dungeon_01"), FVector(-110.f, -155.f, 87.7f), FVector(20.f, 20.f, 30.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/small_deco/SM_PROP_bottle_dungeon_04"), FVector(-110.f, -108.f, 87.7f), FVector(20.f, 20.f, 30.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/small_deco/SM_PROP_bottle_dungeon_07"), FVector(-110.f, -62.f, 87.7f), FVector(20.f, 20.f, 30.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/container/SM_PROP_crate_dungeon_01"), FVector(100.f, 20.f, 12.f), FVector(65.f, 65.f, 65.f)),
        Prop(TEXT("/Game/Dungeon_Modular_V1/Meshes/SM_Cauldron_01"), FVector(-150.f, -330.f, 12.f), FVector(65.f, 65.f, 70.f))
    };
    Profiles.Add(MoveTemp(Alchemy));

    FEncounterStageVisualProfile Shrine = Profile(TEXT("Shrine"), FGameplayTagQuery::MakeQuery_MatchTag(FRunEncounterOffer::GetRevivalTag()), 10, TEXT("/Game/Primitive_Characters_Pack/Mesh/Primitive_04/Mesh_UE5/Full/SKM_Primitive_04_Full_01"), PrimitiveIdle);
    Shrine.LightColor = FLinearColor(0.85f, 0.82f, 1.f);
    Shrine.Props = {
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_altar_dungeon_02"), FVector(-95.f, -125.f, 12.f), FVector(125.f, 200.f, 76.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/small_deco/SM_PROP_book_dungeon_06"), FVector(-95.f, -125.f, 83.6f), FVector(48.f, 40.f, 12.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/light/SM_PROP_candle_dungeon_13"), FVector(120.f, -330.f, 12.f), FVector(40.f, 40.f, 165.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/light/SM_PROP_candle_dungeon_13"), FVector(130.f, 60.f, 12.f), FVector(40.f, 40.f, 165.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_readingstand_dungeon"), FVector(175.f, -250.f, 12.f), FVector(55.f, 55.f, 75.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/light/SM_PROP_candle_dungeon_07"), FVector(-105.f, -195.f, 83.6f), FVector(16.f, 16.f, 26.f))
    };
    Profiles.Add(MoveTemp(Shrine));
}

const FEncounterStageVisualProfile* UEncounterStageVisualCatalog::Resolve(const FGameplayTagContainer& StageTags) const
{
    const FEncounterStageVisualProfile* Selected = nullptr;
    for (const FEncounterStageVisualProfile& Candidate : Profiles)
    {
        if (Candidate.ProfileId.IsNone() || Candidate.StageQuery.IsEmpty() || !Candidate.StageQuery.Matches(StageTags)) continue;
        // Tie-breaking depends on the stable profile name rather than array order or asset loading order.
        // 동순위는 배열 순서나 에셋 로드 순서 대신 안정적인 프로필 이름으로 결정합니다.
        if (!Selected || Candidate.Priority > Selected->Priority || (Candidate.Priority == Selected->Priority && Candidate.ProfileId.LexicalLess(Selected->ProfileId))) Selected = &Candidate;
    }
    return Selected;
}

void UEncounterStageVisualCatalog::GetReferencedAssets(TArray<FSoftObjectPath>& OutAssets) const
{
    const auto AddAsset = [&OutAssets](const FSoftObjectPath& Path)
    {
        if (Path.IsValid()) OutAssets.AddUnique(Path);
    };
    for (const FEncounterStageVisualProfile& Profile : Profiles)
    {
        AddAsset(Profile.CharacterMesh.ToSoftObjectPath());
        AddAsset(Profile.IdleAnimation.ToSoftObjectPath());
        for (const TSoftObjectPtr<USkeletalMesh>& Part : Profile.CharacterParts) AddAsset(Part.ToSoftObjectPath());
        for (const FEncounterStageProp& Prop : Profile.Props) AddAsset(Prop.Mesh.ToSoftObjectPath());
    }
}
