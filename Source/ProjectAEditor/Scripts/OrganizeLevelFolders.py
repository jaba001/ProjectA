import hashlib
import json
import math
import re
import sys
from pathlib import Path

import unreal


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))

import ConfigureLevelLighting as lighting


ROOT = Path(unreal.Paths.project_dir()).resolve()
DIRECTORY = ROOT / "Saved/Automation/LevelFolders"
LAYOUT_FILE = SCRIPT_DIRECTORY / "LevelFolderLayout.json"
LEVEL_ROOT = "/Game/User_JeHoon/LEVEL/"
CONTROLLER = "/Game/User_JeHoon/Blueprint/Controller/BP_MainMenuPlayerController"
COMMANDS = unreal.SystemLibrary.get_command_line().split()
RESUME_ALIASES = "-LevelFoldersResumeAliases" in COMMANDS
APPLY = "-LevelFoldersApply" in COMMANDS or RESUME_ALIASES
VERIFY_ONLY = "-LevelFoldersVerifyOnly" in COMMANDS
REGISTRY = unreal.AssetRegistryHelpers.get_asset_registry()
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EXPECTED_GROUPS = {"MainMenu": "Core", "Gameplay": "Core", "DebugCombat": "Development", "WorldMap": "Legacy", "DungeonFantasy": "Environment/Dungeon", "DungeonStone": "Environment/Dungeon", "MeadowBloom": "Environment/Grassland", "SavannahGrove": "Environment/Grassland", "PineRidge": "Environment/Forest", "CrimsonForest": "Environment/Forest", "BambooGarden": "Environment/Forest", "DarkMarsh": "Environment/Forest", "DesertCanyon": "Environment/Desert", "DesertOasis": "Environment/Desert", "FrozenPass": "Environment/Ice", "IceCitadel": "Environment/Ice", "PalmCoast": "Environment/Summer", "SunsetLagoon": "Environment/Summer"}
WORLD_PROPERTIES = ["default_game_mode", "world_partition", "world_to_meters", "kill_z", "kill_z_damage_type", "enable_world_bounds_checks", "override_world_gravity", "global_gravity_z", "force_no_precomputed_lighting", "lightmass_settings", "navigation_system_config", "navigation_system_config_override", "default_base_sound_mix", "world_composition", "enable_world_composition"]
ACTOR_PROPERTIES = ["hidden", "is_editor_only_actor", "is_spatially_loaded", "auto_activate_for_player", "auto_possess_player", "auto_possess_ai", "camera_actor", "camera_anchor", "camera_transform", "preview_actor_classes", "focused_camera_distance_scale", "grid", "tile_class", "row_count", "col_count", "spacing", "gap_spacing", "gap_start_index", "default_game_mode", "party_definition", "enemy_definition", "player_unit_classes", "enemy_unit_classes", "player_coords", "enemy_coords", "unbound", "priority", "blend_radius", "blend_weight", "enabled", "settings"]
LIGHT_PROPERTIES = ["intensity", "light_color", "cast_shadows", "cast_static_shadows", "cast_dynamic_shadows", "indirect_lighting_intensity", "volumetric_scattering_intensity", "temperature", "use_temperature", "affects_world", "affect_translucent_lighting", "lighting_channels", "intensity_units", "attenuation_radius", "source_radius", "soft_source_radius", "source_length", "inner_cone_angle", "outer_cone_angle", "light_function_material", "ies_texture", "source_type", "cubemap", "sky_distance_threshold", "real_time_capture", "lower_hemisphere_is_black", "lower_hemisphere_color", "fog_density", "fog_height_falloff", "fog_inscattering_luminance", "fog_max_opacity", "start_distance", "enable_volumetric_fog", "volumetric_fog_scattering_distribution", "volumetric_fog_albedo", "volumetric_fog_emissive", "volumetric_fog_extinction_scale", "volumetric_fog_distance", "material"]
MENU_PROPERTIES = ["gameplay_level_name", "main_menu_root_widget_class", "main_menu_screen_widget_class", "character_creation_widget_class"]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)
    return condition


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def json_hash(value):
    return hashlib.sha256(json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()


def write_report(filename, report):
    DIRECTORY.mkdir(parents=True, exist_ok=True)
    temporary = DIRECTORY / (filename + ".tmp")
    temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(DIRECTORY / filename)


def object_path(package):
    return package + "." + package.rsplit("/", 1)[1]


def content_file(package, extension=".umap"):
    require(package.startswith("/Game/User_JeHoon/") and ".." not in package and "." not in package, "Only an exact project-owned package is allowed: " + package)
    return "Content/" + package.removeprefix("/Game/") + extension


def read_layout():
    layout = json.loads(LAYOUT_FILE.read_text(encoding="utf-8-sig"))
    require(layout.get("schema_version") == 1 and len(layout.get("maps", [])) == 18 and len(layout.get("legacy_redirectors", [])) == 2, "The reviewed schema requires exactly 18 worlds and two original dungeon aliases")
    require({entry["name"] for entry in layout["maps"]} == set(EXPECTED_GROUPS), "The exact reviewed world allowlist differs")
    for entry in layout["maps"]:
        name = entry["name"]
        old_group = "Environment/" if EXPECTED_GROUPS[name].startswith("Environment/") else ""
        require(entry["old_path"] == LEVEL_ROOT + old_group + name and entry["path"] == LEVEL_ROOT + EXPECTED_GROUPS[name] + "/" + name, "World migration differs from the reviewed exact layout: " + name)
    expected_legacy = {LEVEL_ROOT + name: LEVEL_ROOT + "Environment/Dungeon/" + name for name in ["DungeonFantasy", "DungeonStone"]}
    require({entry["old_path"]: entry["path"] for entry in layout["legacy_redirectors"]} == expected_legacy, "Original dungeon alias allowlist differs")
    aliases = layout["maps"] + layout["legacy_redirectors"]
    require(len({entry["old_path"] for entry in aliases}) == 20 and len({entry["path"] for entry in layout["maps"]}) == 18, "World and alias paths must be unique")
    return layout, aliases


def canonical(value, aliases):
    # Normalize only the reviewed package identity; every saved scene value is otherwise compared unchanged.
    # 검토한 패키지 식별자만 정규화하고 나머지 저장된 씬 값은 모두 그대로 비교합니다.
    if isinstance(value, dict):
        return {key: canonical(item, aliases) for key, item in value.items()}
    if isinstance(value, list):
        return [canonical(item, aliases) for item in value]
    if isinstance(value, str):
        for entry in sorted(aliases, key=lambda item: len(item["old_path"]), reverse=True):
            value = re.sub(re.escape(entry["old_path"]) + r"(?=[.:'\"/]|$)", lambda match, path=entry["path"]: path, value)
    return value


def serialize_value(value):
    if value is None or isinstance(value, (str, bool, int)):
        return value
    if isinstance(value, float):
        require(math.isfinite(value), "Nonfinite saved scene value")
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, "export_text"):
        return value.export_text()
    if hasattr(value, "items"):
        return {str(key): serialize_value(item) for key, item in value.items()}
    if isinstance(value, (list, tuple, unreal.Array, unreal.Set)):
        return [serialize_value(item) for item in value]
    return str(value)


def properties(obj, names, mandatory=()):
    result = {}
    for name in names:
        try:
            value = obj.get_editor_property(name)
        except Exception:
            require(name not in mandatory, "Required reflected property unavailable: " + obj.get_class().get_path_name() + "." + name)
            result[name] = {"reflected_property_unavailable": True}
            continue
        result[name] = serialize_value(value)
    return result


def package_data(package):
    return REGISTRY.get_assets_by_package_name(unreal.Name(package), True)


def exact_world(package):
    rows = package_data(package)
    require(len(rows) == 1 and str(rows[0].asset_class_path.asset_name) == "World", "Expected exactly one on-disk World, without redirector resolution: " + package)
    world = unreal.EditorLoadingAndSavingUtils.load_map(package)
    require(isinstance(world, unreal.World) and world.get_path_name() == object_path(package), "The exact typed world did not load: " + package)
    return world


def world_state(package, aliases):
    world = exact_world(package)
    settings = world.get_world_settings()
    require(settings.get_editor_property("world_partition") is None, "WorldPartition migration requires a separate reviewed external package plan: " + package)
    states = []
    for actor in ACTORS.get_all_level_actors():
        components = []
        for component in actor.get_components_by_class(unreal.ActorComponent):
            state = lighting.component_state(component)
            state["lighting_and_environment"] = properties(component, LIGHT_PROPERTIES, ["intensity", "light_color", "cast_shadows"] if isinstance(component, unreal.LightComponentBase) else [])
            components.append(state)
        mandatory = ["preview_actor_classes", "focused_camera_distance_scale"] if isinstance(actor, unreal.MainMenuPreviewStage) else ["grid", "camera_anchor", "camera_transform", "player_coords", "enemy_coords"] if isinstance(actor, unreal.CombatArena) else []
        states.append({"name": actor.get_name(), "label": actor.get_actor_label(), "class": actor.get_class().get_path_name(), "transform": actor.get_actor_transform().export_text(), "collision": actor.get_actor_enable_collision(), "folder": str(actor.get_folder_path()), "parent": serialize_value(actor.get_attach_parent_actor()), "tags": sorted(str(tag) for tag in actor.get_editor_property("tags")), "properties": properties(actor, ACTOR_PROPERTIES, mandatory), "components": sorted(components, key=lambda item: item["name"])})
    state = {"settings": properties(settings, WORLD_PROPERTIES, ["default_game_mode", "world_partition", "world_to_meters", "kill_z", "force_no_precomputed_lighting", "lightmass_settings"]), "actors": sorted(states, key=lambda item: item["name"])}
    return canonical(state, aliases)


def menu_state(aliases):
    blueprint = unreal.load_asset(CONTROLLER)
    require(isinstance(blueprint, unreal.Blueprint) and blueprint.get_path_name() == object_path(CONTROLLER), "The exact existing main menu controller Blueprint is required")
    generated = blueprint.generated_class()
    defaults = unreal.get_default_object(generated) if generated else None
    require(isinstance(defaults, unreal.MainMenuPlayerController), "Main menu generated class or original CDO type is invalid")
    parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)
    require(parent and unreal.MathLibrary.class_is_child_of(generated, parent), "The actual Blueprint parent or generated class ancestry is invalid")
    require(blueprint.get_editor_property("status") is not None, "Blueprint compile status must be readable before any asset mutation")
    state = {"class": generated.get_path_name(), "parent_class": serialize_value(parent), "properties": properties(defaults, MENU_PROPERTIES, MENU_PROPERTIES)}
    return blueprint, defaults, canonical(state, aliases)


def reference_options(hard, soft, other=False):
    return unreal.AssetRegistryDependencyOptions(include_soft_package_references=soft, include_hard_package_references=hard, include_searchable_names=other, include_soft_management_references=other, include_hard_management_references=other)


def reference_state(package):
    # The Python registry API returns None when a removed package has no dependency node.
    # 삭제된 패키지의 의존 노드가 없으면 Python Registry API는 None을 반환합니다.
    def query(method, options):
        return sorted(str(item) for item in (method(unreal.Name(package), options) or []))
    return {"hard": query(REGISTRY.get_referencers, reference_options(True, False)), "soft": query(REGISTRY.get_referencers, reference_options(False, True)), "all_categories": query(REGISTRY.get_referencers, reference_options(True, True, True)), "dependencies_hard": query(REGISTRY.get_dependencies, reference_options(True, False)), "dependencies_soft": query(REGISTRY.get_dependencies, reference_options(False, True)), "dependencies_all_categories": query(REGISTRY.get_dependencies, reference_options(True, True, True))}


def stale_dependencies(aliases):
    # Inspect forward disk-package edges too; deleting a registry node must not conceal an unresolved reference.
    # Registry 노드 삭제가 미해결 참조를 숨기지 않도록 디스크 패키지의 정방향 연결도 검사합니다.
    old_paths = {entry["old_path"] for entry in aliases}
    packages = sorted({"/Game/" + path.relative_to(ROOT / "Content").with_suffix("").as_posix() for path in (ROOT / "Content").rglob("*") if path.is_file() and path.suffix.lower() in [".uasset", ".umap"]})
    result = {}
    options = reference_options(True, True, True)
    for package in packages:
        dependencies = REGISTRY.get_dependencies(unreal.Name(package), options)
        require(dependencies is not None, "A remaining disk package has no dependency record; do not report its references as verified: " + package)
        matches = {str(item) for item in dependencies} & old_paths
        if matches:
            result[package] = sorted(matches)
    return {"disk_packages_checked": len(packages), "old_path_dependencies": result}


def scan_content():
    return {path.relative_to(ROOT).as_posix(): {"bytes": path.stat().st_size, "mtime_ns": path.stat().st_mtime_ns} for path in sorted((ROOT / "Content").rglob("*")) if path.is_file()}


def config_hashes():
    return {path.relative_to(ROOT).as_posix(): digest(path) for path in sorted((ROOT / "Config").rglob("*")) if path.is_file()}


def external_packages():
    markers = ["__ExternalActors__", "__ExternalObjects__", "ExternalActors", "ExternalObjects"]
    prefixes = ["Content/" + marker + "/User_JeHoon/LEVEL/" for marker in markers]
    return sorted(path.relative_to(ROOT).as_posix() for path in (ROOT / "Content").rglob("*") if path.is_file() and (any(path.relative_to(ROOT).as_posix().startswith(prefix) for prefix in prefixes) or (path.relative_to(ROOT).as_posix().startswith("Content/User_JeHoon/LEVEL/") and any(part in markers for part in path.parts))))


def protected_state(layout, aliases, fresh):
    protected = json.loads((DIRECTORY / "ProtectedBefore.json").read_text(encoding="utf-8-sig"))
    allowed = {content_file(entry["old_path"]) for entry in aliases} | {content_file(CONTROLLER, ".uasset")}
    require(set(protected["allowed_existing_files"]) == allowed and len(allowed) == 21, "The root-reviewed existing-file write allowlist must contain exactly 21 packages")
    current = scan_content()
    new_files = {content_file(entry["path"]) for entry in layout["maps"]}
    if fresh:
        require(set(current) == set(protected["content"]) and not (new_files & set(current)), "Fresh audit/apply requires the unchanged original Content set and no destination map")
        for relative in allowed:
            expected = protected["content"][relative]
            require(current[relative]["bytes"] == expected["bytes"] and digest(ROOT / relative) == expected["sha256"], "An original allowed package changed before migration: " + relative)
            require((DIRECTORY / "Before" / relative).is_file() and digest(DIRECTORY / "Before" / relative) == expected["sha256"], "A byte-exact root-owned backup is required: " + relative)
    for relative, expected in protected["content"].items():
        if relative not in allowed:
            require(relative in current and current[relative]["bytes"] == expected["bytes"], "Protected Content file is missing or resized: " + relative)
    current_user = {path.relative_to(ROOT).as_posix() for path in (ROOT / "Saved/SaveGames").rglob("*") if path.is_file()}
    current_user |= {path.relative_to(ROOT).as_posix() for path in (ROOT / "Saved/Config").glob("**/GameUserSettings.ini") if path.is_file()}
    require(current_user == set(protected["user"]), "User save/settings file set changed")
    for relative, expected in protected["user"].items():
        path = ROOT / relative
        require(path.stat().st_size == expected["bytes"] and digest(path) == expected["sha256"], "A user save/settings file changed: " + relative)
    for filename, expected in protected["external"].items():
        path = Path(filename)
        require(path.is_file() and digest(path) == expected, "The original external editor preference changed: " + filename)
    require(not external_packages(), "External Actors/Objects are no longer empty; do not move their files outside engine APIs")
    return protected, current, allowed, new_files


def verify_protected_metadata(before, allowed, expected_files):
    after = scan_content()
    require(set(after) == expected_files, "Content file additions/removals exceed the exact level migration allowlist")
    for relative, expected in before.items():
        if relative not in allowed:
            require(after.get(relative) == expected, "A protected Content file changed during this engine run: " + relative)
    require(not external_packages(), "Migration produced unexpected External Actors/Objects")
    return after


def resume_aliases_preflight(layout, aliases, protected, before, allowed, report):
    # Resume only the observed unattended cancellation after deleting the two original aliases, before any world moved.
    # 기존 별칭 두 개 삭제 후 맵 이동 전 관찰된 무인 실행 취소 단계만 재개합니다.
    previous_file = DIRECTORY / "Apply.json"
    previous = json.loads(previous_file.read_text(encoding="utf-8"))
    legacy = {entry["old_path"] for entry in layout["legacy_redirectors"]}
    legacy_files = {content_file(package) for package in legacy}
    require(previous.get("mode") == "apply" and previous.get("completed") is False and previous.get("passed") is False and previous.get("layout_sha256") == report["layout_sha256"], "Resume requires the exact incomplete apply record for this layout")
    require(previous.get("error") == "Official AssetTools world rename failed: " + LEVEL_ROOT + "MainMenu" and previous.get("renamed_worlds") == [] and previous.get("worlds") == [] and previous.get("menu_blueprint_save_count") == 0 and previous.get("old_paths_removed_by_asset_tools") == [] and previous.get("alias_resolution") == [], "Only the observed first MainMenu rename cancellation may resume; arbitrary partial migration is forbidden")
    require(len(previous.get("redirectors_removed", [])) == 2 and set(previous["redirectors_removed"]) == legacy, "Resume requires explicit successful deletion of exactly the two original aliases")
    require(set(before) == set(protected["content"]) - legacy_files, "Resume Content must differ only by the two deleted original alias packages")
    for relative in allowed:
        expected = protected["content"][relative]
        require((DIRECTORY / "Before" / relative).is_file() and digest(DIRECTORY / "Before" / relative) == expected["sha256"], "Resume requires all original package backups unchanged: " + relative)
        if relative not in legacy_files:
            require(relative in before and before[relative]["bytes"] == expected["bytes"] and digest(ROOT / relative) == expected["sha256"], "Resume requires all eighteen original worlds and the original controller unchanged: " + relative)
    for entry in layout["maps"]:
        require(not (ROOT / content_file(entry["path"])).exists() and not package_data(entry["path"]), "Resume requires every destination world absent: " + entry["path"])
    for package in legacy:
        require(not (ROOT / content_file(package)).exists() and not package_data(package), "Resume requires both original aliases absent: " + package)
    archive = DIRECTORY / "Apply.BeforeResumeAliases.json"
    if archive.exists():
        require(json.loads(archive.read_text(encoding="utf-8")) == previous, "Never replace a different archived first-failure apply record")
    else:
        write_report(archive.name, previous)
    report["resume_aliases_previous_apply"] = {"report_sha256": digest(previous_file), "archive": archive.name, "record": previous}
    report["redirectors_removed"] = list(previous["redirectors_removed"])


def audit(layout, aliases, before, allowed, config_before, report):
    for entry in layout["maps"]:
        state = world_state(entry["old_path"], aliases)
        report["worlds"].append({"name": entry["name"], "old_path": entry["old_path"], "path": entry["path"], "state": state, "state_sha256": json_hash(state), "references": reference_state(entry["old_path"])})
    for entry in layout["legacy_redirectors"]:
        data = package_data(entry["old_path"])
        require(len(data) == 1 and str(data[0].asset_class_path.asset_name) == "ObjectRedirector", "The exact original dungeon alias must be a single redirector: " + entry["old_path"])
    blueprint, defaults, state = menu_state(aliases)
    require(str(defaults.get_editor_property("gameplay_level_name")) == LEVEL_ROOT + "Gameplay", "Audit requires the original serialized Gameplay level name")
    report["menu"] = state
    report["menu_blueprint_status"] = str(blueprint.get_editor_property("status"))
    report["alias_references"] = {entry["old_path"]: reference_state(entry["old_path"]) for entry in aliases}
    verify_protected_metadata(before, set(), set(before))
    require(config_hashes() == config_before, "Read-only audit changed project configuration")
    report.update(completed=True, passed=True, original_packages_unchanged=True, protected_content_metadata=before)
    baseline = DIRECTORY / "Baseline.json"
    if baseline.exists():
        previous = json.loads(baseline.read_text(encoding="utf-8"))
        require(previous.get("completed") and previous.get("layout_sha256") == report["layout_sha256"] and previous.get("worlds") == report["worlds"] and previous.get("menu") == state, "Never overwrite a different reviewed scene baseline")
    else:
        write_report("Baseline.json", report)


def verify_worlds(layout, aliases, baseline, report):
    expected = {entry["name"]: entry for entry in baseline["worlds"]}
    for entry in layout["maps"]:
        state = world_state(entry["path"], aliases)
        require(state == expected[entry["name"]]["state"], "Saved geometry, actors, instances, lighting or WorldSettings differ: " + entry["path"])
        report["worlds"].append({"name": entry["name"], "path": entry["path"], "state_sha256": json_hash(state), "state_preserved": True, "references": reference_state(entry["path"])})
    _, defaults, state = menu_state(aliases)
    gameplay = next(entry["path"] for entry in layout["maps"] if entry["name"] == "Gameplay")
    require(str(defaults.get_editor_property("gameplay_level_name")) == gameplay and state == baseline["menu"], "Menu controller CDO or original widget bindings differ")
    report["menu"] = state


def remove_exact_redirectors(aliases, report):
    # Delete only empty-reference redirector packages; stop for an engine FixupReferencers bridge instead of force deletion.
    # 참조가 빈 Redirector 패키지만 삭제하며 강제 삭제 대신 엔진 FixupReferencers 보조 기능이 필요하면 중단합니다.
    pending = {entry["old_path"] for entry in aliases}
    while pending:
        progressed = False
        for package in sorted(pending):
            data = package_data(package)
            if not data:
                require(not (ROOT / content_file(package)).exists(), "An old physical package is missing from the registry: " + package)
                if package not in report["redirectors_removed"]:
                    report["old_paths_removed_by_asset_tools"].append(package)
                pending.remove(package)
                progressed = True
                continue
            require(len(data) == 1 and str(data[0].asset_class_path.asset_name) == "ObjectRedirector", "Refuse to delete anything other than the exact old redirector: " + package)
            references = reference_state(package)
            if references["all_categories"]:
                continue
            require(unreal.WarriorAssetLibrary.remove_unused_asset_redirector(unreal.Name(package)), "Official unused redirector cleanup rejected: " + package)
            require(not (ROOT / content_file(package)).exists(), "Official cleanup did not remove the old package: " + package)
            pending.remove(package)
            report["redirectors_removed"].append(package)
            REGISTRY.scan_paths_synchronous([LEVEL_ROOT.rstrip("/")], True)
            progressed = True
        if not progressed:
            report["fixup_required"] = {package: reference_state(package) for package in sorted(pending)}
            write_report("Apply.json", report)
            raise RuntimeError("Old redirectors retain registry package references; root must provide a reviewed engine FixupReferencers bridge before cleanup")


def apply(layout, aliases, baseline, report):
    require(baseline.get("completed") and baseline.get("mode") == "audit" and baseline.get("layout_sha256") == report["layout_sha256"], "A complete unchanged audit baseline is required before engine mutation")
    writable_packages = {entry["old_path"] for entry in aliases} | {CONTROLLER}
    for entry in aliases:
        references = reference_state(entry["old_path"])
        require(set(references["all_categories"]) <= writable_packages, "Rename would rewrite a package outside the reviewed 21-file allowlist: " + entry["old_path"])
    expected = {entry["name"]: entry["state"] for entry in baseline["worlds"]}
    for entry in layout["maps"]:
        require(world_state(entry["old_path"], aliases) == expected[entry["name"]], "An original saved world changed since audit: " + entry["old_path"])
    _, defaults, previous_menu = menu_state(aliases)
    require(previous_menu == baseline["menu"] and str(defaults.get_editor_property("gameplay_level_name")) == LEVEL_ROOT + "Gameplay", "The menu CDO changed before any engine mutation")
    if not RESUME_ALIASES:
        remove_exact_redirectors(layout["legacy_redirectors"], report)
    write_report("Apply.json", report)
    for entry in layout["maps"]:
        world = exact_world(entry["old_path"])
        rename = unreal.AssetRenameData(asset=world, new_package_path=entry["path"].rsplit("/", 1)[0], new_name=entry["name"])
        require(ASSET_TOOLS.rename_assets([rename]), "Official AssetTools world rename failed: " + entry["old_path"])
        require(world.get_path_name() == object_path(entry["path"]), "Official world rename produced an unexpected identity")
        require(unreal.EditorLoadingAndSavingUtils.save_map(world, entry["path"]), "Official save of the moved existing world failed: " + entry["path"])
        report["renamed_worlds"].append(entry)
        write_report("Apply.json", report)
    blueprint, defaults, previous_menu = menu_state(aliases)
    require(previous_menu == baseline["menu"] and str(defaults.get_editor_property("gameplay_level_name")) == LEVEL_ROOT + "Gameplay", "The menu CDO changed before the one-field update")
    gameplay = next(entry["path"] for entry in layout["maps"] if entry["name"] == "Gameplay")
    defaults.set_editor_property("gameplay_level_name", unreal.Name(gameplay))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    status = str(blueprint.get_editor_property("status"))
    report["menu_blueprint_compile_status"] = status
    require("UPTODATE" in re.sub(r"[^A-Z]", "", status.upper()) and menu_state(aliases)[2] == baseline["menu"], "Menu Blueprint compilation must finish up to date while preserving other inspected defaults: " + status)
    require(unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False), "Official save of the original menu controller failed")
    report["menu_blueprint_save_count"] = 1
    REGISTRY.scan_paths_synchronous(["/Game/User_JeHoon"], True)
    verify_worlds(layout, aliases, baseline, report)
    remove_exact_redirectors(aliases, report)
    report["alias_references"] = {entry["old_path"]: reference_state(entry["old_path"]) for entry in aliases}
    removed = set(report["redirectors_removed"])
    engine_removed = set(report["old_paths_removed_by_asset_tools"])
    require(not (removed & engine_removed) and len(removed) == len(report["redirectors_removed"]) and len(engine_removed) == len(report["old_paths_removed_by_asset_tools"]) and removed | engine_removed == {entry["old_path"] for entry in aliases}, "Official cleanup must account for all twenty old paths without assuming AssetTools always creates a redirector")
    require({entry["old_path"] for entry in layout["legacy_redirectors"]} <= removed, "Both original dungeon redirectors require explicit official cleanup evidence")
    report["old_paths_absent"] = [entry["old_path"] for entry in aliases if not (ROOT / content_file(entry["old_path"])).exists() and not package_data(entry["old_path"])]
    require(len(report["old_paths_absent"]) == 20, "Old level packages or redirectors remain after official cleanup")
    require(all(not value["all_categories"] for value in report["alias_references"].values()), "Stale registry references remain after exact redirector cleanup")
    report["stale_dependency_scan"] = stale_dependencies(aliases)
    require(not report["stale_dependency_scan"]["old_path_dependencies"], "Remaining disk packages retain dependencies on an old level path")
    report["old_alias_resolution"] = "pending root CoreRedirect configuration; final VerifyOnly must run in a fresh engine process"


def verify(layout, aliases, baseline, report):
    authored = json.loads((DIRECTORY / "Apply.json").read_text(encoding="utf-8"))
    require(authored.get("completed") and authored.get("layout_sha256") == report["layout_sha256"], "A complete engine apply record is required for final reload verification")
    verify_worlds(layout, aliases, baseline, report)
    for entry in aliases:
        require(not (ROOT / content_file(entry["old_path"])).exists() and not package_data(entry["old_path"]), "An old alias still has a physical asset redirector: " + entry["old_path"])
        references = reference_state(entry["old_path"])
        require(not references["all_categories"], "Final registry retains stale references: " + entry["old_path"])
        resolved = unreal.WarriorAssetLibrary.load_saved_asset_reference(unreal.SoftObjectPath(object_path(entry["old_path"])))
        require(isinstance(resolved, unreal.World) and resolved.get_path_name() == object_path(entry["path"]), "The historical map alias does not resolve through the configured engine CoreRedirect: " + entry["old_path"])
        report["alias_resolution"].append({"old_path": entry["old_path"], "path": resolved.get_path_name(), "references": references})
    report["stale_dependency_scan"] = stale_dependencies(aliases)
    require(not report["stale_dependency_scan"]["old_path_dependencies"], "Final disk packages retain dependencies on an old level path")
    report["menu_blueprint_save_count"] = 0


def main():
    require(not (APPLY and VERIFY_ONLY), "Apply and VerifyOnly cannot be combined")
    layout, aliases = read_layout()
    mode = "apply" if APPLY else "verify" if VERIFY_ONLY else "audit"
    filename = "Apply.json" if APPLY else "Verify.json" if VERIFY_ONLY else "Audit.json"
    report = {"mode": mode, "completed": False, "passed": False, "layout_sha256": json_hash(layout), "worlds": [], "renamed_worlds": [], "redirectors_removed": [], "old_paths_removed_by_asset_tools": [], "alias_resolution": [], "menu_blueprint_save_count": 0, "external_actor_object_files": 0, "gameplay_test": "not run", "render_test": "not run", "pie_test": "not run", "protection_limits": "Protected Content size/mtime and relevant package/user SHA are checked here; the root independently verifies every protected Content byte hash. Snapshots inspect saved actor/component/instance geometry, reflected lighting/environment properties, camera post process and WorldSettings; unavailable optional properties are recorded rather than claimed inspected."}
    config_before = config_hashes()
    try:
        REGISTRY.search_all_assets(True)
        protected, before, allowed, new_files = protected_state(layout, aliases, not (VERIFY_ONLY or RESUME_ALIASES))
        if not APPLY and not VERIFY_ONLY:
            audit(layout, aliases, before, allowed, config_before, report)
        else:
            baseline = json.loads((DIRECTORY / "Baseline.json").read_text(encoding="utf-8"))
            require(baseline.get("completed") and baseline.get("layout_sha256") == report["layout_sha256"], "Reviewed original scene baseline differs")
            if RESUME_ALIASES:
                resume_aliases_preflight(layout, aliases, protected, before, allowed, report)
            if APPLY:
                apply(layout, aliases, baseline, report)
            else:
                verify(layout, aliases, baseline, report)
            expected_files = (set(protected["content"]) - {content_file(entry["old_path"]) for entry in aliases}) | new_files
            verify_protected_metadata(baseline["protected_content_metadata"], allowed, expected_files)
            require(config_hashes() == config_before, "This engine run changed project configuration instead of leaving CoreRedirect changes to root")
            report.update(completed=True, passed=True, original_world_state_preserved=True)
        protected_state(layout, aliases, not (APPLY or VERIFY_ONLY))
        report.update(protected_user_files_unchanged=len(protected["user"]), protected_content_file_count=len(protected["content"]) - len(allowed), original_backups="Saved/Automation/LevelFolders/Before", protected_content_metadata_unchanged=True)
        write_report(filename, report)
        unreal.log("LEVEL_FOLDERS_COMPLETE " + str(DIRECTORY / filename))
    except Exception as error:
        report.update(completed=False, passed=False, error=str(error))
        failure_filename = "ResumeAliases.PreflightFailed.json" if RESUME_ALIASES and "resume_aliases_previous_apply" not in report else filename
        write_report(failure_filename, report)
        unreal.log_error("LEVEL_FOLDERS_FAILED " + str(error))
        raise


if __name__ == "__main__":
    main()
