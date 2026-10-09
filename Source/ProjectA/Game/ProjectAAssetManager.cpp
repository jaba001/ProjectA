#include "Game/ProjectAAssetManager.h"

#if WITH_EDITOR
#include "DataAsset/EncounterStageVisualCatalog.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "DataAsset/TargetRunDefinitionDataAsset.h"
#include "Game/Run/RunLevelDesign.h"
#include "Game/Run/RunRecoveryTypes.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectAAssetManager, Log, All);

void UProjectAAssetManager::ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook)
{
    Super::ModifyCook(TargetPlatforms, PackagesToCook, PackagesToNeverCook);
    // The consumable uses a runtime path; cook only its authored package and normal dependencies.
    // 소모품은 런타임 경로를 사용하므로 작성된 패키지 하나와 일반 의존성만 쿠킹에 포함합니다.
    const FName RecoveryPackage = RunRecoveryRules::GetHealingSkillPath().GetLongPackageFName();
    if (PackagesToNeverCook.Contains(RecoveryPackage) || !FPackageName::DoesPackageExist(RecoveryPackage.ToString()))
    {
        UE_LOG(LogProjectAAssetManager, Error, TEXT("Recovery skill package is missing or excluded from cooking: %s"), *RecoveryPackage.ToString());
        return;
    }
    PackagesToCook.AddUnique(RecoveryPackage);
    // Include only the selected library scenery and its dependencies, never an entire source pack.
    // 원본 팩 전체가 아니라 선택한 라이브러리 무대 에셋과 해당 의존성만 포함합니다.
    TArray<FSoftObjectPath> StageAssets;
    GetDefault<UEncounterStageVisualCatalog>()->GetReferencedAssets(StageAssets);
    for (const FSoftObjectPath& Asset : StageAssets)
    {
        const FName Package = Asset.GetLongPackageFName();
        if (PackagesToNeverCook.Contains(Package) || !FPackageName::DoesPackageExist(Package.ToString()))
        {
            UE_LOG(LogProjectAAssetManager, Error, TEXT("Encounter stage package is missing or excluded from cooking: %s"), *Package.ToString());
            continue;
        }
        PackagesToCook.AddUnique(Package);
    }
    TArray<FRunItemDefinition> Catalog;
    FText Error;
    if (!RunItemShopCatalog::Load(Catalog, Error))
    {
        UE_LOG(LogProjectAAssetManager, Error, TEXT("Cannot collect item catalog packages for cooking: %s"), *Error.ToString());
        return;
    }

    // CSV paths are runtime references; include every listed item without cooking its entire source pack.
    // CSV 경로는 런타임 참조이므로 원본 팩 전체 대신 목록에 기록된 아이템을 모두 포함합니다.
    for (const FRunItemDefinition& Item : Catalog)
    {
        const FName Package = Item.Asset.GetLongPackageFName();
        if (PackagesToNeverCook.Contains(Package) || !FPackageName::DoesPackageExist(Package.ToString()))
        {
            UE_LOG(LogProjectAAssetManager, Error, TEXT("Item catalog package is missing or excluded from cooking: %s"), *Package.ToString());
            continue;
        }
        PackagesToCook.AddUnique(Package);
    }

    // Cook every authored monster candidate, including candidates absent from this seed's roster.
    // 이 시드의 편성에 뽑히지 않은 후보까지 제작된 몬스터 카탈로그 전체를 쿠킹에 포함합니다.
    FRunTargetState Monsters;
    Monsters.SchemaVersion = 1;
    Monsters.Groups = GetDefault<UTargetRunDefinitionDataAsset>()->Groups;
    if (!RunLevelDesign::Load(Monsters, 4, Error))
    {
        UE_LOG(LogProjectAAssetManager, Error, TEXT("Cannot collect monster catalog packages for cooking: %s"), *Error.ToString());
        return;
    }
    for (const FRunMonsterDefinition& Monster : Monsters.LevelDesign.Catalog)
    {
        const FName Packages[] = {Monster.UnitClass.GetLongPackageFName(), Monster.Skill.GetLongPackageFName()};
        for (const FName Package : Packages)
        {
            if (PackagesToNeverCook.Contains(Package) || !FPackageName::DoesPackageExist(Package.ToString()))
            {
                UE_LOG(LogProjectAAssetManager, Error, TEXT("Monster catalog package is missing or excluded from cooking: %s"), *Package.ToString());
                continue;
            }
            PackagesToCook.AddUnique(Package);
        }
    }
}
#endif
