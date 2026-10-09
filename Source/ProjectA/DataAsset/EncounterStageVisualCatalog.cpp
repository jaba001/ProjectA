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

    void Merchandise(FEncounterStageVisualProfile& Value, const TCHAR* Type, const TCHAR* Rarity = nullptr)
    {
        const TCHAR* Colors[] = {TEXT("White"), TEXT("Green"), TEXT("Blue"), TEXT("Purple"), TEXT("Orange")};
        for (const TCHAR* Color : Colors)
        {
            if (Rarity && FCString::Strcmp(Color, Rarity) != 0) continue;
            FGameplayTagContainer Sample(FGameplayTag::RequestGameplayTag(FName(Type)));
            Sample.AddTag(FGameplayTag::RequestGameplayTag(FName(FString(TEXT("Item.Rarity.")) + Color)));
            Value.MerchandiseSamples.Add(MoveTemp(Sample));
        }
    }

    FEncounterStageProp HeldProp(const TCHAR* Package, FName Bone, FVector MaxSize, FRotator Rotation, FVector Offset = FVector::ZeroVector)
    {
        FEncounterStageProp Value = Prop(Package, Offset, MaxSize);
        Value.AttachBone = Bone;
        Value.bCenterAnchor = true;
        Value.Rotation = Rotation;
        return Value;
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

    const FGameplayTagQuery TagShop = FGameplayTagQuery::MakeQuery_MatchTag(FRunEncounterOffer::GetTagItemShopTag());
    const TCHAR* WarriorIdle = TEXT("/Game/Fantasy_Pack/Animations/1With_Weapon/Anim_Warrior_Idle");
    FEncounterStageVisualProfile Swordsmith = Profile(TEXT("Swordsmith"), TagShop, 20, TEXT("/Game/Fantasy_Pack/Characters/Warrior/Mesh/SK_Warrior"), WarriorIdle);
    Merchandise(Swordsmith, TEXT("Item.Weapon.Sword"));
    Swordsmith.LightColor = FLinearColor(1.f, 0.68f, 0.38f);
    Swordsmith.Props = Profiles[1].Props;
    Swordsmith.Props.Add(HeldProp(TEXT("/Game/PurePoly/FreeLowPolyFantasyRPGWeapons/Meshes/SM_PP_Theme_11_Sword_One-Handed_003"), TEXT("hand_r"), FVector(105.f), FRotator(0.f, 0.f, 90.f), FVector(0.f, 0.f, 32.f)));
    Profiles.Add(MoveTemp(Swordsmith));

    FEncounterStageVisualProfile Ranger = Profile(TEXT("Ranger"), TagShop, 20, TEXT("/Game/Primitive_Characters_Pack/Mesh/Primitive_02/Mesh_UE5/Full/SKM_Primitive_02_Full"), PrimitiveIdle);
    Merchandise(Ranger, TEXT("Item.Weapon.Bow"));
    Ranger.LightColor = FLinearColor(0.64f, 0.92f, 0.66f);
    Ranger.Props = {
        HeldProp(TEXT("/Game/CR/LQ/Weapon/Weapon_001_Ravi/Mesh/SM_Bow_001_01"), TEXT("hand_l"), FVector(130.f), FRotator(0.f, 90.f, 0.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_table_dungeon_02"), FVector(-100.f, -255.f, 12.f), FVector(90.f, 90.f, 65.f)),
        Prop(TEXT("/Game/CR/LQ/Weapon/Weapon_001_Ravi/Mesh/SM_Bow_001_01"), FVector(150.f, -280.f, 40.f), FVector(145.f), 90.f),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/container/SM_PROP_crate_dungeon_01"), FVector(150.f, -280.f, 12.f), FVector(90.f, 85.f, 65.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/container/SM_PROP_barrel_dungeon_01"), FVector(115.f, 25.f, 12.f), FVector(65.f, 65.f, 80.f)),
        Prop(TEXT("/Game/Orasot_Bundle/StylizedForestLandscape/Meshes/SM_Fern_2"), FVector(-130.f, 50.f, 12.f), FVector(95.f, 95.f, 70.f))
    };
    Profiles.Add(MoveTemp(Ranger));

    FEncounterStageVisualProfile Rogue = Profile(TEXT("Rogue"), TagShop, 20, TEXT("/Game/Assassin/Mesh/SKM_Assassin_Skin3"), TEXT("/Game/Assassin/Animation/Anim_Assassin_idle1"));
    Merchandise(Rogue, TEXT("Item.Weapon.Dagger"));
    Rogue.LightColor = FLinearColor(0.7f, 0.75f, 1.f);
    Rogue.Props = {
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_table_dungeon_02"), FVector(-110.f, -240.f, 12.f), FVector(90.f, 90.f, 65.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/small_deco/SM_PROP_book_dungeon_07"), FVector(-110.f, -240.f, 77.f), FVector(38.f, 30.f, 10.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/container/SM_PROP_crate_dungeon_01"), FVector(115.f, -280.f, 12.f), FVector(90.f, 90.f, 70.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/container/SM_PROP_crate_dungeon_01"), FVector(110.f, -275.f, 55.8f), FVector(60.f, 60.f, 48.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/light/SM_PROP_candle_dungeon_13"), FVector(150.f, 25.f, 12.f), FVector(40.f, 40.f, 150.f))
    };
    Profiles.Add(MoveTemp(Rogue));

    FEncounterStageVisualProfile Arcanist = Profile(TEXT("Arcanist"), TagShop, 20, TEXT("/Game/Primitive_Characters_Pack/Mesh/Primitive_04/Mesh_UE5/Full/SKM_Primitive_04_Full_02"), PrimitiveIdle);
    Merchandise(Arcanist, TEXT("Item.Weapon.StaffWand"));
    Arcanist.LightColor = FLinearColor(0.5f, 0.72f, 1.f);
    Arcanist.Props = {
        HeldProp(TEXT("/Game/MageStaff_FreeWeapons/SM_Staff_01"), TEXT("hand_r"), FVector(155.f), FRotator(0.f, 0.f, 90.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_bookshelf_dungeon_02"), FVector(175.f, -285.f, 12.f), FVector(130.f, 65.f, 200.f), 90.f),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_readingstand_dungeon"), FVector(-105.f, -245.f, 12.f), FVector(60.f, 60.f, 78.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/small_deco/SM_PROP_book_dungeon_06"), FVector(-105.f, -245.f, 82.2f), FVector(38.f, 30.f, 10.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/light/SM_PROP_candle_dungeon_13"), FVector(150.f, 40.f, 12.f), FVector(40.f, 40.f, 155.f))
    };
    Profiles.Add(MoveTemp(Arcanist));

    FEncounterStageVisualProfile Warden = Profile(TEXT("Warden"), TagShop, 20, TEXT("/Game/Fantasy_Pack/Characters/RPG_Knight/Mesh/SK_Knight_Full"), WarriorIdle);
    Merchandise(Warden, TEXT("Item.Weapon.Shield"));
    Warden.LightColor = FLinearColor(0.72f, 0.85f, 1.f);
    Warden.Props = {
        HeldProp(TEXT("/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Shield"), TEXT("hand_l"), FVector(72.f), FRotator(0.f, 90.f, 0.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/furniture/SM_PROP_table_dungeon_02"), FVector(-115.f, -280.f, 12.f), FVector(80.f, 80.f, 65.f)),
        Prop(TEXT("/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Shield"), FVector(150.f, -275.f, 48.f), FVector(90.f), 90.f),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/container/SM_PROP_crate_dungeon_01"), FVector(150.f, -275.f, 12.f), FVector(90.f, 90.f, 60.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/light/SM_PROP_brazier_dungeon_02"), FVector(130.f, 45.f, 12.f), FVector(55.f, 55.f, 75.f))
    };
    Profiles.Add(MoveTemp(Warden));

    // Rarity stalls use different merchant silhouettes, displays and lighting while retaining the saved merchandise filter.
    // 등급점은 저장된 상품 필터를 유지하면서 상인의 실루엣과 진열, 조명으로 구분합니다.
    const TCHAR* Rarities[] = {TEXT("White"), TEXT("Green"), TEXT("Blue"), TEXT("Purple"), TEXT("Orange")};
    const int32 MerchantProfiles[] = {0, 1, 2, 5, 6};
    const FLinearColor RarityColors[] = {FLinearColor(0.9f, 0.91f, 0.86f), FLinearColor(0.5f, 1.f, 0.64f), FLinearColor(0.4f, 0.65f, 1.f), FLinearColor(0.8f, 0.5f, 1.f), FLinearColor(1.f, 0.68f, 0.26f)};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Rarities); ++Index)
    {
        FEncounterStageVisualProfile Merchant = Profiles[MerchantProfiles[Index]];
        Merchant.ProfileId = FName(FString(TEXT("Rarity")) + Rarities[Index]);
        Merchant.StageQuery = FGameplayTagQuery::MakeQuery_MatchTag(FRunEncounterOffer::GetRarityItemShopTag());
        Merchant.Priority = 20;
        Merchant.MerchandiseSamples.Reset();
        Merchandise(Merchant, TEXT("Item.Weapon"), Rarities[Index]);
        Merchant.LightColor = RarityColors[Index];
        if (Index == 2) Merchant.CharacterMesh = TSoftObjectPtr<USkeletalMesh>(ObjectPath(TEXT("/Game/Fantasy_Pack/Characters/Viking_Ulf/Mesh/SK_Ulf_Full")));
        Profiles.Add(MoveTemp(Merchant));
    }

    FEncounterStageVisualProfile Spring;
    Spring.ProfileId = TEXT("HealingSpring");
    Spring.StageQuery = FGameplayTagQuery::MakeQuery_MatchTag(FRunEncounterOffer::GetRecoveryTag());
    Spring.Priority = 10;
    Spring.bEnvironmentOnly = true;
    Spring.PresentationFocus = FVector(35.f, -130.f, 85.f);
    Spring.PresentationFocusRadius = 55.f;
    Spring.LightColor = FLinearColor(0.28f, 0.87f, 1.f);
    Spring.LightLocation = FVector(-25.f, -130.f, 145.f);
    Spring.LightIntensity = 1650.f;
    Spring.Props = {
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/construct/SM_PROP_well_dungeon_02"), FVector(35.f, -130.f, 12.f), FVector(215.f, 215.f, 90.f)),
        Prop(TEXT("/Game/Orasot_Bundle/Stylized_Landscape_5_Bioms/Global/StaticMeshes/SM_Water_Plane"), FVector(35.f, -130.f, 71.f), FVector(126.f, 126.f, 1.f)),
        Prop(TEXT("/Game/InfinityBladeIceLands/Environments/Ice/EXO_RockyRuins/StaticMesh/SM_Statue01"), FVector(185.f, -130.f, 12.f), FVector(110.f, 105.f, 225.f), 180.f),
        Prop(TEXT("/Game/Orasot_Bundle/StylizedForestLandscape/Meshes/SM_Fern_2"), FVector(-55.f, -265.f, 12.f), FVector(95.f, 95.f, 65.f)),
        Prop(TEXT("/Game/Orasot_Bundle/StylizedForestLandscape/Meshes/SM_Fern_2"), FVector(80.f, 15.f, 12.f), FVector(105.f, 105.f, 75.f), 135.f),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/light/SM_PROP_candle_dungeon_13"), FVector(145.f, -305.f, 12.f), FVector(30.f, 30.f, 115.f)),
        Prop(TEXT("/Game/Fantastic_Dungeon_Pack/meshes/props/light/SM_PROP_candle_dungeon_13"), FVector(145.f, 45.f, 12.f), FVector(30.f, 30.f, 115.f))
    };
    Profiles.Add(MoveTemp(Spring));

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
    return Resolve(StageTags, FGameplayTagQuery());
}

const FEncounterStageVisualProfile* UEncounterStageVisualCatalog::Resolve(const FRunEncounterOffer& Offer) const
{
    FGameplayTagContainer StageTags;
    const FGameplayTag ResolvedTag = Offer.GetResolvedTag();
    if (!ResolvedTag.IsValid()) return nullptr;
    StageTags.AddTag(ResolvedTag);
    if (Offer.SelectionGroupTag.IsValid()) StageTags.AddTag(Offer.SelectionGroupTag);
    return Resolve(StageTags, Offer.ItemQuery);
}

const FEncounterStageVisualProfile* UEncounterStageVisualCatalog::Resolve(const FGameplayTagContainer& StageTags, const FGameplayTagQuery& MerchandiseQuery) const
{
    const FEncounterStageVisualProfile* Selected = nullptr;
    for (const FEncounterStageVisualProfile& Candidate : Profiles)
    {
        if (Candidate.ProfileId.IsNone() || Candidate.StageQuery.IsEmpty() || !Candidate.StageQuery.Matches(StageTags)) continue;
        if (!Candidate.MerchandiseSamples.IsEmpty() && (MerchandiseQuery.IsEmpty() || !Candidate.MerchandiseSamples.ContainsByPredicate([&MerchandiseQuery](const FGameplayTagContainer& Sample) { return MerchandiseQuery.Matches(Sample); }))) continue;
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
        for (const FEncounterStageProp& Prop : Profile.Props)
        {
            AddAsset(Prop.Mesh.ToSoftObjectPath());
            AddAsset(Prop.MaterialOverride.ToSoftObjectPath());
        }
    }
}
