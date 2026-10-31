#include "AssetFolderOrganizationLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "UObject/ObjectRedirector.h"

bool UAssetFolderOrganizationLibrary::RenameAuditedFolderAssets(const TArray<UObject*>& Assets)
{
    if (!IsInGameThread() || Assets.Num() != 213 || FApp::IsUnattended()) return false;
    TArray<FAssetRenameData> Renames;
    TSet<FString> Names;
    for (UObject* Asset : Assets)
    {
        if (!IsValid(Asset)) return false;
        const FString Package = Asset->GetOutermost()->GetName();
        if (!Package.StartsWith(TEXT("/Game/User_JeHoon/")) || Package.StartsWith(TEXT("/Game/User_JeHoon/Assets/"))) return false;
        const FString Destination = TEXT("/Game/User_JeHoon/Assets/") + FPackageName::GetLongPackagePath(Package).Mid(18);
        Renames.Emplace(Asset, Destination, Asset->GetName());
        Names.Add(Asset->GetName());
    }
    auto Dialog = FCoreDelegates::ModalMessageDialog;
    Dialog.BindLambda([&Names](EAppMsgCategory Category, EAppMsgType::Type Type, const FText& Message, const FText& Title)
    {
        FString Source = Message.BuildSourceString();
        const FString Prefix = TEXT("Source code, config INI, and text files may need Find/Replace for:\n\n");
        const FString Suffix = TEXT("\n\nOtherwise assets can be missing from cooked builds. Continue with rename?");
        if (Type != EAppMsgType::OkCancel || !Source.RemoveFromStart(Prefix) || !Source.RemoveFromEnd(Suffix)) return EAppReturnType::Cancel;
        TArray<FString> ReferencedNames;
        Source.ParseIntoArrayLines(ReferencedNames, true);
        for (const FString& Name : ReferencedNames) if (!Names.Contains(Name)) return EAppReturnType::Cancel;
        UE_LOG(LogTemp, Display, TEXT("[AssetFolders] Accepted the audited native-reference notice; source/config path migration is required / 감사한 코드 참조 안내만 처리하며 소스·설정 경로를 함께 갱신합니다"));
        return EAppReturnType::Ok;
    });
    // Scope the engine's editor dialog hook to this exact audited operation; every other prompt cancels.
    // 감사한 이 작업에만 엔진 에디터 대화 훅을 한정하며 다른 모든 안내는 취소합니다.
    TGuardValue<decltype(FCoreDelegates::ModalMessageDialog)> DialogGuard(FCoreDelegates::ModalMessageDialog, Dialog);
    TGuardValue<bool> CommandletGuard(PRIVATE_GIsRunningCommandlet, false);
    TGuardValue<bool> ScriptGuard(GIsRunningUnattendedScript, false);
    return FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().RenameAssets(Renames);
}

bool UAssetFolderOrganizationLibrary::ConsolidateRetainedFolderAsset(UObject* OldAsset, UObject* NewAsset)
{
    if (!GEditor || !IsValid(OldAsset) || !IsValid(NewAsset) || OldAsset->GetClass() != NewAsset->GetClass()) return false;
    const FString OldPath = OldAsset->GetPathName();
    if (!OldPath.StartsWith(TEXT("/Game/User_JeHoon/")) || NewAsset->GetPathName() != TEXT("/Game/User_JeHoon/Assets/") + OldPath.Mid(18)) return false;
    // Transfer an existing startup root to the moved asset so the original package can become a redirector.
    // 시작 시 유지되던 루트 소유권을 이동본으로 넘겨 원래 패키지를 Redirector로 정리할 수 있게 합니다.
    const bool bRooted = OldAsset->IsRooted();
    if (bRooted)
    {
        NewAsset->AddToRoot();
        OldAsset->RemoveFromRoot();
    }
    const bool bResult = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()->ConsolidateAssets(NewAsset, {OldAsset});
    if (!bResult && bRooted && IsValid(OldAsset)) OldAsset->AddToRoot();
    return bResult;
}

bool UAssetFolderOrganizationLibrary::FixupFolderRedirectors(const TArray<FName>& PackageNames)
{
    if (!IsInGameThread() || PackageNames.IsEmpty()) return false;
    const TSet<FString> ProtectedFolders = {TEXT("Assets"), TEXT("Blueprint"), TEXT("LEVEL"), TEXT("Localization"), TEXT("UI"), TEXT("Validation")};
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<UObjectRedirector*> Redirectors;
    for (FName PackageName : PackageNames)
    {
        const FString Package = PackageName.ToString();
        FString Folder;
        FString Relative;
        const TSet<FString> ValidationWidgets = {TEXT("WBP_CharacterCreationWidget"), TEXT("WBP_MainMenuRootWidget"), TEXT("WBP_MainMenuScreenWidget")};
        const bool bMenuValidation = Package == TEXT("/Game/User_JeHoon/Validation/T12/") + FPackageName::GetShortName(Package) && ValidationWidgets.Contains(FPackageName::GetShortName(Package));
        if (!Package.StartsWith(TEXT("/Game/User_JeHoon/")) || !Package.Mid(18).Split(TEXT("/"), &Folder, &Relative) || (ProtectedFolders.Contains(Folder) && !bMenuValidation)) return false;
        if (!FPackageName::DoesPackageExist(Package)) continue;
        TArray<FAssetData> Assets;
        Registry.GetAssetsByPackageName(PackageName, Assets, true);
        if (Assets.IsEmpty()) continue;
        for (const FAssetData& Data : Assets)
        {
            if (Data.AssetClassPath != UObjectRedirector::StaticClass()->GetClassPathName())
            {
                UE_LOG(LogTemp, Error, TEXT("[AssetFolders] Package contains a non-redirector asset: %s / Redirector 이외의 에셋이 남아 있습니다"), *Package);
                return false;
            }
            UObjectRedirector* Redirector = Cast<UObjectRedirector>(StaticLoadObject(UObjectRedirector::StaticClass(), nullptr, *Data.GetSoftObjectPath().ToString(), nullptr, LOAD_NoRedirects));
            if (!Redirector || !Redirector->DestinationObject || Redirector->GetOutermost()->GetFName() != PackageName)
            {
                UE_LOG(LogTemp, Error, TEXT("[AssetFolders] Cannot load the exact redirector %s / 정확한 Redirector를 불러오지 못했습니다"), *Package);
                return false;
            }
            Redirectors.Add(Redirector);
        }
    }
    TSet<FName> SourcePackages(PackageNames);
    TMap<FSoftObjectPath, FSoftObjectPath> PathMap;
    TSet<FName> ReferencingPackages;
    for (FName PackageName : PackageNames)
    {
        const FString Package = PackageName.ToString();
        const FString Name = FPackageName::GetShortName(Package);
        const FString Destination = Package.StartsWith(TEXT("/Game/User_JeHoon/Validation/T12/")) ? TEXT("/Game/User_JeHoon/UI/Validation/T12/") + Name : TEXT("/Game/User_JeHoon/Assets/") + Package.Mid(18);
        PathMap.Add(FSoftObjectPath(Package + TEXT(".") + Name), FSoftObjectPath(Destination + TEXT(".") + Name));
        PathMap.Add(FSoftObjectPath(Package + TEXT(".") + Name + TEXT("_C")), FSoftObjectPath(Destination + TEXT(".") + Name + TEXT("_C")));
        TArray<FName> Referencers;
        Registry.GetReferencers(PackageName, Referencers, UE::AssetRegistry::EDependencyCategory::Package);
        for (FName Referencer : Referencers) if (!SourcePackages.Contains(Referencer)) ReferencingPackages.Add(Referencer);
    }
    for (UObjectRedirector* Redirector : Redirectors) PathMap.Add(FSoftObjectPath(Redirector), FSoftObjectPath(Redirector->DestinationObject));
    TArray<UPackage*> Packages;
    for (FName PackageName : ReferencingPackages)
    {
        if (!PackageName.ToString().StartsWith(TEXT("/Game/User_JeHoon/"))) return false;
        UPackage* Package = LoadPackage(nullptr, *PackageName.ToString(), LOAD_None);
        if (!Package) return false;
        Packages.Add(Package);
    }
    // Soft references retain their stored strings until the engine explicitly rewrites and saves their packages.
    // 소프트 참조 문자열은 엔진으로 명시적으로 갱신한 뒤 패키지를 저장해야 새 경로로 유지됩니다.
    FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().RenameReferencingSoftObjectPaths(Packages, PathMap);
    if (!UEditorLoadingAndSavingUtils::SavePackages(Packages, false)) return false;
    Registry.ScanPathsSynchronous({TEXT("/Game/User_JeHoon")}, true);
    for (FName PackageName : PackageNames)
    {
        TArray<FName> Referencers;
        Registry.GetReferencers(PackageName, Referencers, UE::AssetRegistry::EDependencyCategory::Package);
        for (FName Referencer : Referencers)
        {
            if (!SourcePackages.Contains(Referencer))
            {
                UE_LOG(LogTemp, Error, TEXT("[AssetFolders] Redirector %s still has referencer %s / Redirector를 참조하는 패키지가 남아 있습니다"), *PackageName.ToString(), *Referencer.ToString());
                return false;
            }
        }
    }
    // The caller has resaved every project package; only the audited, reference-free redirector group is deleted.
    // 호출부가 모든 프로젝트 패키지를 다시 저장했으며 참조가 없는 감사 대상 Redirector 묶음만 엔진으로 삭제합니다.
    TArray<UObject*> Objects;
    for (UObjectRedirector* Redirector : Redirectors) Objects.Add(Redirector);
    return Objects.IsEmpty() || ObjectTools::DeleteObjectsUnchecked(Objects) >= Objects.Num();
}
