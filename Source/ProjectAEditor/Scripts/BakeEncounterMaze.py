"""Bake the shared editable BP_EncounterMaze_V7 without replacing manual edits.

공통 미로 V7을 굽고 -EncounterMazeOverwrite를 명시한 경우에만 덮어씁니다.
-EncounterMazeAudit inspects assets only; -EncounterMazeConsolidate removes unreferenced V0~V6 through the editor.
-EncounterMazeAudit는 에셋만 검사하고 -EncounterMazeConsolidate는 참조 없는 V0~V6을 에디터 기능으로 삭제합니다.
"""
import hashlib
import json
import os
from pathlib import Path

import unreal


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def inspect_and_consolidate(consolidate):
    assets = unreal.EditorAssetLibrary
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    folder = "/Game/User_JeHoon/Blueprint/Game/Encounter/Maze/"
    names = [f"BP_EncounterMaze_V{index}" for index in range(8)]
    target = require(assets.load_asset(folder + names[7]), "Shared V7 Blueprint is missing")
    require(unreal.get_default_object(target.generated_class()).get_editor_property("layout_variant") == 7, "Shared maze must retain V7 coordinates")
    filename = Path(unreal.Paths.project_content_dir()) / "User_JeHoon/Blueprint/Game/Encounter/Maze/BP_EncounterMaze_V7.uasset"
    original_hash = hashlib.sha256(filename.read_bytes()).hexdigest()
    report = {"shared_asset": target.get_path_name(), "shared_sha256": original_hash, "assets": [], "deleted": []}
    sources = []
    for index, name in enumerate(names):
        path = folder + name
        if not assets.does_asset_exist(path):
            continue
        data = registry.get_assets_by_package_name(path)
        if data and all(str(entry.asset_class_path.asset_name) == "ObjectRedirector" for entry in data):
            report["assets"].append({"path": path, "status": "redirectors"})
            continue
        blueprint = require(assets.load_asset(path), "Cannot load " + path)
        require(isinstance(blueprint, unreal.Blueprint), "Expected a Blueprint: " + path)
        defaults = unreal.get_default_object(blueprint.generated_class())
        require(isinstance(defaults, unreal.EncounterMazeLayout), "Unexpected maze parent: " + path)
        options = unreal.AssetRegistryDependencyOptions(True, True, True, True, True)
        references = sorted(str(value) for value in registry.get_referencers(path, options) if str(value) != path)
        record = {"path": path, "layout_variant": defaults.get_editor_property("layout_variant"), "referencers": references, "components": {}}
        for property_name in ["floor", "ceiling", "mortar", "wall_blocks", "pillars", "torches"]:
            component = defaults.get_editor_property(property_name)
            transforms = [component.get_instance_transform(instance, False).export_text() for instance in range(component.get_instance_count())]
            record["components"][property_name] = {"count": len(transforms), "sha256": hashlib.sha256(json.dumps(transforms).encode("utf-8")).hexdigest()}
        report["assets"].append(record)
        if index < 7:
            # Stop for referenced assets instead of force-deleting a saved map or Blueprint dependency.
            # 저장 맵이나 Blueprint 의존성을 강제로 지우지 않고 참조가 있으면 중단합니다.
            if consolidate:
                require(not references, "Migrate references before removing " + path + ": " + str(references))
            sources.append(path)
    if consolidate:
        blueprint = defaults = component = None
        unreal.SystemLibrary.collect_garbage()
        if sources:
            require(assets.consolidate_assets(target, [assets.load_asset(path) for path in sources]), "Editor consolidation into V7 failed")
        unreal.SystemLibrary.collect_garbage()
        require(unreal.EncounterMazeBakeLibrary.fixup_retired_maze_redirectors(), "Retired maze redirector cleanup failed")
        for name in names[:7]:
            path = folder + name
            require(not assets.does_asset_exist(path), "Retired maze asset or redirector remains: " + path)
            report["deleted"].append(path)
    require(hashlib.sha256(filename.read_bytes()).hexdigest() == original_hash, "Authored V7 Blueprint bytes changed")
    output = Path(unreal.Paths.project_saved_dir()) / "Automation/EncounterMaze"
    output.mkdir(parents=True, exist_ok=True)
    (output / ("Consolidate.json" if consolidate else "Audit.json")).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log(f"[EncounterMaze] inspected={len(report['assets'])} deleted={len(report['deleted'])} V7={original_hash}")


command_line = unreal.SystemLibrary.get_command_line().lower()
if "-encountermazeaudit" in command_line or "-encountermazeconsolidate" in command_line:
    require("-encountermazeoverwrite" not in command_line, "Inspection or consolidation must preserve authored V7")
    inspect_and_consolidate("-encountermazeconsolidate" in command_line)
else:
    overwrite = "-encountermazeoverwrite" in command_line
    count, report = unreal.EncounterMazeBakeLibrary.bake_encounter_maze_blueprints(overwrite)
    output = os.path.join(unreal.Paths.project_saved_dir(), "Automation", "EncounterMaze")
    os.makedirs(output, exist_ok=True)
    with open(os.path.join(output, "Bake.txt"), "w", encoding="utf-8") as handle:
        handle.write(f"baked={count} overwrite={overwrite}\n{report}")
    unreal.log(f"[EncounterMaze] baked={count}\n{report}")
