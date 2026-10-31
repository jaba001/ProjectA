import hashlib
import json
import shutil
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.project_dir()).resolve()
OLD = "/Game/User_JeHoon/Validation/T12"
NEW = "/Game/User_JeHoon/UI/Validation/T12"
NAMES = ["WBP_CharacterCreationWidget", "WBP_MainMenuRootWidget", "WBP_MainMenuScreenWidget"]
DIRECTORY = PROJECT / "Saved/ValidationWidgetOrganization"
REGISTRY = unreal.AssetRegistryHelpers.get_asset_registry()
OPTIONS = unreal.AssetRegistryDependencyOptions(include_hard_package_references=True, include_soft_package_references=True)
COMMANDS = unreal.SystemLibrary.get_command_line().split()


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def filename(package):
    return PROJECT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def write(name, value):
    DIRECTORY.mkdir(parents=True, exist_ok=True)
    (DIRECTORY / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def audit():
    require(not (DIRECTORY / "Audit.json").exists(), "An audit already exists; preserve its original backup")
    records = []
    for name in NAMES:
        old = OLD + "/" + name
        asset = require(unreal.load_asset(old), "Missing validation widget: " + old)
        require(asset.get_class().get_name() == "WidgetBlueprint" and not unreal.EditorAssetLibrary.does_asset_exist(NEW + "/" + name), "Unexpected widget type or occupied destination")
        backup = DIRECTORY / "Before" / (name + ".uasset")
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(filename(old), backup)
        records.append({"old": old, "new": NEW + "/" + name, "sha256": digest(filename(old)), "referencers": sorted(str(package) for package in REGISTRY.get_referencers(old, OPTIONS)), "dependencies": sorted(str(package) for package in REGISTRY.get_dependencies(old, OPTIONS))})
    originals = {str(path.relative_to(PROJECT)): digest(path) for path in (PROJECT / "Content/User_JeHoon/UI/MainMenu").glob("*.uasset")}
    report = {"completed": True, "records": records, "original_menu_hashes": originals}
    write("Audit.json", report)
    unreal.log("VALIDATION_WIDGET_AUDIT " + json.dumps(records))


def apply():
    baseline = json.loads((DIRECTORY / "Audit.json").read_text(encoding="utf-8"))
    require(not (DIRECTORY / "Apply.json").exists(), "The move has already started; inspect the saved record")
    assets = []
    for record in baseline["records"]:
        require(digest(filename(record["old"])) == record["sha256"], "A validation widget changed after audit")
        assets.append(require(unreal.load_asset(record["old"]), "Cannot load validation widget"))
    renames = [unreal.AssetRenameData(asset=asset, new_package_path=NEW, new_name=name) for asset, name in zip(assets, NAMES)]
    require(unreal.AssetToolsHelpers.get_asset_tools().rename_assets(renames), "Official widget rename failed")
    require(unreal.EditorAssetLibrary.save_loaded_assets(assets, only_if_is_dirty=False), "Could not save moved widgets")
    require(unreal.EditorAssetLibrary.save_directory(OLD, only_if_is_dirty=False, recursive=True), "Could not save old redirector packages")
    REGISTRY.scan_paths_synchronous([OLD, NEW], True)
    require(unreal.AssetFolderOrganizationLibrary.fixup_folder_redirectors([unreal.Name(record["old"]) for record in baseline["records"]]), "Could not clean old validation widget redirectors")
    for record in baseline["records"]:
        require(not filename(record["old"]).exists(), "An old widget package remains")
    root = PROJECT / "Content/User_JeHoon/Validation"
    require(not any(path.is_file() for path in root.rglob("*")), "The old validation folder still contains other files")
    require(unreal.EditorAssetLibrary.delete_directory("/Game/User_JeHoon/Validation"), "Could not remove the empty validation folder")
    write("Apply.json", {"completed": True, "moved": [record["new"] for record in baseline["records"]]})
    unreal.log("VALIDATION_WIDGET_APPLY 3 widgets moved")


def verify():
    baseline = json.loads((DIRECTORY / "Audit.json").read_text(encoding="utf-8"))
    require(json.loads((DIRECTORY / "Apply.json").read_text(encoding="utf-8"))["completed"], "A completed move is required")
    records = []
    for record in baseline["records"]:
        asset = require(unreal.load_asset(record["new"]), "Cannot reload moved validation widget")
        require(asset.get_class().get_name() == "WidgetBlueprint" and "ERROR" not in str(asset.get_editor_property("status")).upper(), "Widget Blueprint has compile errors")
        expected = {package.replace(OLD + "/", NEW + "/") for package in record["dependencies"]}
        current = {str(package) for package in REGISTRY.get_dependencies(record["new"], OPTIONS)}
        require(expected == current, "Widget dependencies differ beyond the path change")
        old_object = record["old"] + "." + record["old"].rsplit("/", 1)[1]
        alias = require(unreal.WarriorAssetLibrary.load_saved_asset_reference(unreal.SoftObjectPath(old_object)), "Old widget reference cannot load")
        require(alias.get_path_name().split(".")[0] == record["new"], "Old widget path resolves incorrectly")
        require(not filename(record["old"]).exists(), "An old widget package was recreated")
        records.append({"path": record["new"], "status": str(asset.get_editor_property("status"))})
    for path, expected in baseline["original_menu_hashes"].items():
        require(digest(PROJECT / path) == expected, "An original game menu widget changed")
    write("Verify.json", {"completed": True, "widgets": records, "original_game_menu_preserved": True})
    unreal.log("VALIDATION_WIDGET_VERIFY 3 widgets and 3 legacy aliases passed; original game menu bytes preserved")


REGISTRY.search_all_assets(True)
if "-ValidationWidgetsApply" in COMMANDS:
    apply()
elif "-ValidationWidgetsVerifyOnly" in COMMANDS:
    verify()
else:
    audit()
