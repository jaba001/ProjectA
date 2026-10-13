#include "EncounterMazeBakeLibrary.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Game/Encounter/EncounterDungeonLayout.h"
#include "Game/Encounter/EncounterMazeLayout.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ObjectTools.h"

int32 UEncounterMazeBakeLibrary::BakeEncounterMazeBlueprints(bool bOverwriteExisting, FString& OutReport)
{
    OutReport.Reset();
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        OutReport = TEXT("No editor world.");
        return 0;
    }
    int32 Baked = 0;
    TArray<UPackage*> Packages;
    for (int32 Variant = 0; Variant < EncounterDungeonLayout::VariantCount; ++Variant)
    {
        const FString Name = FString::Printf(TEXT("BP_EncounterMaze_V%d"), Variant);
        const FString PackageName = FString::Printf(TEXT("%s/%s"), EncounterDungeonLayout::BakedMazeFolder, *Name);
        if (UBlueprint* Existing = LoadObject<UBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackageName, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
        {
            if (!bOverwriteExisting)
            {
                OutReport += FString::Printf(TEXT("%s: kept existing\n"), *Name);
                continue;
            }
            // Remove the old asset through the editor so references and redirectors are handled by the engine.
            // 참조와 리디렉터를 엔진이 처리하도록 기존 에셋을 에디터 기능으로 삭제합니다.
            if (ObjectTools::DeleteObjectsUnchecked({Existing}) != 1)
            {
                OutReport += FString::Printf(TEXT("%s: failed to delete existing\n"), *Name);
                continue;
            }
        }
        FActorSpawnParameters Parameters;
        Parameters.ObjectFlags |= RF_Transient;
        AEncounterMazeLayout* Actor = World->SpawnActor<AEncounterMazeLayout>(AEncounterMazeLayout::StaticClass(), FTransform::Identity, Parameters);
        if (!Actor || !Actor->BuildFromLayout(Variant))
        {
            OutReport += FString::Printf(TEXT("%s: build failed\n"), *Name);
            if (Actor) Actor->Destroy();
            continue;
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
            continue;
        }
        Packages.Add(Blueprint->GetPackage());
        OutReport += FString::Printf(TEXT("%s: walls %d, floor %d, mortar %d, pillars %d, torches %d\n"), *Name, Defaults->WallBlocks->GetInstanceCount(), Defaults->Floor->GetInstanceCount(), Defaults->Mortar->GetInstanceCount(), Defaults->Pillars->GetInstanceCount(), Defaults->TorchMounts.Num());
        ++Baked;
    }
    if (!Packages.IsEmpty() && !UEditorLoadingAndSavingUtils::SavePackages(Packages, false))
    {
        OutReport += TEXT("Saving failed\n");
        return 0;
    }
    return Baked;
}
