#include "EncounterMazeBakeLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Game/Encounter/EncounterDungeonLayout.h"
#include "Game/Encounter/EncounterMazeLayout.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ObjectTools.h"
#include "UObject/ObjectRedirector.h"
#include "Misc/PackageName.h"

bool UEncounterMazeBakeLibrary::FixupRetiredMazeRedirectors()
{
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.SearchAllAssets(true);
    TArray<UObject*> Redirectors;
    TArray<FString> Packages;
    for (int32 Variant = 0; Variant < EncounterDungeonLayout::SharedLayoutVariant; ++Variant)
    {
        const FString PackageName = FString::Printf(TEXT("%s/BP_EncounterMaze_V%d"), EncounterDungeonLayout::BakedMazeFolder, Variant);
        TArray<FAssetData> Assets;
        Registry.GetAssetsByPackageName(FName(*PackageName), Assets, true, false);
        TArray<FName> Referencers;
        Registry.GetReferencers(FName(*PackageName), Referencers, UE::AssetRegistry::EDependencyCategory::All);
        Referencers.Remove(FName(*PackageName));
        if (!Referencers.IsEmpty()) return false;
        for (const FAssetData& Asset : Assets)
        {
            if (Asset.AssetClassPath != UObjectRedirector::StaticClass()->GetClassPathName()) return false;
            UObjectRedirector* Redirector = Cast<UObjectRedirector>(Asset.GetAsset());
            if (!Redirector || !Redirector->DestinationObject) return false;
            const FString SharedPackage = FString::Printf(TEXT("%s/%s"), EncounterDungeonLayout::BakedMazeFolder, EncounterDungeonLayout::BakedMazeName);
            if (Redirector->DestinationObject->GetOutermost()->GetName() != SharedPackage) return false;
            Redirectors.Add(Redirector);
        }
        Packages.Add(PackageName);
    }
    // Registry checks above guarantee these exact retired packages contain only unused redirects to V7.
    // 위 레지스트리 검사로 지정된 폐기 패키지에 참조 없는 V7 Redirector만 있음을 확인합니다.
    if (!Redirectors.IsEmpty() && ObjectTools::DeleteObjectsUnchecked(Redirectors) != Redirectors.Num()) return false;
    for (const FString& PackageName : Packages)
    {
        if (FPackageName::DoesPackageExist(PackageName)) return false;
    }
    return true;
}

int32 UEncounterMazeBakeLibrary::BakeEncounterMazeBlueprints(bool bOverwriteExisting, FString& OutReport)
{
    OutReport.Reset();
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        OutReport = TEXT("No editor world.");
        return 0;
    }
    const int32 Variant = EncounterDungeonLayout::SharedLayoutVariant;
    const FString Name = EncounterDungeonLayout::BakedMazeName;
    const FString PackageName = FString::Printf(TEXT("%s/%s"), EncounterDungeonLayout::BakedMazeFolder, *Name);
    if (UBlueprint* Existing = LoadObject<UBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackageName, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
    {
        if (!bOverwriteExisting)
        {
            OutReport += FString::Printf(TEXT("%s: kept existing\n"), *Name);
            return 0;
        }
        // Remove the old asset through the editor so references and redirectors are handled by the engine.
        // 참조와 리디렉터를 엔진이 처리하도록 기존 에셋을 에디터 기능으로 삭제합니다.
        if (ObjectTools::DeleteObjectsUnchecked({Existing}) != 1)
        {
            OutReport += FString::Printf(TEXT("%s: failed to delete existing\n"), *Name);
            return 0;
        }
    }
    FActorSpawnParameters Parameters;
    Parameters.ObjectFlags |= RF_Transient;
    AEncounterMazeLayout* Actor = World->SpawnActor<AEncounterMazeLayout>(AEncounterMazeLayout::StaticClass(), FTransform::Identity, Parameters);
    if (!Actor || !Actor->BuildFromLayout(Variant))
    {
        OutReport += FString::Printf(TEXT("%s: build failed\n"), *Name);
        if (Actor) Actor->Destroy();
        return 0;
    }
    FKismetEditorUtilities::FCreateBlueprintFromActorParams BlueprintParams;
    BlueprintParams.bReplaceActor = false;
    BlueprintParams.bKeepMobility = true;
    BlueprintParams.bOpenBlueprint = false;
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor(PackageName, Actor, BlueprintParams);
    Actor->Destroy();
    const AEncounterMazeLayout* Defaults = Blueprint && Blueprint->GeneratedClass ? Cast<AEncounterMazeLayout>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!Defaults || Defaults->LayoutVariant != Variant)
    {
        OutReport += FString::Printf(TEXT("%s: blueprint creation failed\n"), *Name);
        return 0;
    }
    if (!UEditorLoadingAndSavingUtils::SavePackages({Blueprint->GetPackage()}, false))
    {
        OutReport += TEXT("Saving failed\n");
        return 0;
    }
    OutReport += FString::Printf(TEXT("%s: walls %d, floor %d, mortar %d, pillars %d, torches %d\n"), *Name,
        Defaults->WallBlocks->GetInstanceCount(), Defaults->Floor->GetInstanceCount(), Defaults->Mortar->GetInstanceCount(), Defaults->Pillars->GetInstanceCount(), Defaults->TorchMounts.Num());
    return 1;
}
