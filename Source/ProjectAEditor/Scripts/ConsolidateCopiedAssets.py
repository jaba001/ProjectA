import hashlib
import json
import math
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.project_dir()).resolve()
ROOT = "/Game/User_JeHoon"
OUTPUT = PROJECT / "Saved/Automation"
MIGRATION = OUTPUT / "CopiedAssetsMigration.json"
LIBRARY = unreal.EditorAssetLibrary
REGISTRY = unreal.AssetRegistryHelpers.get_asset_registry()
HELPER = unreal.AssetDeduplicationLibrary
PAIRS = [("Manny", "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple", "/Game/Characters/Mannequins/Meshes/SK_Mannequin"), ("GKnight", "/Game/GKnight/Meshes/SK_GothicKnight_VA", "/Game/GKnight/Meshes/SK_GothicKnight_Skeleton"), ("SkeletonGuard", "/Game/Skeleton_Guard/Mesh_UE4/Full/SKM_Skeleton_Guard_Body", "/Game/Skeleton_Guard/Demoscene_UE4/Mesh/UE4_Mannequin_Skeleton")]
# Enable a mesh only after its geometry and authored settings have been compared with the original.
# 원본과 지오메트리 및 작성 설정을 비교한 메시만 통합 대상으로 허용합니다.
CONFIRMED_MESH_SOURCES = {mesh for name, mesh, skeleton in PAIRS}


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def load(path):
    return require(unreal.load_asset(path), "Missing asset: " + path)


def copied(path):
    return ROOT + path[len("/Game"):]


def filename(path):
    return PROJECT / "Content" / (path[len("/Game/"):] + ".uasset")


def write_report(name, report):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT / name).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


def original_hashes():
    hashes = {}
    for name, mesh, skeleton in PAIRS:
        for path in [mesh, skeleton]:
            with filename(path).open("rb") as stream:
                hashes[path] = hashlib.file_digest(stream, "sha256").hexdigest()
    return hashes


def save_authored(asset):
    require(asset.get_path_name().startswith(ROOT + "/"), "Refusing to save an original asset: " + asset.get_path_name())
    require(LIBRARY.save_loaded_asset(asset, only_if_is_dirty=False), "Could not save " + asset.get_path_name())


def referencers(path):
    options = unreal.AssetRegistryDependencyOptions(include_hard_package_references=True, include_soft_package_references=True)
    return {str(package) for package in REGISTRY.get_referencers(path, options)}


def preflight():
    results = []
    for name, mesh_path, skeleton_path in PAIRS:
        mesh = load(mesh_path)
        skeleton = load(skeleton_path)
        old_mesh = unreal.load_asset(copied(mesh_path)) if LIBRARY.does_asset_exist(copied(mesh_path)) else None
        old_skeleton = unreal.load_asset(copied(skeleton_path)) if LIBRARY.does_asset_exist(copied(skeleton_path)) else None
        entry = {"name": name, "mesh": mesh_path, "skeleton": skeleton_path, "eligible": False, "status": "pending"}
        if old_mesh == mesh and old_skeleton == skeleton:
            entry["status"] = "already_consolidated"
        elif not old_mesh or not old_skeleton:
            entry["status"] = "missing_copy"
        elif not HELPER.can_replace_skeleton(old_skeleton, skeleton):
            entry["status"] = "different_skeleton_preserved"
        elif mesh_path not in CONFIRMED_MESH_SOURCES or not HELPER.have_identical_skeletal_mesh_geometry(old_mesh, mesh):
            entry["status"] = "mesh_equivalence_not_confirmed"
        else:
            refs = referencers(copied(mesh_path)) | referencers(copied(skeleton_path))
            external = sorted(path for path in refs if not path.startswith(ROOT + "/"))
            entry["external_referencers"] = external
            entry["referencers"] = sorted(refs)
            entry["eligible"] = not external
            entry["status"] = "ready" if not external else "external_referencers_preserved"
        results.append(entry)
    return results


def object_path(reference):
    return reference.get_path_name() if isinstance(reference, unreal.Object) else str(reference)


def migrate_animations(entry):
    source = load(copied(entry["skeleton"]))
    target = load(entry["skeleton"])
    original_mesh = load(entry["mesh"])
    old_mesh_path = copied(entry["mesh"]) + "." + entry["mesh"].rsplit("/", 1)[1]
    # Collect paths first and migrate sequences before their montage or blend-space referencers.
    # 경로를 먼저 수집하고 몽타주나 블렌드 스페이스보다 시퀀스를 먼저 이전합니다.
    paths = sorted(entry["referencers"], key=lambda path: (0 if str(LIBRARY.find_asset_data(path).asset_class_path.asset_name) == "AnimSequence" else 2 if str(LIBRARY.find_asset_data(path).asset_class_path.asset_name) == "AnimBlueprint" else 1, path))
    changed = []
    for index, path in enumerate(paths):
        asset = load(path)
        modified = False
        if isinstance(asset, unreal.AnimationAsset):
            if asset.get_editor_property("skeleton") == source:
                require(HELPER.replace_animation_skeleton(asset, target), "Animation skeleton replacement failed; do not save this session: " + path)
                asset.set_preview_skeletal_mesh(original_mesh)
                modified = True
            if isinstance(asset, unreal.AnimSequence) and object_path(asset.get_retarget_source_asset()) == old_mesh_path:
                asset.set_retarget_source_asset(original_mesh)
                modified = True
        elif isinstance(asset, unreal.AnimBlueprint) and asset.get_editor_property("target_skeleton") == source:
            asset.set_editor_property("target_skeleton", target)
            require(HELPER.set_animation_blueprint_preview_mesh(asset, original_mesh), "Could not update animation Blueprint preview: " + path)
            modified = True
        if modified:
            save_authored(asset)
            field = "target_skeleton" if isinstance(asset, unreal.AnimBlueprint) else "skeleton"
            changed.append({"path": path, "skeleton": object_path(asset.get_editor_property(field))})
        asset = None
        if (index + 1) % 50 == 0:
            unreal.SystemLibrary.collect_garbage()
            unreal.log("COPIED_ASSETS_ANIMATION_PROGRESS " + entry["name"] + " " + str(index + 1) + "/" + str(len(paths)))
    return changed


def consolidate(entry):
    for key in ["mesh", "skeleton"]:
        target = load(entry[key])
        source = load(copied(entry[key]))
        if source != target:
            require(source.get_path_name().split(".")[0] == copied(entry[key]), "Unexpected copied asset redirection")
            require(all(path.startswith(ROOT + "/") for path in referencers(copied(entry[key]))), "An original asset references the copy")
            require(LIBRARY.consolidate_assets(target, [source]), "Engine consolidation failed: " + copied(entry[key]))
        source = None
        target = None
        unreal.SystemLibrary.collect_garbage()


def verify(report):
    require(original_hashes() == report["original_hashes"], "Original mesh or skeleton bytes changed")
    for entry in report["pairs"]:
        if entry.get("status") != "consolidated":
            continue
        mesh = load(entry["mesh"])
        skeleton = load(entry["skeleton"])
        for key, asset in [("mesh", mesh), ("skeleton", skeleton)]:
            old = copied(entry[key])
            require(load(old) == asset, "Historical copied path does not resolve to its original: " + old)
            data = REGISTRY.get_assets_by_package_name(old)
            require(data and all(str(item.asset_class_path.asset_name) == "ObjectRedirector" for item in data), "Copied payload remains: " + old)
        for index, migrated in enumerate(entry["migrated_assets"]):
            asset = load(migrated["path"])
            field = "target_skeleton" if isinstance(asset, unreal.AnimBlueprint) else "skeleton"
            require(object_path(asset.get_editor_property(field)) == migrated["skeleton"], "Migrated animation skeleton mismatch: " + migrated["path"])
            if asset.get_editor_property(field) == skeleton:
                preview = HELPER.get_animation_blueprint_preview_mesh(asset) if isinstance(asset, unreal.AnimBlueprint) else LIBRARY.find_asset_data(migrated["path"]).get_tag_value("PreviewSkeletalMesh")
                require(preview == mesh if isinstance(asset, unreal.AnimBlueprint) else preview and mesh.get_path_name() in str(preview), "Migrated animation preview mismatch: " + migrated["path"])
            asset = None
            if (index + 1) % 50 == 0:
                unreal.SystemLibrary.collect_garbage()
    sword = load(ROOT + "/Blueprint/DataAsset/Skills/BPDA_swoard_attack")
    for old, target in [("Characters/Warrior/SK_Warrior", PAIRS[1][1]), ("Characters/Warrior/SKEL_Warrior", PAIRS[1][2]), ("Characters/SwordEnemy/SK_SwordEnemy", PAIRS[2][1]), ("Characters/SwordEnemy/SKEL_SwordEnemy", PAIRS[2][2])]:
        reference = ROOT + "/" + old + "." + old.rsplit("/", 1)[1]
        require(unreal.WarriorAssetLibrary.load_saved_asset_reference(unreal.SoftObjectPath(reference)) == load(target), "Legacy saved reference does not resolve: " + old)
    for name, mesh_path in [("BP_PlayerUnit", PAIRS[0][1]), ("BP_EnemyUnit", PAIRS[2][1]), ("BP_SnapshotOpponent", PAIRS[2][1])]:
        blueprint = load(ROOT + "/Blueprint/Unit/" + name)
        defaults = unreal.get_default_object(blueprint.generated_class())
        require(defaults.get_editor_property("mesh").get_editor_property("skeletal_mesh_asset") == load(mesh_path), "Shared unit does not reference original mesh: " + name)
        for index in range(41):
            endpoints = list(unreal.WarriorAssetLibrary.sample_weapon_blade(blueprint, sword, 0.23 + index * 0.005))
            require(len(endpoints) == 2, "Shared unit weapon trace no longer resolves: " + name)
            points = [(point.x, point.y, point.z) for point in endpoints]
            require(all(math.isfinite(value) for point in points for value in point) and 90.0 < math.dist(*points) < 100.0, "Invalid shared unit blade pose: " + name)
    unreal.log("COPIED_ASSETS_COMPLETE " + str(sum(entry.get("status") == "consolidated" for entry in report["pairs"])))


def main():
    REGISTRY.search_all_assets(True)
    command_line = unreal.SystemLibrary.get_command_line()
    if "-CopiedAssetsVerifyOnly" in command_line:
        report = json.loads(MIGRATION.read_text(encoding="utf-8"))
        verify(report)
        write_report("CopiedAssetsReload.json", report)
        return
    before = original_hashes()
    previous = json.loads(MIGRATION.read_text(encoding="utf-8")) if MIGRATION.exists() else {}
    customizations = previous.get("original_customizations", [])
    if "-CopiedAssetsApply" in command_line:
        for name, mesh_path, skeleton_path in PAIRS[1:]:
            source = load(copied(skeleton_path))
            target = load(skeleton_path)
            if source != target and not HELPER.can_replace_skeleton(source, target):
                require(HELPER.have_identical_skeletal_mesh_geometry(load(copied(mesh_path)), load(mesh_path)), "Copied mesh geometry differs: " + mesh_path)
                require(HELPER.merge_copied_skeleton_slots(source, target), "Original skeleton differs beyond missing montage slots: " + skeleton_path)
                require(LIBRARY.save_loaded_asset(target, only_if_is_dirty=True), "Could not preserve montage slots on original skeleton")
                customizations.append({"skeleton": skeleton_path, "before_sha256": before[skeleton_path], "change": "Preserve copied montage slots on original skeleton"})
    before = original_hashes()
    report = {"pairs": preflight(), "original_hashes": before, "original_customizations": customizations, "gameplay_test": "not run"}
    for entry in report["pairs"]:
        if entry["status"] == "already_consolidated":
            completed = next((pair for pair in previous.get("pairs", []) if pair["name"] == entry["name"] and pair["status"] == "consolidated"), None)
            if completed:
                entry.update(completed)
                entry["eligible"] = False
    write_report("CopiedAssetsPreflight.json", report)
    if "-CopiedAssetsApply" not in command_line:
        unreal.log("COPIED_ASSETS_PREFLIGHT " + json.dumps([{key: entry[key] for key in ["name", "status"]} for entry in report["pairs"]]))
        return
    require(not any(package.get_name().startswith(ROOT + "/") for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()), "Start copied asset migration without unsaved authored assets")
    write_report(MIGRATION.name, report)
    for entry in report["pairs"]:
        if not entry["eligible"]:
            continue
        entry["migrated_assets"] = migrate_animations(entry)
        write_report(MIGRATION.name, report)
        consolidate(entry)
        entry["status"] = "consolidated"
        require(original_hashes() == before, "Original assets changed during consolidation")
        write_report(MIGRATION.name, report)
    verify(report)


if __name__ == "__main__":
    main()
