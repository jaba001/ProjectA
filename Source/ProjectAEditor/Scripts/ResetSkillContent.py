import hashlib
import json
import subprocess
import sys
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
DIRECTORY = ROOT / "Saved/Automation/SkillReset"
PLAN_PATH = Path(__file__).with_name("RetiredSkillContent.json")
PLAN = json.loads(PLAN_PATH.read_text(encoding="utf-8"))
PLAN_SHA = hashlib.sha256(PLAN_PATH.read_bytes()).hexdigest()
APPLY = "-SkillResetApply" in unreal.SystemLibrary.get_command_line().split()
VERIFY = "-SkillResetVerifyOnly" in unreal.SystemLibrary.get_command_line().split()
RESUME = "-SkillResetResume" in unreal.SystemLibrary.get_command_line().split()
REGISTRY = unreal.AssetRegistryHelpers.get_asset_registry()
ASSETS = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
OPTIONS = unreal.AssetRegistryDependencyOptions(include_hard_package_references=True, include_soft_package_references=True, include_searchable_names=True, include_hard_management_references=True, include_soft_management_references=True)
ENTRIES = {entry["package"]: entry for entry in PLAN["assets"]}
RETAINED = set(PLAN["retained_skills"] + PLAN["preserved_monster_skills"])
REPORT = {"mode": "reload" if VERIFY else "apply" if APPLY else "audit", "manifest_sha256": PLAN_SHA, "completed": False, "deleted": [], "gameplay_test": "not run"}
PREVIOUSLY_DELETED = set()
if RESUME:
    previous = json.loads((DIRECTORY / "Apply.json").read_text(encoding="utf-8"))
    if previous.get("manifest_sha256") != PLAN_SHA or not APPLY or VERIFY:
        raise RuntimeError("Resume requires the same manifest and an interrupted apply report")
    PREVIOUSLY_DELETED = set(previous["deleted"])
    REPORT["deleted"] = sorted(PREVIOUSLY_DELETED)
    REPORT["resumed_from_deleted_count"] = len(PREVIOUSLY_DELETED)


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def filename(package):
    require(package in ENTRIES, "Package is outside the exact reviewed manifest")
    entry = ENTRIES[package]
    path = (ROOT / entry["file"]).resolve()
    expected = (ROOT / "Content" / package.removeprefix("/Game/")).resolve()
    require(package.startswith("/Game/") and path.is_relative_to((ROOT / "Content").resolve()) and path.with_suffix("") == expected and path.suffix in [".uasset", ".umap"], "Invalid reviewed package filename")
    return path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_report():
    DIRECTORY.mkdir(parents=True, exist_ok=True)
    (DIRECTORY / ("Reload.json" if VERIFY else "Apply.json" if APPLY else "Preflight.json")).write_text(json.dumps(REPORT, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def dependencies(package):
    return set(str(value) for value in REGISTRY.get_dependencies(package, OPTIONS) or [])


def outside_referencers(package):
    return sorted(set(str(value) for value in REGISTRY.get_referencers(package, OPTIONS) or []) - set(ENTRIES))


def validate_plan():
    require(not (APPLY and VERIFY), "Apply and verify modes cannot be combined")
    require(len(ENTRIES) == len(PLAN["assets"]) == 2813 and len(RETAINED) == 14 and not PLAN["mutable_packages"], "Reviewed reset scope changed")
    require(not set(ENTRIES) & RETAINED and len(PLAN["vfx_pack_roots"]) == 13, "Retained skills overlap deletion candidates")
    if not VERIFY:
        head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
        require(head == PLAN["base_commit"], "Git HEAD differs from the reviewed reset plan")
    for package, entry in ENTRIES.items():
        allowed = package.startswith("/Game/User_JeHoon/") or any(package.startswith(root + "/") for root in PLAN["vfx_pack_roots"]) or package.startswith(("/Game/__ExternalActors__/Free_Magic/", "/Game/__ExternalObjects__/Free_Magic/"))
        require(allowed, "Package is outside reviewed combat VFX roots")
        path = filename(package)
        if VERIFY or package in PREVIOUSLY_DELETED:
            require(not path.exists() and not REGISTRY.get_assets_by_package_name(package), "Deleted package remains: " + package)
        else:
            require(path.is_file() and path.stat().st_size == entry["bytes"] and digest(path) == entry["sha256"], "Reviewed asset bytes changed: " + package)
            classes = sorted(str(data.asset_class_path.asset_name) for data in REGISTRY.get_assets_by_package_name(package))
            require(classes == sorted(entry["asset_classes"]), "Reviewed asset classes changed: " + package)
            require(not outside_referencers(package), "Retained package references deletion candidate: " + package)


def deletion_groups():
    # Treat reference cycles as one closed group, then delete referencers before dependencies.
    # 참조 순환을 닫힌 묶음으로 처리하고 참조하는 에셋을 의존 에셋보다 먼저 삭제합니다.
    edges = {package: dependencies(package) & set(ENTRIES) for package in ENTRIES}
    indices = {}
    low = {}
    stack = []
    active = set()
    groups = []
    sys.setrecursionlimit(10000)

    def visit(package):
        indices[package] = low[package] = len(indices)
        stack.append(package)
        active.add(package)
        for target in sorted(edges[package]):
            if target not in indices:
                visit(target)
                low[package] = min(low[package], low[target])
            elif target in active:
                low[package] = min(low[package], indices[target])
        if low[package] == indices[package]:
            group = []
            while True:
                target = stack.pop()
                active.remove(target)
                group.append(target)
                if target == package:
                    break
            groups.append(sorted(group))

    for package in sorted(edges):
        if package not in indices:
            visit(package)
    # Tarjan emits dependencies first; reverse its component order for deletion.
    # Tarjan은 의존 묶음을 먼저 반환하므로 삭제 순서에서는 묶음 순서를 뒤집습니다.
    groups.reverse()
    REPORT["cyclic_groups"] = [group for group in groups if len(group) > 1]
    return groups


def delete_batch(packages):
    require(digest(PLAN_PATH) == PLAN_SHA, "Manifest changed during deletion")
    registered = []
    if any(package.startswith("/Game/__External") for package in packages):
        world_rows = REGISTRY.get_assets_by_package_name("/Game/Free_Magic/Maps/Free_Magic_Map")
        if world_rows:
            require(unreal.AssetDeduplicationLibrary.load_asset_with_external_objects(world_rows[0]), "Could not load the reviewed world and its external objects")
    for package in packages:
        path = filename(package)
        # World deletion may already clean its reviewed external actor, object and build-data packages.
        # 월드 삭제가 검토된 External Actor·Object·빌드 데이터 패키지를 이미 정리할 수 있습니다.
        if not path.exists():
            require(not REGISTRY.get_assets_by_package_name(package), "Engine-cleaned package remains registered")
            continue
        require(digest(path) == ENTRIES[package]["sha256"] and not outside_referencers(package), "Asset changed or gained a retained reference before deletion")
        data = REGISTRY.get_assets_by_package_name(package)
        require(data, "Reviewed package is not registered")
        registered.extend(data)
    if registered:
        require(unreal.AssetDeduplicationLibrary.delete_reviewed_combat_assets(registered), "Unreal reviewed batch deletion failed")
        registered.clear()
        unreal.SystemLibrary.collect_garbage()
    remaining = [package for package in packages if filename(package).exists()]
    for package in remaining:
        require(digest(filename(package)) == ENTRIES[package]["sha256"] and not REGISTRY.get_assets_by_package_name(package) and not outside_referencers(package), "Deleted asset package changed or remains registered")
    if remaining:
        require(unreal.AssetDeduplicationLibrary.cleanup_deleted_combat_asset_packages([unreal.Name(package) for package in remaining]), "Could not finish Unreal package cleanup")
    for package in packages:
        require(not filename(package).exists() and not REGISTRY.get_assets_by_package_name(package), "Unreal package deletion was incomplete: " + package)
    REPORT["deleted"] = sorted(package for package in ENTRIES if not filename(package).exists())
    write_report()
    unreal.log("SKILL_RESET_PROGRESS " + str(len(REPORT["deleted"])) + "/" + str(len(ENTRIES)))


def cleanup_folders():
    roots = [ROOT / "Content" / root.removeprefix("/Game/") for root in PLAN["vfx_pack_roots"]]
    roots.extend(ROOT / "Content" / name for name in ["__ExternalActors__/Free_Magic", "__ExternalObjects__/Free_Magic", "User_JeHoon/RPGEffects", "User_JeHoon/Free_Magic"])
    content = (ROOT / "Content").resolve()
    removed = []
    for folder in roots:
        resolved = folder.resolve()
        require(resolved.is_relative_to(content) and resolved != content, "Folder escapes the reviewed Content roots")
        if APPLY and folder.exists():
            # Unreal removes every package first; filesystem cleanup removes empty directories only.
            # 모든 패키지를 Unreal로 먼저 삭제하고 파일 시스템에서는 빈 폴더만 제거합니다.
            require(not any(path.is_file() for path in folder.rglob("*")), "Reviewed folder still contains files: " + str(folder))
            for child in sorted((path for path in folder.rglob("*") if path.is_dir()), key=lambda path: len(path.parts), reverse=True):
                require(child.resolve().is_relative_to(resolved), "Empty folder escapes the reviewed root")
                child.rmdir()
            folder.rmdir()
        require(not folder.exists(), "Reviewed VFX folder remains: " + str(folder))
        removed.append(folder.relative_to(ROOT).as_posix())
    REPORT["removed_folder_roots"] = removed


def delete_demo_world():
    world_package = "/Game/Free_Magic/Maps/Free_Magic_Map"
    external = [package for package in ENTRIES if package.startswith("/Game/__External")]
    if filename(world_package).exists():
        require(digest(filename(world_package)) == ENTRIES[world_package]["sha256"] and not outside_referencers(world_package), "Reviewed demo world changed or gained a retained reference")
        rows = REGISTRY.get_assets_by_package_name(world_package)
        world = require(unreal.AssetDeduplicationLibrary.load_asset_with_external_objects(rows[0]), "Could not load retired demo world")
        require(ASSETS.delete_loaded_asset(world), "Could not delete the retired demo world through Unreal")
        world = None
        unreal.SystemLibrary.collect_garbage()
    require(not filename(world_package).exists(), "Demo world must be deleted before orphan cleanup")
    remaining = [package for package in external if filename(package).exists()]
    for package in remaining:
        require(digest(filename(package)) == ENTRIES[package]["sha256"] and not outside_referencers(package), "Reviewed external package changed or gained a retained reference")
    require(unreal.AssetDeduplicationLibrary.delete_retired_demo_external_packages([unreal.Name(package) for package in remaining]), "Could not clean the retired world's reviewed orphan packages")
    REPORT["deleted"] = sorted(package for package in ENTRIES if not filename(package).exists())
    write_report()


def verify_graph():
    for package in ENTRIES:
        require(not filename(package).exists() and not REGISTRY.get_assets_by_package_name(package), "Deletion incomplete: " + package)
        require(not outside_referencers(package), "Retained package references deleted content: " + package)
    surviving = {str(data.package_name) for data in REGISTRY.get_all_assets(include_only_on_disk_assets=True)} - set(ENTRIES)
    for package in surviving:
        require(not dependencies(package) & set(ENTRIES), "Surviving dependency reaches deleted content: " + package)
    skills = {str(data.package_name) for data in REGISTRY.get_assets_by_path("/Game", recursive=True) if str(data.asset_class_path.asset_name) == "SkillDefinitionDataAsset"}
    require(skills == RETAINED, "Retained basic or monster skills changed")
    for package in skills:
        skill = require(unreal.load_asset(package), "Could not load retained skill")
        require(unreal.MonsterAssetLibrary.validate_monster_skill(skill), "Retained skill fails data validation")
    REPORT.update({"surviving_packages_checked": len(surviving), "remaining_skills": sorted(skills), "deleted_package_count": len(ENTRIES), "deleted_bytes": sum(entry["bytes"] for entry in ENTRIES.values())})


def verify_files():
    before = json.loads((DIRECTORY / "FilesBefore.json").read_text(encoding="utf-8"))
    removed = {entry["file"] for entry in ENTRIES.values()}
    current = {path.relative_to(ROOT).as_posix(): [path.stat().st_size, path.stat().st_mtime_ns] for path in (ROOT / "Content").rglob("*") if path.is_file()}
    require(set(before) - set(current) == removed and not set(current) - set(before), "Unexpected Content file deletion or creation")
    require(all(before[name] == current[name] for name in current), "Retained Content file changed")
    hashes = json.loads((DIRECTORY / "TrackedHashes.json").read_text(encoding="utf-8"))
    protected = {name: value for name, value in hashes.items() if name not in removed}
    require(all(digest(ROOT / name) == value for name, value in protected.items()), "Retained tracked Content bytes changed")
    REPORT.update({"protected_tracked_hashes": len(protected), "protected_content_metadata": len(current), "remaining_content_files": len(current)})


def main():
    REGISTRY.search_all_assets(True)
    REGISTRY.scan_paths_synchronous(["/Game"], force_rescan=True)
    try:
        validate_plan()
        if not VERIFY:
            groups = deletion_groups()
            REPORT["planned_packages"] = len(ENTRIES)
            write_report()
            if APPLY:
                delete_demo_world()
                batch = []
                for group in groups:
                    group = [package for package in group if package not in set(REPORT["deleted"])]
                    if batch and len(batch) + len(group) > 50:
                        delete_batch(batch)
                        batch = []
                    batch.extend(group)
                if batch:
                    delete_batch(batch)
        if APPLY or VERIFY:
            verify_graph()
            verify_files()
            cleanup_folders()
        REPORT["completed"] = True
        write_report()
        unreal.log("SKILL_RESET_COMPLETE " + str(DIRECTORY))
    except Exception as error:
        REPORT["error"] = str(error)
        write_report()
        raise


if __name__ == "__main__":
    main()
