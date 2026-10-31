import hashlib
import json
import re
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.project_dir()).resolve()
LAYOUT = json.loads(Path(__file__).with_name("AssetFolderLayout.json").read_text(encoding="utf-8-sig"))
ROOT = LAYOUT["root"]
DESTINATION = LAYOUT["destination"]
DIRECTORY = PROJECT / "Saved/AssetFolderOrganization"
REGISTRY = unreal.AssetRegistryHelpers.get_asset_registry()
OPTIONS = unreal.AssetRegistryDependencyOptions(include_hard_package_references=True, include_soft_package_references=True, include_searchable_names=True, include_hard_management_references=True, include_soft_management_references=True)
COMMAND_LINE = unreal.SystemLibrary.get_command_line().split()


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def write(name, value):
    DIRECTORY.mkdir(parents=True, exist_ok=True)
    (DIRECTORY / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def mapped(package):
    for folder in LAYOUT["folders"]:
        prefix = ROOT + "/" + folder + "/"
        if package.startswith(prefix):
            return DESTINATION + "/" + package[len(ROOT) + 1:]
    return package


def audit():
    records = []
    redirectors = []
    for folder in LAYOUT["folders"]:
        assets = REGISTRY.get_assets_by_path(ROOT + "/" + folder, recursive=True)
        require(assets, "Source folder has no registered assets: " + folder)
        for asset in assets:
            old = str(asset.package_name)
            require(str(asset.asset_class_path.asset_name) != "World", "Worlds require separate handling: " + old)
            if str(asset.asset_class_path.asset_name) == "ObjectRedirector":
                target = asset.get_tag_value("DestinationObject")
                require(target, "Redirector target is missing: " + old)
                redirectors.append({"old": old, "target": str(target), "referencers": sorted(str(name) for name in REGISTRY.get_referencers(old, OPTIONS))})
                continue
            new = mapped(old)
            require(new != old and not unreal.EditorAssetLibrary.does_asset_exist(new), "Destination already exists or mapping is invalid: " + new)
            relative = old.removeprefix("/Game/") + ".uasset"
            file = PROJECT / "Content" / relative
            require(file.is_file(), "Source package is not a regular uasset: " + old)
            records.append({"old": old, "new": new, "class": str(asset.asset_class_path.package_name) + "." + str(asset.asset_class_path.asset_name), "sha256": hashlib.sha256(file.read_bytes()).hexdigest(), "referencers": sorted(str(name) for name in REGISTRY.get_referencers(old, OPTIONS)), "dependencies": sorted(str(name) for name in REGISTRY.get_dependencies(old, OPTIONS))})
    require(len(records) + len(redirectors) == 219 and len({record["new"] for record in records}) == len(records), "The expected 219 unique asset packages changed: " + str((len(records), len(redirectors))))
    external = sorted({package for record in records for package in record["referencers"] if package.startswith("/Game/") and not package.startswith(ROOT + "/")})
    result = {"layout": LAYOUT, "records": records, "redirectors": redirectors, "external_referencers": external, "completed": True}
    write("Audit.json", result)
    unreal.log("ASSET_FOLDER_AUDIT " + json.dumps({"count": len(records), "redirectors": len(redirectors), "external_referencers": external}))


def apply():
    baseline = json.loads((DIRECTORY / "Audit.json").read_text(encoding="utf-8"))
    require(baseline.get("completed") and baseline["layout"] == LAYOUT and not baseline["external_referencers"], "A complete audit with no external referencers is required")
    require((DIRECTORY / "Before/Content/User_JeHoon").is_dir(), "The content backup is required before moving assets")
    if (DIRECTORY / "Apply.json").exists():
        previous = json.loads((DIRECTORY / "Apply.json").read_text(encoding="utf-8"))
        require(not previous["renamed"] and not previous["completed"] and all(not unreal.EditorAssetLibrary.does_asset_exist(record["new"]) for record in baseline["records"]), "Only a cancelled rename with zero moved assets may be retried")
    assets = []
    imports = {}
    for record in baseline["records"]:
        source = PROJECT / "Content" / (record["old"].removeprefix("/Game/") + ".uasset")
        require(hashlib.sha256(source.read_bytes()).hexdigest() == record["sha256"], "An asset changed after audit: " + record["old"])
        asset = require(unreal.load_asset(record["old"]), "Cannot load source asset: " + record["old"])
        import_data = getattr(asset, "asset_import_data", None)
        if import_data:
            imports[record["new"]] = [str(name) for name in import_data.extract_filenames()]
        assets.append(asset)
    report = {"completed": False, "renamed": [], "import_sources": imports}
    write("Apply.json", report)
    require(unreal.AssetFolderOrganizationLibrary.rename_audited_folder_assets(assets), "Official AssetTools batch rename failed; inspect the saved apply record before resuming")
    for asset, record in zip(assets, baseline["records"]):
        require(asset.get_path_name().split(".")[0] == record["new"], "Unexpected renamed asset identity: " + record["old"])
        if record["new"] in imports:
            current = [str(name) for name in asset.asset_import_data.extract_filenames()]
            require(current == imports[record["new"]], "A source import filename changed: " + record["new"])
        require(unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), "Could not save moved asset: " + record["new"])
        report["renamed"].append(record["new"])
    write("Apply.json", report)
    cleanup(baseline, report)


def cleanup(baseline, report):
    require(len(report["renamed"]) == 213 and all(unreal.EditorAssetLibrary.does_asset_exist(record["new"]) for record in baseline["records"]), "Cleanup requires every audited destination to exist")
    # Recover only original bytes retained when the rename's save prompt was cancelled by headless execution.
    # 비대화 실행의 저장 안내 취소로 남은 패키지는 감사 당시 원본 바이트와 같을 때만 복구 정리합니다.
    recovered = report.setdefault("retained_originals_consolidated", [])
    for record in baseline["records"]:
        source = PROJECT / "Content" / (record["old"].removeprefix("/Game/") + ".uasset")
        data = REGISTRY.get_assets_by_package_name(record["old"])
        if not source.is_file() or not data or str(data[0].asset_class_path.asset_name) == "ObjectRedirector":
            continue
        require(hashlib.sha256(source.read_bytes()).hexdigest() == record["sha256"], "A retained original changed since audit: " + record["old"])
        old_asset = require(unreal.load_asset(record["old"]), "Cannot load retained original: " + record["old"])
        new_asset = require(unreal.load_asset(record["new"]), "Cannot load moved asset: " + record["new"])
        require(old_asset.get_class() == new_asset.get_class() and old_asset.get_path_name().split(".")[0] == record["old"], "Unexpected retained asset identity")
        require(unreal.AssetFolderOrganizationLibrary.consolidate_retained_folder_asset(old_asset, new_asset), "Official cleanup of retained original failed: " + record["old"])
        recovered.append(record["old"])
        write("Apply.json", report)
    require(unreal.EditorAssetLibrary.save_directory(ROOT, only_if_is_dirty=False, recursive=True), "Could not save updated project-owned references before redirector cleanup")
    # All cleanup is performed through AssetTools, never by moving or deleting package files directly.
    # 패키지 파일을 직접 이동하거나 삭제하지 않고 AssetTools로 모든 정리를 수행합니다.
    old_packages = [unreal.Name(record["old"]) for record in baseline["records"] + baseline["redirectors"]]
    REGISTRY.scan_paths_synchronous([ROOT], True)
    require(unreal.AssetFolderOrganizationLibrary.fixup_folder_redirectors(old_packages), "Engine redirector cleanup rejected the audited package list")
    require(unreal.EditorAssetLibrary.save_directory(ROOT, only_if_is_dirty=True, recursive=True), "Could not save updated project-owned references")
    REGISTRY.scan_paths_synchronous([ROOT], True)
    report["remaining_sources"] = [str(package) for package in old_packages if (PROJECT / "Content" / (str(package).removeprefix("/Game/") + ".uasset")).exists()]
    write("Apply.json", report)
    require(not report["remaining_sources"], "Some source redirectors remain; inspect Apply.json for exact paths")
    for folder in LAYOUT["folders"]:
        physical = PROJECT / "Content/User_JeHoon" / folder
        if physical.is_dir():
            require(not any(path.is_file() for path in physical.rglob("*")), "An old folder still contains files: " + folder)
            require(unreal.EditorAssetLibrary.delete_directory(ROOT + "/" + folder), "Engine could not remove the empty source folder: " + folder)
    report["completed"] = True
    write("Apply.json", report)
    unreal.log("ASSET_FOLDER_APPLY " + json.dumps({"moved": len(report["renamed"]), "remaining_sources": report["remaining_sources"]}))


def verify():
    baseline = json.loads((DIRECTORY / "Audit.json").read_text(encoding="utf-8"))
    applied = json.loads((DIRECTORY / "Apply.json").read_text(encoding="utf-8"))
    aliases = json.loads((DIRECTORY / "PathAliases.json").read_text(encoding="utf-8"))
    require(applied["completed"] and len(applied["renamed"]) == 213, "A completed audited move is required")
    report = {"completed": False, "loaded": [], "referencers_loaded": [], "legacy_aliases": [], "missing_dependencies": [], "preexisting_missing_dependencies": [], "dependency_changes": []}
    for record in baseline["records"]:
        asset = require(unreal.load_asset(record["new"]), "Cannot reload moved asset: " + record["new"])
        require(asset.get_path_name().split(".")[0] == record["new"], "Moved identity differs on reload")
        require(str(asset.get_class().get_path_name()) == record["class"], "Asset class changed during organization: " + record["new"])
        import_data = getattr(asset, "asset_import_data", None)
        if record["new"] in applied["import_sources"]:
            require(import_data and [str(name) for name in import_data.extract_filenames()] == applied["import_sources"][record["new"]], "Imported source filenames differ after reload")
        dependencies = {str(package) for package in REGISTRY.get_dependencies(record["new"], OPTIONS)}
        expected = {aliases.get(package, package) for package in record["dependencies"]}
        if expected != dependencies:
            report["dependency_changes"].append({"asset": record["new"], "removed": sorted(expected - dependencies), "added": sorted(dependencies - expected)})
        for dependency in dependencies:
            if dependency.startswith("/Game/"):
                relative = PROJECT / "Content" / dependency.removeprefix("/Game/")
                if not relative.with_suffix(".uasset").is_file() and not relative.with_suffix(".umap").is_file():
                    target = "preexisting_missing_dependencies" if dependency in record["dependencies"] and not dependency.startswith(ROOT + "/") else "missing_dependencies"
                    report[target].append({"asset": record["new"], "dependency": dependency})
        report["loaded"].append(record["new"])
    for data in REGISTRY.get_assets_by_path(ROOT, recursive=True):
        package = str(data.package_name)
        if package.startswith(DESTINATION + "/"):
            continue
        asset = require(data.get_asset(), "Cannot reload a project referencer: " + package)
        require(asset.get_path_name().split(".")[0] == package, "A project referencer resolved to an unexpected package")
        report["referencers_loaded"].append(package)
    for old, new in aliases.items():
        old_object = old + "." + old.rsplit("/", 1)[1]
        asset = require(unreal.WarriorAssetLibrary.load_saved_asset_reference(unreal.SoftObjectPath(old_object)), "A saved asset alias cannot load: " + old)
        require(asset.get_path_name().split(".")[0] == new, "A saved asset alias resolves to the wrong destination: " + old)
        require(not (PROJECT / "Content" / (old.removeprefix("/Game/") + ".uasset")).exists(), "An old physical package remains: " + old)
        report["legacy_aliases"].append(old)
    write("Verify.json", report)
    require(not report["missing_dependencies"], "Moved assets have missing on-disk dependencies; inspect Verify.json")
    require(not report["dependency_changes"], "Moved asset dependencies differ beyond the audited path substitutions")
    report["completed"] = True
    write("Verify.json", report)
    unreal.log("ASSET_FOLDER_VERIFY " + json.dumps({"loaded": len(report["loaded"]), "referencers_loaded": len(report["referencers_loaded"]), "legacy_aliases": len(report["legacy_aliases"]), "missing_dependencies": len(report["missing_dependencies"]), "preexisting_missing_dependencies": len(report["preexisting_missing_dependencies"]), "dependency_changes": len(report["dependency_changes"])}))


REGISTRY.search_all_assets(True)
if "-AssetFoldersApply" in COMMAND_LINE:
    apply()
elif "-AssetFoldersCleanup" in COMMAND_LINE:
    cleanup(json.loads((DIRECTORY / "Audit.json").read_text(encoding="utf-8")), json.loads((DIRECTORY / "Apply.json").read_text(encoding="utf-8")))
elif "-AssetFoldersVerifyOnly" in COMMAND_LINE:
    verify()
else:
    audit()
