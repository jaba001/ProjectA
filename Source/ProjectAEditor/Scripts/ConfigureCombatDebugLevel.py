import hashlib
import json
from collections import Counter
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_MAP = "/Game/User_JeHoon/LEVEL/Core/Gameplay"
DEBUG_MAP = "/Game/User_JeHoon/LEVEL/Development/DebugCombat"
MODE_PATH = "/Game/User_JeHoon/Blueprint/Game/BP_CombatDebugGameMode"
PARTY_PATH = "/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty"
ENEMY_PATH = "/Game/User_JeHoon/Blueprint/DataAsset/Encounters/DA_DefaultEncounter"
TEST_POOL_PATH = "/Game/User_JeHoon/Blueprint/DataAsset/Encounters/DA_RunEncounterPool_TestSkills"
VERIFY_ONLY = "-CombatDebugVerifyOnly" in unreal.SystemLibrary.get_command_line()
ASSETS = unreal.EditorAssetLibrary
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def digest(filename):
    result = hashlib.sha256()
    with filename.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def package_file(package, extension=".uasset"):
    require(package.startswith("/Game/"), "Unexpected package root: " + package)
    return ROOT / "Content" / (package.removeprefix("/Game/") + extension)


def source_hashes():
    files = [package_file(SOURCE_MAP, ".umap")]
    build_data = package_file(SOURCE_MAP + "_BuiltData")
    if build_data.exists():
        files.append(build_data)
    level_relative = SOURCE_MAP.removeprefix("/Game/")
    for category in ["__ExternalActors__", "__ExternalObjects__"]:
        directory = ROOT / "Content" / category / level_relative
        if directory.exists():
            files.extend(sorted(path for path in directory.rglob("*") if path.is_file()))
    return {str(path.relative_to(ROOT)): digest(path) for path in files}


def coordinates(values):
    return [(value.x, value.y) for value in values]


def normalized(value):
    if isinstance(value, unreal.StructBase):
        return value.export_text()
    if isinstance(value, (unreal.Name, unreal.Text)):
        return str(value)
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, "items"):
        return {str(key): normalized(item) for key, item in sorted(value.items(), key=lambda pair: str(pair[0]))}
    if isinstance(value, (list, tuple, unreal.Array)):
        return [normalized(item) for item in value]
    return value


def inspect_level(package):
    world = require(unreal.EditorLoadingAndSavingUtils.load_map(package), "Could not load level: " + package)
    require(world.get_path_name() == package + "." + package.rsplit("/", 1)[1], "Unexpected loaded world")
    actors = ACTORS.get_all_level_actors()
    arenas = [actor for actor in actors if isinstance(actor, unreal.CombatArena)]
    grids = [actor for actor in actors if isinstance(actor, unreal.CombatGridManager)]
    require(len(arenas) == 1 and len(grids) == 1, "Level must contain exactly one arena and grid: " + package)
    arena, grid = arenas[0], grids[0]
    require(arena.get_editor_property("grid") == grid, "Arena grid must belong to its loaded level")
    camera = require(arena.get_editor_property("camera_anchor"), "Arena camera is missing")
    require(isinstance(camera, unreal.CameraActor) and camera in actors, "Arena camera must be a placed actor in the same level")
    require(not any(isinstance(actor, unreal.UnitBase) or isinstance(actor, unreal.CombatManager) for actor in actors), "Level contains placed combat units or managers")
    classes = Counter(actor.get_class().get_path_name() for actor in actors)
    require(classes["/Script/NavigationSystem.NavMeshBoundsVolume"] > 0, "Navigation bounds are missing")
    require(classes["/Script/NavigationSystem.RecastNavMesh"] > 0, "Navigation data actor is missing")
    tile_class = require(grid.get_editor_property("tile_class"), "Grid tile class is missing")
    camera_component = camera.get_editor_property("camera_component")
    layout = {
        "actor_classes": dict(sorted(classes.items())),
        "grid_class": grid.get_class().get_path_name(),
        "grid_transform": grid.get_actor_transform().export_text(),
        "tile_class": tile_class.get_path_name(),
        "rows": grid.get_editor_property("row_count"),
        "columns": grid.get_editor_property("col_count"),
        "player_coords": coordinates(arena.get_editor_property("player_coords")),
        "enemy_coords": coordinates(arena.get_editor_property("enemy_coords")),
        "arena_transform": arena.get_actor_transform().export_text(),
        "camera_transform": camera.get_actor_transform().export_text(),
        "camera_fov": camera_component.get_editor_property("field_of_view"),
        "camera_constrain_aspect": camera_component.get_editor_property("constrain_aspect_ratio"),
        "camera_override_axis": camera_component.get_editor_property("override_aspect_ratio_axis_constraint"),
        "camera_axis": str(camera_component.get_editor_property("aspect_ratio_axis_constraint")),
    }
    require(layout["rows"] == 4 and layout["columns"] == 4, "Expected the current 4 x 4 combat grid")
    require(len(layout["player_coords"]) == 4 and len(layout["enemy_coords"]) == 4, "Expected the current four-slot arena")
    return world, layout


def main():
    protected = source_hashes()
    enemy_hash = digest(package_file(ENEMY_PATH))
    skill_files = sorted((ROOT / "Content/User_JeHoon/Blueprint/DataAsset/Skills").rglob("*.uasset"))
    skill_hashes = {str(path.relative_to(ROOT)): digest(path) for path in skill_files}
    party = require(unreal.load_asset(PARTY_PATH), "Party definition is missing")
    enemy = require(unreal.load_asset(ENEMY_PATH), "Enemy definition is missing")
    require(isinstance(party, unreal.PartyDefinitionDataAsset) and isinstance(enemy, unreal.EncounterDefinitionDataAsset), "Unexpected combat definition classes")
    require(len(enemy.get_editor_property("enemy_unit_classes")) == 4, "Debug level expects the four existing enemies")
    require(str(enemy.get_editor_property("opponent_snapshot_slot")) == "None", "Debug enemies must not load a saved opponent snapshot")
    professions = party.get_editor_property("professions")
    require(unreal.Name("Warrior") in professions, "Warrior profession is missing")
    warrior = professions[unreal.Name("Warrior")]
    player_classes = party.get_editor_property("player_unit_classes")
    player_class = warrior.get_editor_property("combat_class") or player_classes.get(unreal.Name("Warrior")) or party.get_editor_property("fallback_player_unit_class")
    player_defaults = unreal.get_default_object(require(player_class, "Warrior combat class is missing"))
    require(isinstance(player_defaults, unreal.PlayerUnit), "Warrior combat class must derive from PlayerUnit")
    appearance = require(player_defaults.get_editor_property("character_appearance"), "Warrior appearance component is missing")
    class_catalog = appearance.get_editor_property("appearance_catalog")
    catalog = warrior.get_editor_property("appearance_catalog") or class_catalog
    require(catalog == class_catalog, "Warrior profession and unit appearance catalogs differ")
    if catalog:
        accepted = catalog.validate_selection(unreal.CharacterAppearanceSelection())
        require(accepted[0] if isinstance(accepted, tuple) else accepted, "Warrior default appearance selection is invalid")
    require(party.get_editor_property("unarmed_starting_skill"), "Shared unarmed starting skill is missing")
    require(all(value and isinstance(unreal.get_default_object(value), unreal.EnemyUnit) for value in enemy.get_editor_property("enemy_unit_classes")), "Enemy classes must derive from EnemyUnit")
    party_properties = ["unarmed_starting_skill", "encounter_skill_pool", "professions", "player_unit_classes", "fallback_player_unit_class"]
    party_before = {name: normalized(party.get_editor_property(name)) for name in party_properties}
    current_pool = party.get_editor_property("run_encounter_pool")
    require(not current_pool or current_pool.get_path_name() == TEST_POOL_PATH + "." + TEST_POOL_PATH.rsplit("/", 1)[1], "Party uses a different encounter pool; refusing to replace it")
    mode_class = require(unreal.load_class(None, "/Script/ProjectA.CombatDebugGameMode"), "Compile the debug GameMode before authoring assets")
    controller_class = require(unreal.load_class(None, "/Script/ProjectA.CombatDebugPlayerController"), "Compile the debug controller before authoring assets")
    mode_exists = ASSETS.does_asset_exist(MODE_PATH)
    map_exists = ASSETS.does_asset_exist(DEBUG_MAP)
    require(not VERIFY_ONLY or (mode_exists and map_exists), "Debug level and GameMode must exist before reload verification")
    if mode_exists:
        mode = require(unreal.load_asset(MODE_PATH), "Debug GameMode Blueprint failed to load")
        require(isinstance(mode, unreal.Blueprint) and isinstance(unreal.get_default_object(mode.generated_class()), unreal.CombatDebugGameMode), "Existing debug GameMode has a different native parent")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", mode_class)
        directory, name = MODE_PATH.rsplit("/", 1)
        mode = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, directory, unreal.Blueprint, factory), "Could not create debug GameMode Blueprint")
        defaults = unreal.get_default_object(mode.generated_class())
        defaults.set_editor_property("party_definition", party)
        defaults.set_editor_property("enemy_definition", enemy)
        unreal.BlueprintEditorLibrary.compile_blueprint(mode)
        require(ASSETS.save_loaded_asset(mode), "Could not save debug GameMode")
    defaults = unreal.get_default_object(mode.generated_class())
    require(defaults.get_editor_property("party_definition") == party and defaults.get_editor_property("enemy_definition") == enemy, "Debug combat definitions changed; inspect before overwriting")
    require(defaults.get_editor_property("player_controller_class") == controller_class, "Debug GameMode must use the native isolated controller")
    _, source_layout = inspect_level(SOURCE_MAP)
    if not map_exists:
        # Let Unreal duplicate and remap the level's internal actor references.
        # Unreal이 레벨을 복제하고 내부 액터 참조를 새 레벨로 갱신하도록 합니다.
        require(ASSETS.duplicate_asset(SOURCE_MAP, DEBUG_MAP), "Could not duplicate Gameplay as DebugCombat")
    world, debug_layout = inspect_level(DEBUG_MAP)
    require(debug_layout == source_layout, "Debug level layout differs from the current Gameplay level")
    settings = world.get_world_settings()
    if not map_exists:
        settings.set_editor_property("default_game_mode", mode.generated_class())
        require(unreal.EditorLoadingAndSavingUtils.save_map(world, DEBUG_MAP), "Could not save debug level")
    require(settings.get_editor_property("default_game_mode") == mode.generated_class(), "Debug level must override only its own GameMode")
    removed_pool = False
    if VERIFY_ONLY:
        require(not current_pool, "Party still references the test skill shop")
        require(not ASSETS.does_asset_exist(TEST_POOL_PATH), "Obsolete test shop pool still exists")
    else:
        if current_pool:
            party.set_editor_property("run_encounter_pool", None)
            require(ASSETS.save_loaded_asset(party), "Could not restore the native encounter pool fallback")
        if ASSETS.does_asset_exist(TEST_POOL_PATH):
            pool = require(unreal.load_asset(TEST_POOL_PATH), "Could not inspect the obsolete test pool")
            require(isinstance(pool, unreal.RunEncounterPoolDataAsset), "Obsolete pool has an unexpected class")
            referencers = ASSETS.find_package_referencers_for_asset(TEST_POOL_PATH, True)
            require(not referencers, "Obsolete test pool remains referenced: " + str(referencers))
            require(ASSETS.delete_asset(TEST_POOL_PATH), "Could not remove obsolete test pool through Unreal")
            removed_pool = True
    require({name: normalized(party.get_editor_property(name)) for name in party_properties} == party_before, "Unrelated party properties changed")
    require(not party.get_editor_property("run_encounter_pool"), "Party did not return to its native normal shop defaults")
    require(source_hashes() == protected, "Source Gameplay level or external actor/object package changed")
    require(digest(package_file(ENEMY_PATH)) == enemy_hash, "Original enemy definition changed")
    require(skill_hashes == {str(path.relative_to(ROOT)): digest(path) for path in skill_files}, "Original skill definitions changed")
    report = {
        "mode": "reload" if VERIFY_ONLY else "configure",
        "level": DEBUG_MAP,
        "game_mode": mode.generated_class().get_path_name(),
        "controller": controller_class.get_path_name(),
        "party": PARTY_PATH,
        "enemy": ENEMY_PATH,
        "warrior_class": player_class.get_path_name(),
        "default_appearance_valid": True,
        "layout": debug_layout,
        "source_level_hashes": protected,
        "source_level_unchanged": True,
        "skill_packages_unchanged": len(skill_hashes),
        "party_pool": None,
        "test_pool_removed": removed_pool,
        "test_pool_absent": not ASSETS.does_asset_exist(TEST_POOL_PATH),
        "gameplay_test": "not run",
    }
    output = ROOT / "Saved/Automation/CombatDebug" / ("Reload.json" if VERIFY_ONLY else "Configuration.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("COMBAT_DEBUG_AUTHORING_COMPLETE " + str(output))


if __name__ == "__main__":
    main()
