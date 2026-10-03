import hashlib
import heapq
import json
import re
import subprocess
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.project_dir()).resolve()
CONTENT_ROOT = (PROJECT / "Content/User_JeHoon").resolve()
DIRECTORY = PROJECT / "Saved/Automation/AssetCleanup"
MANIFEST = DIRECTORY / "DeletionPlan.json"
PACKAGE_ROOT = "/Game/User_JeHoon/"
ALLOWED_CLASSES = {"AnimSequence", "AnimMontage", "AnimBlueprint", "BlendSpace"}
BATCH_SIZE = 50
COMMAND_LINE = unreal.SystemLibrary.get_command_line().split()
APPLY = "-CleanupUnusedApply" in COMMAND_LINE
VERIFY = "-CleanupUnusedVerifyOnly" in COMMAND_LINE
REGISTRY = unreal.AssetRegistryHelpers.get_asset_registry()
ASSETS = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
OPTIONS = unreal.AssetRegistryDependencyOptions(include_hard_package_references=True, include_soft_package_references=True, include_searchable_names=True, include_hard_management_references=True, include_soft_management_references=True)


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def git_head():
    result = subprocess.run(["git", "rev-parse", "HEAD"], cwd=PROJECT, capture_output=True, text=True, check=True)
    return result.stdout.strip()


def file_hash(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def write_report(filename, report):
    DIRECTORY.mkdir(parents=True, exist_ok=True)
    target = DIRECTORY / filename
    temporary = target.with_suffix(".tmp")
    temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(target)


def read_manifest():
    require(not (APPLY and VERIFY), "Apply and VerifyOnly cannot be combined")
    manifest_hash = file_hash(MANIFEST)
    plan = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    require(plan.get("schema_version") == 1, "Unsupported cleanup manifest schema")
    require(isinstance(plan.get("base_commit"), str) and re.fullmatch(r"[0-9a-f]{40}", plan["base_commit"]), "Manifest must record the full Git commit")
    require(git_head() == plan["base_commit"], "Git HEAD differs from the reviewed cleanup plan")
    require(isinstance(plan.get("assets"), list) and plan["assets"], "Cleanup manifest must contain reviewed assets")
    require(isinstance(plan.get("retained_roots"), list) and plan["retained_roots"], "Cleanup manifest must preserve explicit roots")
    require(all(isinstance(root, str) and root.startswith("/") for root in plan["retained_roots"]), "Retained roots must be package names")
    entries = {}
    files = set()
    for entry in plan["assets"]:
        package = entry.get("package")
        require(isinstance(package, str) and package.startswith(PACKAGE_ROOT) and all(part and part not in {".", ".."} for part in package.split("/")[1:]) and "." not in package and "\\" not in package, "Refusing an external or invalid cleanup package")
        require(package not in entries and package not in plan["retained_roots"], "Duplicated package or retained root in deletion plan: " + package)
        require(entry.get("asset_class") in ALLOWED_CLASSES, "Only the four reviewed animation classes may be deleted: " + package)
        require(isinstance(entry.get("file"), str), "Manifest file path is missing: " + package)
        path = (PROJECT / entry["file"]).resolve()
        expected = (PROJECT / "Content" / (package[len("/Game/"):] + ".uasset")).resolve()
        require(path == expected and path.is_relative_to(CONTENT_ROOT) and path not in files, "Manifest file is outside its exact project package: " + package)
        require(type(entry.get("bytes")) is int and entry["bytes"] > 0, "Invalid reviewed file size: " + package)
        require(isinstance(entry.get("sha256"), str) and re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]), "Invalid reviewed file hash: " + package)
        entries[package] = (entry, path)
        files.add(path)
    return plan, entries, manifest_hash


def unchanged_plan(plan, manifest_hash):
    require(file_hash(MANIFEST) == manifest_hash and git_head() == plan["base_commit"], "The manifest or Git HEAD changed during cleanup")


def dependencies(package):
    return {str(value) for value in (REGISTRY.get_dependencies(package, OPTIONS) or [])}


def outside_referencers(package, candidates):
    # Loaded-object confirmation may omit soft references; retain all registry dependency categories.
    # 메모리 참조 확인은 소프트 참조를 놓칠 수 있으므로 Registry의 모든 의존 분류를 유지합니다.
    return sorted(str(value) for value in (REGISTRY.get_referencers(package, OPTIONS) or []) if str(value) not in candidates)


def check_file(entry, path):
    require(path.is_file() and path.stat().st_size == entry["bytes"] and file_hash(path) == entry["sha256"], "Reviewed asset bytes changed: " + entry["package"])


def audit(plan, entries):
    candidates = set(entries)
    for package, (entry, path) in entries.items():
        check_file(entry, path)
        data = list(REGISTRY.get_assets_by_package_name(package))
        require(len(data) == 1 and str(data[0].asset_class_path.asset_name) == entry["asset_class"], "Registry asset class differs from the reviewed plan: " + package)
        references = outside_referencers(package, candidates)
        require(not references, "Retained packages reference deletion candidate " + package + ": " + ", ".join(references[:8]))
    # Delete referencers before their dependencies; reject cycles rather than force-breaking a retained edge.
    # 참조하는 에셋을 의존 에셋보다 먼저 삭제하며 순환 참조는 강제로 끊지 않고 거절합니다.
    edges = {package: (dependencies(package) & candidates) - {package} for package in entries}
    incoming = dict.fromkeys(entries, 0)
    for targets in edges.values():
        for target in targets:
            incoming[target] += 1
    ready = [package for package, count in incoming.items() if count == 0]
    heapq.heapify(ready)
    order = []
    while ready:
        package = heapq.heappop(ready)
        order.append(package)
        for target in sorted(edges[package]):
            incoming[target] -= 1
            if incoming[target] == 0:
                heapq.heappush(ready, target)
    require(len(order) == len(entries), "Deletion candidates contain a dependency cycle; revise the manifest")
    return order


def verify_deleted(plan, entries):
    candidates = set(entries)
    for package, (_, path) in entries.items():
        require(not path.exists() and not REGISTRY.get_assets_by_package_name(package), "Deletion candidate still exists: " + package)
        references = outside_referencers(package, candidates)
        require(not references, "Surviving package still references a deleted asset " + package + ": " + ", ".join(references[:8]))
    # Deleted registry nodes may lose reverse edges; inspect every surviving disk package as well.
    # 삭제된 Registry 노드는 역방향 참조를 잃을 수 있으므로 남은 디스크 패키지도 전부 검사합니다.
    surviving = {str(data.package_name) for data in REGISTRY.get_all_assets(include_only_on_disk_assets=True)} - candidates
    for package in surviving:
        broken = dependencies(package) & candidates
        require(not broken, "Surviving package depends on deleted assets " + package + ": " + ", ".join(sorted(broken)[:8]))
    pending = list(plan["retained_roots"])
    visited = set()
    while pending:
        package = pending.pop()
        if package in visited:
            continue
        visited.add(package)
        require(package not in candidates, "Retained root closure reaches a deleted asset: " + package)
        pending.extend(dependencies(package) - visited)
    return {"retained_closure_count": len(visited), "surviving_packages_checked": len(surviving)}


def main():
    plan, entries, manifest_hash = read_manifest()
    # Discover Engine/plugin referencers too, then refresh project packages without starting gameplay.
    # 게임을 실행하지 않고 Engine과 플러그인 참조까지 찾은 뒤 프로젝트 패키지를 갱신합니다.
    REGISTRY.search_all_assets(True)
    REGISTRY.scan_paths_synchronous(["/Game"], force_rescan=True)
    report = {"schema_version": 1, "mode": "verify" if VERIFY else "apply" if APPLY else "audit", "base_commit": plan["base_commit"], "manifest_sha256": manifest_hash, "candidate_count": len(entries), "planned_bytes": sum(entry["bytes"] for entry, _ in entries.values()), "deleted": [], "completed": False, "gameplay_test": "not run"}
    report_name = "Verify.json" if VERIFY else "Apply.json" if APPLY else "Audit.json"
    try:
        if VERIFY:
            report.update(verify_deleted(plan, entries))
        else:
            order = audit(plan, entries)
            report["deletion_order"] = order
            unchanged_plan(plan, manifest_hash)
            write_report(report_name, report)
            if APPLY:
                # Force-delete APIs do not protect references; the complete graph/hash audit must pass first.
                # 강제 삭제 API는 참조를 보호하지 않으므로 전체 그래프와 해시 검사를 먼저 통과해야 합니다.
                candidates = set(entries)
                for offset in range(0, len(order), BATCH_SIZE):
                    batch = order[offset:offset + BATCH_SIZE]
                    loaded = []
                    incoming = {}
                    for package in batch:
                        entry, path = entries[package]
                        check_file(entry, path)
                        require(not outside_referencers(package, candidates), "A new retained reference appeared before deletion: " + package)
                        incoming[package] = outside_referencers(package, set(batch))
                        asset = require(unreal.load_asset(package), "Could not load reviewed deletion candidate: " + package)
                        require(asset.get_path_name().split(".")[0] == package and asset.get_class().get_name() == entry["asset_class"], "Loaded asset differs from the reviewed package: " + package)
                        loaded.append(asset)
                    unchanged_plan(plan, manifest_hash)
                    # UE 5.8 exposes DeleteLoadedAssets, allowing one force-delete/GC pass per ordered batch.
                    # UE 5.8의 DeleteLoadedAssets로 정렬된 묶음마다 강제 삭제와 GC를 한 번 처리합니다.
                    result = ASSETS.delete_loaded_assets(loaded)
                    deleted = [package for package in batch if not entries[package][1].exists()]
                    report["deleted"].extend(deleted)
                    report["last_batch_incoming_referencers"] = incoming
                    loaded.clear()
                    asset = None
                    unreal.SystemLibrary.collect_garbage()
                    unchanged_plan(plan, manifest_hash)
                    write_report(report_name, report)
                    unreal.log("ASSET_CLEANUP_PROGRESS " + str(len(report["deleted"])) + "/" + str(len(order)))
                    require(result and len(deleted) == len(batch), "Unreal batch deletion was incomplete; preserve Apply.json and revise the remaining plan")
                report.update(verify_deleted(plan, entries))
        unchanged_plan(plan, manifest_hash)
        report["completed"] = True
        write_report(report_name, report)
        unreal.log("ASSET_CLEANUP_COMPLETE " + str(DIRECTORY / report_name))
    except Exception as error:
        report["error"] = str(error)
        write_report(report_name, report)
        raise


if __name__ == "__main__":
    main()
